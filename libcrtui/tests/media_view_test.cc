/* Tranche 6: real hardware-decoded crtmedia GPU frames submitted to a
 * MediaView, then composed between CRT UI planes. Default mode is a headless
 * GPU CTest; passing a frame count additionally presents that many decoded
 * frames through a native crtgfx swapchain. */

#include "crtui/skia_media.h"

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/window.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined"
#endif

namespace {

int failures = 0;
int release_callbacks = 0;

#define CHECK(cond, msg)                                                    \
  do {                                                                      \
    if (!(cond)) {                                                          \
      fprintf(stderr, "crtui_media_view_test: FAILED %s\n", msg);          \
      ++failures;                                                           \
    }                                                                       \
  } while (0)

struct TrackedRelease {
  crtmedia_gpu_frame_release_fn original;
  void* original_context;
};

void tracked_release(crtmedia_gpu_frame* frame, void* user) {
  auto* tracked = static_cast<TrackedRelease*>(user);
  crtmedia_gpu_frame_release_fn original = tracked->original;
  void* original_context = tracked->original_context;
  free(tracked);
  frame->release = original;
  frame->release_context = original_context;
  original(frame, original_context);
  ++release_callbacks;
}

bool track_release(crtmedia_gpu_frame* frame) {
  auto* tracked = static_cast<TrackedRelease*>(malloc(sizeof(TrackedRelease)));
  if (tracked == nullptr || frame->release == nullptr) {
    free(tracked);
    return false;
  }
  tracked->original = frame->release;
  tracked->original_context = frame->release_context;
  frame->release = tracked_release;
  frame->release_context = tracked;
  return true;
}

const char* backend_name(crtgfx_gpu_backend backend) {
  switch (backend) {
    case CRTGFX_GPU_BACKEND_D3D12: return "d3d12";
    case CRTGFX_GPU_BACKEND_METAL: return "metal";
    case CRTGFX_GPU_BACKEND_VULKAN: return "vulkan";
    default: return "none";
  }
}

const char* interop_name(crtgfx_gpu_backend backend) {
  return backend == CRTGFX_GPU_BACKEND_D3D12 ? "gpu-copy" : "zero-copy";
}

struct Decoder {
  crtmedia_extractor* extractor = nullptr;
  crtmedia_codec* codec = nullptr;
  int video_track = -1;
  int extractor_eof = 0;
  int codec_eof = 0;
};

bool decoder_create(Decoder* decoder) {
  if (crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &decoder->extractor) != CRTMEDIA_OK) {
    return false;
  }
  crtmedia_format* selected = nullptr;
  uint32_t count = crtmedia_extractor_track_count(decoder->extractor);
  for (uint32_t i = 0; i < count; ++i) {
    crtmedia_format* format = nullptr;
    if (crtmedia_extractor_track_format(decoder->extractor, i, &format) != CRTMEDIA_OK || format == nullptr) {
      continue;
    }
    const char* mime = nullptr;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    if (selected == nullptr && mime != nullptr && strncmp(mime, "video/", 6) == 0) {
      decoder->video_track = static_cast<int>(i);
      selected = format;
    } else {
      crtmedia_format_release(format);
    }
  }
  if (selected == nullptr ||
      crtmedia_extractor_select_track(decoder->extractor, static_cast<uint32_t>(decoder->video_track)) != CRTMEDIA_OK) {
    if (selected != nullptr) crtmedia_format_release(selected);
    return false;
  }
  crtmedia_format_set_int32(selected, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, 1);
  bool ok = crtmedia_codec_create_decoder(selected, &decoder->codec) == CRTMEDIA_OK && decoder->codec != nullptr;
  crtmedia_format_release(selected);
  return ok;
}

void decoder_destroy(Decoder* decoder) {
  if (decoder->codec != nullptr) crtmedia_codec_release(decoder->codec);
  if (decoder->extractor != nullptr) crtmedia_extractor_release(decoder->extractor);
  *decoder = Decoder{};
}

struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget below = CRTUI_INVALID_WIDGET;
  crtui_widget media = CRTUI_INVALID_WIDGET;
  crtui_widget above = CRTUI_INVALID_WIDGET;
};

void fill(crtui_context* ui, crtui_widget widget, uint32_t rgb) {
  crtui_style style = {};
  style.mask = CRTUI_STYLE_BACKGROUND;
  style.background_rgb = rgb;
  CHECK(crtui_widget_set_style(ui, widget, &style) == CRTUI_OK, "set UI color");
}

bool scene_create(int width, int height, Scene* scene) {
  CHECK(crtui_context_create(&scene->ui) == CRTUI_OK, "create UI context");
  CHECK(crtui_window_create(scene->ui, &scene->window) == CRTUI_OK, "create UI window");
  CHECK(crtui_window_set_size(scene->ui, scene->window, width, height, 1.0f) == CRTUI_OK, "size UI window");
  CHECK(crtui_container_create(scene->ui, scene->window, &scene->below) == CRTUI_OK, "create UI below video");
  CHECK(crtui_widget_set_bounds(scene->ui, scene->below, 10, 10, width - 20, height - 20) == CRTUI_OK,
        "size UI below video");
  CHECK(crtui_media_view_create(scene->ui, scene->window, &scene->media) == CRTUI_OK, "create MediaView");
  CHECK(crtui_widget_set_bounds(scene->ui, scene->media, 40, 20, width - 80, height - 40) == CRTUI_OK,
        "size MediaView");
  CHECK(crtui_container_create(scene->ui, scene->window, &scene->above) == CRTUI_OK, "create UI above video");
  CHECK(crtui_widget_set_bounds(scene->ui, scene->above, 60, 30, 40, 30) == CRTUI_OK, "size UI above video");
  fill(scene->ui, scene->below, 0xFF0000u);
  fill(scene->ui, scene->above, 0x0000FFu);
  CHECK(crtui_surface_view_clear_damage(scene->ui, scene->media) == CRTUI_OK, "clear initial MediaView damage");
  return failures == 0;
}

bool scene_resize(Scene* scene, int width, int height) {
  return crtui_window_set_size(scene->ui, scene->window, width, height, 1.0f) == CRTUI_OK &&
         crtui_widget_set_bounds(scene->ui, scene->below, 10, 10, width - 20, height - 20) == CRTUI_OK &&
         crtui_widget_set_bounds(scene->ui, scene->media, 40, 20, width - 80, height - 40) == CRTUI_OK;
}

bool pixel_near(const uint8_t* pixels, size_t stride, int x, int y, int b, int g, int r) {
  const uint8_t* p = pixels + static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4u;
  return abs(static_cast<int>(p[0]) - b) <= 3 && abs(static_cast<int>(p[1]) - g) <= 3 &&
         abs(static_cast<int>(p[2]) - r) <= 3 && p[3] >= 250;
}

bool verify_composed_pixels(SkSurface* target) {
  int width = target->width();
  int height = target->height();
  size_t stride = static_cast<size_t>(width) * 4u;
  auto* pixels = static_cast<uint8_t*>(calloc(static_cast<size_t>(height), stride));
  if (pixels == nullptr) return false;
  SkImageInfo info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  bool ok = target->readPixels(info, pixels, stride, 0, 0);
  CHECK(ok, "read composed pixels");
  if (ok) {
    CHECK(pixel_near(pixels, stride, 20, 20, 0, 0, 255), "UI below MediaView");
    CHECK(pixel_near(pixels, stride, 70, 40, 255, 0, 0), "UI above MediaView");
    bool varying = false;
    uint8_t first = pixels[50u * stride + 120u * 4u];
    for (int y = 50; y < height - 30 && !varying; ++y) {
      for (int x = 120; x < width - 50; ++x) {
        if (pixels[static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4u] != first) {
          varying = true;
          break;
        }
      }
    }
    CHECK(varying, "decoded video region is non-degenerate");
    ok = ok && varying;
  }
  free(pixels);
  return ok;
}

struct Run {
  GrDirectContext* context = nullptr;
  crtgfx_gpu_device* device = nullptr;
  crtgfx_gpu_backend backend = CRTGFX_GPU_BACKEND_NONE;
  bool live = false;
  unsigned frame_limit = 0;
  unsigned gpu_frames = 0;
  unsigned cpu_frames = 0;
  bool resized = false;
  int64_t last_timestamp = CRTMEDIA_FRAME_TIMESTAMP_NONE;
  Scene scene;
  crtui_skia_media_provider* media_provider = nullptr;
  crtui_skia_surface_provider surface_provider = {};
  crtgfx_window* native_window = nullptr;
  crtgfx_gpu_surface* native_surface = nullptr;
};

bool render_frame(Run* run, crtmedia_gpu_frame* frame) {
  if (frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU) {
    ++run->cpu_frames;
    crtmedia_gpu_frame_release(frame);
    return true;
  }
  CHECK(frame->native_handle != nullptr && frame->plane_count == 0, "GPU frame stays non-CPU-addressable");
  if (!track_release(frame)) {
    CHECK(false, "install frame release tracker");
    crtmedia_gpu_frame_release(frame);
    return false;
  }
  int64_t timestamp = frame->timestamp_us;
  crtui_result submitted = crtui_skia_media_provider_submit(run->media_provider, run->scene.media, frame);
  CHECK(submitted == CRTUI_OK, "submit GPU frame to MediaView");
  CHECK(frame->release == nullptr && frame->native_handle == nullptr, "MediaView submit moves frame ownership");
  if (submitted != CRTUI_OK) {
    crtmedia_gpu_frame_release(frame);
    return false;
  }
  if (!run->live && run->gpu_frames == 2) {
    CHECK(scene_resize(&run->scene, 360, 200), "resize MediaView composition scene");
    run->resized = true;
  }
  size_t layer_count = 0;
  crtui_surface_layer layer = {};
  CHECK(crtui_window_get_surface_layers(run->scene.ui, run->scene.window, nullptr, 0, &layer_count) == CRTUI_OK &&
            layer_count == 1,
        "MediaView appears once in external-surface snapshot");
  CHECK(crtui_window_get_surface_layers(run->scene.ui, run->scene.window, &layer, 1, &layer_count) == CRTUI_OK &&
            layer.view == run->scene.media && layer.has_damage,
        "submitted frame marks MediaView damaged");

  sk_sp<SkSurface> target;
  if (run->live) {
    CHECK(crtgfx_gpu_surface_acquire(run->native_surface, 1000000u) == CRTGFX_OK, "acquire native surface");
    target = crtgfx_skia_wrap_gpu_surface(run->context, run->native_surface);
  } else {
    int32_t width = 0, height = 0;
    float scale = 0.0f;
    crtui_window_get_size(run->scene.ui, run->scene.window, &width, &height, &scale);
    target = crtgfx_skia_make_gpu_offscreen_surface(run->context, width, height);
  }
  CHECK(target != nullptr, "create MediaView composition target");
  if (target == nullptr) return false;
  CHECK(crtui_skia_compose(run->scene.ui, run->scene.window, target.get(), &run->surface_provider) == CRTUI_OK,
        "compose decoded frame with UI planes");
  if (run->live) {
    CHECK(crtgfx_skia_gpu_surface_present(run->context, target.get(), run->native_surface) == CRTGFX_OK,
          "present MediaView frame");
    crtgfx_window_pump_events(1);
  } else {
    run->context->flushAndSubmit(target.get(), GrSyncCpu::kYes);
    if (run->gpu_frames == 0) verify_composed_pixels(target.get());
  }
  target.reset();
  CHECK(crtui_surface_view_clear_damage(run->scene.ui, run->scene.media) == CRTUI_OK,
        "acknowledge MediaView damage");
  if (run->last_timestamp != CRTMEDIA_FRAME_TIMESTAMP_NONE && timestamp != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
    CHECK(timestamp >= run->last_timestamp, "decoded timestamps are monotonic");
  }
  run->last_timestamp = timestamp;
  ++run->gpu_frames;
  return failures == 0;
}

int drain_outputs(Decoder* decoder, Run* run) {
  int drained = 0;
  while (run->gpu_frames < run->frame_limit) {
    crtmedia_gpu_frame frame = {};
    int eof = 0;
    crtmedia_result result = crtmedia_codec_dequeue_gpu_frame(decoder->codec, &frame, nullptr, &eof);
    if (result == CRTMEDIA_WOULD_BLOCK) return drained;
    CHECK(result == CRTMEDIA_OK, "dequeue decoded GPU frame");
    if (result != CRTMEDIA_OK) return -1;
    if (eof) {
      decoder->codec_eof = 1;
      return drained;
    }
    if (!render_frame(run, &frame)) return -1;
    ++drained;
  }
  return drained;
}

bool queue_with_backpressure(
    Decoder* decoder, Run* run, const uint8_t* data, size_t size,
    int64_t timestamp, crtmedia_codec_buffer_flags flags) {
  for (int guard = 0; guard < 20000; ++guard) {
    crtmedia_result result = crtmedia_codec_queue_input(decoder->codec, data, size, timestamp, flags);
    if (result == CRTMEDIA_OK) return true;
    CHECK(result == CRTMEDIA_WOULD_BLOCK, "decoder input uses bounded backpressure");
    if (result != CRTMEDIA_WOULD_BLOCK || drain_outputs(decoder, run) <= 0) return false;
  }
  return false;
}

bool drive_decoder(Decoder* decoder, Run* run) {
  for (int guard = 0; guard < 20000 && run->gpu_frames < run->frame_limit; ++guard) {
    if (!decoder->extractor_eof) {
      crtmedia_sample sample = {};
      int eof = 0;
      if (crtmedia_extractor_read_sample(decoder->extractor, &sample, &eof) != CRTMEDIA_OK) return false;
      if (eof) {
        decoder->extractor_eof = 1;
        if (!queue_with_backpressure(
                decoder, run, nullptr, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
                CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) return false;
      } else if (static_cast<int>(sample.track_index) == decoder->video_track) {
        bool queued = queue_with_backpressure(
            decoder, run, static_cast<const uint8_t*>(sample.data), sample.size, sample.pts_us,
            CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
        crtmedia_sample_release(&sample);
        if (!queued) return false;
      } else {
        crtmedia_sample_release(&sample);
      }
    }
    if (drain_outputs(decoder, run) < 0) return false;
    if (decoder->extractor_eof && decoder->codec_eof) break;
  }
  return run->gpu_frames == run->frame_limit;
}

enum RunResult { RUN_FAILED = -1, RUN_SKIPPED = 0, RUN_PASSED = 1 };

RunResult run_media_view(
    GrDirectContext* context, crtgfx_gpu_device* device,
    crtgfx_gpu_backend backend, bool live, unsigned frame_limit) {
  Run run;
  run.context = context;
  run.device = device;
  run.backend = backend;
  run.live = live;
  run.frame_limit = frame_limit;
  int release_start = release_callbacks;
  int width = 320, height = 180;
  if (live) {
    crtgfx_window_desc desc = {"crtui MediaView", static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                               CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION};
    CHECK(crtgfx_window_create(&desc, &run.native_window) == CRTGFX_OK, "create MediaView native window");
    CHECK(run.native_window != nullptr &&
              crtgfx_gpu_surface_create(device, run.native_window, &run.native_surface) == CRTGFX_OK,
          "create MediaView native surface");
    uint32_t active_width = 0, active_height = 0;
    if (run.native_surface != nullptr &&
        crtgfx_gpu_surface_get_size(run.native_surface, &active_width, &active_height) == CRTGFX_OK) {
      width = static_cast<int>(active_width);
      height = static_cast<int>(active_height);
    }
  }
  scene_create(width, height, &run.scene);
  CHECK(crtui_skia_media_provider_create(run.scene.ui, context, device, &run.media_provider) == CRTUI_OK,
        "create MediaView frame provider");
  CHECK(crtui_skia_media_provider_get_surface_provider(run.media_provider, &run.surface_provider) == CRTUI_OK,
        "get final-compositor callbacks");
  Decoder decoder;
  CHECK(decoder_create(&decoder), "create hardware-preferring fixture decoder");
  bool decoded = failures == 0 && drive_decoder(&decoder, &run);

  bool hardware_unavailable = run.gpu_frames == 0 && run.cpu_frames != 0;
  if (!hardware_unavailable) {
    CHECK(run.gpu_frames == frame_limit, "requested GPU frames reach MediaView");
    if (!live) CHECK(run.resized, "MediaView survives final-compositor resize");
  }
  CHECK(crtui_skia_media_provider_clear(run.media_provider, run.scene.media) == CRTUI_OK,
        "clear current MediaView frame");
  if (!live) {
    int32_t current_width = width, current_height = height;
    float current_scale = 1.0f;
    (void)crtui_window_get_size(run.scene.ui, run.scene.window, &current_width, &current_height, &current_scale);
    sk_sp<SkSurface> empty = crtgfx_skia_make_gpu_offscreen_surface(context, current_width, current_height);
    crtui_result cleared_result = empty == nullptr
        ? CRTUI_ERROR_IO
        : crtui_skia_compose(run.scene.ui, run.scene.window, empty.get(), &run.surface_provider);
    if (cleared_result != CRTUI_WOULD_BLOCK) printf("cleared compose result=%d\n", static_cast<int>(cleared_result));
    CHECK(cleared_result == CRTUI_WOULD_BLOCK, "cleared MediaView has no producer frame");
  }
  crtui_skia_media_provider_destroy(run.media_provider);
  run.media_provider = nullptr;
  context->freeGpuResources();
  CHECK(release_callbacks - release_start == static_cast<int>(run.gpu_frames),
        "every submitted decoder frame is released exactly once");
  decoder_destroy(&decoder);
  if (run.scene.ui != nullptr) crtui_context_destroy(run.scene.ui);
  if (run.native_surface != nullptr) crtgfx_gpu_surface_release(run.native_surface);
  if (run.native_window != nullptr) crtgfx_window_destroy(run.native_window);
  printf("crtui_media_view_test: %s backend=%s interop=%s gpu_frames=%u cpu_frames=%u releases=%d\n",
         live ? "window" : "offscreen", backend_name(backend), interop_name(backend),
         run.gpu_frames, run.cpu_frames, release_callbacks - release_start);
  if (hardware_unavailable && failures == 0) return RUN_SKIPPED;
  return decoded && failures == 0 ? RUN_PASSED : RUN_FAILED;
}

}  // namespace

extern "C" int main(int argc, char** argv) {
  crtgfx_gpu_capabilities caps = {};
  if (crtgfx_gpu_query_capabilities(&caps) != CRTGFX_OK || caps.device_count == 0) {
    printf("crtui_media_view_test: skipped (no GPU device)\n");
    return 0;
  }
  crtgfx_gpu_device* device = nullptr;
  if (crtgfx_gpu_device_create(0, &device) != CRTGFX_OK || device == nullptr) {
    printf("crtui_media_view_test: skipped (GPU device unavailable)\n");
    return 0;
  }
  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (context == nullptr) {
    crtgfx_gpu_device_release(device);
    printf("crtui_media_view_test: skipped (Ganesh context unavailable)\n");
    return 0;
  }
  RunResult result = run_media_view(context.get(), device, caps.backend, false, 5);
  if (result == RUN_SKIPPED) {
    context.reset();
    crtgfx_gpu_device_release(device);
    printf("crtui_media_view_test: skipped (hardware GPU-frame decode unavailable)\n");
    return 0;
  }
  bool ok = result == RUN_PASSED;
  if (ok && argc > 1) {
    unsigned long frames = strtoul(argv[1], nullptr, 10);
    ok = frames != 0 &&
         run_media_view(context.get(), device, caps.backend, true,
                        static_cast<unsigned>(frames)) == RUN_PASSED;
  }
  context.reset();
  crtgfx_gpu_device_release(device);
  if (!ok || failures != 0) {
    printf("crtui_media_view_test: FAILED (see the failed checks above)\n");
    return 1;
  }
  printf("crtui_media_view_test: ok frames=pass texture=pass composition=pass damage=pass ownership=pass\n");
  return 0;
}
