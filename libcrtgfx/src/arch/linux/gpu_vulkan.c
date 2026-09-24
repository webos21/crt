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
#include "gpu_vulkan_test.h"
#include "window_wayland_native.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static atomic_uint crtgfx_gpu_vulkan_test_device_failure_step;
static atomic_uint crtgfx_gpu_vulkan_test_device_cleanup_mask;

static int crtgfx_gpu_vulkan_test_should_fail_device_create(uint32_t step) {
  uint32_t expected = step;
  return atomic_compare_exchange_strong_explicit(
      &crtgfx_gpu_vulkan_test_device_failure_step, &expected, 0u,
      memory_order_acq_rel, memory_order_acquire);
}

void crtgfx_gpu_vulkan_test_inject_device_create_failure(uint32_t step) {
  atomic_store_explicit(&crtgfx_gpu_vulkan_test_device_cleanup_mask, 0u, memory_order_release);
  atomic_store_explicit(&crtgfx_gpu_vulkan_test_device_failure_step, step, memory_order_release);
}

uint32_t crtgfx_gpu_vulkan_test_take_device_cleanup_mask(void) {
  return atomic_exchange_explicit(
      &crtgfx_gpu_vulkan_test_device_cleanup_mask, 0u, memory_order_acq_rel);
}

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

/* ---- Real Vulkan WSI (VK_KHR_surface/VK_KHR_wayland_surface/
 * VK_KHR_swapchain) additions, 2026-09-07 -- the Linux native-Wayland-
 * backend vertical slice's own real presentation path. Same hand-
 * declaration discipline as everything above: every type/struct/enum
 * value below was cross-checked verbatim against this repo's own real,
 * vendored Skia Vulkan header (under out/.../crtgfx-skia-smoke/external/
 * skia/src/include/third_party/vulkan/vulkan/vulkan_core.h -- the actual upstream
 * Khronos header, not memory), EXCEPT VkWaylandSurfaceCreateInfoKHR and
 * vkCreateWaylandSurfaceKHR/vkGetPhysicalDeviceWaylandPresentationSupportKHR
 * themselves: Skia's own vendored copy is core-only (it never creates a
 * real window/surface itself, so it has no reason to vendor any platform-
 * specific vulkan_wayland.h), so this one piece is reasoned from the
 * spec's own long-stable, unchanged-since-introduction, publicly
 * documented shape and this project's own already-confirmed-correct
 * VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR value (1000006000,
 * genuinely verified against the vendored core header, which does list
 * platform-agnostic struct-type enumerants) -- flagged here, matching
 * this project's own "reasoned but flagged unverified" discipline for
 * anything not independently confirmed against a real local source,
 * rather than silently assumed. */
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkSwapchainKHR_T* VkSwapchainKHR;
typedef struct VkImage_T* VkImage;
typedef struct VkSemaphore_T* VkSemaphore;
typedef struct VkFence_T* VkFence;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkCommandBuffer_T* VkCommandBuffer;

typedef uint32_t VkFormat;
#define CRTGFX_VK_FORMAT_B8G8R8A8_UNORM 44u
#define CRTGFX_VK_FORMAT_B8G8R8A8_SRGB 50u

typedef uint32_t VkColorSpaceKHR;
#define CRTGFX_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 0u

typedef uint32_t VkPresentModeKHR;
#define CRTGFX_VK_PRESENT_MODE_FIFO_KHR 2u

typedef VkFlags VkSurfaceTransformFlagsKHR;
typedef uint32_t VkSurfaceTransformFlagBitsKHR;
#define CRTGFX_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR 0x00000001u

typedef VkFlags VkCompositeAlphaFlagsKHR;
typedef uint32_t VkCompositeAlphaFlagBitsKHR;
#define CRTGFX_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 0x00000001u

typedef VkFlags VkImageUsageFlags;
#define CRTGFX_VK_IMAGE_USAGE_TRANSFER_SRC_BIT 0x00000001u
#define CRTGFX_VK_IMAGE_USAGE_TRANSFER_DST_BIT 0x00000002u
#define CRTGFX_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 0x00000010u

typedef uint32_t VkSharingMode;
#define CRTGFX_VK_SHARING_MODE_EXCLUSIVE 0u

typedef uint32_t VkImageLayout;
#define CRTGFX_VK_IMAGE_LAYOUT_UNDEFINED 0u
#define CRTGFX_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 7u
#define CRTGFX_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 1000001002u

typedef VkFlags VkImageAspectFlags;
#define CRTGFX_VK_IMAGE_ASPECT_COLOR_BIT 0x00000001u

typedef VkFlags VkAccessFlags;
#define CRTGFX_VK_ACCESS_TRANSFER_WRITE_BIT 0x00001000u
#define CRTGFX_VK_ACCESS_MEMORY_READ_BIT 0x00008000u

typedef VkFlags VkPipelineStageFlags;
#define CRTGFX_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT 0x00000001u
#define CRTGFX_VK_PIPELINE_STAGE_TRANSFER_BIT 0x00001000u
#define CRTGFX_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT 0x00002000u
#define CRTGFX_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 0x00000400u

typedef VkFlags VkDependencyFlags;
typedef VkFlags VkFenceCreateFlags;
#define CRTGFX_VK_FENCE_CREATE_SIGNALED_BIT 0x00000001u
typedef VkFlags VkSemaphoreCreateFlags;
typedef VkFlags VkCommandPoolCreateFlags;
#define CRTGFX_VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002u
typedef VkFlags VkCommandBufferUsageFlags;
#define CRTGFX_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001u
typedef uint32_t VkCommandBufferLevel;
#define CRTGFX_VK_COMMAND_BUFFER_LEVEL_PRIMARY 0u

#define CRTGFX_VK_TIMEOUT 2
#define CRTGFX_VK_SUBOPTIMAL_KHR 1000001003
#define CRTGFX_VK_ERROR_OUT_OF_DATE_KHR (-1000001004)

#define CRTGFX_VK_QUEUE_FAMILY_IGNORED (~0u)
#define CRTGFX_VK_REMAINING_MIP_LEVELS (~0u)
#define CRTGFX_VK_REMAINING_ARRAY_LAYERS (~0u)
#define CRTGFX_VK_MAX_EXTENSION_NAME_SIZE 256u

#define CRTGFX_VK_STRUCTURE_TYPE_SUBMIT_INFO 4u
#define CRTGFX_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 8u
#define CRTGFX_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 9u
#define CRTGFX_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39u
#define CRTGFX_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40u
#define CRTGFX_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 42u
#define CRTGFX_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER 45u
#define CRTGFX_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR 1000001000u
#define CRTGFX_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR 1000001001u
#define CRTGFX_VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR 1000006000u

typedef struct VkExtent2D {
  uint32_t width;
  uint32_t height;
} VkExtent2D;

typedef struct VkExtensionProperties {
  char extensionName[CRTGFX_VK_MAX_EXTENSION_NAME_SIZE];
  uint32_t specVersion;
} VkExtensionProperties;

typedef struct VkSurfaceCapabilitiesKHR {
  uint32_t minImageCount;
  uint32_t maxImageCount;
  VkExtent2D currentExtent;
  VkExtent2D minImageExtent;
  VkExtent2D maxImageExtent;
  uint32_t maxImageArrayLayers;
  VkSurfaceTransformFlagsKHR supportedTransforms;
  VkSurfaceTransformFlagBitsKHR currentTransform;
  VkCompositeAlphaFlagsKHR supportedCompositeAlpha;
  VkImageUsageFlags supportedUsageFlags;
} VkSurfaceCapabilitiesKHR;

typedef struct VkSurfaceFormatKHR {
  VkFormat format;
  VkColorSpaceKHR colorSpace;
} VkSurfaceFormatKHR;

typedef struct VkSwapchainCreateInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkFlags flags;
  VkSurfaceKHR surface;
  uint32_t minImageCount;
  VkFormat imageFormat;
  VkColorSpaceKHR imageColorSpace;
  VkExtent2D imageExtent;
  uint32_t imageArrayLayers;
  VkImageUsageFlags imageUsage;
  VkSharingMode imageSharingMode;
  uint32_t queueFamilyIndexCount;
  const uint32_t* pQueueFamilyIndices;
  VkSurfaceTransformFlagBitsKHR preTransform;
  VkCompositeAlphaFlagBitsKHR compositeAlpha;
  VkPresentModeKHR presentMode;
  VkBool32 clipped;
  VkSwapchainKHR oldSwapchain;
} VkSwapchainCreateInfoKHR;

typedef struct VkPresentInfoKHR {
  VkStructureType sType;
  const void* pNext;
  uint32_t waitSemaphoreCount;
  const VkSemaphore* pWaitSemaphores;
  uint32_t swapchainCount;
  const VkSwapchainKHR* pSwapchains;
  const uint32_t* pImageIndices;
  VkResult* pResults;
} VkPresentInfoKHR;

/* Reasoned-but-not-locally-verified -- see this block's own top comment. */
typedef struct VkWaylandSurfaceCreateInfoKHR {
  VkStructureType sType;
  const void* pNext;
  VkFlags flags;
  void* display; /* struct wl_display* */
  void* surface; /* struct wl_surface* */
} VkWaylandSurfaceCreateInfoKHR;

typedef struct VkSemaphoreCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkSemaphoreCreateFlags flags;
} VkSemaphoreCreateInfo;

typedef struct VkFenceCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkFenceCreateFlags flags;
} VkFenceCreateInfo;

typedef struct VkCommandPoolCreateInfo {
  VkStructureType sType;
  const void* pNext;
  VkCommandPoolCreateFlags flags;
  uint32_t queueFamilyIndex;
} VkCommandPoolCreateInfo;

typedef struct VkCommandBufferAllocateInfo {
  VkStructureType sType;
  const void* pNext;
  VkCommandPool commandPool;
  VkCommandBufferLevel level;
  uint32_t commandBufferCount;
} VkCommandBufferAllocateInfo;

typedef struct VkCommandBufferBeginInfo {
  VkStructureType sType;
  const void* pNext;
  VkCommandBufferUsageFlags flags;
  const void* pInheritanceInfo;
} VkCommandBufferBeginInfo;

typedef struct VkImageSubresourceRange {
  VkImageAspectFlags aspectMask;
  uint32_t baseMipLevel;
  uint32_t levelCount;
  uint32_t baseArrayLayer;
  uint32_t layerCount;
} VkImageSubresourceRange;

typedef struct VkImageMemoryBarrier {
  VkStructureType sType;
  const void* pNext;
  VkAccessFlags srcAccessMask;
  VkAccessFlags dstAccessMask;
  VkImageLayout oldLayout;
  VkImageLayout newLayout;
  uint32_t srcQueueFamilyIndex;
  uint32_t dstQueueFamilyIndex;
  VkImage image;
  VkImageSubresourceRange subresourceRange;
} VkImageMemoryBarrier;

/* Real type is a union (float32[4]/int32[4]/uint32[4]) -- this backend
 * only ever writes the float32 interpretation, so a plain struct with
 * just that one (correctly-sized, 16-byte) member is ABI-identical for
 * every real use here. */
typedef struct VkClearColorValue {
  float float32[4];
} VkClearColorValue;

typedef struct VkSubmitInfo {
  VkStructureType sType;
  const void* pNext;
  uint32_t waitSemaphoreCount;
  const VkSemaphore* pWaitSemaphores;
  const VkPipelineStageFlags* pWaitDstStageMask;
  uint32_t commandBufferCount;
  const VkCommandBuffer* pCommandBuffers;
  uint32_t signalSemaphoreCount;
  const VkSemaphore* pSignalSemaphores;
} VkSubmitInfo;

extern VkResult vkEnumerateInstanceExtensionProperties(
    const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties);
extern VkResult vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties);
extern VkResult vkCreateWaylandSurfaceKHR(
    VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const void* pAllocator,
    VkSurfaceKHR* pSurface);
extern void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const void* pAllocator);
extern VkResult vkGetPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported);
extern VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities);
extern VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount,
    VkSurfaceFormatKHR* pSurfaceFormats);
extern VkResult vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const void* pAllocator,
    VkSwapchainKHR* pSwapchain);
extern void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const void* pAllocator);
extern VkResult vkGetSwapchainImagesKHR(
    VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages);
extern VkResult vkAcquireNextImageKHR(
    VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence,
    uint32_t* pImageIndex);
extern VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
extern VkResult vkCreateSemaphore(
    VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const void* pAllocator, VkSemaphore* pSemaphore);
extern void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const void* pAllocator);
extern VkResult vkCreateFence(
    VkDevice device, const VkFenceCreateInfo* pCreateInfo, const void* pAllocator, VkFence* pFence);
extern void vkDestroyFence(VkDevice device, VkFence fence, const void* pAllocator);
extern VkResult vkWaitForFences(
    VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout);
extern VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences);
extern VkResult vkCreateCommandPool(
    VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const void* pAllocator,
    VkCommandPool* pCommandPool);
extern void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const void* pAllocator);
extern VkResult vkAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers);
extern VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo);
extern VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer);
extern void vkCmdClearColorImage(
    VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor,
    uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
extern void vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const void* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const void* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers);
extern VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
extern VkResult vkDeviceWaitIdle(VkDevice device);

#define CRTGFX_GPU_VULKAN_MAX_EXTENSION_PROPERTIES 512u

/* The instance extensions crtgfx_gpu_vulkan_create_instance() actually
 * enabled, kept in file-static storage (string literals) so
 * crtgfx_gpu_vulkan_borrow_device() can hand the same list to Skia's
 * skgpu::VulkanExtensions (Skia gates DRM-format-modifier textures on it --
 * "Zero-copy decoded textures" Tranche 3). Every instance this file creates
 * enables the same set, so one shared copy is correct. */
static const char* crtgfx_gpu_vulkan_instance_extension_names[2];
static uint32_t crtgfx_gpu_vulkan_instance_extension_count = 0;

/* Device extensions for importing a VA-API-exported dma-buf as a VkImage
 * (docs/crtmedia_zero_copy_decode_acceptance.md, Linux row). Enabled
 * all-or-nothing, only when the physical device supports every one -- a
 * device without them creates exactly the device it always did and
 * borrow_device() reports dmabuf_import = 0 (the bridge then declines and the
 * caller keeps the honest CPU path). */
#define CRTGFX_GPU_VULKAN_MAX_DEVICE_EXTENSIONS 8u
static const char* const crtgfx_gpu_vulkan_dmabuf_extension_names[] = {
    "VK_KHR_external_memory_fd",
    "VK_EXT_external_memory_dma_buf",
    "VK_EXT_image_drm_format_modifier",
    "VK_KHR_image_format_list",
    "VK_EXT_queue_family_foreign",
};
#define CRTGFX_GPU_VULKAN_DMABUF_EXTENSION_COUNT \
  (sizeof(crtgfx_gpu_vulkan_dmabuf_extension_names) / sizeof(crtgfx_gpu_vulkan_dmabuf_extension_names[0]))

static int crtgfx_gpu_vulkan_instance_has_extension(const char* name) {
  static VkExtensionProperties props[CRTGFX_GPU_VULKAN_MAX_EXTENSION_PROPERTIES];
  uint32_t count = CRTGFX_GPU_VULKAN_MAX_EXTENSION_PROPERTIES;
  uint32_t i;
  VkResult result = vkEnumerateInstanceExtensionProperties(NULL, &count, props);
  if (result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_INCOMPLETE) {
    return 0;
  }
  for (i = 0; i < count; ++i) {
    if (strcmp(props[i].extensionName, name) == 0) {
      return 1;
    }
  }
  return 0;
}

static int crtgfx_gpu_vulkan_device_has_extension(VkPhysicalDevice physical_device, const char* name) {
  static VkExtensionProperties props[CRTGFX_GPU_VULKAN_MAX_EXTENSION_PROPERTIES];
  uint32_t count = CRTGFX_GPU_VULKAN_MAX_EXTENSION_PROPERTIES;
  uint32_t i;
  VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, props);
  if (result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_INCOMPLETE) {
    return 0;
  }
  for (i = 0; i < count; ++i) {
    if (strcmp(props[i].extensionName, name) == 0) {
      return 1;
    }
  }
  return 0;
}

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

  /* Real WSI instance extensions (2026-09-07, the native-Wayland-backend
   * vertical slice) -- probed, not forced: enabling them unconditionally
   * here (rather than only when a caller specifically asks for
   * presentation) is deliberate, since crtgfx_gpu_surface_create()'s own
   * documented intent (crtgfx/gpu.h's own top comment) is to become the
   * *sole* path a window acquires a device through, so any device this
   * function's own caller (crtgfx_gpu_vulkan_device_create(), below) ends
   * up creating should already be presentation-capable if the host can
   * support it at all. Probing first (crtgfx_gpu_vulkan_instance_has_
   * extension()) rather than just listing both names unconditionally is
   * what keeps this a real, zero-regression-risk addition: a host with no
   * WSI support at all (a headless CI runner, or any real ICD that never
   * implements presentation) gets exactly the same zero-extension instance
   * this function always created before today, so the already-verified
   * offscreen Ganesh vertical slice (crtgfx_skia_gpu_offscreen_smoke)
   * keeps working completely unchanged there. */
  {
    uint32_t wsi_extension_count = 0;
    if (crtgfx_gpu_vulkan_instance_has_extension("VK_KHR_surface") &&
        crtgfx_gpu_vulkan_instance_has_extension("VK_KHR_wayland_surface")) {
      crtgfx_gpu_vulkan_instance_extension_names[wsi_extension_count++] = "VK_KHR_surface";
      crtgfx_gpu_vulkan_instance_extension_names[wsi_extension_count++] = "VK_KHR_wayland_surface";
    }
    crtgfx_gpu_vulkan_instance_extension_count = wsi_extension_count;
    create_info.enabledExtensionCount = wsi_extension_count;
    create_info.ppEnabledExtensionNames =
        (wsi_extension_count > 0) ? crtgfx_gpu_vulkan_instance_extension_names : NULL;
  }

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

struct crtgfx_gpu_vulkan_device_state {
  void* vk_instance;
  void* vk_physical_device;
  void* vk_device;
  void* vk_queue;
  uint32_t vk_queue_family_index;
  /* Enabled device extensions (string literals) and whether the dma-buf
   * import set was among them -- see CRTGFX_GPU_VULKAN_MAX_DEVICE_EXTENSIONS. */
  const char* vk_device_extension_names[CRTGFX_GPU_VULKAN_MAX_DEVICE_EXTENSIONS];
  uint32_t vk_device_extension_count;
  int vk_dmabuf_import_enabled;
};

struct crtgfx_gpu_vulkan_surface_state {
  struct crtgfx_gpu_vulkan_device_state* device;
  void* vk_surface;
  void* vk_swapchain;
  void** vk_images;
  uint32_t vk_image_count;
  uint32_t vk_format;
  uint32_t vk_image_usage_flags;
  uint32_t width;
  uint32_t height;
  void* vk_image_available_semaphore;
  void* vk_render_finished_semaphore;
  void* vk_command_pool;
  void* vk_command_buffer;
  void* vk_frame_fence;
  uint32_t vk_current_image_index;
  int vk_image_acquired;
  int ganesh_wrapped;
};

static crtgfx_result crtgfx_gpu_vulkan_device_state_create(
    uint32_t device_index, struct crtgfx_gpu_vulkan_device_state* device) {
  VkInstance instance;
  VkPhysicalDevice devices[CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES];
  uint32_t count = 0;
  uint32_t queue_family_index;
  static const float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info;
  VkPhysicalDeviceFeatures supported_features;
  const char* dev_ext_names[CRTGFX_GPU_VULKAN_MAX_DEVICE_EXTENSIONS] = {0};
  uint32_t dev_ext_count = 0;
  int dmabuf_import = 0;
  VkDeviceCreateInfo device_create_info;
  VkDevice vk_device;
  VkQueue vk_queue;
  VkResult result;

  if (crtgfx_gpu_vulkan_create_instance(&instance) != CRTGFX_OK) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  if (crtgfx_gpu_vulkan_test_should_fail_device_create(
          CRTGFX_GPU_VULKAN_TEST_FAIL_AFTER_INSTANCE)) {
    vkDestroyInstance(instance, NULL);
    atomic_fetch_or_explicit(
        &crtgfx_gpu_vulkan_test_device_cleanup_mask,
        CRTGFX_GPU_VULKAN_TEST_CLEANED_INSTANCE, memory_order_release);
    return CRTGFX_ERROR_HOST;
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
  /* VK_KHR_swapchain (2026-09-07) -- same real probe-first, zero-
   * regression-risk discipline as the instance's own VK_KHR_surface/
   * VK_KHR_wayland_surface addition above (crtgfx_gpu_vulkan_create_
   * instance()'s own comment): a device on a host with no real swapchain
   * support (or that never got a real VK_KHR_wayland_surface-capable
   * instance in the first place) gets exactly the same zero-extension
   * device this function always created before today. A future crtgfx_
   * gpu_surface_create() call against a device that did not end up with
   * this extension enabled fails at vkCreateSwapchainKHR() itself with a
   * real, honest error (mapped to CRTGFX_ERROR_UNSUPPORTED) -- no separate
   * "is this device presentation-capable" flag needs tracking here. */
  {
    uint32_t i;
    dev_ext_count = 0;
    if (crtgfx_gpu_vulkan_device_has_extension(devices[device_index], "VK_KHR_swapchain")) {
      dev_ext_names[dev_ext_count++] = "VK_KHR_swapchain";
    }
    dmabuf_import = 1;
    for (i = 0; i < CRTGFX_GPU_VULKAN_DMABUF_EXTENSION_COUNT; ++i) {
      if (!crtgfx_gpu_vulkan_device_has_extension(
              devices[device_index], crtgfx_gpu_vulkan_dmabuf_extension_names[i])) {
        dmabuf_import = 0;
        break;
      }
    }
    if (dmabuf_import) {
      for (i = 0; i < CRTGFX_GPU_VULKAN_DMABUF_EXTENSION_COUNT; ++i) {
        dev_ext_names[dev_ext_count++] = crtgfx_gpu_vulkan_dmabuf_extension_names[i];
      }
    }
    device_create_info.enabledExtensionCount = dev_ext_count;
    device_create_info.ppEnabledExtensionNames = (dev_ext_count > 0) ? dev_ext_names : NULL;
  }
  device_create_info.pEnabledFeatures = &supported_features;

  result = vkCreateDevice(devices[device_index], &device_create_info, NULL, &vk_device);
  if (result != CRTGFX_VK_SUCCESS) {
    vkDestroyInstance(instance, NULL);
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  vkGetDeviceQueue(vk_device, queue_family_index, 0, &vk_queue);
  if (crtgfx_gpu_vulkan_test_should_fail_device_create(
          CRTGFX_GPU_VULKAN_TEST_FAIL_AFTER_DEVICE)) {
    vkDestroyDevice(vk_device, NULL);
    atomic_fetch_or_explicit(
        &crtgfx_gpu_vulkan_test_device_cleanup_mask,
        CRTGFX_GPU_VULKAN_TEST_CLEANED_DEVICE, memory_order_release);
    vkDestroyInstance(instance, NULL);
    atomic_fetch_or_explicit(
        &crtgfx_gpu_vulkan_test_device_cleanup_mask,
        CRTGFX_GPU_VULKAN_TEST_CLEANED_INSTANCE, memory_order_release);
    return CRTGFX_ERROR_HOST;
  }

  device->vk_instance = (void*)instance;
  device->vk_physical_device = (void*)devices[device_index];
  device->vk_device = (void*)vk_device;
  device->vk_queue = (void*)vk_queue;
  device->vk_queue_family_index = queue_family_index;
  memcpy(device->vk_device_extension_names, dev_ext_names, sizeof(dev_ext_names));
  device->vk_device_extension_count = dev_ext_count;
  device->vk_dmabuf_import_enabled = dmabuf_import;
  return CRTGFX_OK;
}

static void crtgfx_gpu_vulkan_device_state_destroy(struct crtgfx_gpu_vulkan_device_state* device) {
  if (device->vk_device != NULL) {
    vkDestroyDevice((VkDevice)device->vk_device, NULL);
  }
  if (device->vk_instance != NULL) {
    vkDestroyInstance((VkInstance)device->vk_instance, NULL);
  }
}

/* Real, fixed cap on swapchain images -- every real Vulkan implementation
 * this project targets returns a small handful (2-4 is typical; the real
 * spec itself has no upper bound, but this project's own real device pool
 * follows the same "small, bounded list, no dynamic allocation" precedent
 * CRTGFX_GPU_VULKAN_MAX_PHYSICAL_DEVICES/_MAX_QUEUE_FAMILIES already set
 * above). If a real driver ever legitimately wants more, minImageCount is
 * clamped down to this cap below rather than overflowing a fixed array. */
#define CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES 8u

#if defined(CRTGFX_HAVE_NATIVE_WAYLAND)
/* Real swapchain/surface vertical slice (2026-09-07). `wl_display`/
 * `wl_surface` are already-resolved, real, live handles from this backend's
 * public-window adapter below. This function's job is the real Vulkan side:
 * VkSurfaceKHR, a real presentation-queue-support
 * check, a real VkSwapchainKHR sized to the surface's own current extent,
 * its real images, and the per-frame sync objects/command buffer crtgfx_
 * gpu_vulkan_surface_acquire()/_clear()/_present() (below) drive. Single,
 * linear `goto fail` teardown (matching src/arch/linux/window_wayland.c's
 * own crtgfx_wl_window_attach()'s established style for this same kind of
 * multi-step, any-step-can-fail real resource bring-up) -- every handle is
 * zero-initialized up front so the fail: block can safely destroy exactly
 * what was actually created, in reverse order, regardless of which step
 * failed. */
static crtgfx_result crtgfx_gpu_vulkan_surface_state_create(
    struct crtgfx_gpu_vulkan_device_state* device, void* wl_display, void* wl_surface,
    uint32_t width, uint32_t height, struct crtgfx_gpu_vulkan_surface_state* surface) {
  VkInstance instance = (VkInstance)device->vk_instance;
  VkPhysicalDevice physical_device = (VkPhysicalDevice)device->vk_physical_device;
  VkDevice vk_device = (VkDevice)device->vk_device;
  VkWaylandSurfaceCreateInfoKHR wl_surface_info;
  VkSurfaceCapabilitiesKHR caps;
  VkSurfaceFormatKHR formats[64];
  uint32_t format_count = 64u;
  VkFormat chosen_format = CRTGFX_VK_FORMAT_B8G8R8A8_UNORM;
  VkColorSpaceKHR chosen_color_space = CRTGFX_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  uint32_t image_count;
  VkExtent2D extent;
  VkSwapchainCreateInfoKHR swapchain_info;
  VkImage raw_images[CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES];
  uint32_t real_image_count = CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES;
  VkSemaphoreCreateInfo semaphore_info;
  VkFenceCreateInfo fence_info;
  VkCommandPoolCreateInfo pool_info;
  VkCommandBufferAllocateInfo cmd_alloc_info;
  VkBool32 present_supported = 0;
  VkResult result;
  uint32_t i;

  VkSurfaceKHR vk_surface = NULL;
  VkSwapchainKHR vk_swapchain = NULL;
  VkSemaphore image_available_sem = NULL;
  VkSemaphore render_finished_sem = NULL;
  VkFence frame_fence = NULL;
  VkCommandPool command_pool = NULL;
  VkCommandBuffer command_buffer = NULL;
  void** image_array = NULL;

  wl_surface_info.sType = CRTGFX_VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  wl_surface_info.pNext = NULL;
  wl_surface_info.flags = 0;
  wl_surface_info.display = wl_display;
  wl_surface_info.surface = wl_surface;
  if (vkCreateWaylandSurfaceKHR(instance, &wl_surface_info, NULL, &vk_surface) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  result = vkGetPhysicalDeviceSurfaceSupportKHR(
      physical_device, device->vk_queue_family_index, vk_surface, &present_supported);
  if (result != CRTGFX_VK_SUCCESS || !present_supported) {
    /* Real, honest "this device's own graphics queue family cannot
     * present to this surface" outcome -- not expected on any real
     * desktop Linux driver (they overwhelmingly support presentation on
     * every graphics-capable queue family), but a genuine, spec-legal
     * possibility this function must not silently assume away. */
    goto fail;
  }

  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vk_surface, &caps) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vk_surface, &format_count, formats);
  if ((result == CRTGFX_VK_SUCCESS || result == CRTGFX_VK_INCOMPLETE) && format_count > 0u) {
    chosen_format = formats[0].format;
    chosen_color_space = formats[0].colorSpace;
    for (i = 0; i < format_count; ++i) {
      /* Prefer real, non-sRGB-encoded BGRA8 -- matches crtgfx/window.h's
       * own CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED software-frame
       * contract's own "plain sRGB-range integers with no color
       * management applied anywhere in this pipeline" convention, so a
       * future Ganesh-onto-swapchain pass (this vertical slice's own
       * explicitly deferred next step) has the least surprising format to
       * target first. */
      if (formats[i].format == CRTGFX_VK_FORMAT_B8G8R8A8_UNORM) {
        chosen_format = formats[i].format;
        chosen_color_space = formats[i].colorSpace;
        break;
      }
    }
  }

  image_count = caps.minImageCount + 1u;
  if (caps.maxImageCount > 0u && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }
  if (image_count > CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES) {
    image_count = CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES;
  }

  /* Real, spec-documented convention: currentExtent.width == 0xFFFFFFFF
   * means the surface has no fixed size of its own and defers entirely to
   * whatever extent this call requests (clamped to min/maxImageExtent);
   * any other value means the compositor already knows this window's own
   * real, current size and the swapchain must match it exactly. */
  if (caps.currentExtent.width != 0xFFFFFFFFu) {
    extent = caps.currentExtent;
  } else {
    extent.width = width;
    if (extent.width < caps.minImageExtent.width) {
      extent.width = caps.minImageExtent.width;
    }
    if (extent.width > caps.maxImageExtent.width) {
      extent.width = caps.maxImageExtent.width;
    }
    extent.height = height;
    if (extent.height < caps.minImageExtent.height) {
      extent.height = caps.minImageExtent.height;
    }
    if (extent.height > caps.maxImageExtent.height) {
      extent.height = caps.maxImageExtent.height;
    }
  }
  if (extent.width == 0u || extent.height == 0u) {
    goto fail;
  }

  swapchain_info.sType = CRTGFX_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = NULL;
  swapchain_info.flags = 0;
  swapchain_info.surface = vk_surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = chosen_format;
  swapchain_info.imageColorSpace = chosen_color_space;
  swapchain_info.imageExtent = extent;
  swapchain_info.imageArrayLayers = 1;
  /* A tenth real, confirmed bug (2026-09-13), found chasing crtgfx_skia_
   * wrap_gpu_surface() (skia_bridge.cc) failing at GrVkGpu::onWrapBackend
   * RenderTarget()'s own check_image_info() -- confirmed with a one-off
   * debug fprintf patched directly into a throwaway extracted copy of
   * Skia's own vendored src/gpu/ganesh/vk/GrVkGpu.cc: Ganesh unconditionally
   * requires BOTH VK_IMAGE_USAGE_TRANSFER_SRC_BIT and _DST_BIT set on any
   * VkImage it wraps as a render target (that file's own check_image_info(),
   * "We currently require everything to be made with transfer bits set"),
   * but this swapchain was only ever created with _DST_BIT -- missing
   * _SRC_BIT unconditionally failed every real Ganesh-Vulkan wrap of a live
   * swapchain image, even though the plain (non-Ganesh) crtgfx_gpu_surface_
   * clear()/_present() path (this same file) never needed it and kept
   * working throughout this whole investigation. Added conditionally, not
   * unconditionally: VkSurfaceCapabilitiesKHR::supportedUsageFlags is the
   * real, spec-documented source of truth for which VK_IMAGE_USAGE_* bits a
   * given surface's own presentable images can actually be created with --
   * every real desktop Linux Vulkan/WSI implementation this project targets
   * advertises TRANSFER_SRC_BIT support (confirmed for real: llvmpipe on
   * this host does), but a future/unusual host that does not would
   * otherwise fail vkCreateSwapchainKHR() outright with VK_ERROR_
   * INITIALIZATION_FAILED for requesting an unsupported usage bit -- the
   * same probe-first, zero-regression-risk discipline as this file's own
   * VK_KHR_wayland_surface/VK_KHR_swapchain extension probing
   * (crtgfx_gpu_vulkan_create_instance()/_device_create()). A host without
   * TRANSFER_SRC_BIT support here also cannot support crtgfx_skia_wrap_
   * gpu_surface() at all -- skia_bridge.cc's own image_info.fImageUsageFlags
   * must exactly match this swapchain's own real, created usage flags (a
   * mismatch there is exactly this same bug moved one layer over), so it
   * mirrors this same conditional. */
  swapchain_info.imageUsage = CRTGFX_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | CRTGFX_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((caps.supportedUsageFlags & CRTGFX_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u) {
    swapchain_info.imageUsage |= CRTGFX_VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  swapchain_info.imageSharingMode = CRTGFX_VK_SHARING_MODE_EXCLUSIVE;
  swapchain_info.queueFamilyIndexCount = 0;
  swapchain_info.pQueueFamilyIndices = NULL;
  swapchain_info.preTransform = ((caps.supportedTransforms & CRTGFX_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0u)
                                     ? CRTGFX_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                                     : caps.currentTransform;
  swapchain_info.compositeAlpha = CRTGFX_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  /* FIFO: the one real present mode the spec guarantees every conformant
   * implementation supports unconditionally -- no vkGetPhysicalDeviceSurf
   * acePresentModesKHR() probe needed to safely request it. Also the
   * closest real analogue to "vsync on", a reasonable, uncontroversial
   * default for a first vertical slice; a later pass can offer MAILBOX/
   * IMMEDIATE as a real, opt-in choice once something actually needs one. */
  swapchain_info.presentMode = CRTGFX_VK_PRESENT_MODE_FIFO_KHR;
  swapchain_info.clipped = 1;
  swapchain_info.oldSwapchain = NULL;

  if (vkCreateSwapchainKHR(vk_device, &swapchain_info, NULL, &vk_swapchain) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  result = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &real_image_count, raw_images);
  if ((result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_INCOMPLETE) || real_image_count == 0u) {
    goto fail;
  }

  semaphore_info.sType = CRTGFX_VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = NULL;
  semaphore_info.flags = 0;
  if (vkCreateSemaphore(vk_device, &semaphore_info, NULL, &image_available_sem) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }
  if (vkCreateSemaphore(vk_device, &semaphore_info, NULL, &render_finished_sem) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  fence_info.sType = CRTGFX_VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = NULL;
  /* Real, deliberate: created already-signaled, so the very first crtgfx_
   * gpu_vulkan_surface_acquire()'s own vkWaitForFences() call (below)
   * returns immediately instead of waiting on a frame that never ran. */
  fence_info.flags = CRTGFX_VK_FENCE_CREATE_SIGNALED_BIT;
  if (vkCreateFence(vk_device, &fence_info, NULL, &frame_fence) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  pool_info.sType = CRTGFX_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.pNext = NULL;
  pool_info.flags = CRTGFX_VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = device->vk_queue_family_index;
  if (vkCreateCommandPool(vk_device, &pool_info, NULL, &command_pool) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  cmd_alloc_info.sType = CRTGFX_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_alloc_info.pNext = NULL;
  cmd_alloc_info.commandPool = command_pool;
  cmd_alloc_info.level = CRTGFX_VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_alloc_info.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(vk_device, &cmd_alloc_info, &command_buffer) != CRTGFX_VK_SUCCESS) {
    goto fail;
  }

  image_array = (void**)calloc(real_image_count, sizeof(void*));
  if (image_array == NULL) {
    goto fail;
  }
  for (i = 0; i < real_image_count; ++i) {
    image_array[i] = (void*)raw_images[i];
  }

  surface->device = device;
  surface->vk_surface = (void*)vk_surface;
  surface->vk_swapchain = (void*)vk_swapchain;
  surface->vk_images = image_array;
  surface->vk_image_count = real_image_count;
  surface->vk_format = chosen_format;
  surface->vk_image_usage_flags = swapchain_info.imageUsage;
  surface->width = extent.width;
  surface->height = extent.height;
  surface->vk_image_available_semaphore = (void*)image_available_sem;
  surface->vk_render_finished_semaphore = (void*)render_finished_sem;
  surface->vk_command_pool = (void*)command_pool;
  surface->vk_command_buffer = (void*)command_buffer;
  surface->vk_frame_fence = (void*)frame_fence;
  surface->vk_current_image_index = 0;
  surface->vk_image_acquired = 0;
  return CRTGFX_OK;

fail:
  /* command_buffer needs no separate destroy call -- vkDestroyCommandPool
   * (below) implicitly frees every command buffer allocated from it, per
   * the real spec's own documented VkCommandPool contract. */
  if (command_pool != NULL) {
    vkDestroyCommandPool(vk_device, command_pool, NULL);
  }
  if (frame_fence != NULL) {
    vkDestroyFence(vk_device, frame_fence, NULL);
  }
  if (render_finished_sem != NULL) {
    vkDestroySemaphore(vk_device, render_finished_sem, NULL);
  }
  if (image_available_sem != NULL) {
    vkDestroySemaphore(vk_device, image_available_sem, NULL);
  }
  if (vk_swapchain != NULL) {
    vkDestroySwapchainKHR(vk_device, vk_swapchain, NULL);
  }
  if (vk_surface != NULL) {
    vkDestroySurfaceKHR(instance, vk_surface, NULL);
  }
  return CRTGFX_ERROR_UNSUPPORTED;
}
#endif

static void crtgfx_gpu_vulkan_surface_state_destroy(struct crtgfx_gpu_vulkan_surface_state* surface) {
  VkDevice vk_device;

  if (surface->device == NULL) {
    return;
  }
  vk_device = (VkDevice)surface->device->vk_device;
  /* Real spec requirement: every one of these objects must not be
   * destroyed while the GPU may still be using them -- a plain
   * vkDeviceWaitIdle() is the simplest real way to guarantee that for a
   * surface being torn down (crtgfx_gpu_surface_release()'s own contract
   * has no separate "wait for in-flight frames" step of its own, so this
   * function must provide it). */
  vkDeviceWaitIdle(vk_device);
  if (surface->vk_command_pool != NULL) {
    vkDestroyCommandPool(vk_device, (VkCommandPool)surface->vk_command_pool, NULL);
  }
  if (surface->vk_frame_fence != NULL) {
    vkDestroyFence(vk_device, (VkFence)surface->vk_frame_fence, NULL);
  }
  if (surface->vk_render_finished_semaphore != NULL) {
    vkDestroySemaphore(vk_device, (VkSemaphore)surface->vk_render_finished_semaphore, NULL);
  }
  if (surface->vk_image_available_semaphore != NULL) {
    vkDestroySemaphore(vk_device, (VkSemaphore)surface->vk_image_available_semaphore, NULL);
  }
  if (surface->vk_swapchain != NULL) {
    vkDestroySwapchainKHR(vk_device, (VkSwapchainKHR)surface->vk_swapchain, NULL);
  }
  if (surface->vk_surface != NULL) {
    vkDestroySurfaceKHR((VkInstance)surface->device->vk_instance, (VkSurfaceKHR)surface->vk_surface, NULL);
  }
  free(surface->vk_images);
}

static crtgfx_result crtgfx_gpu_vulkan_surface_state_acquire(
    struct crtgfx_gpu_vulkan_surface_state* surface, uint64_t timeout_us) {
  VkDevice vk_device = (VkDevice)surface->device->vk_device;
  VkFence frame_fence = (VkFence)surface->vk_frame_fence;
  uint64_t timeout_ns;
  VkResult result;
  uint32_t image_index = 0;

  if (surface->vk_image_acquired) {
    /* Real, honest misuse guard: acquiring twice in a row without an
     * intervening present() would silently discard the first acquired
     * image index -- matches this project's own "reject out-of-order
     * usage with a real error, never a silent no-op" discipline. */
    return CRTGFX_ERROR_HOST;
  }

  /* Real single-frame-in-flight gate (see gpu_internal.h's own comment on
   * this struct's sync fields): wait for the GPU to actually finish the
   * *previous* frame's submitted command buffer before reusing it. */
  if (vkWaitForFences(vk_device, 1, &frame_fence, 1, UINT64_MAX) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }
  if (vkResetFences(vk_device, 1, &frame_fence) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }

  /* timeout_us * 1000 can overflow a uint64_t only for a timeout_us value
   * no real caller would ever pass (> ~584,942 years); UINT64_MAX itself
   * is real Vulkan's own documented "wait forever" sentinel, so an
   * astronomically large timeout_us is simply treated the same as
   * "forever" rather than wrapping around to a near-zero real timeout. */
  timeout_ns = (timeout_us > UINT64_MAX / 1000u) ? UINT64_MAX : timeout_us * 1000u;

  result = vkAcquireNextImageKHR(
      vk_device, (VkSwapchainKHR)surface->vk_swapchain, timeout_ns,
      (VkSemaphore)surface->vk_image_available_semaphore, NULL, &image_index);
  if (result == CRTGFX_VK_TIMEOUT) {
    return CRTGFX_ERROR_TIMEOUT;
  }
  if (result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_SUBOPTIMAL_KHR) {
    /* Includes VK_ERROR_OUT_OF_DATE_KHR (the surface's own real size no
     * longer matches this swapchain's, e.g. after a real resize) -- real,
     * later work: recreate the swapchain in place and retry, matching
     * what any production Vulkan presentation loop does. Not attempted in
     * this first vertical slice (see docs/libcrtgfx_wayland_plan.md's own
     * notes on this) -- surfaces this as a real, honest CRTGFX_ERROR_HOST
     * rather than silently limping on with a stale swapchain. */
    return CRTGFX_ERROR_HOST;
  }
  surface->vk_current_image_index = image_index;
  surface->vk_image_acquired = 1;
  surface->ganesh_wrapped = 0;
  return CRTGFX_OK;
}

static crtgfx_result crtgfx_gpu_vulkan_surface_state_clear(
    struct crtgfx_gpu_vulkan_surface_state* surface, float r, float g, float b, float a) {
  VkCommandBuffer cmd = (VkCommandBuffer)surface->vk_command_buffer;
  VkImage image = (VkImage)surface->vk_images[surface->vk_current_image_index];
  VkCommandBufferBeginInfo begin_info;
  VkImageSubresourceRange range;
  VkImageMemoryBarrier to_transfer;
  VkImageMemoryBarrier to_present;
  VkClearColorValue color;
  VkSemaphore wait_sem = (VkSemaphore)surface->vk_image_available_semaphore;
  VkSemaphore signal_sem = (VkSemaphore)surface->vk_render_finished_semaphore;
  VkPipelineStageFlags wait_stage = CRTGFX_VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info;

  if (!surface->vk_image_acquired) {
    return CRTGFX_ERROR_HOST;
  }
  if (surface->ganesh_wrapped) {
    /* Real, honest misuse guard (2026-09-07, the Ganesh-wrap vertical
     * slice): this frame's image was handed to crtgfx_skia_wrap_gpu_
     * surface() instead -- mixing the solid-color stand-in with a real
     * Ganesh-drawn frame is real, rejected misuse, matching every other
     * out-of-order guard in this file. */
    return CRTGFX_ERROR_HOST;
  }

  range.aspectMask = CRTGFX_VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = 0;
  range.levelCount = CRTGFX_VK_REMAINING_MIP_LEVELS;
  range.baseArrayLayer = 0;
  range.layerCount = CRTGFX_VK_REMAINING_ARRAY_LAYERS;

  color.float32[0] = r;
  color.float32[1] = g;
  color.float32[2] = b;
  color.float32[3] = a;

  to_transfer.sType = CRTGFX_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  to_transfer.pNext = NULL;
  to_transfer.srcAccessMask = 0;
  to_transfer.dstAccessMask = CRTGFX_VK_ACCESS_TRANSFER_WRITE_BIT;
  to_transfer.oldLayout = CRTGFX_VK_IMAGE_LAYOUT_UNDEFINED;
  to_transfer.newLayout = CRTGFX_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  to_transfer.srcQueueFamilyIndex = CRTGFX_VK_QUEUE_FAMILY_IGNORED;
  to_transfer.dstQueueFamilyIndex = CRTGFX_VK_QUEUE_FAMILY_IGNORED;
  to_transfer.image = image;
  to_transfer.subresourceRange = range;

  to_present.sType = CRTGFX_VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  to_present.pNext = NULL;
  to_present.srcAccessMask = CRTGFX_VK_ACCESS_TRANSFER_WRITE_BIT;
  to_present.dstAccessMask = CRTGFX_VK_ACCESS_MEMORY_READ_BIT;
  to_present.oldLayout = CRTGFX_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  to_present.newLayout = CRTGFX_VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  to_present.srcQueueFamilyIndex = CRTGFX_VK_QUEUE_FAMILY_IGNORED;
  to_present.dstQueueFamilyIndex = CRTGFX_VK_QUEUE_FAMILY_IGNORED;
  to_present.image = image;
  to_present.subresourceRange = range;

  begin_info.sType = CRTGFX_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = NULL;
  begin_info.flags = CRTGFX_VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;
  if (vkBeginCommandBuffer(cmd, &begin_info) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }
  vkCmdPipelineBarrier(
      cmd, CRTGFX_VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, CRTGFX_VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
      &to_transfer);
  vkCmdClearColorImage(cmd, image, CRTGFX_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
  vkCmdPipelineBarrier(
      cmd, CRTGFX_VK_PIPELINE_STAGE_TRANSFER_BIT, CRTGFX_VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
      &to_present);
  if (vkEndCommandBuffer(cmd) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }

  submit_info.sType = CRTGFX_VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &wait_sem;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &signal_sem;

  if (vkQueueSubmit(
          (VkQueue)surface->device->vk_queue, 1, &submit_info, (VkFence)surface->vk_frame_fence) !=
      CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}

/* Real swapchain recreation (2026-09-07, closing the "resize on all GPU
 * hosts" gap this vertical slice's own follow-up work left open -- see
 * crtgfx/gpu.h's own crtgfx_gpu_surface_resize() comment for the full,
 * host-independent contract). Vulkan's own real, documented idiom: build
 * a brand-new VkSwapchainKHR with `oldSwapchain` set to the surface's
 * current one (the spec guarantees the old swapchain stays valid to
 * present through only until vkCreateSwapchainKHR() itself returns, not
 * any longer -- this function creates the replacement first, then retires
 * the old one, matching that ordering exactly), fetch its own real images
 * fresh (the count can legitimately differ from before -- caps.min/
 * maxImageCount are themselves allowed to change across a real resize),
 * and swap the surface's own vk_swapchain/vk_images/vk_image_count/width/
 * height over to the new ones. The per-surface sync objects (semaphores,
 * frame fence, command pool/buffer) are not recreated -- they are not
 * swapchain-size-dependent, same reasoning as crtgfx_gpu_win32_surface_
 * resize()'s own choice to reuse its RTV heap rather than rebuild it. */
static crtgfx_result crtgfx_gpu_vulkan_surface_state_resize(
    struct crtgfx_gpu_vulkan_surface_state* surface, uint32_t width, uint32_t height) {
  VkDevice vk_device = (VkDevice)surface->device->vk_device;
  VkPhysicalDevice physical_device = (VkPhysicalDevice)surface->device->vk_physical_device;
  VkSurfaceKHR vk_surface = (VkSurfaceKHR)surface->vk_surface;
  VkSwapchainKHR old_swapchain = (VkSwapchainKHR)surface->vk_swapchain;
  VkSurfaceCapabilitiesKHR caps;
  VkSurfaceFormatKHR formats[64];
  uint32_t format_count = 64u;
  VkFormat chosen_format = CRTGFX_VK_FORMAT_B8G8R8A8_UNORM;
  VkColorSpaceKHR chosen_color_space = CRTGFX_VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  uint32_t image_count;
  VkExtent2D extent;
  VkSwapchainCreateInfoKHR swapchain_info;
  VkImage raw_images[CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES];
  uint32_t real_image_count = CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES;
  VkSwapchainKHR new_swapchain = NULL;
  void** new_image_array;
  VkResult result;
  uint32_t i;

  if (surface->vk_image_acquired) {
    /* Same real, honest misuse guard as every other out-of-order call this
     * contract already rejects -- recreating the swapchain while an image
     * is acquired is not real, defined Vulkan behavior. */
    return CRTGFX_ERROR_HOST;
  }
  if (width == surface->width && height == surface->height) {
    /* Real, cheap no-op -- see crtgfx/gpu.h's own comment on this
     * function. */
    return CRTGFX_OK;
  }

  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, vk_surface, &caps) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }
  /* Same real format/color-space probe as crtgfx_gpu_vulkan_surface_
   * create() above -- not persisted on the surface itself beyond
   * vk_format, so re-derived fresh here rather than assumed unchanged. */
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, vk_surface, &format_count, formats);
  if ((result == CRTGFX_VK_SUCCESS || result == CRTGFX_VK_INCOMPLETE) && format_count > 0u) {
    chosen_format = formats[0].format;
    chosen_color_space = formats[0].colorSpace;
    for (i = 0; i < format_count; ++i) {
      if (formats[i].format == CRTGFX_VK_FORMAT_B8G8R8A8_UNORM) {
        chosen_format = formats[i].format;
        chosen_color_space = formats[i].colorSpace;
        break;
      }
    }
  }

  /* Same real currentExtent-or-clamped-request convention as create(). */
  if (caps.currentExtent.width != 0xFFFFFFFFu) {
    extent = caps.currentExtent;
  } else {
    extent.width = width;
    if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
    if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
    extent.height = height;
    if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
    if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
  }
  if (extent.width == 0u || extent.height == 0u) {
    return CRTGFX_ERROR_HOST;
  }

  image_count = caps.minImageCount + 1u;
  if (caps.maxImageCount > 0u && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }
  if (image_count > CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES) {
    image_count = CRTGFX_GPU_VULKAN_MAX_SWAPCHAIN_IMAGES;
  }

  swapchain_info.sType = CRTGFX_VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = NULL;
  swapchain_info.flags = 0;
  swapchain_info.surface = vk_surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = chosen_format;
  swapchain_info.imageColorSpace = chosen_color_space;
  swapchain_info.imageExtent = extent;
  swapchain_info.imageArrayLayers = 1;
  /* A tenth real, confirmed bug (2026-09-13), found chasing crtgfx_skia_
   * wrap_gpu_surface() (skia_bridge.cc) failing at GrVkGpu::onWrapBackend
   * RenderTarget()'s own check_image_info() -- confirmed with a one-off
   * debug fprintf patched directly into a throwaway extracted copy of
   * Skia's own vendored src/gpu/ganesh/vk/GrVkGpu.cc: Ganesh unconditionally
   * requires BOTH VK_IMAGE_USAGE_TRANSFER_SRC_BIT and _DST_BIT set on any
   * VkImage it wraps as a render target (that file's own check_image_info(),
   * "We currently require everything to be made with transfer bits set"),
   * but this swapchain was only ever created with _DST_BIT -- missing
   * _SRC_BIT unconditionally failed every real Ganesh-Vulkan wrap of a live
   * swapchain image, even though the plain (non-Ganesh) crtgfx_gpu_surface_
   * clear()/_present() path (this same file) never needed it and kept
   * working throughout this whole investigation. Added conditionally, not
   * unconditionally: VkSurfaceCapabilitiesKHR::supportedUsageFlags is the
   * real, spec-documented source of truth for which VK_IMAGE_USAGE_* bits a
   * given surface's own presentable images can actually be created with --
   * every real desktop Linux Vulkan/WSI implementation this project targets
   * advertises TRANSFER_SRC_BIT support (confirmed for real: llvmpipe on
   * this host does), but a future/unusual host that does not would
   * otherwise fail vkCreateSwapchainKHR() outright with VK_ERROR_
   * INITIALIZATION_FAILED for requesting an unsupported usage bit -- the
   * same probe-first, zero-regression-risk discipline as this file's own
   * VK_KHR_wayland_surface/VK_KHR_swapchain extension probing
   * (crtgfx_gpu_vulkan_create_instance()/_device_create()). A host without
   * TRANSFER_SRC_BIT support here also cannot support crtgfx_skia_wrap_
   * gpu_surface() at all -- skia_bridge.cc's own image_info.fImageUsageFlags
   * must exactly match this swapchain's own real, created usage flags (a
   * mismatch there is exactly this same bug moved one layer over), so it
   * mirrors this same conditional. */
  swapchain_info.imageUsage = CRTGFX_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | CRTGFX_VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if ((caps.supportedUsageFlags & CRTGFX_VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u) {
    swapchain_info.imageUsage |= CRTGFX_VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  swapchain_info.imageSharingMode = CRTGFX_VK_SHARING_MODE_EXCLUSIVE;
  swapchain_info.queueFamilyIndexCount = 0;
  swapchain_info.pQueueFamilyIndices = NULL;
  swapchain_info.preTransform = ((caps.supportedTransforms & CRTGFX_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0u)
                                     ? CRTGFX_VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                                     : caps.currentTransform;
  swapchain_info.compositeAlpha = CRTGFX_VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = CRTGFX_VK_PRESENT_MODE_FIFO_KHR;
  swapchain_info.clipped = 1;
  swapchain_info.oldSwapchain = old_swapchain;

  if (vkCreateSwapchainKHR(vk_device, &swapchain_info, NULL, &new_swapchain) != CRTGFX_VK_SUCCESS) {
    return CRTGFX_ERROR_HOST;
  }

  /* Real spec requirement: the GPU must be done with the old swapchain's
   * own images before it is destroyed -- same "no separate wait-idle step
   * of its own" reasoning as crtgfx_gpu_vulkan_surface_destroy() above. */
  vkDeviceWaitIdle(vk_device);
  vkDestroySwapchainKHR(vk_device, old_swapchain, NULL);

  result = vkGetSwapchainImagesKHR(vk_device, new_swapchain, &real_image_count, raw_images);
  if ((result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_INCOMPLETE) || real_image_count == 0u) {
    /* The new swapchain itself is still real and valid even though this
     * one query failed -- left attached to the surface below (matches
     * crtgfx/gpu.h's own "safe to release, not otherwise usable" contract
     * for a failed resize) rather than leaking it. */
    surface->vk_swapchain = (void*)new_swapchain;
    return CRTGFX_ERROR_HOST;
  }
  new_image_array = (void**)calloc(real_image_count, sizeof(void*));
  if (new_image_array == NULL) {
    surface->vk_swapchain = (void*)new_swapchain;
    return CRTGFX_ERROR_HOST;
  }
  for (i = 0; i < real_image_count; ++i) {
    new_image_array[i] = (void*)raw_images[i];
  }

  free(surface->vk_images);
  surface->vk_swapchain = (void*)new_swapchain;
  surface->vk_images = new_image_array;
  surface->vk_image_count = real_image_count;
  surface->vk_format = chosen_format;
  surface->vk_image_usage_flags = swapchain_info.imageUsage;
  surface->width = extent.width;
  surface->height = extent.height;
  return CRTGFX_OK;
}

static crtgfx_result crtgfx_gpu_vulkan_surface_state_present(
    struct crtgfx_gpu_vulkan_surface_state* surface) {
  VkSwapchainKHR swapchain = (VkSwapchainKHR)surface->vk_swapchain;
  VkSemaphore wait_sem = (VkSemaphore)surface->vk_render_finished_semaphore;
  VkPresentInfoKHR present_info;
  VkResult result;

  if (!surface->vk_image_acquired) {
    return CRTGFX_ERROR_HOST;
  }
  if (surface->ganesh_wrapped) {
    /* Real, honest misuse guard (2026-09-07, the Ganesh-wrap vertical
     * slice): this frame's image was handed to crtgfx_skia_wrap_gpu_
     * surface() -- a caller must present it via crtgfx_skia_gpu_surface_
     * present() (which itself clears this flag before deferring to this
     * exact function), not this function directly, matching crtgfx_gpu_
     * vulkan_surface_clear()'s own identical guard above. */
    return CRTGFX_ERROR_HOST;
  }

  present_info.sType = CRTGFX_VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &wait_sem;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain;
  present_info.pImageIndices = &surface->vk_current_image_index;
  present_info.pResults = NULL;

  result = vkQueuePresentKHR((VkQueue)surface->device->vk_queue, &present_info);
  surface->vk_image_acquired = 0;
  if (result != CRTGFX_VK_SUCCESS && result != CRTGFX_VK_SUBOPTIMAL_KHR) {
    /* Same real VK_ERROR_OUT_OF_DATE_KHR "not handled yet" limitation as
     * crtgfx_gpu_vulkan_surface_acquire()'s own comment above. */
    return CRTGFX_ERROR_HOST;
  }
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_vulkan_device_create(
    uint32_t device_index, struct crtgfx_gpu_device* device) {
  struct crtgfx_gpu_vulkan_device_state* state =
      (struct crtgfx_gpu_vulkan_device_state*)calloc(1, sizeof(*state));
  crtgfx_result result;
  if (state == NULL) return CRTGFX_ERROR_UNSUPPORTED;
  result = crtgfx_gpu_vulkan_device_state_create(device_index, state);
  if (result != CRTGFX_OK) {
    free(state);
    return result;
  }
  device->backend_state = state;
  return CRTGFX_OK;
}

void crtgfx_gpu_vulkan_device_destroy(struct crtgfx_gpu_device* device) {
  struct crtgfx_gpu_vulkan_device_state* state =
      (struct crtgfx_gpu_vulkan_device_state*)device->backend_state;
  if (state == NULL) return;
  crtgfx_gpu_vulkan_device_state_destroy(state);
  free(state);
  device->backend_state = NULL;
}

crtgfx_result crtgfx_gpu_vulkan_surface_create(
    struct crtgfx_gpu_device* device, crtgfx_window* window, struct crtgfx_gpu_surface* surface) {
#if defined(CRTGFX_HAVE_NATIVE_WAYLAND)
  struct crtgfx_gpu_vulkan_surface_state* state;
  void* wl_display = NULL;
  void* wl_surface = NULL;
  crtgfx_result result;
  if (crtgfx_native_wl_get_surface_handles(
          &window->toplevel, &wl_display, &wl_surface) == 0) {
    return CRTGFX_ERROR_UNSUPPORTED;
  }
  state = (struct crtgfx_gpu_vulkan_surface_state*)calloc(1, sizeof(*state));
  if (state == NULL) return CRTGFX_ERROR_UNSUPPORTED;
  result = crtgfx_gpu_vulkan_surface_state_create(
      (struct crtgfx_gpu_vulkan_device_state*)device->backend_state,
      wl_display, wl_surface, window->toplevel.width, window->toplevel.height, state);
  if (result != CRTGFX_OK) {
    free(state);
    return result;
  }
  surface->backend_state = state;
  return CRTGFX_OK;
#else
  (void)device;
  (void)window;
  (void)surface;
  return CRTGFX_ERROR_UNSUPPORTED;
#endif
}

void crtgfx_gpu_vulkan_surface_destroy(struct crtgfx_gpu_surface* surface) {
  struct crtgfx_gpu_vulkan_surface_state* state =
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state;
  if (state == NULL) return;
  crtgfx_gpu_vulkan_surface_state_destroy(state);
  free(state);
  surface->backend_state = NULL;
}

crtgfx_result crtgfx_gpu_vulkan_surface_get_size(
    struct crtgfx_gpu_surface* surface, uint32_t* out_width, uint32_t* out_height) {
  struct crtgfx_gpu_vulkan_surface_state* state =
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state;
  if (state == NULL) return CRTGFX_ERROR_HOST;
  *out_width = state->width;
  *out_height = state->height;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_vulkan_surface_acquire(
    struct crtgfx_gpu_surface* surface, uint64_t timeout_us) {
  return crtgfx_gpu_vulkan_surface_state_acquire(
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state, timeout_us);
}

crtgfx_result crtgfx_gpu_vulkan_surface_clear(
    struct crtgfx_gpu_surface* surface, float r, float g, float b, float a) {
  return crtgfx_gpu_vulkan_surface_state_clear(
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state, r, g, b, a);
}

crtgfx_result crtgfx_gpu_vulkan_surface_resize(
    struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height) {
  return crtgfx_gpu_vulkan_surface_state_resize(
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state, width, height);
}

crtgfx_result crtgfx_gpu_vulkan_surface_present(struct crtgfx_gpu_surface* surface) {
  return crtgfx_gpu_vulkan_surface_state_present(
      (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state);
}

int crtgfx_gpu_vulkan_borrow_device(
    const struct crtgfx_gpu_device* device, struct crtgfx_gpu_vulkan_device_view* out_view) {
  const struct crtgfx_gpu_vulkan_device_state* state;
  if (device == NULL || out_view == NULL || device->backend != CRTGFX_GPU_BACKEND_VULKAN) return 0;
  state = (const struct crtgfx_gpu_vulkan_device_state*)device->backend_state;
  if (state == NULL || state->vk_instance == NULL || state->vk_device == NULL) return 0;
  out_view->instance = state->vk_instance;
  out_view->physical_device = state->vk_physical_device;
  out_view->device = state->vk_device;
  out_view->queue = state->vk_queue;
  out_view->queue_family_index = state->vk_queue_family_index;
  out_view->instance_extension_names = crtgfx_gpu_vulkan_instance_extension_names;
  out_view->instance_extension_count = crtgfx_gpu_vulkan_instance_extension_count;
  out_view->device_extension_names = state->vk_device_extension_names;
  out_view->device_extension_count = state->vk_device_extension_count;
  out_view->dmabuf_import = state->vk_dmabuf_import_enabled;
  return 1;
}

static void crtgfx_gpu_vulkan_fill_surface_view(
    struct crtgfx_gpu_vulkan_surface_state* state,
    struct crtgfx_gpu_vulkan_surface_view* out_view) {
  out_view->image = state->vk_images[state->vk_current_image_index];
  out_view->format = state->vk_format;
  out_view->image_usage_flags = state->vk_image_usage_flags;
  out_view->image_available_semaphore = state->vk_image_available_semaphore;
  out_view->render_finished_semaphore = state->vk_render_finished_semaphore;
  out_view->queue_family_index = state->device->vk_queue_family_index;
  out_view->width = state->width;
  out_view->height = state->height;
}

int crtgfx_gpu_vulkan_begin_ganesh(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_vulkan_surface_view* out_view) {
  struct crtgfx_gpu_vulkan_surface_state* state;
  if (surface == NULL || out_view == NULL || surface->backend != CRTGFX_GPU_BACKEND_VULKAN) return 0;
  state = (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state;
  if (state == NULL || !state->vk_image_acquired || state->ganesh_wrapped) return 0;
  crtgfx_gpu_vulkan_fill_surface_view(state, out_view);
  state->ganesh_wrapped = 1;
  return 1;
}

int crtgfx_gpu_vulkan_get_ganesh_present_view(
    struct crtgfx_gpu_surface* surface, struct crtgfx_gpu_vulkan_surface_view* out_view) {
  struct crtgfx_gpu_vulkan_surface_state* state;
  if (surface == NULL || out_view == NULL || surface->backend != CRTGFX_GPU_BACKEND_VULKAN) return 0;
  state = (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state;
  if (state == NULL || !state->ganesh_wrapped) return 0;
  crtgfx_gpu_vulkan_fill_surface_view(state, out_view);
  return 1;
}

void crtgfx_gpu_vulkan_end_ganesh(struct crtgfx_gpu_surface* surface) {
  struct crtgfx_gpu_vulkan_surface_state* state;
  VkSubmitInfo submit_info;
  if (surface == NULL || surface->backend != CRTGFX_GPU_BACKEND_VULKAN) return;
  state = (struct crtgfx_gpu_vulkan_surface_state*)surface->backend_state;
  if (state == NULL) return;
  state->ganesh_wrapped = 0;
  /* Real, confirmed bug (2026-09-17, found chasing Tranche 2's own "re-run
   * headless Ganesh plus native Wayland presentation/resize" acceptance --
   * TODO.md): crtgfx_gpu_vulkan_surface_state_acquire()'s own single-
   * frame-in-flight gate (above) unconditionally vkWaitForFences()s on
   * vk_frame_fence and then resets it, but only crtgfx_gpu_vulkan_surface_
   * state_clear()'s own plain, non-Ganesh path ever signals that fence
   * again (its own vkQueueSubmit(..., frame_fence) call) -- the Ganesh path
   * (this function's own caller, skia_bridge.cc's crtgfx_skia_gpu_surface_
   * present()) never submits anything against it at all, since Ganesh
   * itself owns the real submission via context->submit(). The result: a
   * live Wayland window running the Ganesh demo appeared to work (a
   * single static frame really did reach the screen, matching every past
   * "it displayed" observation on this host), but the *second* real
   * acquire() -- confirmed directly via GDB, blocked inside vkWaitForFences
   * at this exact call site, `crtgfx_gpu_vulkan_surface_state_acquire()` --
   * deadlocked forever waiting on a fence nothing would ever signal again;
   * a plain, no-resize 2-frame run reproduces it exactly like the resize
   * case does, so this was never resize-specific. By the time this
   * function runs, crtgfx_skia_gpu_surface_present()'s own context->
   * submit(GrSyncCpu::kYes) call has already synchronously blocked until
   * the GPU finished this frame's real work, so an empty vkQueueSubmit()
   * that only signals vk_frame_fence (zero real submitCount, a real,
   * spec-documented way to signal a fence with no queued work) is not a
   * lie -- it completes immediately and correctly represents "nothing
   * outstanding on this Vulkan frame slot" for the next acquire()'s own
   * wait, exactly like the plain path's real submission already does for
   * itself. Runs unconditionally (also covers the crtgfx_skia_wrap_gpu_
   * surface() failure path that also reaches this function) since the
   * fence is always left reset-but-never-resignaled by acquire() either
   * way. */
  submit_info.sType = CRTGFX_VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = NULL;
  submit_info.pWaitDstStageMask = NULL;
  submit_info.commandBufferCount = 0;
  submit_info.pCommandBuffers = NULL;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;
  vkQueueSubmit((VkQueue)state->device->vk_queue, 1, &submit_info, (VkFence)state->vk_frame_fence);
}

crtgfx_result crtgfx_gpu_vulkan_test_force_device_loss(struct crtgfx_gpu_device* device) {
  struct crtgfx_gpu_vulkan_device_state* state;
  if (device == NULL || device->backend != CRTGFX_GPU_BACKEND_VULKAN) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  state = (struct crtgfx_gpu_vulkan_device_state*)device->backend_state;
  if (state == NULL || state->vk_device == NULL) return CRTGFX_ERROR_HOST;
  vkDestroyDevice((VkDevice)state->vk_device, NULL);
  state->vk_device = NULL;
  state->vk_queue = NULL;
  return CRTGFX_OK;
}
