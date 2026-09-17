#ifndef CRTGFX_GPU_WIN32_TEST_H
#define CRTGFX_GPU_WIN32_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Test-only adapter-selection control. While enabled, capability queries and
 * device creation enumerate only DXGI's real WARP adapter. This is deliberately
 * private and uninstalled; production selection remains hardware-preferred
 * with WARP used only as the no-adapter fallback. */
void crtgfx_gpu_win32_test_force_warp(int enabled);

#ifdef __cplusplus
}
#endif

#endif
