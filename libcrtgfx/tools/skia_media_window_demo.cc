// Manual, real on-screen demo for the decoded-texture GPU bridge
// (2026-09-23, "Zero-copy decoded textures" Tranche 1 -- crtgfx/skia_media.h,
// docs/crtmedia_zero_copy_decode_acceptance.md has the full frozen
// contract). The real, end-to-end proof that tranche's own acceptance gate
// requires: crtmedia_codec_dequeue_gpu_frame()'s real hardware-resident
// output (memory_kind == CRTMEDIA_GPU_MEMORY_GPU) reaches a real, live,
// on-screen Ganesh/Skia window through crtgfx_skia_import_media_frame(),
// with no decoder-to-bridge CPU readback, and with the same resize-
// and-pixel-check discipline docs/libcrtgfx_live_presentation_
// acceptance.md already established for tools/skia_gpu_window_demo.cc's
// own synthetic-scene sibling.
//
// Unlike that sibling, this demo's "scene" is a real decoded video frame,
// not a deterministic synthetic one, so its own pixel check cannot assert
// exact expected values -- it asserts real, non-degenerate image content
// instead (the same honest standard libcrtmedia/tests/hw_decode_test.c's
// own check_nv12_convert() and libcrtmedia/tests/zero_copy_test.c's own
// check_cpu_gpu_frame_convert() already use for this same fixture's real
// content), plus the normalized end-to-end interop classification:
// macOS/Linux are zero-copy, Windows is one GPU copy, and a CPU-resident
// fallback is cpu-copy. The explicit acceptance readPixels() below is a
// validation probe and is not part of the decoder-to-bridge classification.
//
// Requires a usable GPU and desktop session; deliberately not registered
// with headless CTest, matching tools/skia_gpu_window_demo.cc's own
// precedent. Usage: crtgfx_skia_media_window_demo [frame-count
// [resize-width resize-height]]; omitted/zero frame-count runs until the
// window is closed or the fixture's real frames are exhausted, whichever
// comes first (this demo, unlike its synthetic-scene sibling, has a real,
// finite source of frames -- libcrtmedia/assets/test_video.mp4, 25 frames).

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/skia_media.h"
#include "crtgfx/window.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "codec_test_control.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtgfx/CMakeLists.txt)"
#endif

namespace {

const char* backend_name(crtgfx_gpu_backend backend) {
  switch (backend) {
    case CRTGFX_GPU_BACKEND_VULKAN: return "vulkan";
    case CRTGFX_GPU_BACKEND_D3D12: return "d3d12";
    case CRTGFX_GPU_BACKEND_METAL: return "metal";
    default: return "none";
  }
}

const char* interop_name(crtgfx_gpu_backend backend, crtmedia_gpu_memory_kind memory_kind) {
  if (memory_kind != CRTMEDIA_GPU_MEMORY_GPU) return "cpu-copy";
  if (backend == CRTGFX_GPU_BACKEND_D3D12) return "gpu-copy";
  if (backend == CRTGFX_GPU_BACKEND_METAL || backend == CRTGFX_GPU_BACKEND_VULKAN) return "zero-copy";
  return "cpu-copy";
}

// Same real, non-degenerate-content standard this fixture's other real
// checks already use (see this file's own top comment) -- applied here to
// the actually-presented Ganesh surface, not an intermediate CPU
// conversion, so this is checking the whole GPU-frame pipeline's
// real output, not just crtmedia's own half of it.
bool check_presented_frame_nondegenerate(SkSurface* sk_surface, uint32_t width, uint32_t height) {
  size_t stride = static_cast<size_t>(width) * 4u;
  uint8_t* pixels = (uint8_t*)calloc(1, stride * height);
  if (pixels == nullptr) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: pixel_check: readback buffer allocation failed\n");
    return false;
  }
  SkImageInfo info =
      SkImageInfo::Make(static_cast<int>(width), static_cast<int>(height), kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  bool ok = sk_surface->readPixels(info, pixels, stride, 0, 0);
  if (!ok) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: pixel_check: readPixels() failed\n");
  } else {
    uint8_t first_b = pixels[0];
    bool saw_varying = false;
    for (uint32_t y = 0; y < height && !saw_varying; ++y) {
      for (uint32_t x = 0; x < width; ++x) {
        if (pixels[(size_t)y * stride + (size_t)x * 4u] != first_b) {
          saw_varying = true;
          break;
        }
      }
    }
    if (!saw_varying) {
      fprintf(stderr, "crtgfx_skia_media_window_demo: pixel_check: presented frame is uniform (degenerate)\n");
    }
    ok = saw_varying;
  }
  free(pixels);
  return ok;
}

}  // namespace

extern "C" int main(int argc, char** argv) {
  unsigned long frame_limit = argc > 1 ? strtoul(argv[1], NULL, 10) : 0;
  uint32_t scripted_resize_width = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
  uint32_t scripted_resize_height = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 0;
  if ((scripted_resize_width == 0) != (scripted_resize_height == 0)) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: resize requires nonzero width and height\n");
    return 1;
  }

  crtgfx_gpu_capabilities caps = {};
  int rc = crtgfx_gpu_query_capabilities(&caps);
  if (rc != CRTGFX_OK || caps.device_count == 0u) {
    fprintf(
        stderr, "crtgfx_skia_media_window_demo: no usable GPU backend on this host (rc=%d, device_count=%u)\n", rc,
        caps.device_count);
    return 1;
  }

  crtgfx_gpu_device* device;
  rc = crtgfx_gpu_device_create(0, &device);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_gpu_device_create failed (%d)\n", rc);
    return 1;
  }
  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (context == nullptr) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_skia_make_gpu_context failed\n");
    crtgfx_gpu_device_release(device);
    return 1;
  }

  crtgfx_window_desc desc;
  desc.title = "crtgfx zero-copy media surface";
  desc.width = 800;
  desc.height = 480;
  desc.flags = CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION;
  crtgfx_window* window;
  rc = crtgfx_window_create(&desc, &window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_window_create failed (%d)\n", rc);
    context.reset();
    crtgfx_gpu_device_release(device);
    return 1;
  }
  crtgfx_gpu_surface* surface;
  rc = crtgfx_gpu_surface_create(device, window, &surface);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_gpu_surface_create failed (%d)\n", rc);
    crtgfx_window_destroy(window);
    context.reset();
    crtgfx_gpu_device_release(device);
    return 1;
  }

  // The real, hardware-preferring decode side (mirrors libcrtmedia/tests/
  // zero_copy_test.c's own setup exactly).
  crtmedia_extractor* extractor = NULL;
  crtmedia_result mr = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  int video_track = -1;
  crtmedia_format* video_format = NULL;
  int failed = (mr != CRTMEDIA_OK);
  if (!failed) {
    uint32_t track_count = crtmedia_extractor_track_count(extractor);
    for (uint32_t i = 0; i < track_count; ++i) {
      crtmedia_format* format = NULL;
      if (crtmedia_extractor_track_format(extractor, i, &format) != CRTMEDIA_OK || format == NULL) continue;
      const char* mime = NULL;
      crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
      if (mime != NULL && strncmp(mime, "video/", 6) == 0 && video_track < 0) {
        video_track = (int)i;
        video_format = format;
      } else {
        crtmedia_format_release(format);
      }
    }
    failed = video_track < 0;
    if (!failed) crtmedia_extractor_select_track(extractor, (uint32_t)video_track);
  }
  crtmedia_codec* video_codec = NULL;
  if (!failed) {
    int32_t prefer_hw = 1;
    crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, prefer_hw);
    failed = crtmedia_codec_create_decoder(video_format, &video_codec) != CRTMEDIA_OK || video_codec == NULL;
    crtmedia_format_release(video_format);
  }
  if (failed) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: fixture/decoder setup failed\n");
    if (video_codec != NULL) crtmedia_codec_release(video_codec);
    if (extractor != NULL) crtmedia_extractor_release(extractor);
    crtgfx_gpu_surface_release(surface);
    crtgfx_window_destroy(window);
    context.reset();
    crtgfx_gpu_device_release(device);
    return 1;
  }

  fprintf(
      stderr,
      "crtgfx_skia_media_window_demo: backend=%s device_count=%u -- presenting real decoded video frames; close "
      "the window to exit\n",
      backend_name(caps.backend), caps.device_count);

  uint32_t width, height;
  if (crtgfx_gpu_surface_get_size(surface, &width, &height) != CRTGFX_OK || width == 0 || height == 0) {
    fprintf(stderr, "crtgfx_skia_media_window_demo: initial surface size check failed\n");
    failed = 1;
  }

  unsigned long presented = 0;
  unsigned int timeouts = 0;
  int extractor_eof = 0, video_eof = 0;
  int pixel_check_pending = (scripted_resize_width == 0) ? 1 : 0;
  int pixel_check_done = 0, pixel_check_ok = 0, gpu_frame_check_ok = 0;
  int checked_gpu_frame = -1, checked_texture_backed = -1, checked_cpu_readback = -1;
  const char* checked_interop = "n/a";
  unsigned long resize_frame = 0;
  int post_resize_present_ok = -1;

  while (!failed && !crtgfx_window_should_close(window) && !(extractor_eof && video_eof)) {
    crtgfx_event event;
    while (crtgfx_window_poll_event(window, &event) == CRTGFX_OK && event.type != CRTGFX_EVENT_NONE) {
      if (event.type == CRTGFX_EVENT_RESIZE) {
        if (crtgfx_gpu_surface_resize(surface, event.data.resize.width, event.data.resize.height) != CRTGFX_OK) {
          failed = 1;
          break;
        }
      }
    }
    if (failed) break;

    // Drive the real demux+decode pipeline until one real video frame is
    // ready, exactly like libcrtmedia/tests/zero_copy_test.c's own drive
    // loop.
    crtmedia_gpu_frame frame;
    memset(&frame, 0, sizeof(frame));
    int have_frame = 0;
    for (int guard = 0; !have_frame && !(extractor_eof && video_eof) && guard < 2000; ++guard) {
      if (!extractor_eof) {
        crtmedia_sample sample;
        int sample_eof = 0;
        if (crtmedia_extractor_read_sample(extractor, &sample, &sample_eof) == CRTMEDIA_OK) {
          if (sample_eof) {
            extractor_eof = 1;
            crtmedia_codec_queue_input(
                video_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
          } else if ((int)sample.track_index == video_track) {
            crtmedia_codec_queue_input(
                video_codec, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
            crtmedia_sample_release(&sample);
          } else {
            crtmedia_sample_release(&sample);
          }
        }
      }
      int frame_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_gpu_frame(video_codec, &frame, NULL, &frame_eof);
      if (dr == CRTMEDIA_OK && frame_eof) {
        video_eof = 1;
      } else if (dr == CRTMEDIA_OK) {
        have_frame = 1;
      }
    }
    if (!have_frame) break;

    crtmedia_gpu_memory_kind frame_memory_kind = frame.memory_kind;
    const char* frame_interop = interop_name(caps.backend, frame_memory_kind);
    sk_sp<SkImage> image = crtgfx_skia_import_media_frame(context.get(), device, &frame);
    if (image == nullptr) {
      fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_skia_import_media_frame failed\n");
      crtmedia_gpu_frame_release(&frame);
      failed = 1;
      break;
    }
    if (!image->isTextureBacked()) {
      fprintf(stderr, "crtgfx_skia_media_window_demo: imported image is not GPU texture-backed\n");
      failed = 1;
      break;
    }

    rc = crtgfx_gpu_surface_acquire(surface, 1000000u);
    if (rc != CRTGFX_OK) {
      if (rc != CRTGFX_ERROR_TIMEOUT) {
        fprintf(stderr, "crtgfx_skia_media_window_demo: acquire failed (%d), stopping\n", rc);
        failed = 1;
      } else if (++timeouts >= 60) {
        failed = 1;
      }
      crtgfx_window_pump_events(16);
      continue;
    }
    timeouts = 0;
    sk_sp<SkSurface> sk_surface = crtgfx_skia_wrap_gpu_surface(context.get(), surface);
    if (sk_surface == nullptr) {
      fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_skia_wrap_gpu_surface failed\n");
      failed = 1;
      break;
    }
    SkCanvas* canvas = sk_surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    canvas->drawImage(image, 0, 0);

    bool checking_this_frame = pixel_check_pending && !pixel_check_done;
    if (checking_this_frame) {
      pixel_check_ok = check_presented_frame_nondegenerate(sk_surface.get(), width, height);
      crtmedia_codec_hw_diagnostics diagnostics;
      memset(&diagnostics, 0, sizeof(diagnostics));
      crtmedia_codec_test_get_hw_diagnostics(video_codec, &diagnostics);
      gpu_frame_check_ok = diagnostics.hw_gpu_frame_delivered;
      checked_gpu_frame = frame_memory_kind == CRTMEDIA_GPU_MEMORY_GPU;
      checked_texture_backed = image->isTextureBacked();
      checked_cpu_readback = frame_memory_kind == CRTMEDIA_GPU_MEMORY_CPU;
      checked_interop = frame_interop;
      pixel_check_done = 1;
      resize_frame = presented + 1;
    }

    rc = crtgfx_skia_gpu_surface_present(context.get(), sk_surface.get(), surface);
    sk_surface.reset();
    if (checking_this_frame && scripted_resize_width != 0) {
      post_resize_present_ok = (rc == CRTGFX_OK);
    }
    if (rc != CRTGFX_OK) {
      fprintf(stderr, "crtgfx_skia_media_window_demo: crtgfx_skia_gpu_surface_present failed (%d)\n", rc);
      failed = 1;
      break;
    }
    ++presented;
    if (presented == 1 && scripted_resize_width != 0) {
      rc = crtgfx_gpu_surface_resize(surface, scripted_resize_width, scripted_resize_height);
      if (rc != CRTGFX_OK || crtgfx_gpu_surface_get_size(surface, &width, &height) != CRTGFX_OK) {
        fprintf(stderr, "crtgfx_skia_media_window_demo: scripted resize failed (%d)\n", rc);
        failed = 1;
        break;
      }
      fprintf(
          stderr, "crtgfx_skia_media_window_demo: scripted resize requested=%ux%u active=%ux%u\n",
          scripted_resize_width, scripted_resize_height, width, height);
      pixel_check_pending = 1;
    }
    if (presented == frame_limit && frame_limit != 0) break;
    crtgfx_window_pump_events(16);
  }

  crtgfx_gpu_surface_release(surface);
  crtgfx_window_destroy(window);
  context.reset();
  crtgfx_gpu_device_release(device);
  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);

  fprintf(stderr, "crtgfx_skia_media_window_demo: presented=%lu\n", presented);
  int clean_exit = !failed && (frame_limit == 0 || presented == frame_limit || (extractor_eof && video_eof));
  char resize_frame_str[32];
  if (resize_frame == 0) {
    snprintf(resize_frame_str, sizeof(resize_frame_str), "n/a");
  } else {
    snprintf(resize_frame_str, sizeof(resize_frame_str), "%lu", resize_frame);
  }
  fprintf(
      stderr,
      "crtgfx_skia_media_window_demo: RESULT backend=%s interop=%s gpu_frame=%s texture_backed=%s "
      "cpu_readback=%s frames_presented=%lu resize_frame=%s pixel_check=%s post_resize_present=%s clean_exit=%s\n",
      backend_name(caps.backend), checked_interop,
      checked_gpu_frame < 0 ? "n/a" : (checked_gpu_frame ? "yes" : "no"),
      checked_texture_backed < 0 ? "n/a" : (checked_texture_backed ? "yes" : "no"),
      checked_cpu_readback < 0 ? "n/a" : (checked_cpu_readback ? "yes" : "no"), presented, resize_frame_str,
      pixel_check_done ? (pixel_check_ok ? "pass" : "fail") : "n/a",
      post_resize_present_ok < 0 ? "n/a" : (post_resize_present_ok ? "pass" : "fail"),
      clean_exit ? "pass" : "fail");

  return failed ||
             (pixel_check_done &&
              (!pixel_check_ok || !gpu_frame_check_ok || !checked_gpu_frame || !checked_texture_backed ||
               checked_cpu_readback))
         ? 1
         : 0;
}
