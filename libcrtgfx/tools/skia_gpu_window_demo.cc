// Manual, real on-screen demo for the Ganesh-on-swapchain vertical slice
// (2026-09-07 -- crtgfx_skia_wrap_gpu_surface()/crtgfx_skia_gpu_surface_
// present(), crtgfx/skia.h; TODO.md's "Finish live GPU presentation
// everywhere" step, its own last remaining piece). Draws the same shared
// reference scene (tests/skia_reference_scene.h) crtgfx_skia_gpu_
// offscreen_smoke already proves pixel-correct offscreen, but through a
// real, live swapchain/layer image every frame instead of an offscreen
// one -- real, visually-checkable proof that a real Ganesh scene reaches
// a real on-screen window, not just that the wrap/present calls succeed.
// Requires a usable GPU and desktop session; deliberately not registered
// with headless CTest, matching tools/gpu_window_demo.c's own precedent
// (this file's own C++/Skia sibling -- that one still covers the
// separate, non-Ganesh solid-color acquire/clear/present path unchanged).
// Usage: crtgfx_skia_gpu_window_demo [frame-count]; omitted/zero runs
// until the window is closed. Also drains CRTGFX_EVENT_RESIZE and calls
// crtgfx_gpu_surface_resize() every frame before drawing, exactly like
// tools/gpu_window_demo.c -- proving the resize and Ganesh-wrap features
// compose correctly, not just each in isolation.

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/window.h"

#include "skia_reference_scene.h"

#include <stdio.h>
#include <stdlib.h>

extern "C" int main(int argc, char** argv) {
  crtgfx_gpu_capabilities caps = {};
  crtgfx_gpu_device* device;
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_gpu_surface* surface;
  int rc;
  unsigned long frame_limit = argc > 1 ? strtoul(argv[1], NULL, 10) : 0;
  unsigned long presented = 0;
  int failed = 0;
  unsigned int timeouts = 0;
  uint32_t width, height;

  rc = crtgfx_gpu_query_capabilities(&caps);
  if (rc != CRTGFX_OK || caps.device_count == 0u) {
    fprintf(
        stderr,
        "crtgfx_skia_gpu_window_demo: no usable GPU backend on this host (rc=%d, device_count=%u) -- "
        "a usable GPU and desktop session are required\n",
        rc, caps.device_count);
    return 1;
  }
  fprintf(
      stderr, "crtgfx_skia_gpu_window_demo: backend=%d device_count=%u\n", (int)caps.backend, caps.device_count);

  rc = crtgfx_gpu_device_create(0, &device);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_gpu_device_create failed (%d)\n", rc);
    return 1;
  }

  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (context == nullptr) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_skia_make_gpu_context failed\n");
    crtgfx_gpu_device_release(device);
    return 1;
  }

  desc.title = "crtgfx Ganesh GPU surface";
  desc.width = 800;
  desc.height = 480;
  desc.flags = CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION;

  rc = crtgfx_window_create(&desc, &window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_window_create failed (%d)\n", rc);
    context.reset();
    crtgfx_gpu_device_release(device);
    return 1;
  }

  rc = crtgfx_gpu_surface_create(device, window, &surface);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_gpu_surface_create failed (%d)\n", rc);
    crtgfx_window_destroy(window);
    context.reset();
    crtgfx_gpu_device_release(device);
    return 1;
  }

  fprintf(
      stderr,
      "crtgfx_skia_gpu_window_demo: real Ganesh-drawn swapchain-backed surface created -- presenting "
      "the shared reference scene; close the window to exit\n");

  if (crtgfx_gpu_surface_get_size(surface, &width, &height) != CRTGFX_OK || width == 0 || height == 0) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: initial surface size check failed\n");
    failed = 1;
  }

  while (!failed && !crtgfx_window_should_close(window)) {
    crtgfx_event event;

    // Drain every queued event (crtgfx_window_pump_events() below is what
    // actually receives new ones) before this frame's acquire/draw/
    // present -- a resize must land before the next acquire() sees a
    // mismatched window size, matching tools/gpu_window_demo.c's own
    // identical ordering.
    while (crtgfx_window_poll_event(window, &event) == CRTGFX_OK && event.type != CRTGFX_EVENT_NONE) {
      if (event.type == CRTGFX_EVENT_RESIZE) {
        rc = crtgfx_gpu_surface_resize(surface, event.data.resize.width, event.data.resize.height);
        if (rc != CRTGFX_OK) {
          fprintf(
              stderr, "crtgfx_skia_gpu_window_demo: resize to %ux%u failed (%d)\n",
              event.data.resize.width, event.data.resize.height, rc);
          failed = 1;
          break;
        }
        fprintf(
            stderr, "crtgfx_skia_gpu_window_demo: resized to %ux%u\n", event.data.resize.width,
            event.data.resize.height);
      }
    }
    if (failed) break;

    rc = crtgfx_gpu_surface_acquire(surface, 1000000u);
    if (rc == CRTGFX_OK) {
      timeouts = 0;
      sk_sp<SkSurface> sk_surface = crtgfx_skia_wrap_gpu_surface(context.get(), surface);
      if (sk_surface == nullptr) {
        fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_skia_wrap_gpu_surface failed\n");
        failed = 1;
        break;
      }
      crtgfx_test::draw_reference_scene(sk_surface->getCanvas());
      rc = crtgfx_skia_gpu_surface_present(context.get(), sk_surface.get(), surface);
      sk_surface.reset();
      if (rc != CRTGFX_OK) {
        fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_skia_gpu_surface_present failed (%d)\n", rc);
        failed = 1;
        break;
      }
      if (++presented == frame_limit && frame_limit != 0) break;
    } else if (rc != CRTGFX_ERROR_TIMEOUT) {
      fprintf(stderr, "crtgfx_skia_gpu_window_demo: acquire failed (%d), stopping\n", rc);
      failed = 1;
      break;
    }
    if (rc == CRTGFX_ERROR_TIMEOUT && frame_limit != 0 && ++timeouts >= 60) {
      failed = 1;
      break;
    }
    crtgfx_window_pump_events(16);
  }

  crtgfx_gpu_surface_release(surface);
  crtgfx_window_destroy(window);
  context.reset();
  crtgfx_gpu_device_release(device);
  fprintf(stderr, "crtgfx_skia_gpu_window_demo: presented=%lu\n", presented);
  return failed || (frame_limit != 0 && presented != frame_limit);
}
