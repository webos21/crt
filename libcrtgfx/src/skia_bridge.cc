#include "crtgfx/skia.h"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"

#include <cstring>

sk_sp<SkSurface> crtgfx_skia_make_raster_surface(const crtgfx_framebuffer* framebuffer) {
  if (framebuffer == nullptr || framebuffer->pixels == nullptr || framebuffer->width == 0 ||
      framebuffer->height == 0 || framebuffer->stride < framebuffer->width * 4u ||
      framebuffer->format != CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED) {
    return nullptr;
  }

  SkImageInfo info = SkImageInfo::Make(
      (int)framebuffer->width, (int)framebuffer->height, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType);
  return SkSurfaces::WrapPixels(info, framebuffer->pixels, framebuffer->stride);
}

// crtgfx_skia_default_typeface() itself now lives inline in crtgfx/skia.h,
// not here -- see that declaration's own comment for the real, confirmed
// cross-DLL-boundary crash (2026-09-12) this fixed.

#if defined(CRTGFX_HAVE_VULKAN)

// Real Ganesh/Vulkan offscreen vertical slice (2026-09-03) -- see crtgfx/
// skia.h's own, fuller comment on both functions below.

#include "gpu_internal.h"

#include <cstdio>

#include "include/gpu/GpuTypes.h"
#include "include/gpu/MutableTextureState.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "include/gpu/vk/VulkanMemoryAllocator.h"
#include "include/gpu/vk/VulkanMutableTextureState.h"
#include "include/gpu/vk/VulkanTypes.h"

namespace {

// A real, minimal, "one dedicated VkDeviceMemory per allocation, no
// suballocation" skgpu::VulkanMemoryAllocator -- this vertical slice's own
// deliberate substitute for Skia's real default allocator (AMD's VMA,
// wired in via skgpu::VulkanMemoryAllocators::Make()). Confirmed for real
// (2026-09-03) that this project's own Skia build cannot use that default
// at all: tools/build_skia.py deliberately sets skia_use_vma=false
// (avoiding a new third_party/externals/vulkanmemoryallocator vendor
// checkout this project's fetch pipeline does not provide), and
// GrVkGpu::Make() (src/gpu/ganesh/vk/GrVkGpu.cpp) compiles that entire
// internal fallback path out under that same GN flag -- leaving
// GrVkBackendContext::fMemoryAllocator null under this exact build config
// made GrDirectContexts::MakeVulkan() fail *silently* (SkDEBUGFAIL is a
// no-op in this project's own official/release Skia build, so nothing
// printed at all), not because of any real Vulkan/device problem. A real,
// suballocating allocator is explicitly out of scope for this vertical
// slice (a real production concern once this contract needs to handle
// many resources efficiently, not this slice's own "prove Ganesh/Vulkan
// rendering correctness" goal) -- every allocation here gets its own
// dedicated VkDeviceMemory, correct but not space- or count-efficient.
struct DumbVulkanAlloc {
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  bool mappable = false;
};

class DumbVulkanMemoryAllocator : public skgpu::VulkanMemoryAllocator {
 public:
  DumbVulkanMemoryAllocator(VkDevice device, VkPhysicalDevice physical_device)
      : fDevice(device), fPhysicalDevice(physical_device) {}

  VkResult allocateImageMemory(
      VkImage image, uint32_t /*allocationPropertyFlags*/,
      skgpu::VulkanBackendMemory* out) override {
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(fDevice, image, &requirements);
    // Ganesh itself calls vkBindImageMemory() afterward using getAllocInfo()'s
    // own fMemory/fOffset (confirmed by reading the real default allocator,
    // VulkanAMDMemoryAllocator::allocateImageMemory -- it calls
    // vmaAllocateMemoryForImage(), VMA's own "allocate but do not bind"
    // entry point, matching that exact division of responsibility) -- this
    // function only ever allocates, never binds.
    return this->allocate(requirements, /*want_host_visible=*/false, out);
  }

  VkResult allocateBufferMemory(
      VkBuffer buffer, BufferUsage usage, uint32_t /*allocationPropertyFlags*/,
      skgpu::VulkanBackendMemory* out) override {
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(fDevice, buffer, &requirements);
    bool want_host_visible = (usage != BufferUsage::kGpuOnly);
    return this->allocate(requirements, want_host_visible, out);
  }

  void getAllocInfo(
      const skgpu::VulkanBackendMemory& handle, skgpu::VulkanAlloc* out) const override {
    const DumbVulkanAlloc* record = reinterpret_cast<const DumbVulkanAlloc*>(handle);
    out->fMemory = record->memory;
    out->fOffset = 0;
    out->fSize = record->size;
    out->fFlags = record->mappable ? skgpu::VulkanAlloc::kMappable_Flag : 0;
    out->fBackendMemory = handle;
  }

  VkResult mapMemory(const skgpu::VulkanBackendMemory& handle, void** data) override {
    const DumbVulkanAlloc* record = reinterpret_cast<const DumbVulkanAlloc*>(handle);
    return vkMapMemory(fDevice, record->memory, 0, record->size, 0, data);
  }

  void unmapMemory(const skgpu::VulkanBackendMemory& handle) override {
    const DumbVulkanAlloc* record = reinterpret_cast<const DumbVulkanAlloc*>(handle);
    vkUnmapMemory(fDevice, record->memory);
  }

  // flushMappedMemory()/invalidateMappedMemory() deliberately left at the
  // base class's own real, non-pure default (a no-op): allocate() below
  // only ever requests HOST_COHERENT memory for anything host-visible, so
  // there is never non-coherent memory this allocator would need to flush
  // or invalidate by hand.

  void freeMemory(const skgpu::VulkanBackendMemory& handle) override {
    DumbVulkanAlloc* record = reinterpret_cast<DumbVulkanAlloc*>(handle);
    vkFreeMemory(fDevice, record->memory, nullptr);
    delete record;
  }

  std::pair<uint64_t, uint64_t> totalAllocatedAndUsedMemory() const override {
    // Not tracked -- nothing in this vertical slice reads this; a real
    // accounting story is exactly the kind of production concern this
    // deliberately minimal allocator does not take on.
    return {0, 0};
  }

 private:
  VkResult allocate(
      const VkMemoryRequirements& requirements, bool want_host_visible,
      skgpu::VulkanBackendMemory* out) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(fPhysicalDevice, &memory_properties);

    VkMemoryPropertyFlags desired = want_host_visible
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    int type_index = -1;
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
      bool type_allowed = (requirements.memoryTypeBits & (1u << i)) != 0;
      bool has_desired = (memory_properties.memoryTypes[i].propertyFlags & desired) == desired;
      if (type_allowed && has_desired) {
        type_index = static_cast<int>(i);
        break;
      }
    }
    if (type_index < 0) {
      // Fall back to any memory type the resource itself allows at all,
      // ignoring the preferred property flags -- still correct, just not
      // necessarily optimal (matches this allocator's own deliberately
      // minimal scope).
      for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) != 0) {
          type_index = static_cast<int>(i);
          break;
        }
      }
    }
    if (type_index < 0) {
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    VkMemoryAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = static_cast<uint32_t>(type_index);

    VkDeviceMemory memory;
    VkResult result = vkAllocateMemory(fDevice, &info, nullptr, &memory);
    if (result != VK_SUCCESS) {
      return result;
    }

    DumbVulkanAlloc* record = new DumbVulkanAlloc();
    record->memory = memory;
    record->size = requirements.size;
    record->mappable = want_host_visible;
    *out = reinterpret_cast<skgpu::VulkanBackendMemory>(record);
    return VK_SUCCESS;
  }

  VkDevice fDevice;
  VkPhysicalDevice fPhysicalDevice;
};

}  // namespace

sk_sp<GrDirectContext> crtgfx_skia_make_gpu_context(const crtgfx_gpu_device* device) {
  crtgfx_gpu_vulkan_device_view view = {};
  if (!crtgfx_gpu_vulkan_borrow_device(device, &view)) {
    return nullptr;
  }

  skgpu::VulkanBackendContext backend_context;
  backend_context.fInstance = reinterpret_cast<VkInstance>(view.instance);
  backend_context.fPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(view.physical_device);
  backend_context.fDevice = reinterpret_cast<VkDevice>(view.device);
  backend_context.fQueue = reinterpret_cast<VkQueue>(view.queue);
  backend_context.fGraphicsQueueIndex = view.queue_family_index;
  // Must match (or exceed) src/arch/linux/gpu_vulkan.c's own real
  // VkApplicationInfo::apiVersion -- Skia's own Ganesh Vulkan backend
  // refuses anything below Vulkan 1.1 (see that file's own comment on the
  // exact real error this mismatch produced the first time this vertical
  // slice actually ran).
  backend_context.fMaxAPIVersion = VK_API_VERSION_1_1;
  // Real, standard get-proc-addr trampoline: prefer the device-level loader
  // entry point once a real VkDevice exists (skips one real dispatch-table
  // indirection per Vulkan's own documented guidance), falling back to the
  // instance-level one otherwise. Both are real, directly-linked functions
  // here (this translation unit links libvulkan directly, exactly like
  // src/arch/linux/gpu_vulkan.c -- see that file's own top comment for why
  // this project's own dlopen() is not used instead), not looked up at
  // runtime via dlopen()/dlsym().
  backend_context.fGetProc = [](const char* proc_name, VkInstance instance,
                                 VkDevice vk_device) -> PFN_vkVoidFunction {
    if (vk_device != VK_NULL_HANDLE) {
      return vkGetDeviceProcAddr(vk_device, proc_name);
    }
    return vkGetInstanceProcAddr(instance, proc_name);
  };
  // A real, if deliberately minimal, memory allocator -- NOT left null.
  // Confirmed for real (2026-09-03) this project's own Skia build cannot
  // rely on GrVkGpu::Make()'s usual "construct a real GrVkAMDMemoryAllocator
  // when null" fallback: tools/build_skia.py's own skia_use_vma=false
  // compiles that whole fallback path out, so a null fMemoryAllocator here
  // made GrDirectContexts::MakeVulkan() fail silently instead (see
  // DumbVulkanMemoryAllocator's own top comment, above, for the full story).
  backend_context.fMemoryAllocator = sk_make_sp<DumbVulkanMemoryAllocator>(
      backend_context.fDevice, backend_context.fPhysicalDevice);

  // Tell Skia which extensions are actually enabled so its caps (notably
  // VK_EXT_image_drm_format_modifier -> DRM-modifier textures, used by the
  // zero-copy media bridge below) match the device. The lists borrowed from
  // the device view outlive MakeVulkan(); Skia copies what it needs.
  skgpu::VulkanExtensions extensions;
  extensions.init(
      backend_context.fGetProc, backend_context.fInstance, backend_context.fPhysicalDevice,
      view.instance_extension_count, view.instance_extension_names, view.device_extension_count,
      view.device_extension_names);
  backend_context.fVkExtensions = &extensions;

  return GrDirectContexts::MakeVulkan(backend_context);
}

sk_sp<SkSurface> crtgfx_skia_make_gpu_offscreen_surface(
    GrDirectContext* context, uint32_t width, uint32_t height) {
  if (context == nullptr || width == 0 || height == 0) {
    return nullptr;
  }
  SkImageInfo info = SkImageInfo::Make(
      (int)width, (int)height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  // SkSurfaces::RenderTarget() -- Ganesh's own GrResourceProvider allocates
  // and owns a real backing VkImage/VkDeviceMemory internally; this
  // vertical slice never hand-manages one itself (see crtgfx/skia.h's own
  // comment on why that is deliberate, not a shortcut).
  return SkSurfaces::RenderTarget(context, skgpu::Budgeted::kNo, info);
}

// Wires the offscreen Ganesh pipeline above onto a live crtgfx_gpu_
// surface's own acquired VkImage (2026-09-07 -- see crtgfx/skia.h's own,
// fuller comment on both functions). Real Vulkan core types/constants
// used directly here (VkImage, VK_IMAGE_LAYOUT_*, VK_IMAGE_USAGE_*,
// VK_SHARING_MODE_EXCLUSIVE, VK_QUEUE_FAMILY_IGNORED) are already real,
// not hand-declared -- this branch's own crtgfx_skia_make_gpu_context()
// above already uses real VkInstance/VkPhysicalDevice/VkDevice/VkQueue,
// confirming Skia's own vendored real vulkan_core.h (SK_USE_INTERNAL_
// VULKAN_HEADERS) is already transitively available in this translation
// unit via VulkanTypes.h/VulkanBackendContext.h.
sk_sp<SkSurface> crtgfx_skia_wrap_gpu_surface(GrDirectContext* context, crtgfx_gpu_surface* surface) {
  crtgfx_gpu_vulkan_surface_view view = {};
  if (context == nullptr || !crtgfx_gpu_vulkan_begin_ganesh(surface, &view)) {
    return nullptr;
  }

  GrVkImageInfo image_info;
  image_info.fImage = reinterpret_cast<VkImage>(view.image);
  // fAlloc left default-constructed (fMemory=VK_NULL_HANDLE): a real,
  // spec-documented, legal "borrowed" render-target shape (VulkanTypes.h's
  // own comment on VulkanAlloc::fMemory) -- this project's own real
  // VkDeviceMemory backing this swapchain image is never owned or freed
  // through this allocation record; crtgfx_gpu_vulkan_surface_destroy()
  // (gpu_vulkan.c) is the only real owner, via vkDestroySwapchainKHR().
  image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
  // A freshly-acquired image's own real layout -- matches crtgfx_gpu_
  // vulkan_surface_clear()'s own first barrier's own identical oldLayout
  // assumption (gpu_vulkan.c), the real, same-shaped stand-in this
  // function replaces for a caller that chooses the Ganesh path instead.
  image_info.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.fFormat = static_cast<VkFormat>(view.format);
  // A real, confirmed bug (2026-09-13): this used to hardcode
  // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
  // which *used* to match gpu_vulkan.c's own swapchain creation exactly --
  // until GrVkGpu::onWrapBackendRenderTarget()'s own check_image_info()
  // (Skia's vendored src/gpu/ganesh/vk/GrVkGpu.cpp) turned out to
  // unconditionally require VK_IMAGE_USAGE_TRANSFER_SRC_BIT too ("We
  // currently require everything to be made with transfer bits set"),
  // confirmed via a one-off debug fprintf patched directly into a
  // throwaway extracted copy of that file: SkSurfaces::WrapBackendRenderTarget
  // silently returned null with zero Vulkan API calls, all the way down to
  // this exact check. Fixed at the real source (gpu_vulkan.c's own
  // crtgfx_gpu_vulkan_surface_create()/_resize(), conditionally probing
  // VkSurfaceCapabilitiesKHR::supportedUsageFlags first -- see that file's
  // own matching comment) rather than just adding the bit here too: a
  // second hardcoded copy of "what this swapchain's images were created
  // with" is exactly the kind of drift this project has already been
  // burned by elsewhere. Reading the real, live value the swapchain was
  // actually created with guarantees this can never silently disagree
  // with it again, on this host or a future one whose surface does not
  // support TRANSFER_SRC_BIT at all.
  image_info.fImageUsageFlags = view.image_usage_flags;
  image_info.fSampleCount = 1;
  image_info.fLevelCount = 1;
  image_info.fCurrentQueueFamily = view.queue_family_index;
  image_info.fProtected = skgpu::Protected::kNo;
  image_info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  GrBackendRenderTarget backend_target = GrBackendRenderTargets::MakeVk(
      static_cast<int>(view.width), static_cast<int>(view.height), image_info);
  sk_sp<SkSurface> sk_surface = SkSurfaces::WrapBackendRenderTarget(
      context, backend_target, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
  if (sk_surface != nullptr) {
    // vkAcquireNextImageKHR signals this semaphore before the swapchain
    // image may be used. The solid-clear path waits on it in vkQueueSubmit;
    // Ganesh owns the submit in this path, so insert the same wait into its
    // command stream before the caller records any drawing.
    GrBackendSemaphore acquire_semaphore = GrBackendSemaphores::MakeVk(
        reinterpret_cast<VkSemaphore>(view.image_available_semaphore));
    if (!context->wait(1, &acquire_semaphore, false)) {
      crtgfx_gpu_vulkan_end_ganesh(surface);
      return nullptr;
    }
  } else {
    crtgfx_gpu_vulkan_end_ganesh(surface);
  }
  return sk_surface;
}

crtgfx_result crtgfx_skia_gpu_surface_present(
    GrDirectContext* context, SkSurface* surface, crtgfx_gpu_surface* gpu_surface) {
  if (context == nullptr || surface == nullptr || gpu_surface == nullptr) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  crtgfx_gpu_vulkan_surface_view view = {};
  if (!crtgfx_gpu_vulkan_get_ganesh_present_view(gpu_surface, &view)) {
    return CRTGFX_ERROR_HOST;
  }

  // Ganesh itself, as part of this one real flush/submit, both signals
  // the *existing* vk_render_finished_semaphore (the exact one crtgfx_
  // gpu_vulkan_surface_present() -- unchanged, called at the very end of
  // this function -- already waits on) and transitions the image to the
  // real, required VK_IMAGE_LAYOUT_PRESENT_SRC_KHR -- no manual barrier/
  // command buffer needed on this project's own side at all, unlike the
  // D3D12 branch below (which has no equivalent flush-time mechanism).
  GrBackendSemaphore signal_semaphore = GrBackendSemaphores::MakeVk(
      reinterpret_cast<VkSemaphore>(view.render_finished_semaphore));
  GrFlushInfo flush_info;
  flush_info.fNumSemaphores = 1;
  flush_info.fSignalSemaphores = &signal_semaphore;
  skgpu::MutableTextureState new_state =
      skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_QUEUE_FAMILY_IGNORED);
  context->flush(surface, flush_info, &new_state);
  // GrSyncCpu::kYes, not kNo (2026-09-07, found for real via crtgfx_skia_
  // gpu_window_demo -- a real, live resize immediately after the first
  // Ganesh-drawn frame failed CRTGFX_ERROR_HOST on Windows/D3D12: Ganesh
  // itself defers actually releasing a wrapped resource's own extra COM/
  // driver reference until it has confirmed the GPU is really done with
  // it, not merely until the caller's own sk_sp<SkSurface> is reset() --
  // an async submit() leaves that release still pending when the very
  // next frame's crtgfx_gpu_surface_resize() call runs, and DXGI's own
  // real ResizeBuffers()/Vulkan's own real vkCreateSwapchainKHR both
  // require every real reference to the previous images to be gone
  // first. A synchronous submit here blocks until the GPU has actually
  // finished, so Ganesh's own internal tracking can release that
  // reference before this function returns -- real, same real class of
  // fix on all three backends (Metal/D3D12 below), matching crtgfx_skia_
  // gpu_offscreen_smoke.cc's own already-established GrSyncCpu::kYes
  // convention throughout, chosen here for real correctness over the
  // otherwise-real one-frame-of-latency win an async submit would give.
  context->submit(GrSyncCpu::kYes);

  crtgfx_gpu_vulkan_end_ganesh(gpu_surface);
  return crtgfx_gpu_surface_present(gpu_surface);
}

#if CRTGFX_HAS_SKIA_HEADERS
// Zero-copy decoded textures, Tranche 3 (Linux, 2026-09-24) -- the Vulkan
// branch of crtgfx_skia_import_media_frame() (crtgfx/skia_media.h has the
// frozen contract; docs/crtmedia_zero_copy_decode_acceptance.md records the
// "direct DRM PRIME import, not FFmpeg's Vulkan hwcontext" decision).
// `frame->native_handle` is a crtmedia_vaapi_gpu_frame_handle* (VA-API
// surface already synchronised and exported as dma-bufs by libcrtmedia, so
// this file never touches libva). Each NV12 plane (R8 Y, GR88 UV) is imported
// as its own single-plane VkImage over the same dma-buf object with an
// explicit DRM-format-modifier plane layout, then both go to
// GrYUVABackendTextures -> SkImages::TextureFromYUVATextures -- no RGBA
// intermediate and no CPU readback.
#include "crtgfx/skia_media.h"
#include "gpu_frame_vaapi.h"

#include "include/core/SkYUVAInfo.h"
#include "include/gpu/ganesh/GrYUVABackendTextures.h"
#include "include/gpu/ganesh/SkImageGanesh.h"

#include <atomic>
#include <unistd.h>

namespace {

constexpr uint32_t kDrmFormatR8 = 0x20203852u;    // fourcc 'R8  '
constexpr uint32_t kDrmFormatGR88 = 0x38385247u;  // fourcc 'GR88'
constexpr uint64_t kDrmFormatModInvalid = 0x00ffffffffffffffull;

struct VulkanPlaneImport {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkDeviceSize allocation_size = 0;
  uint32_t memory_type_index = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

// Shared by Skia's release callback and the failure path. The retained
// crtmedia frame is released by the callback only once `frame_moved` says the
// import succeeded (on failure the caller keeps ownership of the frame).
struct MediaFrameReleaseContext {
  crtmedia_gpu_frame frame;
  bool frame_moved = false;
  std::atomic<bool> callback_fired{false};
  VkDevice device = VK_NULL_HANDLE;
  VulkanPlaneImport planes[2];
  std::atomic<int> refs{2};  // Skia's callback + the importing function
};

void DestroyPlanes(MediaFrameReleaseContext* ctx) {
  for (VulkanPlaneImport& plane : ctx->planes) {
    if (plane.image != VK_NULL_HANDLE) {
      vkDestroyImage(ctx->device, plane.image, nullptr);
      plane.image = VK_NULL_HANDLE;
    }
    if (plane.memory != VK_NULL_HANDLE) {
      vkFreeMemory(ctx->device, plane.memory, nullptr);
      plane.memory = VK_NULL_HANDLE;
    }
  }
}

void DropMediaFrameRef(MediaFrameReleaseContext* ctx) {
  if (ctx->refs.fetch_sub(1) == 1) {
    delete ctx;
  }
}

void ReleaseMediaFrame(SkImages::ReleaseContext release_context) {
  auto* ctx = static_cast<MediaFrameReleaseContext*>(release_context);
  ctx->callback_fired = true;
  DestroyPlanes(ctx);
  if (ctx->frame_moved) {
    crtmedia_gpu_frame_release(&ctx->frame);
  }
  DropMediaFrameRef(ctx);
}

bool ImportPlane(
    VkDevice device, VkPhysicalDevice physical_device, PFN_vkGetMemoryFdPropertiesKHR get_fd_properties,
    const crtmedia_vaapi_gpu_frame_handle& handle, const crtmedia_vaapi_gpu_frame_layer& layer,
    VkFormat format, uint32_t width, uint32_t height, VulkanPlaneImport* out) {
  if (layer.num_planes != 1 || layer.object_index[0] >= handle.num_objects) {
    return false;
  }
  const crtmedia_vaapi_gpu_frame_object& object = handle.objects[layer.object_index[0]];
  if (object.fd < 0 || object.drm_format_modifier == kDrmFormatModInvalid) {
    return false;
  }

  VkSubresourceLayout plane_layout = {};
  plane_layout.offset = layer.offset[0];
  plane_layout.rowPitch = layer.pitch[0];

  VkImageDrmFormatModifierExplicitCreateInfoEXT modifier_info = {};
  modifier_info.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
  modifier_info.drmFormatModifier = object.drm_format_modifier;
  modifier_info.drmFormatModifierPlaneCount = 1;
  modifier_info.pPlaneLayouts = &plane_layout;

  VkExternalMemoryImageCreateInfo external_info = {};
  external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  external_info.pNext = &modifier_info;
  external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = &external_info;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = format;
  image_info.extent = {width, height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  // Skia's wrapped-texture validation wants transfer usage alongside
  // sampled (GrVkGpu.cpp check_image_info()).
  image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(device, &image_info, nullptr, &out->image) != VK_SUCCESS) {
    out->image = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryFdPropertiesKHR fd_props = {};
  fd_props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
  if (get_fd_properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, object.fd, &fd_props) != VK_SUCCESS) {
    return false;
  }

  VkMemoryDedicatedRequirements dedicated_reqs = {};
  dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
  VkMemoryRequirements2 reqs = {};
  reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  reqs.pNext = &dedicated_reqs;
  VkImageMemoryRequirementsInfo2 reqs_info = {};
  reqs_info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
  reqs_info.image = out->image;
  vkGetImageMemoryRequirements2(device, &reqs_info, &reqs);

  uint32_t type_bits = reqs.memoryRequirements.memoryTypeBits & fd_props.memoryTypeBits;
  if (type_bits == 0) {
    return false;
  }
  uint32_t memory_type_index = 0;
  while (((type_bits >> memory_type_index) & 1u) == 0) {
    ++memory_type_index;
  }

  // vkAllocateMemory takes ownership of the imported fd on success, so hand
  // it a dup and leave the handle's own fd for the frame's release().
  int fd = dup(object.fd);
  if (fd < 0) {
    return false;
  }
  VkImportMemoryFdInfoKHR import_info = {};
  import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
  import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  import_info.fd = fd;
  VkMemoryDedicatedAllocateInfo dedicated_info = {};
  dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated_info.pNext = &import_info;
  dedicated_info.image = out->image;
  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &dedicated_info;
  alloc_info.allocationSize =
      reqs.memoryRequirements.size > object.size ? reqs.memoryRequirements.size : object.size;
  alloc_info.memoryTypeIndex = memory_type_index;
  if (vkAllocateMemory(device, &alloc_info, nullptr, &out->memory) != VK_SUCCESS) {
    out->memory = VK_NULL_HANDLE;
    close(fd);
    return false;
  }

  VkBindImageMemoryInfo bind_info = {};
  bind_info.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
  bind_info.image = out->image;
  bind_info.memory = out->memory;
  bind_info.memoryOffset = 0;
  if (vkBindImageMemory2(device, 1, &bind_info) != VK_SUCCESS) {
    return false;
  }
  (void)physical_device;
  out->format = format;
  out->allocation_size = alloc_info.allocationSize;
  out->memory_type_index = memory_type_index;
  out->width = width;
  out->height = height;
  return true;
}

GrBackendTexture MakePlaneTexture(const VulkanPlaneImport& plane) {
  GrVkImageInfo info;
  info.fImage = plane.image;
  info.fAlloc = skgpu::VulkanAlloc();
  info.fAlloc.fMemory = plane.memory;
  info.fAlloc.fOffset = 0;
  info.fAlloc.fSize = plane.allocation_size;
  info.fAlloc.fFlags = 0;
  info.fImageTiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  info.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  info.fFormat = plane.format;
  info.fImageUsageFlags =
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  info.fSampleCount = 1;
  info.fLevelCount = 1;
  // Acquire from the external producer (VA-API) on first use.
  info.fCurrentQueueFamily = VK_QUEUE_FAMILY_FOREIGN_EXT;
  info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  return GrBackendTextures::MakeVk(static_cast<int>(plane.width), static_cast<int>(plane.height), info);
}

}  // namespace

sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context, const crtgfx_gpu_device* device, crtmedia_gpu_frame* frame) {
  if (context == nullptr || device == nullptr || frame == nullptr ||
      frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU || frame->native_handle == nullptr ||
      frame->format != CRTMEDIA_PIXEL_FORMAT_NV12) {
    return nullptr;
  }

  crtgfx_gpu_vulkan_device_view view = {};
  if (!crtgfx_gpu_vulkan_borrow_device(device, &view) || !view.dmabuf_import) {
    return nullptr;
  }
  const auto* handle = static_cast<const crtmedia_vaapi_gpu_frame_handle*>(frame->native_handle);
  if (handle->num_layers != 2 || handle->layers[0].drm_format != kDrmFormatR8 ||
      handle->layers[1].drm_format != kDrmFormatGR88 || handle->width != frame->width ||
      handle->height != frame->height) {
    return nullptr;
  }

  VkDevice vk_device = reinterpret_cast<VkDevice>(view.device);
  auto get_fd_properties = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
      vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdPropertiesKHR"));
  if (get_fd_properties == nullptr) {
    return nullptr;
  }

  auto* ctx = new MediaFrameReleaseContext();
  ctx->frame = *frame;
  ctx->device = vk_device;
  VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(view.physical_device);
  uint32_t uv_width = (frame->width + 1) / 2;
  uint32_t uv_height = (frame->height + 1) / 2;
  bool ok = ImportPlane(vk_device, physical_device, get_fd_properties, *handle, handle->layers[0], VK_FORMAT_R8_UNORM,
                        frame->width, frame->height, &ctx->planes[0]) &&
            ImportPlane(vk_device, physical_device, get_fd_properties, *handle, handle->layers[1],
                        VK_FORMAT_R8G8_UNORM, uv_width, uv_height, &ctx->planes[1]);
  if (!ok) {
    DestroyPlanes(ctx);
    delete ctx;
    return nullptr;
  }

  GrBackendTexture textures[SkYUVAInfo::kMaxPlanes] = {
      MakePlaneTexture(ctx->planes[0]), MakePlaneTexture(ctx->planes[1]), {}, {}};
  // BT.709 limited range, same as the Metal/D3D12 branches (no color
  // metadata in crtmedia_gpu_frame yet).
  SkYUVAInfo yuva_info(
      SkISize::Make(static_cast<int>(frame->width), static_cast<int>(frame->height)), SkYUVAInfo::PlaneConfig::kY_UV,
      SkYUVAInfo::Subsampling::k420, kRec709_Limited_SkYUVColorSpace);
  GrYUVABackendTextures yuva_textures(yuva_info, textures, kTopLeft_GrSurfaceOrigin);
  if (!yuva_textures.isValid()) {
    DestroyPlanes(ctx);
    delete ctx;
    return nullptr;
  }

  sk_sp<SkImage> image =
      SkImages::TextureFromYUVATextures(context, yuva_textures, nullptr, ReleaseMediaFrame, ctx);
  if (image == nullptr) {
    // Skia may already have invoked ReleaseMediaFrame on failure (it saw
    // frame_moved == false, so it did not touch the caller's frame). Either
    // way the Vulkan objects must be gone and the caller keeps the frame.
    DestroyPlanes(ctx);
    if (!ctx->callback_fired) {
      DropMediaFrameRef(ctx);  // the reference Skia's callback would have dropped
    }
    DropMediaFrameRef(ctx);
    return nullptr;
  }
  // Ownership transfer point (crtgfx/skia_media.h): the copy in ctx now owns
  // the frame's backing; zero the caller's so it cannot double-release.
  ctx->frame_moved = true;
  memset(frame, 0, sizeof(*frame));
  DropMediaFrameRef(ctx);
  return image;
}
#endif  // CRTGFX_HAS_SKIA_HEADERS

#elif defined(CRTGFX_HAVE_D3D12)

// Real Ganesh/D3D12 offscreen vertical slice (2026-09-03) -- the Windows
// sibling of the Vulkan branch above (see crtgfx/skia.h's own, fuller
// comment on both functions below). Real <d3d12.h>/<dxgi1_4.h> inclusion
// here is forced by Skia's own public include/gpu/ganesh/d3d/GrD3DTypes.h
// (unlike Vulkan, which Skia vendors its own copy of) -- a real, deliberate
// exception to this project's own no-host-SDK-header policy, already
// codified in docs/libcrtgfx_api_policy.md's own "third-party source being
// ported" Non-Goals clause; src/arch/windows/gpu_win32.c itself stays
// real-host-header-free, hand-declaring only what it needs, exactly like
// window_win32.c and src/arch/linux/gpu_vulkan.c already do.

#include "gpu_internal.h"

#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendContext.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DDirectContext.h"
#include "include/gpu/ganesh/d3d/GrD3DTypes.h"

namespace {

// A real, minimal GrD3DMemoryAllocator -- this vertical slice's own
// deliberate substitute for Skia's real default (AMD's separate D3D12
// MemoryAllocator library, wired in via GrD3DAMDMemoryAllocator::Make()).
// Confirmed for real (2026-09-03) that this project's own Skia build
// cannot use that default at all: unlike Vulkan's own skia_use_vma,
// there is no separate GN flag gating GrD3DGpu::Make()'s own internal
// fallback-allocator construction -- it is unconditional C++ once
// skia_use_direct3d is true, but it needs a real vendor checkout
// (third_party/externals/d3d12allocator) this project's fetch pipeline
// does not provide (see tools/build_skia.py's own comment for the full
// story). Real, genuinely simpler than the Vulkan slice's own allocator:
// the interface here is only 2 pure-virtual methods, and D3D12's own real
// ID3D12Device::CreateCommittedResource() allocates *and* binds in one
// real call (unlike Vulkan's separate allocate-then-bind split), so no
// manual free-tracking is needed either -- releasing the returned
// ID3D12Resource COM object is sufficient. GrD3DAlloc itself is an empty
// SkRefCnt tag class (no fields) -- this allocator hands back a trivial
// instance purely because the real interface requires one, never reading
// anything out of it again itself.
class DumbD3DMemoryAllocator : public GrD3DMemoryAllocator {
 public:
  explicit DumbD3DMemoryAllocator(ID3D12Device* device) : fDevice(device) {}

  gr_cp<ID3D12Resource> createResource(
      D3D12_HEAP_TYPE heap_type, const D3D12_RESOURCE_DESC* desc,
      D3D12_RESOURCE_STATES initial_state, sk_sp<GrD3DAlloc>* allocation,
      const D3D12_CLEAR_VALUE* clear_value) override {
    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = heap_type;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    gr_cp<ID3D12Resource> resource;
    // Real IID for ID3D12Resource, hand-declared (transcribed from its own
    // real MIDL_INTERFACE("696442be-a72e-4059-bc79-5b5c98040fad") UUID in
    // the Windows SDK's own um\d3d12.h) rather than IID_PPV_ARGS()/
    // __uuidof() -- both are real MSVC-only extensions (compiler-embedded
    // per-interface GUID metadata via __declspec(uuid(...))) this
    // project's own mingw-target clang invocation (--target=x86_64-w64-
    // mingw32, matching every other Windows compile in this project) does
    // not support, matching this project's own established dxguid.lib-
    // avoiding precedent elsewhere (window_win32.c, gpu_win32.c) -- just
    // applied here through a real, already-included <d3d12.h> GUID type
    // instead of a hand-declared one, since this translation unit already
    // has the real one (Skia's own GrD3DTypes.h forces it).
    static const GUID kIID_ID3D12Resource = {
        0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
    HRESULT hr = fDevice->CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE, desc, initial_state, clear_value,
        kIID_ID3D12Resource, reinterpret_cast<void**>(&resource));
    if (FAILED(hr)) {
      return nullptr;
    }
    *allocation = sk_make_sp<GrD3DAlloc>();
    return resource;
  }

  gr_cp<ID3D12Resource> createAliasingResource(
      sk_sp<GrD3DAlloc>& /*allocation*/, uint64_t /*localOffset*/,
      const D3D12_RESOURCE_DESC* /*desc*/, D3D12_RESOURCE_STATES /*initialResourceState*/,
      const D3D12_CLEAR_VALUE* /*clearValue*/) override {
    // Real resource aliasing (placing a second resource inside an
    // existing allocation's own backing heap) is a real production
    // efficiency concern this deliberately minimal, one-dedicated-heap-
    // per-resource allocator does not take on -- every real resource this
    // vertical slice ever creates goes through createResource() above
    // instead (confirmed empirically: this vertical slice's own real
    // single-render-target usage never reaches this method at all).
    return nullptr;
  }

 private:
  ID3D12Device* fDevice;
};

}  // namespace

sk_sp<GrDirectContext> crtgfx_skia_make_gpu_context(const crtgfx_gpu_device* device) {
  crtgfx_gpu_win32_device_view view = {};
  if (!crtgfx_gpu_win32_borrow_device(device, &view)) {
    return nullptr;
  }

  // .retain(), not a plain `=` (2026-09-04, real, confirmed necessary
  // once a real, complete <d3d12.h>/gr_cp<T> were both actually
  // available to compile against for the first time this session --
  // gr_cp<T> deliberately has no implicit-from-raw-pointer assignment,
  // only from another gr_cp<T> -- confirmed: "no viable overloaded
  // '='"). .retain() calls a real AddRef() on the underlying COM
  // object, so backend_context's own gr_cp<T> destructor later calling
  // Release() is correctly balanced -- `device` itself keeps its own,
  // separate reference (crtgfx_gpu_device_release() still owns and
  // eventually releases the real one), so this must take its own,
  // independent reference rather than adopt the existing one outright
  // (gr_cp<T>'s single-argument constructor does exactly that "adopt,
  // no AddRef" -- correct for a real owning transfer, but would have
  // left crtgfx_gpu_device_release()'s own later real Release() call
  // double-releasing the same COM object once backend_context's own
  // destructor ran first).
  GrD3DBackendContext backend_context;
  backend_context.fAdapter.retain(reinterpret_cast<IDXGIAdapter1*>(view.adapter));
  backend_context.fDevice.retain(reinterpret_cast<ID3D12Device*>(view.device));
  backend_context.fQueue.retain(reinterpret_cast<ID3D12CommandQueue*>(view.command_queue));
  // A real, if deliberately minimal, memory allocator -- NOT left null.
  // Confirmed for real (2026-09-03) this project's own Skia build cannot
  // rely on GrD3DGpu::Make()'s usual "construct a real GrD3DAMDMemoryAllocator
  // when null" fallback (see DumbD3DMemoryAllocator's own top comment,
  // above, for the full story).
  backend_context.fMemoryAllocator = sk_make_sp<DumbD3DMemoryAllocator>(backend_context.fDevice.get());

  return GrDirectContexts::MakeD3D(backend_context);
}

sk_sp<SkSurface> crtgfx_skia_make_gpu_offscreen_surface(
    GrDirectContext* context, uint32_t width, uint32_t height) {
  if (context == nullptr || width == 0 || height == 0) {
    return nullptr;
  }
  SkImageInfo info = SkImageInfo::Make(
      (int)width, (int)height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  // SkSurfaces::RenderTarget() -- confirmed backend-agnostic, the exact
  // same real call already proven for Vulkan (see this file's own Vulkan
  // branch, above): Ganesh's own GrResourceProvider allocates and owns a
  // real backing D3D12 resource internally; this vertical slice never
  // hand-manages one itself (see crtgfx/skia.h's own comment on why that
  // is deliberate, not a shortcut).
  return SkSurfaces::RenderTarget(context, skgpu::Budgeted::kNo, info);
}

// Wires the offscreen Ganesh pipeline above onto a live crtgfx_gpu_
// surface's own acquired back buffer (2026-09-07 -- see crtgfx/skia.h's
// own, fuller comment on both functions). Unlike the Vulkan branch above,
// D3D12 has no flush-time "transition for me" mechanism (confirmed: no
// include/gpu/d3d/...MutableTextureState.h exists in this checkout at
// all) -- crtgfx_skia_gpu_surface_present() below does real, manual work
// instead: reads back whatever real resource state Ganesh's own flush left
// the wrapped resource in, then asks gpu_win32.c to record the PRESENT
// transition, close/execute its command list, and update its monotonic fence
// bookkeeping. Skia sees only the borrowed current back buffer and the resource
// state it must report back; the command allocator/list/queue/fence state
// machine remains entirely inside its D3D12 owner.
sk_sp<SkSurface> crtgfx_skia_wrap_gpu_surface(GrDirectContext* context, crtgfx_gpu_surface* surface) {
  crtgfx_gpu_win32_surface_view view = {};
  if (context == nullptr || !crtgfx_gpu_win32_begin_ganesh(surface, &view)) {
    return nullptr;
  }

  GrD3DTextureResourceInfo info;
  info.fResource.retain(reinterpret_cast<ID3D12Resource*>(view.back_buffer));
  // A freshly-acquired back buffer's own real state, matching crtgfx_gpu_
  // win32_surface_clear()'s own first barrier's own identical state_before
  // assumption (gpu_win32.c), the real, same-shaped stand-in this
  // function replaces for a caller that chooses the Ganesh path instead.
  info.fResourceState = D3D12_RESOURCE_STATE_PRESENT;
  info.fFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
  info.fSampleCount = 1;
  info.fLevelCount = 1;

  GrBackendRenderTarget backend_target = GrBackendRenderTargets::MakeD3D(
      static_cast<int>(view.width), static_cast<int>(view.height), info);
  sk_sp<SkSurface> sk_surface = SkSurfaces::WrapBackendRenderTarget(
      context, backend_target, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
  if (sk_surface == nullptr) {
    crtgfx_gpu_win32_end_ganesh(surface);
  }
  return sk_surface;
}

crtgfx_result crtgfx_skia_gpu_surface_present(
    GrDirectContext* context, SkSurface* surface, crtgfx_gpu_surface* gpu_surface) {
  if (context == nullptr || surface == nullptr || gpu_surface == nullptr) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (!crtgfx_gpu_win32_is_ganesh_wrapped(gpu_surface)) {
    return CRTGFX_ERROR_HOST;
  }

  // SkSurface::BackendHandleAccess::kFlushRead itself performs Ganesh's
  // own real flush of every draw recorded into `surface` (the real,
  // documented meaning of the "Flush" in this access mode's own name),
  // returning a real, live snapshot of the wrapped resource's current
  // state right afterward.
  GrBackendRenderTarget backend_target =
      SkSurfaces::GetBackendRenderTarget(surface, SkSurface::BackendHandleAccess::kFlushRead);
  // GrSyncCpu::kYes -- see the Vulkan branch's own crtgfx_skia_gpu_
  // surface_present() comment above for the real, found-for-real reason
  // (a live crtgfx_gpu_surface_resize() call the very next frame needs
  // every wrapped resource's own extra COM reference already released,
  // which Ganesh only guarantees once it has confirmed the GPU is really
  // done, not merely once flushed/submitted).
  context->submit(GrSyncCpu::kYes);

  GrD3DTextureResourceInfo info = GrBackendRenderTargets::GetD3DTextureResourceInfo(backend_target);
  if (!info.fResource) {
    crtgfx_gpu_win32_end_ganesh(gpu_surface);
    return CRTGFX_ERROR_HOST;
  }

  if (crtgfx_gpu_win32_submit_ganesh(
          gpu_surface, info.fResource.get(), static_cast<uint32_t>(info.fResourceState)) != CRTGFX_OK) {
    crtgfx_gpu_win32_end_ganesh(gpu_surface);
    return CRTGFX_ERROR_HOST;
  }

  // Real, correct bookkeeping for Ganesh's own shared, refcounted
  // GrD3DResourceState (GrD3DTypesMinimal.h's own comment) -- not load-
  // bearing for this vertical slice (a fresh GrBackendRenderTarget is
  // built every frame, never reused across crtgfx_skia_wrap_gpu_
  // surface() calls), but real, correct hygiene regardless.
  GrBackendRenderTargets::SetD3DResourceState(&backend_target, D3D12_RESOURCE_STATE_PRESENT);

  return crtgfx_gpu_surface_present(gpu_surface);
}

// Zero-copy decoded-texture bridge (2026-09-23, "Zero-copy decoded
// textures" Tranche 2 -- see crtgfx/skia_media.h's own top comment for the
// full frozen contract, ownership rule, and the real reason this needs a
// GPU-copy step D3D11VA -> a fresh shareable texture, unlike the macOS
// branch's own direct CVPixelBuffer wrap). Everything below (the plane-
// sliced D3D11.3 SRV1 extraction, the compute-shader plane copy, and the
// NT-handle D3D11->D3D12 share) was confirmed for real via two standalone
// probes on this exact host before landing: one proving a plain shareable
// D3D11 texture opens correctly into D3D12 and round-trips a known byte
// pattern; one proving ID3D11Device3::CreateShaderResourceView1() with
// D3D11_TEX2D_SRV1::PlaneSlice really does extract the Y/UV planes of a
// real NV12 texture as independently readable, byte-exact float/float2
// data.
#include "crtgfx/skia_media.h"

#if CRTGFX_HAS_SKIA_HEADERS && defined(CRTGFX_HAVE_D3D12)

// Real <d3d11.h>/<d3d11_3.h> inclusion, the identical "third-party source
// being ported" exception docs/libcrtgfx_api_policy.md's own Non-Goals
// clause already accepts for this file's own <d3d12.h> above (Skia's own
// GrD3DTypes.h forces that one) -- extended here to D3D11 rather than
// hand-declaring a dozen-plus vtable slots for CreateTexture2D/
// CreateShaderResourceView1/CopyResource/QueryInterface/GetAdapter/
// CreateSharedHandle by hand, the real error-prone alternative this
// project's own window_win32.c/gpu_win32.c otherwise prefer for a handful
// of methods -- not for a surface this wide. gpu_frame_d3d11.h
// (libcrtmedia/src/, reached via this target's own extra -I --
// libcrtgfx/cmake/crtgfx_skia_targets.cmake) supplies the real
// ID3D11Texture2D*/array-index pair this branch reads from crtmedia_gpu_
// frame.native_handle.
#include <d3d11.h>
#include <d3d11_3.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>

#include "gpu_frame_d3d11.h"

#include "include/core/SkYUVAInfo.h"
#include "include/gpu/ganesh/GrYUVABackendTextures.h"
#include "include/gpu/ganesh/SkImageGanesh.h"

namespace {

// Real GUIDs, confirmed directly against mingw-w64's own d3d11_3.h/dxgi.h/
// dxgi1_2.h DEFINE_GUID declarations (not guessed) -- avoids IID_PPV_ARGS()/
// __uuidof(), both real MSVC-only extensions this project's own mingw-
// target clang invocation does not support, matching DumbD3DMemoryAllocator's
// own kIID_ID3D12Resource precedent (this same file, D3D12 branch above).
const GUID kIID_ID3D11Device3 = {
    0xa05c8c37, 0xd2c6, 0x4732, {0xb3, 0xa0, 0x9c, 0xe0, 0xb0, 0xdc, 0x9a, 0xe6}};
const GUID kIID_IDXGIDevice = {
    0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
const GUID kIID_IDXGIResource1 = {
    0x30961379, 0x4609, 0x4a41, {0x99, 0x8e, 0x54, 0xfe, 0x56, 0x7e, 0xe0, 0xc1}};

// Real DXGI_ADAPTER_DESC/DXGI_ADAPTER_DESC1 replacements, NOT the real SDK
// types -- confirmed necessary for real (2026-09-23), a serious, silent
// ABI-corruption bug found only by actually running this code and
// noticing IDXGIAdapter::GetDesc()'s own real `Description` field decoded
// correctly (via WideCharToMultiByte) while every field *after* it
// (VendorId, AdapterLuid, ...) came back looking like garbage. Root
// cause: mingw-w64's own real `WCHAR` is `typedef wchar_t WCHAR;`
// (wtypesbase.h), and this project's entire Windows C++ build passes
// `-Xclang -fwchar-type=int` (CMakeLists.txt's own crt_cxx_build_flags,
// a deliberate project-wide POSIX-style 4-byte wchar_t override) -- so
// `DXGI_ADAPTER_DESC::Description[128]` occupies 512 bytes in *this
// project's own compiled code* instead of the real Windows ABI's 256
// bytes (128 * real 2-byte WCHAR), shifting every subsequent field's
// real, system-DLL-written offset out from under this project's own
// (wrongly-sized) struct definition. gpu_win32.c already hit and solved
// this exact problem for its own hand-declared DXGI structs (see that
// file's own `crtgfx_dxgi_wchar`/`typedef unsigned short
// crtgfx_dxgi_wchar;`) -- this mirrors that precedent for the two real
// SDK struct shapes this file itself needs (COM calls only care about
// real memory layout, never the static C++ type name, so a locally
// correctly-sized redeclaration passed to the exact same real
// GetDesc()/GetDesc1() vtable slot is both safe and correct).
struct RealDxgiAdapterDesc {
  unsigned short Description[128];
  UINT VendorId;
  UINT DeviceId;
  UINT SubSysId;
  UINT Revision;
  SIZE_T DedicatedVideoMemory;
  SIZE_T DedicatedSystemMemory;
  SIZE_T SharedSystemMemory;
  LUID AdapterLuid;
};
struct RealDxgiAdapterDesc1 {
  unsigned short Description[128];
  UINT VendorId;
  UINT DeviceId;
  UINT SubSysId;
  UINT Revision;
  SIZE_T DedicatedVideoMemory;
  SIZE_T DedicatedSystemMemory;
  SIZE_T SharedSystemMemory;
  LUID AdapterLuid;
  UINT Flags;
};

// A tiny, local RAII wrapper -- this file has no COM smart-pointer
// convention of its own beyond Skia's own gr_cp<T> (used only for the
// handful of objects actually handed to Skia, below); everything else
// here is a real, temporary COM object this function itself must not
// leak across its own dozen-plus early-return failure paths.
template <typename T>
class ComPtr {
 public:
  ComPtr() = default;
  ~ComPtr() { Reset(); }
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  T** ReceiveAddressOf() {
    Reset();
    return &ptr_;
  }
  T* Get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }
  void Reset() {
    if (ptr_ != nullptr) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }
  // Real ownership transfer for the two D3D12 resources this function
  // hands off into Skia's own gr_cp<ID3D12Resource> (GrD3DTextureResourceInfo::
  // fResource.retain() below) -- Skia takes its own, independent reference
  // via .retain(), so this wrapper's own destructor releasing its copy
  // afterward is correct, ordinary balanced COM refcounting, not a
  // use-after-free.

 private:
  T* ptr_ = nullptr;
};

// One tiny compute shader per plane component count (R8 = float, R8G8 =
// float2) -- copies exactly one already-plane-sliced source view into an
// independent, differently-allocated destination, entirely on the GPU.
// Compiled once per process via D3DCompile (real, present on every
// DirectX-capable Windows install as d3dcompiler_47.dll -- this project
// already links CRTGFX_WINDOWS_D3DCOMPILER_LIB for Skia's own D3D backend,
// libcrtgfx/CMakeLists.txt), not precompiled offline: this project has no
// existing dependency on a real HLSL compiler toolchain (fxc/dxc) and
// runtime compilation of two three-line shaders is a real, bounded,
// one-time cost, not a per-frame one (cached in the two function-local
// statics below).
const char kCopyPlaneShaderR8[] =
    "Texture2D<float> SrcPlane : register(t0);\n"
    "RWTexture2D<float> DstPlane : register(u0);\n"
    "[numthreads(8,8,1)]\n"
    "void CSMain(uint3 id : SV_DispatchThreadID) { DstPlane[id.xy] = SrcPlane[id.xy]; }\n";
const char kCopyPlaneShaderR8G8[] =
    "Texture2D<float2> SrcPlane : register(t0);\n"
    "RWTexture2D<float2> DstPlane : register(u0);\n"
    "[numthreads(8,8,1)]\n"
    "void CSMain(uint3 id : SV_DispatchThreadID) { DstPlane[id.xy] = SrcPlane[id.xy]; }\n";

// Compiles `hlsl_source` once and caches the resulting ID3D11ComputeShader
// on `device` for the rest of this process's lifetime -- `device` here is
// always the one real ID3D11Device FFmpeg's own hwcontext_d3d11va.c
// created (borrowed from the decoded texture itself, never independently
// created by this bridge), so a single cache slot per shader kind is
// correct for the realistic "one FFmpeg decode device per process" shape
// this project's own crtmedia_codec_create_decoder() already has (it never
// takes a device parameter -- one process, one implicit default-adapter
// D3D11VA device). Returns null on any real compile/create failure; never
// crashes on a null source.
ID3D11ComputeShader* GetOrCreateCopyPlaneShader(ID3D11Device* device, const char* hlsl_source, bool* out_created_now) {
  static ID3D11ComputeShader* cached_r8 = nullptr;
  static ID3D11ComputeShader* cached_r8g8 = nullptr;
  ID3D11ComputeShader** cache_slot = (hlsl_source == kCopyPlaneShaderR8) ? &cached_r8 : &cached_r8g8;
  *out_created_now = false;
  if (*cache_slot != nullptr) {
    return *cache_slot;
  }

  ComPtr<ID3DBlob> blob;
  ComPtr<ID3DBlob> error_blob;
  HRESULT hr = D3DCompile(
      hlsl_source, strlen(hlsl_source), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", 0, 0,
      blob.ReceiveAddressOf(), error_blob.ReceiveAddressOf());
  if (FAILED(hr) || !blob) {
    return nullptr;
  }
  ID3D11ComputeShader* shader = nullptr;
  hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
  if (FAILED(hr)) {
    return nullptr;
  }
  *cache_slot = shader;
  *out_created_now = true;
  return shader;
}

// A persistent (process-lifetime), per-plane cache of the destination
// shareable D3D11 texture and its already-opened D3D12 resource --
// confirmed necessary for real (2026-09-23): creating a brand-new
// NT-shared-handle texture/resource pair from scratch on every single
// frame (this bridge's own original design) works for a while but
// eventually corrupts unrelated DXGI/driver state on this host's own
// Intel iGPU driver -- observed for real as a completely unrelated
// IDXGIAdapter1::GetDesc1() call (this function's own device-affinity
// check, elsewhere in this file) starting to return garbage-looking data
// after roughly twenty real frames' worth of accumulated shared-resource
// churn, a real, apparently finite per-process/per-driver budget for this
// class of object. A small, fixed number of persistent resources (one
// per plane, refreshed via the compute-shader copy every frame instead of
// recreated) avoids the unbounded growth entirely. Video resolution is
// fixed for the lifetime of a real crtmedia_codec instance in every
// caller this project has today, so in practice each slot is created
// once and reused for the rest of the stream; a genuine resolution change
// mid-stream still works correctly (detected by the width/height/format
// comparison below), just pays the one-time recreation cost again.
struct CachedPlaneResource {
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  UINT width = 0;
  UINT height = 0;
  ComPtr<ID3D11Texture2D> d3d11_texture;
  ComPtr<ID3D11UnorderedAccessView> uav;
  ComPtr<ID3D12Resource> d3d12_resource;
};

// Refreshes (creating or resizing on first use / a real resolution
// change) the persistent cache slot for `plane_index` (0 = Y, 1 = UV),
// then copies array slice `array_index`'s plane `plane_slice` of
// `nv12_texture` into it via the real plane-sliced-SRV1 + compute-shader-
// copy mechanism (this file's own top comment). Real, confirmed-for-real
// (this file's own top-comment probe) for a plain (ArraySize == 1) NV12
// texture via D3D11_TEX2D_SRV1/D3D11_SRV_DIMENSION_TEXTURE2D. FFmpeg's
// own real hwcontext_d3d11va.c decode-pool texture is always an *array*
// texture instead (ArraySize == pool size, `frame->data[1]` selects the
// slice -- gpu_frame_d3d11.h's own top comment), so this uses the
// array-shaped sibling instead (D3D11_TEX2D_ARRAY_SRV1/
// D3D11_SRV_DIMENSION_TEXTURE2DARRAY, ArraySize == 1 starting at
// FirstArraySlice == array_index, still carrying its own independent
// PlaneSlice) -- the real, correct API for this shape per this project's
// own local mingw-w64 d3d11_3.h, though not independently re-run through
// a fresh multi-slice-array probe the way the plain single-texture case
// was (the probe used ArraySize == 1, which exercises D3D11_TEX2D_SRV1,
// not this array-indexed sibling). Returns the cache-owned ID3D12Resource*
// (not an extra AddRef for the caller -- the cache itself keeps this
// alive across calls; GrD3DTextureResourceInfo::fResource.retain() at the
// call site takes Skia's own independent reference) on success, nullptr
// on any real failure (the cache slot is reset to force a clean recreate
// attempt next call).
ID3D12Resource* GetOrRefreshPlaneD3D12Resource(
    ID3D11Device* device, ID3D11DeviceContext* context, ID3D12Device* d3d12_device, ID3D11Texture2D* nv12_texture,
    UINT array_index, int plane_index, UINT plane_slice, DXGI_FORMAT plane_format, UINT plane_width,
    UINT plane_height) {
  static CachedPlaneResource cache[2];
  CachedPlaneResource& entry = cache[plane_index];

  if (!entry.d3d12_resource || entry.format != plane_format || entry.width != plane_width ||
      entry.height != plane_height) {
    entry.d3d11_texture.Reset();
    entry.uav.Reset();
    entry.d3d12_resource.Reset();
    entry.format = DXGI_FORMAT_UNKNOWN;

    D3D11_TEXTURE2D_DESC dst_desc = {};
    dst_desc.Width = plane_width;
    dst_desc.Height = plane_height;
    dst_desc.MipLevels = 1;
    dst_desc.ArraySize = 1;
    dst_desc.Format = plane_format;
    dst_desc.SampleDesc.Count = 1;
    dst_desc.Usage = D3D11_USAGE_DEFAULT;
    dst_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    // Real, confirmed-for-real (this file's own top comment, probe #1):
    // D3D11_RESOURCE_MISC_SHARED_NTHANDLE requires D3D11_RESOURCE_MISC_
    // SHARED set alongside it on this driver.
    dst_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    HRESULT hr2 = device->CreateTexture2D(&dst_desc, nullptr, entry.d3d11_texture.ReceiveAddressOf());
    if (FAILED(hr2)) {
      return nullptr;
    }

    HRESULT hr3 =
        device->CreateUnorderedAccessView(entry.d3d11_texture.Get(), nullptr, entry.uav.ReceiveAddressOf());
    if (FAILED(hr3)) {
      entry.d3d11_texture.Reset();
      return nullptr;
    }

    ComPtr<IDXGIResource1> dxgi_resource;
    if (FAILED(entry.d3d11_texture->QueryInterface(kIID_IDXGIResource1, (void**)dxgi_resource.ReceiveAddressOf()))) {
      entry.d3d11_texture.Reset();
      entry.uav.Reset();
      return nullptr;
    }
    HANDLE shared_handle = nullptr;
    if (FAILED(dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &shared_handle)) ||
        shared_handle == nullptr) {
      entry.d3d11_texture.Reset();
      entry.uav.Reset();
      return nullptr;
    }
    // Real IID for ID3D12Resource -- same real value as
    // DumbD3DMemoryAllocator::createResource()'s own kIID_ID3D12Resource
    // above, but that one is a function-local `static const GUID` (no
    // external linkage to reach via `extern` from here), so this is its
    // own, separate copy of the same real constant, not a shared
    // declaration.
    static const GUID kIID_ID3D12Resource = {
        0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
    HRESULT hr4 = d3d12_device->OpenSharedHandle(
        shared_handle, kIID_ID3D12Resource, (void**)entry.d3d12_resource.ReceiveAddressOf());
    CloseHandle(shared_handle);
    if (FAILED(hr4)) {
      entry.d3d11_texture.Reset();
      entry.uav.Reset();
      return nullptr;
    }

    entry.format = plane_format;
    entry.width = plane_width;
    entry.height = plane_height;
  }

  ComPtr<ID3D11Device3> device3;
  HRESULT hr0 = device->QueryInterface(kIID_ID3D11Device3, (void**)device3.ReceiveAddressOf());
  if (FAILED(hr0)) {
    return nullptr;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC1 srv_desc = {};
  srv_desc.Format = plane_format;
  srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  srv_desc.Texture2DArray.MostDetailedMip = 0;
  srv_desc.Texture2DArray.MipLevels = 1;
  srv_desc.Texture2DArray.FirstArraySlice = array_index;
  srv_desc.Texture2DArray.ArraySize = 1;
  srv_desc.Texture2DArray.PlaneSlice = plane_slice;
  ComPtr<ID3D11ShaderResourceView1> srv;
  HRESULT hr1 = device3->CreateShaderResourceView1(nv12_texture, &srv_desc, srv.ReceiveAddressOf());
  if (FAILED(hr1)) {
    return nullptr;
  }

  bool created_now = false;
  ID3D11ComputeShader* shader = GetOrCreateCopyPlaneShader(
      device, plane_format == DXGI_FORMAT_R8_UNORM ? kCopyPlaneShaderR8 : kCopyPlaneShaderR8G8, &created_now);
  if (shader == nullptr) {
    return nullptr;
  }

  ID3D11ShaderResourceView* raw_srv = srv.Get();
  ID3D11UnorderedAccessView* raw_uav = entry.uav.Get();
  context->CSSetShader(shader, nullptr, 0);
  context->CSSetShaderResources(0, 1, &raw_srv);
  context->CSSetUnorderedAccessViews(0, 1, &raw_uav, nullptr);
  context->Dispatch((plane_width + 7u) / 8u, (plane_height + 7u) / 8u, 1);
  ID3D11ShaderResourceView* null_srv = nullptr;
  ID3D11UnorderedAccessView* null_uav = nullptr;
  context->CSSetShaderResources(0, 1, &null_srv);
  context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);

  return entry.d3d12_resource.Get();
}

// The one real thing that must outlive the returned SkImage's own use of
// the two imported D3D12 resources: the retained AVFrame crtmedia_gpu_
// frame.release_context already owns (see gpu_frame_d3d11.h's own top
// comment) -- kept alive only so the FFmpeg-owned shared decode-pool
// texture this bridge's own GPU-copy step reads from is not reused by a
// later decode while Skia might still (in principle) revisit it; the two
// destination D3D12 resources this SkImage actually samples are already
// fully independent copies by this point, matching the "measured GPU-copy
// fallback" (not literal zero-copy) this tranche's own acceptance gate
// explicitly allows -- see crtgfx/skia_media.h's own top comment.
struct MediaFrameReleaseContext {
  crtmedia_gpu_frame frame;
};

void ReleaseMediaFrame(SkImages::ReleaseContext release_context) {
  auto* ctx = static_cast<MediaFrameReleaseContext*>(release_context);
  crtmedia_gpu_frame_release(&ctx->frame);
  delete ctx;
}

}  // namespace

sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context, const crtgfx_gpu_device* device, crtmedia_gpu_frame* frame) {
  if (context == nullptr || device == nullptr || frame == nullptr ||
      frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU || frame->native_handle == nullptr ||
      frame->format != CRTMEDIA_PIXEL_FORMAT_NV12) {
    return nullptr;
  }

  crtgfx_gpu_win32_device_view device_view = {};
  if (!crtgfx_gpu_win32_borrow_device(device, &device_view)) {
    return nullptr;
  }
  auto* d3d12_device = reinterpret_cast<ID3D12Device*>(device_view.device);
  auto* d3d12_adapter = reinterpret_cast<IDXGIAdapter1*>(device_view.adapter);

  auto* handle = static_cast<crtmedia_d3d11_gpu_frame_handle*>(frame->native_handle);
  auto* nv12_texture = static_cast<ID3D11Texture2D*>(handle->texture);

  ComPtr<ID3D11Device> d3d11_device;
  nv12_texture->GetDevice(d3d11_device.ReceiveAddressOf());
  if (!d3d11_device) {
    return nullptr;
  }
  ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device->GetImmediateContext(d3d11_context.ReceiveAddressOf());

  // Real device-affinity check (docs/crtmedia_zero_copy_decode_
  // acceptance.md's own "Device-affinity pairing" scope item, resolved
  // here as this tranche's own frozen decision): FFmpeg's own D3D11VA
  // device and crtgfx_gpu_device's own D3D12 device are each created
  // independently (av_hwdevice_ctx_create(..., NULL, NULL, 0) and
  // crtgfx_gpu_win32_device_create(0, ...) respectively) and are not
  // guaranteed to be the same physical adapter on a real multi-GPU
  // machine -- NT-handle resource sharing only works within one adapter.
  // Confirmed for real on this session's own single-GPU host (this file's
  // own top-comment probe #1); on a host where they differ, this bails out
  // to the caller's existing CPU-transfer fallback rather than attempting
  // (and failing) a cross-adapter share.
  ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device->QueryInterface(kIID_IDXGIDevice, (void**)dxgi_device.ReceiveAddressOf()))) {
    return nullptr;
  }
  ComPtr<IDXGIAdapter> d3d11_adapter;
  if (FAILED(dxgi_device->GetAdapter(d3d11_adapter.ReceiveAddressOf()))) {
    return nullptr;
  }
  // RealDxgiAdapterDesc/RealDxgiAdapterDesc1, not DXGI_ADAPTER_DESC/
  // DXGI_ADAPTER_DESC1 -- see those structs' own top comment (this file,
  // above) for the real -fwchar-type=int ABI-corruption bug this avoids.
  // reinterpret_cast is safe here: GetDesc()/GetDesc1() are real vtable
  // calls into the system's own dxgi.dll, which writes raw bytes per the
  // real Windows ABI regardless of this call site's own static pointer
  // type -- only the byte layout (which these structs now correctly
  // match) matters.
  RealDxgiAdapterDesc d3d11_adapter_desc;
  if (FAILED(d3d11_adapter->GetDesc(reinterpret_cast<DXGI_ADAPTER_DESC*>(&d3d11_adapter_desc)))) {
    return nullptr;
  }
  RealDxgiAdapterDesc1 d3d12_adapter_desc;
  if (FAILED(d3d12_adapter->GetDesc1(reinterpret_cast<DXGI_ADAPTER_DESC1*>(&d3d12_adapter_desc)))) {
    return nullptr;
  }
  if (d3d11_adapter_desc.AdapterLuid.LowPart != d3d12_adapter_desc.AdapterLuid.LowPart ||
      d3d11_adapter_desc.AdapterLuid.HighPart != d3d12_adapter_desc.AdapterLuid.HighPart) {
    return nullptr;
  }
  const UINT y_width = frame->width;
  const UINT y_height = frame->height;
  const UINT uv_width = (frame->width + 1u) / 2u;
  const UINT uv_height = (frame->height + 1u) / 2u;

  const UINT array_index = static_cast<UINT>(handle->array_index);

  ID3D12Resource* y_resource_raw = GetOrRefreshPlaneD3D12Resource(
      d3d11_device.Get(), d3d11_context.Get(), d3d12_device, nv12_texture, array_index, 0, 0, DXGI_FORMAT_R8_UNORM,
      y_width, y_height);
  if (y_resource_raw == nullptr) {
    return nullptr;
  }
  ID3D12Resource* uv_resource_raw = GetOrRefreshPlaneD3D12Resource(
      d3d11_device.Get(), d3d11_context.Get(), d3d12_device, nv12_texture, array_index, 1, 1,
      DXGI_FORMAT_R8G8_UNORM, uv_width, uv_height);
  if (uv_resource_raw == nullptr) {
    return nullptr;
  }
  // Real, CPU-blocking GPU sync (2026-09-23, found necessary for real on
  // this host, not merely theoretical): d3d11_context->Flush() alone only
  // *submits* the compute-shader copy's command list to the GPU -- it does
  // not wait for that work to actually finish executing. Without a real
  // wait here, D3D12's own OpenSharedHandle()+first sample of the shared
  // resource below can race the D3D11 compute-shader write that produces
  // its contents -- confirmed for real: without this block, this exact
  // pipeline read visibly corrupted, garbage-looking data through a
  // completely unrelated DXGI call (IDXGIAdapter1::GetDesc1() on this
  // function's own device-affinity-check adapter, several frames later,
  // once enough outstanding cross-API races had accumulated) -- a real
  // driver-level hazard from unsynchronized cross-API shared-resource
  // access, not a logic bug in the LUID check itself. crtgfx_gpu_fence
  // (crtgfx/gpu.h) is explicitly documented as CPU-only and must not be
  // reused for this (docs/crtmedia_zero_copy_decode_acceptance.md's own
  // Scope section) -- a real device-affine GPU fence, if ever needed, is
  // separate future work; a plain D3D11_QUERY_EVENT CPU-block is the
  // simple, correct, always-available substitute this tranche's own gate
  // does not forbid (it only forbids CPU *pixel* readback, not a CPU wait
  // for GPU completion).
  D3D11_QUERY_DESC sync_query_desc = {};
  sync_query_desc.Query = D3D11_QUERY_EVENT;
  ComPtr<ID3D11Query> sync_query;
  if (SUCCEEDED(d3d11_device->CreateQuery(&sync_query_desc, sync_query.ReceiveAddressOf()))) {
    d3d11_context->End(sync_query.Get());
    d3d11_context->Flush();
    while (d3d11_context->GetData(sync_query.Get(), nullptr, 0, 0) == S_FALSE) {
      /* Real, deliberate CPU spin-wait -- this project has no portable
       * cross-thread sleep primitive available inside this translation
       * unit, and this wait is expected to be short (one small compute
       * dispatch's own real completion latency), matching this file's own
       * "CPU-blocking, not CPU-readback" allowance above. */
    }
  } else {
    // A query is a real, ordinary D3D11 object -- CreateQuery should not
    // realistically fail here, but if it ever does, fall back to the
    // weaker Flush()-only behavior rather than hard-failing the whole
    // import (matches this function's own general "degrade, don't crash"
    // posture elsewhere).
    d3d11_context->Flush();
  }

  GrD3DTextureResourceInfo y_info;
  y_info.fResource.retain(y_resource_raw);
  y_info.fResourceState = D3D12_RESOURCE_STATE_COMMON;
  y_info.fFormat = DXGI_FORMAT_R8_UNORM;
  y_info.fSampleCount = 1;
  y_info.fLevelCount = 1;

  GrD3DTextureResourceInfo uv_info;
  uv_info.fResource.retain(uv_resource_raw);
  uv_info.fResourceState = D3D12_RESOURCE_STATE_COMMON;
  uv_info.fFormat = DXGI_FORMAT_R8G8_UNORM;
  uv_info.fSampleCount = 1;
  uv_info.fLevelCount = 1;

  GrBackendTexture textures[SkYUVAInfo::kMaxPlanes] = {
      GrBackendTextures::MakeD3D(static_cast<int>(y_width), static_cast<int>(y_height), y_info),
      GrBackendTextures::MakeD3D(static_cast<int>(uv_width), static_cast<int>(uv_height), uv_info), {}, {}};

  // BT.709 limited range -- see the Metal branch's own identical comment
  // above for why (crtmedia_gpu_frame carries no color metadata yet).
  SkYUVAInfo yuva_info(
      SkISize::Make(static_cast<int>(frame->width), static_cast<int>(frame->height)), SkYUVAInfo::PlaneConfig::kY_UV,
      SkYUVAInfo::Subsampling::k420, kRec709_Limited_SkYUVColorSpace);
  GrYUVABackendTextures yuva_textures(yuva_info, textures, kTopLeft_GrSurfaceOrigin);
  if (!yuva_textures.isValid()) {
    return nullptr;
  }

  // Ownership transfer point (crtgfx/skia_media.h's own frozen contract) --
  // identical shape to the Metal branch's own matching comment above.
  auto* release_ctx = new MediaFrameReleaseContext{*frame};
  sk_sp<SkImage> image =
      SkImages::TextureFromYUVATextures(context, yuva_textures, nullptr, ReleaseMediaFrame, release_ctx);
  if (image == nullptr) {
    delete release_ctx;
    return nullptr;
  }
  memset(frame, 0, sizeof(*frame));
  return image;
}

#endif  // CRTGFX_HAS_SKIA_HEADERS && CRTGFX_HAVE_D3D12

#elif defined(CRTGFX_HAVE_METAL)

// Real Ganesh/Metal offscreen vertical slice (2026-09-04) -- the macOS
// sibling of the Vulkan/D3D12 branches above (see crtgfx/skia.h's own,
// fuller comment on both functions below). Real <Metal/Metal.h>-adjacent
// headers are forced here by Skia's own public GrMtlBackendContext.h/
// GrMtlTypes.h (GrMtlTypes.h itself #includes <TargetConditionals.h>;
// GrMtlBackendContext.h #includes include/ports/SkCFObject.h, which
// #imports <CoreFoundation/CoreFoundation.h>) -- a real, deliberate
// exception to this project's own no-host-SDK-header policy, already
// codified in docs/libcrtgfx_api_policy.md's own "third-party source
// being ported" Non-Goals clause, exactly like the D3D12 branch's own
// GrD3DTypes.h. This translation unit is compiled with tools/crt-c++'s
// own -fcrt-real-apple-sdk sentinel specifically because of this
// #include (libcrtgfx/CMakeLists.txt) -- see that flag's own top comment
// in tools/crt-c++ for the exact real <sys/cdefs.h>-shadowing/
// ptrcheck.h compile failure it fixes, and the separate, real -dead_strip
// interaction (a real Metal device-creation crash deep in Apple's own
// LaunchServices internals) it also avoids for this exact executable at
// link time. src/arch/macos/gpu_metal.c itself stays real-host-header-
// free (drives the Objective-C runtime directly, matching window_
// cocoa.c's own convention) -- only this file needs the real headers.

#include "gpu_internal.h"

#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"

sk_sp<GrDirectContext> crtgfx_skia_make_gpu_context(const crtgfx_gpu_device* device) {
  crtgfx_gpu_metal_device_view view = {};
  if (!crtgfx_gpu_metal_borrow_device(device, &view)) {
    return nullptr;
  }

  // GrMtlBackendContext's own fDevice/fQueue are sk_cfp<GrMTLHandle> (a
  // CoreFoundation-style CFRetain/CFRelease smart pointer -- real
  // Objective-C objects are toll-free-bridged with CFRetain/CFRelease on
  // Apple platforms, so this works identically to a real -retain/
  // -release message send). .retain(), not sk_cfp's own single-argument
  // "adopt, no extra ref" constructor -- matching the D3D12 branch's own
  // .retain() reasoning exactly: `device` itself keeps its own, separate
  // reference (crtgfx_gpu_device_release() still owns and eventually
  // releases the real one, via gpu_metal.c's own plain -release message
  // sends), so this must take its own, independent reference rather than
  // adopt the existing one outright.
  GrMtlBackendContext backend_context;
  backend_context.fDevice.retain(view.device);
  backend_context.fQueue.retain(view.command_queue);

  // No memory-allocator field to supply at all here -- unlike the Vulkan/
  // D3D12 branches above, GrMtlBackendContext declares only fDevice/
  // fQueue (confirmed by reading Skia's own public GrMtlBackendContext.h
  // directly): Ganesh's own Metal backend allocates through Metal's own
  // real, built-in resource/heap management, needing no caller-supplied
  // substitute.
  return GrDirectContexts::MakeMetal(backend_context);
}

sk_sp<SkSurface> crtgfx_skia_make_gpu_offscreen_surface(
    GrDirectContext* context, uint32_t width, uint32_t height) {
  if (context == nullptr || width == 0 || height == 0) {
    return nullptr;
  }
  SkImageInfo info = SkImageInfo::Make(
      (int)width, (int)height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  // SkSurfaces::RenderTarget() -- confirmed backend-agnostic, the exact
  // same real call already proven for Vulkan and D3D12 (see this file's
  // own Vulkan branch, above): Ganesh's own GrResourceProvider allocates
  // and owns a real backing MTLTexture internally; this vertical slice
  // never hand-manages one itself (see crtgfx/skia.h's own comment on why
  // that is deliberate, not a shortcut).
  return SkSurfaces::RenderTarget(context, skgpu::Budgeted::kNo, info);
}

// Wires the offscreen Ganesh pipeline above onto a live crtgfx_gpu_
// surface's own acquired drawable texture (2026-09-07 -- see crtgfx/
// skia.h's own, fuller comment on both functions). Simplest of the three
// real backends -- MTLTexture has no image-layout/resource-state concept
// at all, unlike the Vulkan/D3D12 branches above, so crtgfx_skia_gpu_
// surface_present() below needs no manual barrier of its own; the only
// real per-host work is handing Ganesh's own flushed/submitted drawable a
// fresh presenting command buffer, via the new crtgfx_gpu_metal_surface_
// prepare_ganesh_present() hook (gpu_metal.c -- see that function's own
// declaration in gpu_internal.h for why this one piece lives there rather
// than here, unlike the Vulkan/D3D12 siblings).
sk_sp<SkSurface> crtgfx_skia_wrap_gpu_surface(GrDirectContext* context, crtgfx_gpu_surface* surface) {
  crtgfx_gpu_metal_surface_view view = {};
  if (context == nullptr || !crtgfx_gpu_metal_begin_ganesh(surface, &view)) {
    return nullptr;
  }

  GrMtlTextureInfo texture_info;
  texture_info.fTexture.retain(view.texture);

  GrBackendRenderTarget backend_target = GrBackendRenderTargets::MakeMtl(
      static_cast<int>(view.width), static_cast<int>(view.height), texture_info);
  sk_sp<SkSurface> sk_surface = SkSurfaces::WrapBackendRenderTarget(
      context, backend_target, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
  if (sk_surface == nullptr) crtgfx_gpu_metal_end_ganesh(surface);
  return sk_surface;
}

crtgfx_result crtgfx_skia_gpu_surface_present(
    GrDirectContext* context, SkSurface* surface, crtgfx_gpu_surface* gpu_surface) {
  if (context == nullptr || surface == nullptr || gpu_surface == nullptr) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (!crtgfx_gpu_metal_is_ganesh_wrapped(gpu_surface)) {
    return CRTGFX_ERROR_HOST;
  }

  // Ganesh submits its own internal command buffer(s), drawing into the
  // wrapped texture, against the same shared crtgfx_gpu_device::mtl_
  // command_queue this surface's own device was built against.
  // GrSyncCpu::kYes -- see the Vulkan branch's own crtgfx_skia_gpu_
  // surface_present() comment (this file, above) for the real, found-for-
  // real reason (not independently re-verified for Metal specifically, no
  // macOS hardware this session, but the same real Ganesh-internal
  // deferred-release risk applies in principle -- see crtgfx_gpu_metal_
  // surface_resize()'s own top comment for this frame's other real
  // reasoned-but-unverified pieces).
  context->flushAndSubmit(surface, GrSyncCpu::kYes);

  if (crtgfx_gpu_metal_surface_prepare_ganesh_present(gpu_surface) != CRTGFX_OK) {
    return CRTGFX_ERROR_HOST;
  }

  return crtgfx_gpu_surface_present(gpu_surface);
}

// Zero-copy decoded-texture bridge (2026-09-23, "Zero-copy decoded
// textures" Tranche 1 -- see crtgfx/skia_media.h's own comment for the
// full frozen contract and ownership rule). CVPixelBuffer -> Metal Y/UV
// textures -> GrYUVABackendTextures -> SkImage, no CPU readback and no
// RGBA intermediate: Ganesh samples the real NV12 planes directly as YUV.
#include "crtgfx/skia_media.h"

#if CRTGFX_HAS_SKIA_HEADERS && defined(CRTGFX_HAVE_METAL)

#include "include/core/SkYUVAInfo.h"
#include "include/gpu/ganesh/GrYUVABackendTextures.h"
#include "include/gpu/ganesh/SkImageGanesh.h"

namespace {

extern "C" {
// Real, stable, exported CoreVideo/CoreFoundation C entry points -- hand-
// declared rather than #import<CoreVideo/CoreVideo.h>/<CoreFoundation/
// CoreFoundation.h>, matching this project's own established no-real-
// Apple-header policy for its own authored code (gpu_metal.c's own top
// comment; the real Apple-SDK headers this translation unit already
// requires, per this file's own top-of-Metal-branch comment, are Skia's
// own forced exception, not license to import more of our own). Every
// signature and the whole retain/release lifetime below is confirmed for
// real (2026-09-23) via two standalone probes on this exact real macOS
// host, outside this project's own toolchain (plain Objective-C compiled
// with the system's real clang): (1) this exact call sequence produces
// real, correctly-sized Y (R8Unorm) and UV (RG8Unorm) MTLTexture objects
// from a real CVPixelBuffer; (2) releasing the CVMetalTextureRef and the
// CVMetalTextureCache itself immediately after independently retaining
// the MTLTexture object (matching GrMtlTextureInfo::fTexture.retain()'s
// own semantics, below) leaves that MTLTexture still valid and still
// sampling the pixel buffer's real, live, current memory -- confirmed by
// writing a fresh byte pattern into the pixel buffer *after* releasing
// both and reading it back through the still-held texture. This is what
// makes keeping only the retained AVFrame (crtmedia_gpu_frame's own
// release_context) alive, and not the CVMetalTextureRef/cache, both
// correct and sufficient below.
int CVMetalTextureCacheCreate(
    const void* allocator, const void* cache_attributes, void* metal_device, const void* texture_attributes,
    void** cache_out);
int CVMetalTextureCacheCreateTextureFromImage(
    const void* allocator, void* texture_cache, void* source_image, const void* texture_attributes,
    unsigned pixel_format, size_t width, size_t height, size_t plane_index, void** texture_out);
void* CVMetalTextureGetTexture(void* image);
size_t CVPixelBufferGetWidthOfPlane(void* pixel_buffer, size_t plane_index);
size_t CVPixelBufferGetHeightOfPlane(void* pixel_buffer, size_t plane_index);
void CFRelease(const void* cf);
}

// Real, stable, public Metal.framework pixel-format values -- confirmed via
// the same standalone probe above, not guessed: MTLPixelFormatR8Unorm/
// RG8Unorm have been part of Metal's public ABI since its 2014 introduction
// and cannot change without breaking every existing real Metal application,
// matching this project's own established "confirmed for real" standard
// for hand-declared host constants elsewhere (gpu_metal.c, gpu_win32.c).
constexpr unsigned kMTLPixelFormatR8Unorm = 10;
constexpr unsigned kMTLPixelFormatRG8Unorm = 30;

// The one real thing that must outlive the returned SkImage's use of the
// imported textures: the retained AVFrame crtmedia_gpu_frame.release_
// context already owns, which is what actually keeps the real
// CVPixelBuffer (frame.native_handle) alive -- not the CVMetalTextureRef/
// CVMetalTextureCache, both released immediately after use above (see
// this file's own confirmed-for-real comment on the hand-declared
// CoreVideo functions).
struct MediaFrameReleaseContext {
  crtmedia_gpu_frame frame;
};

void ReleaseMediaFrame(SkImages::ReleaseContext release_context) {
  auto* ctx = static_cast<MediaFrameReleaseContext*>(release_context);
  crtmedia_gpu_frame_release(&ctx->frame);
  delete ctx;
}

}  // namespace

sk_sp<SkImage> crtgfx_skia_import_media_frame(
    GrDirectContext* context, const crtgfx_gpu_device* device, crtmedia_gpu_frame* frame) {
  if (context == nullptr || device == nullptr || frame == nullptr ||
      frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU || frame->native_handle == nullptr ||
      frame->format != CRTMEDIA_PIXEL_FORMAT_NV12) {
    return nullptr;
  }

  crtgfx_gpu_metal_device_view device_view = {};
  if (!crtgfx_gpu_metal_borrow_device(device, &device_view)) {
    return nullptr;
  }

  void* pixel_buffer = frame->native_handle;
  void* texture_cache = nullptr;
  if (CVMetalTextureCacheCreate(nullptr, nullptr, device_view.device, nullptr, &texture_cache) != 0 ||
      texture_cache == nullptr) {
    return nullptr;
  }

  size_t y_width = CVPixelBufferGetWidthOfPlane(pixel_buffer, 0);
  size_t y_height = CVPixelBufferGetHeightOfPlane(pixel_buffer, 0);
  size_t uv_width = CVPixelBufferGetWidthOfPlane(pixel_buffer, 1);
  size_t uv_height = CVPixelBufferGetHeightOfPlane(pixel_buffer, 1);

  void* y_texture_ref = nullptr;
  int y_result = CVMetalTextureCacheCreateTextureFromImage(
      nullptr, texture_cache, pixel_buffer, nullptr, kMTLPixelFormatR8Unorm, y_width, y_height, 0, &y_texture_ref);
  void* uv_texture_ref = nullptr;
  int uv_result = (y_result != 0) ? -1
                                   : CVMetalTextureCacheCreateTextureFromImage(
                                         nullptr, texture_cache, pixel_buffer, nullptr, kMTLPixelFormatRG8Unorm,
                                         uv_width, uv_height, 1, &uv_texture_ref);

  GrMtlTextureInfo y_info;
  GrMtlTextureInfo uv_info;
  if (y_result == 0 && y_texture_ref != nullptr) {
    y_info.fTexture.retain(CVMetalTextureGetTexture(y_texture_ref));
    CFRelease(y_texture_ref);
  }
  if (uv_result == 0 && uv_texture_ref != nullptr) {
    uv_info.fTexture.retain(CVMetalTextureGetTexture(uv_texture_ref));
    CFRelease(uv_texture_ref);
  }
  CFRelease(texture_cache);
  if (y_result != 0 || uv_result != 0) {
    return nullptr;
  }

  GrBackendTexture textures[SkYUVAInfo::kMaxPlanes] = {
      GrBackendTextures::MakeMtl(
          static_cast<int>(y_width), static_cast<int>(y_height), skgpu::Mipmapped::kNo, y_info),
      GrBackendTextures::MakeMtl(
          static_cast<int>(uv_width), static_cast<int>(uv_height), skgpu::Mipmapped::kNo, uv_info),
      {}, {}};

  // BT.709 limited range: crtmedia_gpu_frame carries no color metadata yet
  // (docs/crtmedia_zero_copy_decode_acceptance.md's own Scope section --
  // matches CRTMEDIA_COLOR_SPACE_UNSPECIFIED's own default for the CPU
  // path, src/frame_convert.c).
  SkYUVAInfo yuva_info(
      SkISize::Make(static_cast<int>(frame->width), static_cast<int>(frame->height)), SkYUVAInfo::PlaneConfig::kY_UV,
      SkYUVAInfo::Subsampling::k420, kRec709_Limited_SkYUVColorSpace);
  GrYUVABackendTextures yuva_textures(yuva_info, textures, kTopLeft_GrSurfaceOrigin);
  if (!yuva_textures.isValid()) {
    return nullptr;
  }

  // Ownership transfer point (crtgfx/skia_media.h's own frozen contract):
  // release_ctx->frame takes over frame's own real backing (the retained
  // AVFrame in release_context) via this copy; the caller's own *frame is
  // zeroed below so a caller that still calls crtmedia_gpu_frame_release()
  // on it finds release == NULL rather than double-releasing that AVFrame.
  auto* release_ctx = new MediaFrameReleaseContext{*frame};
  sk_sp<SkImage> image =
      SkImages::TextureFromYUVATextures(context, yuva_textures, nullptr, ReleaseMediaFrame, release_ctx);
  if (image == nullptr) {
    delete release_ctx;
    return nullptr;
  }
  memset(frame, 0, sizeof(*frame));
  return image;
}

#endif  // CRTGFX_HAS_SKIA_HEADERS && CRTGFX_HAVE_METAL

#endif  // CRTGFX_HAVE_VULKAN / CRTGFX_HAVE_D3D12 / CRTGFX_HAVE_METAL
