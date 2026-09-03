#include "crtgfx/skia.h"

#include "include/core/SkImageInfo.h"

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

sk_sp<SkTypeface> crtgfx_skia_default_typeface(SkFontMgr* font_mgr, const SkFontStyle& style) {
  if (font_mgr == nullptr) {
    return nullptr;
  }
  sk_sp<SkTypeface> typeface = font_mgr->matchFamilyStyle("Pretendard GOV", style);
  if (!typeface) {
    typeface = font_mgr->matchFamilyStyle("DejaVu Sans Mono", style);
  }
  if (!typeface) {
    typeface = font_mgr->legacyMakeTypeface(nullptr, style);
  }
  return typeface;
}

#if defined(CRTGFX_HAVE_VULKAN)

// Real Ganesh/Vulkan offscreen vertical slice (2026-09-03) -- see crtgfx/
// skia.h's own, fuller comment on both functions below.

#include "gpu_internal.h"

#include <cstdio>

#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanMemoryAllocator.h"
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
  if (device == nullptr || device->vk_instance == nullptr || device->vk_device == nullptr) {
    return nullptr;
  }

  skgpu::VulkanBackendContext backend_context;
  backend_context.fInstance = reinterpret_cast<VkInstance>(device->vk_instance);
  backend_context.fPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(device->vk_physical_device);
  backend_context.fDevice = reinterpret_cast<VkDevice>(device->vk_device);
  backend_context.fQueue = reinterpret_cast<VkQueue>(device->vk_queue);
  backend_context.fGraphicsQueueIndex = device->vk_queue_family_index;
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

#endif  // CRTGFX_HAVE_VULKAN
