#pragma once

/* Private, non-installed control surface for GPU acceptance tests. Generic
 * tests may request a backend-owned fault through this header, but cannot see
 * gpu_internal.h, a native API handle, or a concrete backend-state layout. */

#include "crtgfx/gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

crtgfx_result crtgfx_gpu_test_force_device_loss(crtgfx_gpu_device* device);

#ifdef __cplusplus
} /* extern "C" */
#endif
