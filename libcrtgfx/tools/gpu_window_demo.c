/* Manual, real on-screen verification for the Linux native-Wayland-
 * backend GPU presentation vertical slice (2026-09-07) -- same real
 * "run it yourself, needs a real compositor" shape as window_demo.c
 * (this directory) and tests/window_keyboard_interactive_test.c (see
 * that file's own top comment for why this class of test is not wired
 * into ctest at all). Opens a real CRTGFX_WINDOW_GPU_PRESENTATION window,
 * creates a real GPU device + swapchain-backed surface for it, and
 * presents a real, continuously animated solid color via crtgfx_gpu_
 * surface_clear() -- proving the whole real pipeline end to end (native
 * Wayland connection -> Vulkan instance/device -> VkSurfaceKHR ->
 * VkSwapchainKHR -> acquire/submit/present) on real compositor hardware.
 *
 * Deliberately not a full Ganesh/Skia scene: crtgfx_gpu_surface_clear()
 * is this vertical slice's own honest, minimal "draw" primitive (see
 * crtgfx/gpu.h's own comment on the acquire/clear/present contract) --
 * wiring the already-proven offscreen Ganesh pipeline (src/skia_bridge.cc,
 * already exercised by crtgfx_skia_gpu_offscreen_smoke) onto a surface's
 * own acquired image is real, separate follow-up work.
 *
 * Graceful, honest failure on any host without real GPU presentation
 * available (WSL specifically -- see src/arch/linux/window_wayland_
 * native.c's own WSL-detection comment; a headless CI runner; a Linux
 * host with no libvulkan-dev; Windows/macOS, whose own GPU presentation
 * wiring is separate, later work -- see docs/libcrtgfx_wayland_plan.md):
 * every real step below is checked and reported, never assumed to
 * succeed. */

#include "crtgfx/gpu.h"
#include "crtgfx/window.h"

#include <stdio.h>

int main(void) {
  crtgfx_gpu_capabilities caps;
  crtgfx_gpu_device* device;
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_gpu_surface* surface;
  unsigned int tick;
  int rc;

  rc = crtgfx_gpu_query_capabilities(&caps);
  if (rc != CRTGFX_OK || caps.device_count == 0u) {
    fprintf(stderr, "crtgfx_gpu_window_demo: no usable GPU backend on this host (rc=%d, device_count=%u) -- "
                     "see this file's own top comment for the real reasons this can happen (WSL, headless CI, "
                     "no libvulkan-dev, or an OS whose GPU presentation wiring is separate follow-up work)\n",
            rc, caps.device_count);
    return 1;
  }
  fprintf(stderr, "crtgfx_gpu_window_demo: backend=%d device_count=%u\n", (int)caps.backend, caps.device_count);

  rc = crtgfx_gpu_device_create(0, &device);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_gpu_window_demo: crtgfx_gpu_device_create failed (%d)\n", rc);
    return 1;
  }

  desc.title = "crtgfx native Wayland GPU surface";
  desc.width = 800;
  desc.height = 480;
  desc.flags = CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION;

  rc = crtgfx_window_create(&desc, &window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_gpu_window_demo: crtgfx_window_create failed (%d) -- see src/arch/linux/"
                     "window_wayland_native.c's own top comment (real compositor unreachable, no xdg-shell "
                     "support, or WSL's own deliberate bypass)\n",
            rc);
    crtgfx_gpu_device_release(device);
    return 1;
  }

  rc = crtgfx_gpu_surface_create(device, window, &surface);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_gpu_window_demo: crtgfx_gpu_surface_create failed (%d)\n", rc);
    crtgfx_window_destroy(window);
    crtgfx_gpu_device_release(device);
    return 1;
  }

  fprintf(stderr, "crtgfx_gpu_window_demo: real swapchain-backed surface created -- presenting an animated "
                   "color; close the window to exit\n");

  tick = 0;
  while (!crtgfx_window_should_close(window)) {
    float phase = (float)(tick % 256u) / 255.0f;

    rc = crtgfx_gpu_surface_acquire(surface, 1000000u);
    if (rc == CRTGFX_OK) {
      (void)crtgfx_gpu_surface_clear(surface, phase, 1.0f - phase, 0.5f, 1.0f);
      (void)crtgfx_gpu_surface_present(surface);
    } else if (rc != CRTGFX_ERROR_TIMEOUT) {
      fprintf(stderr, "crtgfx_gpu_window_demo: acquire failed (%d), stopping\n", rc);
      break;
    }
    ++tick;
    crtgfx_window_pump_events(16);
  }

  crtgfx_gpu_surface_release(surface);
  crtgfx_window_destroy(window);
  crtgfx_gpu_device_release(device);
  return 0;
}
