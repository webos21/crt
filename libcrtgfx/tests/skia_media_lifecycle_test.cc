// Cross-host ownership/lifecycle regression for "Zero-copy decoded textures"
// Tranche 5 (docs/crtmedia_zero_copy_decode_acceptance.md). This is the
// headless counterpart to tools/skia_media_window_demo.cc: fifteen complete
// extractor/decoder create -> decode -> GPU-frame import/draw -> destroy
// cycles, using a real Ganesh offscreen surface so every imported native
// texture is sampled and submitted before it is released.
//
// The release callback is wrapped per frame. That makes the ownership claim
// directly observable: after each cycle's synchronous submit, image/surface
// destruction, and GrDirectContext resource purge, exactly one callback must
// have run for every GPU frame imported. Since every host bridge destroys its
// imported graphics objects before calling crtmedia_gpu_frame_release(), this
// proves the whole bridge-to-decoder lifetime chain completed, not merely that
// decoder creation remained repeatable. Existing crtmedia_zero_copy_test keeps
// the independent software-only/CPU-resident path coverage.

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/skia_media.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined"
#endif

namespace {

constexpr int kLifecycleIterations = 15;

int g_failures = 0;
int g_release_callbacks = 0;

#define CHECK(cond, msg)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);         \
      ++g_failures;                                                          \
    }                                                                        \
  } while (0)

struct TrackedRelease {
  crtmedia_gpu_frame_release_fn original;
  void* original_context;
};

void tracked_release(crtmedia_gpu_frame* frame, void* release_context) {
  auto* tracked = static_cast<TrackedRelease*>(release_context);
  crtmedia_gpu_frame_release_fn original = tracked->original;
  void* original_context = tracked->original_context;
  free(tracked);
  frame->release = original;
  frame->release_context = original_context;
  original(frame, original_context);
  ++g_release_callbacks;
}

bool track_release(crtmedia_gpu_frame* frame) {
  auto* tracked = static_cast<TrackedRelease*>(malloc(sizeof(TrackedRelease)));
  if (tracked == nullptr) {
    return false;
  }
  tracked->original = frame->release;
  tracked->original_context = frame->release_context;
  frame->release = tracked_release;
  frame->release_context = tracked;
  return true;
}

struct CycleStats {
  uint32_t frames = 0;
  uint32_t gpu_frames = 0;
  uint32_t cpu_frames = 0;
};

bool consume_frame(
    GrDirectContext* context, SkSurface* surface, const crtgfx_gpu_device* device,
    crtmedia_gpu_frame* frame, CycleStats* stats) {
  ++stats->frames;
  if (frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU) {
    ++stats->cpu_frames;
    crtmedia_gpu_frame_release(frame);
    return true;
  }

  ++stats->gpu_frames;
  CHECK(frame->native_handle != nullptr, "GPU frame has a native handle");
  CHECK(frame->plane_count == 0, "GPU frame has no CPU-addressable planes");
  CHECK(frame->release != nullptr, "GPU frame owns its decoder surface");
  if (frame->native_handle == nullptr || frame->plane_count != 0 || frame->release == nullptr) {
    crtmedia_gpu_frame_release(frame);
    return false;
  }
  if (!track_release(frame)) {
    CHECK(false, "release tracker allocation succeeds");
    crtmedia_gpu_frame_release(frame);
    return false;
  }

  sk_sp<SkImage> image = crtgfx_skia_import_media_frame(context, device, frame);
  CHECK(image != nullptr, "GPU frame imports into a SkImage");
  if (image == nullptr) {
    crtmedia_gpu_frame_release(frame);
    return false;
  }
  CHECK(image->isTextureBacked(), "imported image is texture-backed");
  CHECK(frame->release == nullptr && frame->native_handle == nullptr,
        "successful import moves ownership out of the caller frame");

  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawImage(image, 0, 0);
  context->flushAndSubmit(surface, GrSyncCpu::kYes);
  image.reset();
  return true;
}

int drain_outputs(
    crtmedia_codec* codec, GrDirectContext* context, SkSurface* surface,
    const crtgfx_gpu_device* device, CycleStats* stats, int* out_eof) {
  int drained = 0;
  for (;;) {
    crtmedia_gpu_frame frame;
    memset(&frame, 0, sizeof(frame));
    int frame_eof = 0;
    crtmedia_result result = crtmedia_codec_dequeue_gpu_frame(codec, &frame, nullptr, &frame_eof);
    if (result == CRTMEDIA_WOULD_BLOCK) {
      return drained;
    }
    CHECK(result == CRTMEDIA_OK, "crtmedia_codec_dequeue_gpu_frame succeeds");
    if (result != CRTMEDIA_OK) {
      return -1;
    }
    if (frame_eof) {
      *out_eof = 1;
      return drained;
    }
    if (!consume_frame(context, surface, device, &frame, stats)) {
      return -1;
    }
    ++drained;
  }
}

bool queue_input_with_backpressure(
    crtmedia_codec* codec, const uint8_t* data, size_t size, int64_t pts_us,
    crtmedia_codec_buffer_flags flags, GrDirectContext* context, SkSurface* surface,
    const crtgfx_gpu_device* device, CycleStats* stats, int* video_eof) {
  for (int guard = 0; guard < 20000; ++guard) {
    crtmedia_result result = crtmedia_codec_queue_input(codec, data, size, pts_us, flags);
    if (result == CRTMEDIA_OK) {
      return true;
    }
    CHECK(result == CRTMEDIA_WOULD_BLOCK, "crtmedia_codec_queue_input succeeds or applies backpressure");
    if (result != CRTMEDIA_WOULD_BLOCK) {
      return false;
    }
    int drained = drain_outputs(codec, context, surface, device, stats, video_eof);
    CHECK(drained > 0 || *video_eof, "codec backpressure is relieved by output progress");
    if (drained <= 0) {
      return false;
    }
  }
  CHECK(false, "codec input backpressure remains bounded");
  return false;
}

bool run_cycle(
    int iteration, GrDirectContext* context, const crtgfx_gpu_device* device,
    CycleStats* stats, int* out_hardware) {
  crtmedia_extractor* extractor = nullptr;
  crtmedia_codec* codec = nullptr;
  crtmedia_format* video_format = nullptr;
  int video_track = -1;
  bool ok = true;

  CHECK(crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor) == CRTMEDIA_OK,
        "fixture extractor opens");
  if (extractor == nullptr) {
    return false;
  }
  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  for (uint32_t i = 0; i < track_count; ++i) {
    crtmedia_format* format = nullptr;
    if (crtmedia_extractor_track_format(extractor, i, &format) != CRTMEDIA_OK || format == nullptr) {
      continue;
    }
    const char* mime = nullptr;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    if (video_track < 0 && mime != nullptr && strncmp(mime, "video/", 6) == 0) {
      video_track = static_cast<int>(i);
      video_format = format;
    } else {
      crtmedia_format_release(format);
    }
  }
  CHECK(video_track >= 0, "fixture contains a video track");
  if (video_track < 0) {
    crtmedia_extractor_release(extractor);
    return false;
  }
  crtmedia_extractor_select_track(extractor, static_cast<uint32_t>(video_track));
  crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, 1);
  CHECK(crtmedia_codec_create_decoder(video_format, &codec) == CRTMEDIA_OK,
        "hardware-preferring decoder is created");
  crtmedia_format_release(video_format);
  if (codec == nullptr) {
    crtmedia_extractor_release(extractor);
    return false;
  }

  sk_sp<SkSurface> surface = crtgfx_skia_make_gpu_offscreen_surface(context, 64, 64);
  CHECK(surface != nullptr, "GPU offscreen surface is created");
  if (surface == nullptr) {
    crtmedia_codec_release(codec);
    crtmedia_extractor_release(extractor);
    return false;
  }

  int extractor_eof = 0;
  int video_eof = 0;
  for (int guard = 0; guard < 20000 && !(extractor_eof && video_eof); ++guard) {
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      crtmedia_result read_result = crtmedia_extractor_read_sample(extractor, &sample, &sample_eof);
      CHECK(read_result == CRTMEDIA_OK, "fixture sample read succeeds");
      if (read_result != CRTMEDIA_OK) {
        ok = false;
        break;
      }
      if (sample_eof) {
        extractor_eof = 1;
        ok = queue_input_with_backpressure(
            codec, static_cast<const uint8_t*>(0), 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
            CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM,
            context, surface.get(), device, stats, &video_eof);
      } else if (static_cast<int>(sample.track_index) == video_track) {
        ok = queue_input_with_backpressure(
            codec, static_cast<const uint8_t*>(sample.data), sample.size, sample.pts_us,
            CRTMEDIA_CODEC_BUFFER_FLAG_NONE,
            context, surface.get(), device, stats, &video_eof);
        crtmedia_sample_release(&sample);
      } else {
        crtmedia_sample_release(&sample);
      }
      if (!ok) {
        break;
      }
    }
    if (drain_outputs(codec, context, surface.get(), device, stats, &video_eof) < 0) {
      ok = false;
      break;
    }
  }

  CHECK(extractor_eof && video_eof, "decoder reaches extractor and codec EOS");
  CHECK(stats->frames == 25, "cycle decodes all 25 fixture frames");
  CHECK(crtmedia_codec_is_hardware_accelerated(codec, out_hardware) == CRTMEDIA_OK,
        "hardware acceleration state is readable");

  surface.reset();
  context->freeGpuResources();
  crtmedia_codec_release(codec);
  crtmedia_extractor_release(extractor);
  if (!ok || stats->frames != 25) {
    fprintf(stderr, "crtgfx_skia_media_lifecycle_test: iteration %d failed\n", iteration);
    return false;
  }
  return true;
}

}  // namespace

extern "C" int main(void) {
  crtgfx_gpu_capabilities caps = {};
  if (crtgfx_gpu_query_capabilities(&caps) != CRTGFX_OK || caps.device_count == 0) {
    printf("crtgfx_skia_media_lifecycle_test: skipped (no GPU device)\n");
    return 0;
  }
  crtgfx_gpu_device* device = nullptr;
  if (crtgfx_gpu_device_create(0, &device) != CRTGFX_OK || device == nullptr) {
    printf("crtgfx_skia_media_lifecycle_test: skipped (GPU device creation unavailable)\n");
    return 0;
  }
  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (context == nullptr) {
    crtgfx_gpu_device_release(device);
    printf("crtgfx_skia_media_lifecycle_test: skipped (Ganesh context unavailable)\n");
    return 0;
  }

  uint32_t total_gpu_frames = 0;
  int hardware_iterations = 0;
  int completed_iterations = 0;
  for (int iteration = 0; iteration < kLifecycleIterations; ++iteration) {
    int releases_before = g_release_callbacks;
    CycleStats stats;
    int is_hardware = 0;
    if (!run_cycle(iteration, context.get(), device, &stats, &is_hardware)) {
      break;
    }
    if (iteration == 0 && !is_hardware) {
      context.reset();
      crtgfx_gpu_device_release(device);
      printf("crtgfx_skia_media_lifecycle_test: skipped (hardware decode unavailable)\n");
      return 0;
    }
    CHECK(is_hardware, "every accepted lifecycle iteration uses real hardware decode");
    CHECK(stats.gpu_frames == 25 && stats.cpu_frames == 0,
          "every hardware frame stays GPU-resident through the bridge");
    CHECK(g_release_callbacks - releases_before == static_cast<int>(stats.gpu_frames),
          "every imported GPU frame release callback fires exactly once in its cycle");
    if (is_hardware) {
      ++hardware_iterations;
    }
    total_gpu_frames += stats.gpu_frames;
    ++completed_iterations;
  }

  CHECK(completed_iterations == kLifecycleIterations,
        "all bounded lifecycle iterations complete");

  context->freeGpuResources();
  context.reset();
  crtgfx_gpu_device_release(device);
  printf(
      "crtgfx_skia_media_lifecycle_test: RESULT iterations=%d hardware_iterations=%d "
      "gpu_frames=%u release_callbacks=%d\n",
      completed_iterations, hardware_iterations, total_gpu_frames, g_release_callbacks);

  if (g_failures != 0) {
    fprintf(stderr, "crtgfx_skia_media_lifecycle_test: %d failure(s)\n", g_failures);
    return 1;
  }
  printf("crtgfx_skia_media_lifecycle_test: ok\n");
  fflush(stdout);
  return 0;
}
