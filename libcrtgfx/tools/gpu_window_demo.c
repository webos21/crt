/* Manual GPU presentation smoke for Vulkan/native Wayland, D3D12/DXGI,
 * and Metal/CAMetalLayer. Requires a usable GPU and desktop session;
 * deliberately not registered with headless CTest.
 * Usage: crtgfx_gpu_window_demo [frame-count]; omitted/zero runs until close.
 * Animates a submitted solid-color clear, not a Ganesh scene. A finite
 * run fails if it closes early or cannot make progress for 60 attempts.
 * Also drains CRTGFX_EVENT_RESIZE and calls crtgfx_gpu_surface_resize()
 * (2026-09-07) -- a live, on-screen exercise of the resize/swapchain-
 * recreation contract, not just a compile-time check: dragging this
 * demo's own window edge on a real desktop session resizes the real
 * swapchain/layer in place instead of leaving it stale. */

#include "crtgfx/gpu.h"
#include "crtgfx/window.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  crtgfx_gpu_capabilities caps = {0};
  crtgfx_gpu_device* device;
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_gpu_surface* surface;
  unsigned int tick;
  int rc;
  unsigned long frame_limit = argc > 1 ? strtoul(argv[1], NULL, 10) : 0;
  unsigned long presented = 0;
  int failed = 0;
  unsigned int timeouts = 0;
  uint32_t width, height;

  rc = crtgfx_gpu_query_capabilities(&caps);
  if (rc != CRTGFX_OK || caps.device_count == 0u) {
    fprintf(stderr, "crtgfx_gpu_window_demo: no usable GPU backend on this host (rc=%d, device_count=%u) -- "
                     "a usable GPU and desktop session are required\n",
            rc, caps.device_count);
    return 1;
  }
  fprintf(stderr, "crtgfx_gpu_window_demo: backend=%d device_count=%u\n", (int)caps.backend, caps.device_count);

  rc = crtgfx_gpu_device_create(0, &device);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_gpu_window_demo: crtgfx_gpu_device_create failed (%d)\n", rc);
    return 1;
  }

  desc.title = "crtgfx GPU surface";
  desc.width = 800;
  desc.height = 480;
  desc.flags = CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION;

  rc = crtgfx_window_create(&desc, &window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_gpu_window_demo: crtgfx_window_create failed (%d)\n",
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

  /* Exercise the live surface contract before rendering. */
  if (crtgfx_gpu_surface_get_size(surface, &width, &height) != CRTGFX_OK ||
      width == 0 || height == 0 ||
      crtgfx_gpu_surface_present(surface) != CRTGFX_ERROR_HOST ||
      crtgfx_gpu_surface_clear(surface, 0, 0, 0, 1) != CRTGFX_ERROR_HOST) {
    fprintf(stderr, "crtgfx_gpu_window_demo: initial surface contract failed\n");
    failed = 1;
  }
  tick = 0;
  while (!failed && !crtgfx_window_should_close(window)) {
    float phase = (float)(tick % 256u) / 255.0f;
    crtgfx_event event;

    /* Drain every queued event (crtgfx_window_pump_events() below is what
     * actually receives new ones) before this frame's acquire/clear/
     * present -- a resize must land before the next acquire() sees a
     * mismatched window size, matching every real presentation loop's own
     * "handle resize, then render" ordering. */
    while (crtgfx_window_poll_event(window, &event) == CRTGFX_OK && event.type != CRTGFX_EVENT_NONE) {
      if (event.type == CRTGFX_EVENT_RESIZE) {
        rc = crtgfx_gpu_surface_resize(surface, event.data.resize.width, event.data.resize.height);
        if (rc != CRTGFX_OK) {
          fprintf(stderr, "crtgfx_gpu_window_demo: resize to %ux%u failed (%d)\n", event.data.resize.width,
                  event.data.resize.height, rc);
          failed = 1;
          break;
        }
        fprintf(stderr, "crtgfx_gpu_window_demo: resized to %ux%u\n", event.data.resize.width,
                event.data.resize.height);
      }
    }
    if (failed) break;

    rc = crtgfx_gpu_surface_acquire(surface, 1000000u);
    if (rc == CRTGFX_OK) {
      timeouts = 0;
      if (crtgfx_gpu_surface_acquire(surface, 0) != CRTGFX_ERROR_HOST) {
        fprintf(stderr, "crtgfx_gpu_window_demo: duplicate acquire was not rejected\n");
        failed = 1;
        break;
      }
      rc = crtgfx_gpu_surface_clear(surface, phase, 1.0f - phase, 0.5f, 1.0f);
      if (rc == CRTGFX_OK) rc = crtgfx_gpu_surface_present(surface);
      if (rc != CRTGFX_OK) {
        fprintf(stderr, "crtgfx_gpu_window_demo: clear/present failed (%d)\n", rc);
        failed = 1;
        break;
      }
      if (++presented == frame_limit && frame_limit != 0) break;
    } else if (rc != CRTGFX_ERROR_TIMEOUT) {
      fprintf(stderr, "crtgfx_gpu_window_demo: acquire failed (%d), stopping\n", rc);
      failed = 1;
      break;
    }
    if (rc == CRTGFX_ERROR_TIMEOUT && frame_limit != 0 && ++timeouts >= 60) {
      failed = 1;
      break;
    }
    ++tick;
    crtgfx_window_pump_events(16);
  }

  crtgfx_gpu_surface_release(surface);
  crtgfx_window_destroy(window);
  crtgfx_gpu_device_release(device);
  fprintf(stderr, "crtgfx_gpu_window_demo: presented=%lu\n", presented);
  return failed || (frame_limit != 0 && presented != frame_limit);
}
