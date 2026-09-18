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
// Usage: crtgfx_skia_gpu_window_demo [frame-count [resize-width
// resize-height]]; omitted/zero frame-count runs until the window is closed.
// When both resize dimensions are present, the demo requests that GPU-surface
// size after the first presented frame, then presents through the replacement
// swapchain. This gives automated acceptance runs a deterministic resize path
// even when the compositor does not expose remote window-management controls.
// The demo also drains CRTGFX_EVENT_RESIZE and calls
// crtgfx_gpu_surface_resize() every frame before drawing, exactly like
// tools/gpu_window_demo.c -- proving the resize and Ganesh-wrap features
// compose correctly, not just each in isolation.
//
// Backend-neutral acceptance contract (2026-09-18, TODO.md's "Finish live
// GPU presentation evidence before hardware decode" tranche, Step 1 --
// docs/libcrtgfx_live_presentation_acceptance.md has the full frozen
// contract this implements). This one C++ source, unchanged per backend,
// checks the deterministic reference scene's actual pixel content -- not
// just that draw/present calls returned CRTGFX_OK -- via a plain
// SkSurface::readPixels() against the Ganesh-wrapped swapchain surface,
// the same real, generic Skia API tests/skia_gpu_offscreen_smoke.cc already
// uses offscreen; no backend-specific readback hook is needed. When a
// scripted resize is requested, the check runs on the first frame drawn at
// the new backing extent (the deterministic "resize_frame"); otherwise it
// runs on the first frame at the initial extent. On success or failure, a
// final single-line, space-separated `key=value` RESULT record is printed
// to stdout so an automated acceptance run can grep it without parsing the
// rest of this demo's own diagnostic stderr output. This record is
// intentionally the same shape on every backend; only the values (a
// Retina-scaled `backing_size` on macOS, an unset `resize_frame`/
// `pixel_check` when no resize was requested, ...) are expected to differ.

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/window.h"

#include "skia_reference_scene.h"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

const char* backend_name(crtgfx_gpu_backend backend) {
  switch (backend) {
    case CRTGFX_GPU_BACKEND_VULKAN: return "vulkan";
    case CRTGFX_GPU_BACKEND_D3D12: return "d3d12";
    case CRTGFX_GPU_BACKEND_METAL: return "metal";
    default: return "none";
  }
}

// Reads every pixel back from `sk_surface` (assumed already drawn+flushed
// with the shared reference scene at `width`x`height`) and runs the shared
// cross-backend assertions against it -- the exact same real SkSurface::
// readPixels() + tests/skia_reference_scene.h::check_reference_scene()
// pattern tests/skia_gpu_offscreen_smoke.cc already uses offscreen, applied
// here to the live, Ganesh-wrapped swapchain/layer surface instead. Must be
// called after draw_reference_scene() but before crtgfx_skia_gpu_surface_
// present() hands the image to the backend for presentation. Returns false
// (and leaves a diagnostic on stderr) on any allocation/readback/content
// failure.
bool verify_reference_scene_pixels(SkSurface* sk_surface, uint32_t width, uint32_t height) {
  size_t stride = static_cast<size_t>(width) * 4u;
  void* pixels = calloc(1, stride * height);
  if (pixels == nullptr) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: pixel_check: readback buffer allocation failed\n");
    return false;
  }
  SkImageInfo info = SkImageInfo::Make(
      static_cast<int>(width), static_cast<int>(height), kBGRA_8888_SkColorType,
      kPremul_SkAlphaType);
  bool ok = sk_surface->readPixels(info, pixels, stride, 0, 0);
  if (!ok) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: pixel_check: readPixels() failed\n");
  } else {
    crtgfx_test::check_reference_scene(pixels, stride, [&](const char* name, bool check_ok) {
      if (!check_ok) {
        fprintf(stderr, "crtgfx_skia_gpu_window_demo: pixel_check: %s FAILED\n", name);
      }
      ok = ok && check_ok;
    });
  }
  free(pixels);
  return ok;
}

}  // namespace

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
  uint32_t scripted_resize_width = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
  uint32_t scripted_resize_height = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 0;
  uint32_t width, height;
  uint32_t requested_width = 0, requested_height = 0;

  // Backend-neutral acceptance-contract state (see this file's own top
  // comment and docs/libcrtgfx_live_presentation_acceptance.md): a single
  // pixel_check runs on the deterministic "canonical" frame -- the first
  // post-resize frame when a scripted resize was requested (matching the
  // contract's own `resize_frame`), otherwise the first frame drawn at the
  // initial extent. `pixel_check_pending` marks the *next* frame drawn as
  // that canonical frame.
  int pixel_check_pending = (scripted_resize_width == 0) ? 1 : 0;
  int pixel_check_done = 0;
  int pixel_check_ok = 0;
  unsigned long resize_frame = 0;
  int post_resize_present_ok = -1; // -1 == not applicable (no resize requested)

  if ((scripted_resize_width == 0) != (scripted_resize_height == 0)) {
    fprintf(stderr, "crtgfx_skia_gpu_window_demo: resize requires nonzero width and height\n");
    return 1;
  }

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
  requested_width = desc.width;
  requested_height = desc.height;

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
      // The canonical acceptance-contract frame's own pixel check must run
      // here, after drawing but before crtgfx_skia_gpu_surface_present()
      // hands this image to the backend for presentation -- present() may
      // transition or invalidate it for host readback on some backends.
      bool checking_this_frame = pixel_check_pending && !pixel_check_done;
      if (checking_this_frame) {
        pixel_check_ok = verify_reference_scene_pixels(sk_surface.get(), width, height);
        pixel_check_done = 1;
        resize_frame = presented + 1; // 1-based index of the frame about to present
      }
      rc = crtgfx_skia_gpu_surface_present(context.get(), sk_surface.get(), surface);
      sk_surface.reset();
      if (checking_this_frame && scripted_resize_width != 0) {
        post_resize_present_ok = (rc == CRTGFX_OK);
      }
      if (rc != CRTGFX_OK) {
        fprintf(stderr, "crtgfx_skia_gpu_window_demo: crtgfx_skia_gpu_surface_present failed (%d)\n", rc);
        failed = 1;
        break;
      }
      ++presented;
      if (presented == 1 && scripted_resize_width != 0) {
        rc = crtgfx_gpu_surface_resize(surface, scripted_resize_width, scripted_resize_height);
        if (rc != CRTGFX_OK ||
            crtgfx_gpu_surface_get_size(surface, &width, &height) != CRTGFX_OK) {
          fprintf(
              stderr, "crtgfx_skia_gpu_window_demo: scripted resize to %ux%u failed (%d)\n",
              scripted_resize_width, scripted_resize_height, rc);
          failed = 1;
          break;
        }
        fprintf(
            stderr, "crtgfx_skia_gpu_window_demo: scripted resize requested=%ux%u active=%ux%u\n",
            scripted_resize_width, scripted_resize_height, width, height);
        requested_width = scripted_resize_width;
        requested_height = scripted_resize_height;
        pixel_check_pending = 1; // the next frame drawn is the canonical post-resize frame
      }
      if (presented == frame_limit && frame_limit != 0) break;
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

  int clean_exit = !failed && (frame_limit == 0 || presented == frame_limit);
  char requested_size[32], backing_size[32], resize_frame_str[16], post_resize_str[8];
  snprintf(requested_size, sizeof(requested_size), "%ux%u", requested_width, requested_height);
  snprintf(backing_size, sizeof(backing_size), "%ux%u", width, height);
  if (scripted_resize_width != 0) {
    snprintf(resize_frame_str, sizeof(resize_frame_str), "%lu", resize_frame);
    snprintf(post_resize_str, sizeof(post_resize_str), "%s", post_resize_present_ok < 0 ? "n/a" : (post_resize_present_ok ? "pass" : "fail"));
  } else {
    snprintf(resize_frame_str, sizeof(resize_frame_str), "n/a");
    snprintf(post_resize_str, sizeof(post_resize_str), "n/a");
  }
  // The frozen, backend-neutral RESULT record (docs/libcrtgfx_live_
  // presentation_acceptance.md) -- one line, space-separated key=value
  // pairs, printed on every exit path (success or failure) so an
  // automated acceptance run can grep it unconditionally.
  printf(
      "crtgfx_skia_gpu_window_demo: RESULT backend=%s requested_size=%s backing_size=%s "
      "frames_requested=%lu frames_presented=%lu resize_frame=%s pixel_check=%s "
      "post_resize_present=%s clean_exit=%s\n",
      backend_name(caps.backend), requested_size, backing_size, frame_limit, presented,
      resize_frame_str, pixel_check_done ? (pixel_check_ok ? "pass" : "fail") : "skip",
      post_resize_str, clean_exit ? "pass" : "fail");

  return failed || (frame_limit != 0 && presented != frame_limit);
}
