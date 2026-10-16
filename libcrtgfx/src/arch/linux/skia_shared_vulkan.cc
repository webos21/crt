#include "crtgfx/skia_shared.h"

#include <cstring>

#if defined(CRTGFX_HAVE_VULKAN)
#include "../../gpu_internal.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/vk/VulkanMutableTextureState.h"
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

struct Device {
  crtgfx_gpu_vulkan_device_view view {};
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  PFN_vkGetMemoryFdKHR get_memory = nullptr;
  PFN_vkGetSemaphoreFdKHR get_semaphore = nullptr;
  PFN_vkImportSemaphoreFdKHR import_semaphore = nullptr;
  PFN_vkGetFenceFdKHR get_fence = nullptr;
  PFN_vkImportFenceFdKHR import_fence = nullptr;
  bool init(const crtgfx_gpu_device* source) {
    if (!crtgfx_gpu_vulkan_borrow_device(source, &view)) return false;
    auto enabled = [&](const char* name) {
      for (uint32_t i = 0; i < view.device_extension_count; ++i)
        if (!strcmp(name, view.device_extension_names[i])) return true;
      return false;
    };
    if (!enabled("VK_KHR_external_memory_fd") || !enabled("VK_KHR_external_semaphore_fd")
        || !enabled("VK_KHR_external_fence_fd")) return false;
    device = reinterpret_cast<VkDevice>(view.device);
    physical = reinterpret_cast<VkPhysicalDevice>(view.physical_device);
    queue = reinterpret_cast<VkQueue>(view.queue);
    get_memory = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    get_semaphore = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
    import_semaphore = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(vkGetDeviceProcAddr(device, "vkImportSemaphoreFdKHR"));
    get_fence = reinterpret_cast<PFN_vkGetFenceFdKHR>(vkGetDeviceProcAddr(device, "vkGetFenceFdKHR"));
    import_fence = reinterpret_cast<PFN_vkImportFenceFdKHR>(vkGetDeviceProcAddr(device, "vkImportFenceFdKHR"));
    return get_memory && get_semaphore && import_semaphore && get_fence && import_fence;
  }
};

crtgfx_result query(Device& d, crtgfx_shared_image_desc* out, uint32_t width = 0, uint32_t height = 0) {
  VkPhysicalDeviceIDProperties ids {};
  ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  VkPhysicalDeviceProperties2 properties {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &ids;
  vkGetPhysicalDeviceProperties2(d.physical, &properties);
  VkPhysicalDeviceExternalImageFormatInfo external {};
  external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkPhysicalDeviceImageFormatInfo2 info {};
  info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
  info.pNext = &external;
  info.format = VK_FORMAT_B8G8R8A8_UNORM;
  info.type = VK_IMAGE_TYPE_2D;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage;
  info.flags = VK_IMAGE_CREATE_ALIAS_BIT;
  VkExternalImageFormatProperties external_properties {};
  external_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
  VkImageFormatProperties2 image_properties {};
  image_properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
  image_properties.pNext = &external_properties;
  if (vkGetPhysicalDeviceImageFormatProperties2(d.physical, &info, &image_properties) != VK_SUCCESS
      || (external_properties.externalMemoryProperties.externalMemoryFeatures
          & (VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT))
          != (VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT))
    return CRTGFX_ERROR_UNSUPPORTED;
  if (width > image_properties.imageFormatProperties.maxExtent.width
      || height > image_properties.imageFormatProperties.maxExtent.height)
    return CRTGFX_ERROR_UNSUPPORTED;
  VkPhysicalDeviceExternalSemaphoreInfo sem_info {};
  sem_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
  sem_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkExternalSemaphoreProperties sem_properties {};
  sem_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
  vkGetPhysicalDeviceExternalSemaphoreProperties(d.physical, &sem_info, &sem_properties);
  if ((sem_properties.externalSemaphoreFeatures
       & (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT))
      != (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT))
    return CRTGFX_ERROR_UNSUPPORTED;
  VkPhysicalDeviceExternalFenceInfo fence_info {};
  fence_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
  fence_info.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkExternalFenceProperties fence_properties {};
  fence_properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES;
  vkGetPhysicalDeviceExternalFenceProperties(d.physical, &fence_info, &fence_properties);
  if ((fence_properties.externalFenceFeatures
       & (VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT))
      != (VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT | VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT))
    return CRTGFX_ERROR_UNSUPPORTED;
  *out = {};
  out->version = CRTGFX_SHARED_IMAGE_VERSION;
  out->backend = CRTGFX_GPU_BACKEND_VULKAN;
  out->handle_kind = CRTGFX_SHARED_IMAGE_OPAQUE_FD;
  out->format = CRTGFX_SHARED_IMAGE_BGRA8;
  memcpy(out->device_uuid, ids.deviceUUID, sizeof out->device_uuid);
  memcpy(out->driver_uuid, ids.driverUUID, sizeof out->driver_uuid);
  return CRTGFX_OK;
}

int duplicate_fd(int fd) { return fcntl(fd, F_DUPFD_CLOEXEC, 0); }
bool cloexec(int fd) { return fd >= 0 && fcntl(fd, F_SETFD, FD_CLOEXEC) == 0; }

class SharedImage final : public crtgfx_skia_shared_image {
 public:
  Device d;
  crtgfx_shared_image_desc desc {};
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkSemaphore semaphores[2] {};
  VkFence fences[2] {};
  GrBackendTexture texture;
  GrDirectContext* context = nullptr;
  bool producer = false, written = false, writing = false, reading = false, poisoned = false;

  ~SharedImage() override {
    if (!d.device) return;
    // Callers drop all Skia references first. Native objects stay alive through
    // local queue completion; peer imports own independent kernel references.
    vkDeviceWaitIdle(d.device);
    for (auto fence : fences) if (fence) vkDestroyFence(d.device, fence, nullptr);
    for (auto semaphore : semaphores) if (semaphore) vkDestroySemaphore(d.device, semaphore, nullptr);
    if (image) vkDestroyImage(d.device, image, nullptr);
    if (memory) vkFreeMemory(d.device, memory, nullptr);
  }
  const crtgfx_shared_image_desc& description() const override { return desc; }

  crtgfx_result allocate_image(const int* inputs) {
    VkExternalMemoryImageCreateInfo external {};
    external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkImageCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = &external;
    info.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_B8G8R8A8_UNORM;
    info.extent = {desc.width, desc.height, 1};
    info.mipLevels = 1; info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(d.device, &info, nullptr, &image) != VK_SUCCESS) return CRTGFX_ERROR_UNSUPPORTED;
    VkMemoryRequirements requirements {};
    vkGetImageMemoryRequirements(d.device, image, &requirements);
    if (producer) {
      VkPhysicalDeviceMemoryProperties properties {};
      vkGetPhysicalDeviceMemoryProperties(d.physical, &properties);
      uint32_t type = 0;
      for (; type < properties.memoryTypeCount; ++type)
        if ((requirements.memoryTypeBits & (1u << type))
            && (properties.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
      if (type == properties.memoryTypeCount) return CRTGFX_ERROR_UNSUPPORTED;
      desc.memory_type = type; desc.allocation_size = requirements.size;
    } else if (requirements.size != desc.allocation_size || !(requirements.memoryTypeBits & (1u << desc.memory_type)))
      return CRTGFX_ERROR_INVALID_ARGUMENT;
    if (!crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_UNSUPPORTED;
    VkMemoryDedicatedAllocateInfo dedicated {};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicated.image = image;
    VkExportMemoryAllocateInfo export_info {};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.pNext = &dedicated;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    VkImportMemoryFdInfoKHR import_info {};
    import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    import_info.pNext = &dedicated;
    import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    import_info.fd = inputs ? duplicate_fd(inputs[0]) : -1;
    if (inputs && import_info.fd < 0) return CRTGFX_ERROR_INVALID_ARGUMENT;
    VkMemoryAllocateInfo allocation {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.pNext = producer ? static_cast<const void*>(&export_info) : &import_info;
    allocation.allocationSize = desc.allocation_size;
    allocation.memoryTypeIndex = desc.memory_type;
    VkResult allocated = vkAllocateMemory(d.device, &allocation, nullptr, &memory);
    if (allocated != VK_SUCCESS) {
      if (import_info.fd >= 0) close(import_info.fd);
      return CRTGFX_ERROR_UNSUPPORTED;
    }
    if (vkBindImageMemory(d.device, image, memory, 0) != VK_SUCCESS) return CRTGFX_ERROR_HOST;
    return CRTGFX_OK;
  }

  crtgfx_result allocate_sync(const int* inputs) {
    for (unsigned i = 0; i < 2; ++i) {
      VkExportSemaphoreCreateInfo export_sem {};
      export_sem.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
      export_sem.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkSemaphoreCreateInfo sem {};
      sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      sem.pNext = producer ? &export_sem : nullptr;
      if (vkCreateSemaphore(d.device, &sem, nullptr, &semaphores[i]) != VK_SUCCESS) return CRTGFX_ERROR_HOST;
      VkExportFenceCreateInfo export_fence {};
      export_fence.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
      export_fence.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkFenceCreateInfo fence {};
      fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence.pNext = producer ? &export_fence : nullptr;
      if (vkCreateFence(d.device, &fence, nullptr, &fences[i]) != VK_SUCCESS) return CRTGFX_ERROR_HOST;
      if (!inputs) continue;
      VkImportSemaphoreFdInfoKHR import_sem {};
      import_sem.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
      import_sem.semaphore = semaphores[i]; import_sem.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
      import_sem.fd = duplicate_fd(inputs[1 + i]);
      if (import_sem.fd < 0) return CRTGFX_ERROR_INVALID_ARGUMENT;
      if (d.import_semaphore(d.device, &import_sem) != VK_SUCCESS) { close(import_sem.fd); return CRTGFX_ERROR_UNSUPPORTED; }
      VkImportFenceFdInfoKHR import_fence {};
      import_fence.sType = VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR;
      import_fence.fence = fences[i]; import_fence.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
      import_fence.fd = duplicate_fd(inputs[3 + i]);
      if (import_fence.fd < 0) return CRTGFX_ERROR_INVALID_ARGUMENT;
      if (d.import_fence(d.device, &import_fence) != VK_SUCCESS) { close(import_fence.fd); return CRTGFX_ERROR_UNSUPPORTED; }
    }
    return CRTGFX_OK;
  }

  crtgfx_result export_handles(int out[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) out[i] = -1;
    if (!producer) return CRTGFX_ERROR_INVALID_ARGUMENT;
    VkMemoryGetFdInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    info.memory = memory; info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    bool ok = d.get_memory(d.device, &info, &out[0]) == VK_SUCCESS;
    for (unsigned i = 0; ok && i < 2; ++i) {
      VkSemaphoreGetFdInfoKHR sem {};
      sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
      sem.semaphore = semaphores[i]; sem.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
      VkFenceGetFdInfoKHR fence {};
      fence.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
      fence.fence = fences[i]; fence.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
      ok = d.get_semaphore(d.device, &sem, &out[1 + i]) == VK_SUCCESS
          && d.get_fence(d.device, &fence, &out[3 + i]) == VK_SUCCESS;
    }
    for (unsigned i = 0; ok && i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) ok = cloexec(out[i]);
    if (!ok) {
      for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) { if (out[i] >= 0) close(out[i]); out[i] = -1; }
      return CRTGFX_ERROR_HOST;
    }
    return CRTGFX_OK;
  }

  crtgfx_result wait(unsigned index, uint32_t timeout_ms) {
    VkResult result = vkWaitForFences(d.device, 1, &fences[index], VK_TRUE, static_cast<uint64_t>(timeout_ms) * 1000000);
    return result == VK_SUCCESS ? CRTGFX_OK : result == VK_TIMEOUT ? CRTGFX_ERROR_TIMEOUT : CRTGFX_ERROR_HOST;
  }
  crtgfx_result wait_ready(uint32_t timeout_ms) override { return wait(0, timeout_ms); }
  crtgfx_result wait_released(uint32_t timeout_ms) override { return wait(1, timeout_ms); }

  void make_texture(bool first) {
    GrVkImageInfo info;
    info.fImage = image;
    info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    info.fImageLayout = first ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    info.fFormat = VK_FORMAT_B8G8R8A8_UNORM; info.fImageUsageFlags = usage;
    info.fSampleCount = 1; info.fLevelCount = 1;
    info.fCurrentQueueFamily = first ? d.view.queue_family_index : VK_QUEUE_FAMILY_EXTERNAL;
    info.fProtected = skgpu::Protected::kNo; info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    texture = GrBackendTextures::MakeVk(desc.width, desc.height, info);
  }
  bool bind_context(GrDirectContext* ctx) {
    if (!ctx || ctx->abandoned() || poisoned || (context && context != ctx)) return false;
    context = ctx;
    return true;
  }
  bool gpu_wait(GrDirectContext* ctx, unsigned index) {
    GrBackendSemaphore sem = GrBackendSemaphores::MakeVk(semaphores[index]);
    return ctx->wait(1, &sem, false);
  }
  crtgfx_result begin_write(GrDirectContext* ctx, uint32_t timeout_ms, sk_sp<SkSurface>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (!producer || writing || reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    if (written) {
      crtgfx_result result = wait_released(timeout_ms);
      if (result != CRTGFX_OK) return result;
      // Consume the previous epoch's shared fence state before publishing new
      // work. A stale signaled fence must not authorize a second overwrite.
      if (vkResetFences(d.device, 1, &fences[1]) != VK_SUCCESS) return CRTGFX_ERROR_HOST;
      if (!gpu_wait(ctx, 1)) return CRTGFX_ERROR_HOST;
    }
    make_texture(!written);
    *out = SkSurfaces::WrapBackendTexture(ctx, texture, kTopLeft_GrSurfaceOrigin, 1,
        kBGRA_8888_SkColorType, nullptr, nullptr);
    writing = *out != nullptr;
    return writing ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result signal(GrDirectContext* ctx, unsigned index) {
    auto state = skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_EXTERNAL);
    if (!ctx->setBackendTextureState(texture, state) || !ctx->submit(GrSyncCpu::kNo)
        || vkResetFences(d.device, 1, &fences[index]) != VK_SUCCESS) { poisoned = true; return CRTGFX_ERROR_HOST; }
    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &semaphores[index];
    if (vkQueueSubmit(d.queue, 1, &submit, fences[index]) != VK_SUCCESS) { poisoned = true; return CRTGFX_ERROR_HOST; }
    return CRTGFX_OK;
  }
  crtgfx_result end_write(GrDirectContext* ctx, SkSurface* surface) override {
    if (!writing || !surface || surface->recordingContext() != ctx || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush(surface);
    crtgfx_result result = signal(ctx, 0);
    writing = false; written = true;
    return result;
  }
  crtgfx_result begin_read(GrDirectContext* ctx, uint32_t timeout_ms, sk_sp<SkImage>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (producer || writing || reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    crtgfx_result result = wait_ready(timeout_ms);
    if (result != CRTGFX_OK) return result;
    if (vkResetFences(d.device, 1, &fences[0]) != VK_SUCCESS) return CRTGFX_ERROR_HOST;
    if (!gpu_wait(ctx, 0)) return CRTGFX_ERROR_HOST;
    make_texture(false);
    *out = SkImages::BorrowTextureFrom(ctx, texture, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
        kPremul_SkAlphaType, nullptr);
    reading = *out != nullptr;
    return reading ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result end_read(GrDirectContext* ctx) override {
    if (!reading || !bind_context(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush();
    crtgfx_result result = signal(ctx, 1);
    reading = false;
    return result;
  }
};
}  // namespace

crtgfx_result crtgfx_skia_shared_query(const crtgfx_gpu_device* device, crtgfx_shared_image_desc* out) {
  if (!device || !out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  *out = {};
  Device d;
  return d.init(device) ? query(d, out) : CRTGFX_ERROR_UNSUPPORTED;
}

crtgfx_result crtgfx_skia_shared_create(const crtgfx_gpu_device* device, uint32_t width, uint32_t height,
    std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !width || !height || width > CRTGFX_SHARED_IMAGE_MAX_EXTENT || height > CRTGFX_SHARED_IMAGE_MAX_EXTENT)
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  std::unique_ptr<SharedImage> image(new SharedImage);
  image->producer = true;
  if (!image->d.init(device)) return CRTGFX_ERROR_UNSUPPORTED;
  crtgfx_result result = query(image->d, &image->desc, width, height);
  if (result != CRTGFX_OK) return result;
  image->desc.width = width; image->desc.height = height;
  result = image->allocate_image(nullptr);
  if (result == CRTGFX_OK) result = image->allocate_sync(nullptr);
  if (result == CRTGFX_OK) *out = std::move(image);
  return result;
}

crtgfx_result crtgfx_skia_shared_import(const crtgfx_gpu_device* device, const crtgfx_shared_image_desc& desc,
    const int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT], std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !fds || !crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_INVALID_ARGUMENT;
  for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i)
    if (fds[i] < 0 || fcntl(fds[i], F_GETFD) < 0) return CRTGFX_ERROR_INVALID_ARGUMENT;
  std::unique_ptr<SharedImage> image(new SharedImage);
  if (!image->d.init(device)) return CRTGFX_ERROR_UNSUPPORTED;
  crtgfx_shared_image_desc identity {};
  crtgfx_result result = query(image->d, &identity, desc.width, desc.height);
  if (result != CRTGFX_OK) return result;
  if (memcmp(desc.device_uuid, identity.device_uuid, sizeof desc.device_uuid)
      || memcmp(desc.driver_uuid, identity.driver_uuid, sizeof desc.driver_uuid)) return CRTGFX_ERROR_UNSUPPORTED;
  image->desc = desc;
  result = image->allocate_image(fds);
  if (result == CRTGFX_OK) result = image->allocate_sync(fds);
  if (result == CRTGFX_OK) *out = std::move(image);
  return result;
}
#endif
