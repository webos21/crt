/* Real Ganesh/Vulkan offscreen vertical slice (TODO.md's "Enable Skia GPU
 * rendering" step, 2026-09-03) -- the first real crtgfx_gpu_device backend
 * anywhere in this project. See crtgfx/gpu.h's own top comment and
 * libcrtgfx/README.md for the full design record, including the two real
 * pivots that led here (D3D11 -> D3D12 for Windows's own later step; then
 * Windows-first -> Linux-first, driven by hands-on WSL verification that a
 * real, hardware-backed Vulkan device is reachable here via Mesa's `dzn`
 * (Vulkan-over-D3D12) driver, confirmed via `vulkaninfo`).
 *
 * Hand-declares the minimal real Vulkan 1.0 core subset needed to create an
 * instance/device and pick a graphics-capable queue -- matching this
 * project's own consistent no-host-SDK-header policy (hand-declared D3D11
 * COM vtables in window_win32.c, hand-transcribed real ALSA UAPI structs,
 * a hand-rolled real PulseAudio wire protocol, the Objective-C runtime's
 * plain C ABI on macOS). Every type/struct/enum value below is transcribed
 * verbatim from the real, stable, spec-frozen Vulkan 1.0 core ABI
 * (cross-checked against Skia's own vendored include/third_party/vulkan/
 * vulkan/vulkan_core.h, 2026-09-03), not reinvented or guessed.
 *
 * Real linkage is a direct, real link-time dependency on the host's actual
 * libvulkan.so (CMakeLists.txt only compiles this file into crtgfx/
 * crtgfx_shared when a real libvulkan was found via find_library) --
 * deliberately NOT dlopen()/dlsym(): confirmed by reading libdl/src/arch/
 * linux/dl_linux.c directly that this project's own dlopen() does not
 * implement real ELF dynamic loading yet (a non-NULL filename
 * unconditionally reports "not implemented yet"), so the dlopen()-based
 * bootstrap this vertical slice's own plan first sketched is not actually
 * available today. Direct linking instead matches window_win32.c's own
 * D3D11CreateDevice extern-import precedent exactly: a real host function
 * resolved by the linker, not looked up by hand at runtime. VKAPI_ATTR/
 * VKAPI_CALL are both empty on this real target (Linux/x86_64's own SysV
 * ABI -- they only matter for Win32's __stdcall), so a plain `extern`
 * declaration is the real, correct ABI match; no vtable/COM machinery is
 * needed the way D3D11 required, since Vulkan's C ABI is flat. */

#include "gpu_internal.h"

#include <stddef.h>
#include <stdint.h>

typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;

typedef int32_t VkResult;
#define CRTGFX_VK_SUCCESS 0
#define CRTGFX_VK_INCOMPLETE 5

typedef uint32_t VkStructureType;
#define CRTGFX_VK_STRUCTURE_TYPE_APPLICATION_INFO 0u
#define CRTGFX_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1u
#define CRTGFX_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2u
#define CRTGFX_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3u

typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkFlags;
typedef VkFlags VkInstanceCreateFlags;
typedef VkFlags VkDeviceCreateFlags;
typedef VkFlags VkDeviceQueueCreateFlags;
typedef VkFlags VkQueueFlags;
typedef VkFlags VkSampleCountFlags;

#define CRTGFX_VK_QUEUE_GRAPHICS_BIT 0x00000001u

typedef uint32_t VkPhysicalDeviceType;
#define CRTGFX_VK_PHYSICAL_DEVICE_TYPE_CPU 4u

#define CRTGFX_VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256u
#define CRTGFX_VK_UUID_SIZE 16u

/* Real VK_MAKE_API_VERSION() formula (vulkan_core.h:62-63), transcribed
 * verbatim -- variant/major/minor/patch packed into one uint32_t. */
#define CRTGFX_VK_MAKE_API_VERSION(variant, major, minor, patch) \
  ((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | \
   (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))

typedef struct VkApplicationInfo {
  VkStructureType sType;
  const void* pNext;
  const char* pApplicationName;
  uint32_t applicationVersion;
  const char* pEngineName;
  uint32_t engineVersion;
  uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkInstanceCreateFlags flags;
  const VkApplicationInfo* pApplicationInfo;
  uint32_t enabledLayerCount;
  const char* const* ppEnabledLayerNames;
  uint32_t enabledExtensionCount;
  const char* const* ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkExtent3D {
  uint32_t width;
  uint32_t height;
  uint32_t depth;
} VkExtent3D;

typedef struct VkQueueFamilyProperties {
  VkQueueFlags queueFlags;
  uint32_t queueCount;
  uint32_t timestampValidBits;
  VkExtent3D minImageTransferGranularity;
} VkQueueFamilyProperties;

typedef struct VkDeviceQueueCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkDeviceQueueCreateFlags flags;
  uint32_t queueFamilyIndex;
  uint32_t queueCount;
  const float* pQueuePriorities;
} VkDeviceQueueCreateInfo;

/* Real, complete VkPhysicalDeviceFeatures (every field a plain VkBool32) --
 * transcribed verbatim only so VkDeviceCreateInfo::pEnabledFeatures can
 * point at a correctly-sized, all-zero (no optional feature requested)
 * struct; this vertical slice reads none of these fields back. */
typedef struct VkPhysicalDeviceFeatures {
  VkBool32 robustBufferAccess;
  VkBool32 fullDrawIndexUint32;
  VkBool32 imageCubeArray;
  VkBool32 independentBlend;
  VkBool32 geometryShader;
  VkBool32 tessellationShader;
  VkBool32 sampleRateShading;
  VkBool32 dualSrcBlend;
  VkBool32 logicOp;
  VkBool32 multiDrawIndirect;
  VkBool32 drawIndirectFirstInstance;
  VkBool32 depthClamp;
  VkBool32 depthBiasClamp;
  VkBool32 fillModeNonSolid;
  VkBool32 depthBounds;
  VkBool32 wideLines;
  VkBool32 largePoints;
  VkBool32 alphaToOne;
  VkBool32 multiViewport;
  VkBool32 samplerAnisotropy;
  VkBool32 textureCompressionETC2;
  VkBool32 textureCompressionASTC_LDR;
  VkBool32 textureCompressionBC;
  VkBool32 occlusionQueryPrecise;
  VkBool32 pipelineStatisticsQuery;
  VkBool32 vertexPipelineStoresAndAtomics;
  VkBool32 fragmentStoresAndAtomics;
  VkBool32 shaderTessellationAndGeometryPointSize;
  VkBool32 shaderImageGatherExtended;
  VkBool32 shaderStorageImageExtendedFormats;
  VkBool32 shaderStorageImageMultisample;
  VkBool32 shaderStorageImageReadWithoutFormat;
  VkBool32 shaderStorageImageWriteWithoutFormat;
  VkBool32 shaderUniformBufferArrayDynamicIndexing;
  VkBool32 shaderSampledImageArrayDynamicIndexing;
  VkBool32 shaderStorageBufferArrayDynamicIndexing;
  VkBool32 shaderStorageImageArrayDynamicIndexing;
  VkBool32 shaderClipDistance;
  VkBool32 shaderCullDistance;
  VkBool32 shaderFloat64;
  VkBool32 shaderInt64;
  VkBool32 shaderInt16;
  VkBool32 shaderResourceResidency;
  VkBool32 shaderResourceMinLod;
  VkBool32 sparseBinding;
  VkBool32 sparseResidencyBuffer;
  VkBool32 sparseResidencyImage2D;
  VkBool32 sparseResidencyImage3D;
  VkBool32 sparseResidency2Samples;
  VkBool32 sparseResidency4Samples;
  VkBool32 sparseResidency8Samples;
  VkBool32 sparseResidency16Samples;
  VkBool32 sparseResidencyAliased;
  VkBool32 variableMultisampleRate;
  VkBool32 inheritedQueries;
} VkPhysicalDeviceFeatures;

typedef struct VkDeviceCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkDeviceCreateFlags flags;
  uint32_t queueCreateInfoCount;
  const VkDeviceQueueCreateInfo* pQueueCreateInfos;
  uint32_t enabledLayerCount;
  const char* const* ppEnabledLayerNames;
  uint32_t enabledExtensionCount;
  const char* const* ppEnabledExtensionNames;
  const VkPhysicalDeviceFeatures* pEnabledFeatures;
} VkDeviceCreateInfo;

/* Real, complete VkPhysicalDeviceLimits/VkPhysicalDeviceSparseProperties,
 * transcribed verbatim so VkPhysicalDeviceProperties (below) has the real,
 * correct total size vkGetPhysicalDeviceProperties() actually writes --
 * this backend only ever reads ::deviceType out of the result, but the
 * struct passed to a real Vulkan call must be the real, full size or the
 * driver would write past a truncated one. */
typedef struct VkPhysicalDeviceLimits {
  uint32_t maxImageDimension1D;
  uint32_t maxImageDimension2D;
  uint32_t maxImageDimension3D;
  uint32_t maxImageDimensionCube;
  uint32_t maxImageArrayLayers;
  uint32_t maxTexelBufferElements;
  uint32_t maxUniformBufferRange;
  uint32_t maxStorageBufferRange;
  uint32_t maxPushConstantsSize;
  uint32_t maxMemoryAllocationCount;
  uint32_t maxSamplerAllocationCount;
  VkDeviceSize bufferImageGranularity;
  VkDeviceSize sparseAddressSpaceSize;
  uint32_t maxBoundDescriptorSets;
  uint32_t maxPerStageDescriptorSamplers;
  uint32_t maxPerStageDescriptorUniformBuffers;
  uint32_t maxPerStageDescriptorStorageBuffers;
  uint32_t maxPerStageDescriptorSampledImages;
  uint32_t maxPerStageDescriptorStorageImages;
  uint32_t maxPerStageDescriptorInputAttachments;
  uint32_t maxPerStageResources;
  uint32_t maxDescriptorSetSamplers;
  uint32_t maxDescriptorSetUniformBuffers;
  uint32_t maxDescriptorSetUniformBuffersDynamic;
  uint32_t maxDescriptorSetStorageBuffers;
  uint32_t maxDescriptorSetStorageBuffersDynamic;
  uint32_t maxDescriptorSetSampledImages;
  uint32_t maxDescriptorSetStorageImages;
  uint32_t maxDescriptorSetInputAttachments;
  uint32_t maxVertexInputAttributes;
  uint32_t maxVertexInputBindings;
  uint32_t maxVertexInputAttributeOffset;
  uint32_t maxVertexInputBindingStride;
  uint32_t maxVertexOutputComponents;
  uint32_t maxTessellationGenerationLevel;
  uint32_t maxTessellationPatchSize;
  uint32_t maxTessellationControlPerVertexInputComponents;
  uint32_t maxTessellationControlPerVertexOutputComponents;
  uint32_t maxTessellationControlPerPatchOutputComponents;
  uint32_t maxTessellationControlTotalOutputComponents;
  uint32_t maxTessellationEvaluationInputComponents;
  uint32_t maxTessellationEvaluationOutputComponents;
  uint32_t maxGeometryShaderInvocations;
  uint32_t maxGeometryInputComponents;
  uint32_t maxGeometryOutputComponents;
  uint32_t maxGeometryOutputVertices;
  uint32_t maxGeometryTotalOutputComponents;
  uint32_t maxFragmentInputComponents;
  uint32_t maxFragmentOutputAttachments;
  uint32_t maxFragmentDualSrcAttachments;
  uint32_t maxFragmentCombinedOutputResources;
  uint32_t maxComputeSharedMemorySize;
  uint32_t maxComputeWorkGroupCount[3];
  uint32_t maxComputeWorkGroupInvocations;
  uint32_t maxComputeWorkGroupSize[3];
  uint32_t subPixelPrecisionBits;
  uint32_t subTexelPrecisionBits;
  uint32_t mipmapPrecisionBits;
  uint32_t maxDrawIndexedIndexValue;
  uint32_t maxDrawIndirectCount;
  float maxSamplerLodBias;
  float maxSamplerAnisotropy;
  uint32_t maxViewports;
  uint32_t maxViewportDimensions[2];
  float viewportBoundsRange[2];
  uint32_t viewportSubPixelBits;
  size_t minMemoryMapAlignment;
  VkDeviceSize minTexelBufferOffsetAlignment;
  VkDeviceSize minUniformBufferOffsetAlignment;
  VkDeviceSize minStorageBufferOffsetAlignment;
  int32_t minTexelOffset;
  uint32_t maxTexelOffset;
  int32_t minTexelGatherOffset;
  uint32_t maxTexelGatherOffset;
  float minInterpolationOffset;
  float maxInterpolationOffset;
  uint32_t subPixelInterpolationOffsetBits;
  uint32_t maxFramebufferWidth;
  uint32_t maxFramebufferHeight;
  uint32_t maxFramebufferLayers;
  VkSampleCountFlags framebufferColorSampleCounts;
  VkSampleCountFlags framebufferDepthSampleCounts;
  VkSampleCountFlags framebufferStencilSampleCounts;
  VkSampleCountFlags framebufferNoAttachmentsSampleCounts;
  uint32_t maxColorAttachments;
  VkSampleCountFlags sampledImageColorSampleCounts;
  VkSampleCountFlags sampledImageIntegerSampleCounts;
  VkSampleCountFlags sampledImageDepthSampleCounts;
  VkSampleCountFlags sampledImageStencilSampleCounts;
  VkSampleCountFlags storageImageSampleCounts;
  uint32_t maxSampleMaskWords;
  VkBool32 timestampComputeAndGraphics;
  float timestampPeriod;
  uint32_t maxClipDistances;
  uint32_t maxCullDistances;
  uint32_t maxCombinedClipAndCullDistances;
  uint32_t discreteQueuePriorities;
  float pointSizeRange[2];
  float lineWidthRange[2];
  float pointSizeGranularity;
  float lineWidthGranularity;
  VkBool32 strictLines;
  VkBool32 standardSampleLocations;
  VkDeviceSize optimalBufferCopyOffsetAlignment;
  VkDeviceSize optimalBufferCopyRowPitchAlignment;
  VkDeviceSize nonCoherentAtomSize;
} VkPhysicalDeviceLimits;

typedef struct VkPhysicalDeviceSparseProperties {
  VkBool32 residencyStandard2DBlockShape;
  VkBool32 residencyStandard2DMultisampleBlockShape;
  VkBool32 residencyStandard3DBlockShape;
  VkBool32 residencyAlignedMipSize;
  VkBool32 residencyNonResidentStrict;
} VkPhysicalDeviceSparseProperties;

typedef struct VkPhysicalDeviceProperties {
  uint32_t apiVersion;
  uint32_t driverVersion;
  uint32_t vendorID;
  uint32_t deviceID;
  VkPhysicalDeviceType deviceType;
  char deviceName[CRTGFX_VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
  uint8_t pipelineCacheUUID[CRTGFX_VK_UUID_SIZE];
  VkPhysicalDeviceLimits limits;
  VkPhysicalDeviceSparseProperties sparseProperties;
} VkPhysicalDeviceProperties;

/* Real functions -- resolved at link time against the host's real
 * libvulkan.so (see this file's own top comment). pAllocator parameters
 * are declared `const void*` here (always passed NULL) rather than
 * `const VkAllocationCallbacks*`: a pointer parameter's ABI does not
 * depend on its pointee's declared type, and this file never allocates
 * through one, so VkAllocationCallbacks itself is never declared. */
extern VkResult vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo, const void* pAllocator, VkInstance* pInstance);
extern void vkDestroyInstance(VkInstance instance, const void* pAllocator);
extern VkResult vkEnumeratePhysicalDevices(
    VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
extern void vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
extern void vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);
extern void vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties);
extern VkResult vkCreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo,
    const void* pAllocator, VkDevice* pDevice);
extern void vkDestroyDevice(VkDevice device, const void* pAllocator);
extern void vkGetDeviceQueue(
    VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);

/* Real, fixed cap on enumerated physical devices -- generous for any real
 * host (a multi-GPU workstation rarely exceeds single digits); avoids a
 * dynamic allocation for what is, on every real host this project targets,
 * a small, bounded list. */
#define CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES 16u
#define CRTGFX_GPU_VULKAN_MAX_QUEUE_FAMILIES 32u

static crtgfx_result crtgfx_gpu_vulkan_create_instance(VkInstance* out_instance) {
  VkApplicationInfo app_info;
  VkInstanceCreateInfo create_info;
  VkResult result;

  app_info.sType = CRTGFX_VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = NULL;
  app_info.pApplicationName = "crtgfx";
  app_info.applicationVersion = CRTGFX_VK_MAKE_API_VERSION(0, 1, 0, 0);
  app_info.pEngineName = "crtgfx";
  app_info.engineVersion = CRTGFX_VK_MAKE_API_VERSION(0, 1, 0, 0);
  /* Real, confirmed-for-real requirement (2026-09-03): Skia's own Ganesh
   * Vulkan backend refuses anything below Vulkan 1.1 (src/gpu/vk/
   * VulkanUtilsPriv.cpp's own real, fatal check -- confirmed directly via
   * crtgfx_skia_gpu_offscreen_smoke's first real run: "Vulkan 1.1 is
   * required but not available" when this was still requesting 1.0, even
   * though both real devices on this host (dzn: 1.2, llvmpipe: 1.4)
   * genuinely support more). Requesting 1.0 here is what capped Skia's own
   * later GrDirectContexts::MakeVulkan() version check, not a real
   * driver limitation -- this project's own instance/device bootstrap
   * itself has never depended on anything past 1.0. */
  app_info.apiVersion = CRTGFX_VK_MAKE_API_VERSION(0, 1, 1, 0);

  create_info.sType = CRTGFX_VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pNext = NULL;
  create_info.flags = 0;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = NULL;
  create_info.enabledExtensionCount = 0;
  create_info.ppEnabledExtensionNames = NULL;

  result = vkCreateInstance(&create_info, NULL, out_instance);
  return (result == CRTGFX_VK_SUCCESS) ? CRTGFX_OK : CRTGFX_ERROR_UNSUPPORTED;
}

/* Enumerates every real physical device and reorders them so hardware-
 * backed devices (deviceType != CPU) come first -- so a host with both a
 * real GPU (even a translated one, e.g. Mesa's `dzn` over D3D12) and a
 * software rasterizer (llvmpipe) picks the real one at device_index 0 by
 * default, matching window_win32.c's own hardware-then-WARP fallback
 * preference. `device_index` (crtgfx_gpu_device_create()'s own public
 * argument) indexes into this curated order, not raw Vulkan enumeration
 * order. */
static void crtgfx_gpu_vulkan_enumerate_ordered(
    VkInstance instance, VkPhysicalDevice* out_devices, uint32_t* out_count) {
  VkPhysicalDevice raw[CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
  uint32_t raw_count = CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES;
  uint32_t ordered = 0;
  uint32_t i;
  VkResult result;

  *out_count = 0;
  result = vkEnumeratePhysicalDevices(instance, &raw_count, raw);
  if (result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_INCOMPLETE) {
    return;
  }

  for (i = 0; i < raw_count; ++i) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(raw[i], &props);
    if (props.deviceType != CRTGFX_VK_PHYSICAL_DEVICE_TYPE_CPU) {
      out_devices[ordered++] = raw[i];
    }
  }
  for (i = 0; i < raw_count; ++i) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(raw[i], &props);
    if (props.deviceType == CRTGFX_VK_PHYSICAL_DEVICE_TYPE_CPU) {
      out_devices[ordered++] = raw[i];
    }
  }
  *out_count = ordered;
}

static crtgfx_result crtgfx_gpu_vulkan_find_graphics_queue_family(
    VkPhysicalDevice physical_device, uint32_t* out_index) {
  VkQueueFamilyProperties families[CRTGFX_GPU_VULKAN_MAX_QUEUE_FAMILIES];
  uint32_t count = CRTGFX_GPU_VULKAN_MAX_QUEUE_FAMILIES;
  uint32_t i;

  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, families);
  for (i = 0; i < count; ++i) {
    if ((families[i].queueFlags & CRTGFX_VK_QUEUE_GRAPHICS_BIT) != 0) {
      *out_index = i;
      return CRTGFX_OK;
    }
  }
  return CRTGFX_ERROR_UNSUPPORTED;
}

crtgfx_result crtgfx_gpu_vulkan_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  VkInstance instance;
  VkPhysicalDevice devices[CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
  uint32_t count = 0;

  if (crtgfx_gpu_vulkan_create_instance(&instance) != CRTGFX_OK) {
    /* Real, honest report: a loader that failed to produce even an
     * instance means no usable Vulkan on this host right now, not a
     * caller error -- matches crtgfx_window_create()'s own "no usable
     * host backend right now" contract. */
    out_caps->backend = CRTGFX_GPU_BACKEND_NONE;
    out_caps->device_count = 0;
    return CRTGFX_OK;
  }
  crtgfx_gpu_vulkan_enumerate_ordered(instance, devices, &count);
  vkDestroyInstance(instance, NULL);

  out_caps->backend = (count > 0) ? CRTGFX_GPU_BACKEND_VULKAN : CRTGFX_GPU_BACKEND_NONE;
  out_caps->device_count = count;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_vulkan_device_create(uint32_t device_index, struct crtgfx_gpu_device* device) {
  VkInstance instance;
  VkPhysicalDevice devices[CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
  uint32_t count = 0;
  uint32_t queue_family_index;
  static const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info;
  VkPhysicalDeviceFeatures supported_features;
  VkDeviceCreateInfo device_create_info;
  VkDevice vk_device;
  VkQueue vk_queue;
  VkResult result;

  if (crtgfx_gpu_vulkan_create_instance(&instance) != CRTGFX_OK) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  crtgfx_gpu_vulkan_enumerate_ordered(instance, devices, &count);
  if (device_index >= count) {
    vkDestroyInstance(instance, NULL);
    /* device_index out of [0, device_count) -- crtgfx_gpu_query_
     * capabilities()'s own real, current report is the only valid source
     * for that range (gpu.h's own documented contract). */
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (crtgfx_gpu_vulkan_find_graphics_queue_family(devices[device_index], &queue_family_index) !=
      CRTGFX_OK) {
    vkDestroyInstance(instance, NULL);
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  queue_create_info.sType = CRTGFX_VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.pNext = NULL;
  queue_create_info.flags = 0;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  /* Real, all-supported features -- not an all-zero struct. Confirmed for
   * real (2026-09-03): creating the VkDevice with every feature disabled
   * made Skia's own GrDirectContexts::MakeVulkan() silently fail (no
   * crash, no diagnostic -- an official/release Skia build has its own
   * verbose capability-check logging compiled out) against an otherwise
   * completely valid instance/device/queue -- Ganesh's own real
   * capability probing expects at least the features this physical
   * device genuinely supports to actually be enabled, not just present.
   * Querying and passing them all through is also simply the more
   * correct, realistic thing to do regardless (a real embedder enables
   * what it plans to use; this vertical slice has no reason to withhold
   * any real, supported feature from a device it otherwise fully owns). */
  vkGetPhysicalDeviceFeatures(devices[device_index], &supported_features);

  device_create_info.sType = CRTGFX_VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = NULL;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = 0;
  device_create_info.ppEnabledLayerNames = NULL;
  device_create_info.enabledExtensionCount = 0;
  device_create_info.ppEnabledExtensionNames = NULL;
  device_create_info.pEnabledFeatures = &supported_features;

  result = vkCreateDevice(devices[device_index], &device_create_info, NULL, &vk_device);
  if (result != CRTGFX_VK_SUCCESS) {
    vkDestroyInstance(instance, NULL);
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  vkGetDeviceQueue(vk_device, queue_family_index, 0, &vk_queue);

  device->vk_instance = (void*)instance;
  device->vk_physical_device = (void*)devices[device_index];
  device->vk_device = (void*)vk_device;
  device->vk_queue = (void*)vk_queue;
  device->vk_queue_family_index = queue_family_index;
  return CRTGFX_OK;
}

void crtgfx_gpu_vulkan_device_destroy(struct crtgfx_gpu_device* device) {
  if (device->vk_device != NULL) {
    vkDestroyDevice((VkDevice)device->vk_device, NULL);
  }
  if (device->vk_instance != NULL) {
    vkDestroyInstance((VkInstance)device->vk_instance, NULL);
  }
}
