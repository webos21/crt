#pragma once

/* Private, non-installed Vulkan fault controls used only by the focused GPU
 * test. They expose no native handle or backend-state layout and are hidden
 * from the shared-library ABI. */

#include <stdint.h>

#define CRTGFX_GPU_VULKAN_TEST_FAIL_AFTER_INSTANCE 1u
#define CRTGFX_GPU_VULKAN_TEST_FAIL_AFTER_DEVICE 2u

#define CRTGFX_GPU_VULKAN_TEST_CLEANED_INSTANCE 0x1u
#define CRTGFX_GPU_VULKAN_TEST_CLEANED_DEVICE 0x2u

#if defined(__GNUC__) || defined(__clang__)
#define CRTGFX_GPU_VULKAN_TEST_HIDDEN __attribute__((visibility("hidden")))
#else
#define CRTGFX_GPU_VULKAN_TEST_HIDDEN
#endif

CRTGFX_GPU_VULKAN_TEST_HIDDEN void crtgfx_gpu_vulkan_test_inject_device_create_failure(
    uint32_t step);
CRTGFX_GPU_VULKAN_TEST_HIDDEN uint32_t crtgfx_gpu_vulkan_test_take_device_cleanup_mask(void);

