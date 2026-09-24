// Real coverage for "Zero-copy decoded textures" Tranche 0/1
// (2026-09-23, docs/crtmedia_zero_copy_decode_acceptance.md): exercises
// crtmedia_codec_dequeue_gpu_frame() against the same real H.264 MP4
// fixture tests/hw_decode_test.c already proves correct through
// crtmedia_codec_dequeue_output() (libcrtmedia/assets/test_video.mp4 --
// 64x64, 25 real encoded frames).
//
// This is a structural test, not a pixel-correctness test for the
// zero-copy path: crtmedia_gpu_frame(memory_kind == GPU)'s whole point is
// that no CPU-addressable pixel data exists to inspect (plane_count == 0)
// -- that is exactly what makes it zero-copy. Real pixel-content proof for
// that path is crtgfx_skia_media's own job (still open,
// docs/crtmedia_zero_copy_decode_acceptance.md's own per-host acceptance
// gate), once it imports native_handle into a real GPU texture and
// presents it. What this test *can* and does prove, honestly, for every
// host:
//   - the CPU-fallback shape (memory_kind == CPU, native_handle == NULL,
//     real pixel planes -- run through crtmedia_frame_convert_to_rgba()
//     for the same non-degenerate-content check hw_decode_test.c already
//     established) works identically to dequeue_output(), whether that is
//     software decode or the not-yet-zero-copy hardware fallback
//     (D3D11VA/VAAPI today);
//   - on a host with a real zero-copy path implemented (macOS/
//     VideoToolbox today), the real hardware branch instead produces
//     memory_kind == GPU, native_handle != NULL, plane_count == 0, and
//     crtmedia_codec_is_hardware_accelerated()/hw_zero_copy_delivered
//     transition exactly where docs/crtmedia_zero_copy_decode_
//     acceptance.md says they must;
//   - a software-only decoder (PREFER_HARDWARE_DECODE unset) never
//     reports memory_kind == GPU, on any host.
//
// CRTMEDIA_TEST_VIDEO_PATH is a compile-time -D define (libcrtmedia/
// CMakeLists.txt), matching every other demux_decode_*_test.c's own
// convention.

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "codec_test_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

#if defined(CRT_TARGET_OS_MACOS)
#define CRTMEDIA_HW_BACKEND_NAME "videotoolbox"
#define CRTMEDIA_ZERO_COPY_EXPECTED 1
#elif defined(CRT_TARGET_OS_WINDOWS)
#define CRTMEDIA_HW_BACKEND_NAME "d3d11va"
// 1, not 0 (2026-09-23, Windows Zero-copy decoded textures Tranche 2):
// codec.c's own new AV_PIX_FMT_D3D11 branch (fill_gpu_video_frame_d3d11())
// hands off the decode-pool ID3D11Texture2D pointer/array-index pair
// directly as native_handle, with no av_hwframe_transfer_data()/CPU copy in
// codec.c itself -- exactly docs/crtmedia_zero_copy_decode_acceptance.md's
// own "real zero-copy path implemented for this host" row, the same as
// macOS. The real GPU-to-GPU copy this host still needs (D3D11 has no
// direct D3D12 multi-plane share) happens one layer up, inside crtgfx_
// skia_media's own bridge (skia_bridge.cc) -- a structural, no-CPU-readback
// guarantee the acceptance doc's own per-tranche gate checks separately,
// not something this crtmedia-layer diagnostic reports.
#define CRTMEDIA_ZERO_COPY_EXPECTED 1
#elif defined(CRT_TARGET_OS_LINUX)
#define CRTMEDIA_HW_BACKEND_NAME "vaapi"
#define CRTMEDIA_ZERO_COPY_EXPECTED 0
#else
#define CRTMEDIA_HW_BACKEND_NAME "none"
#define CRTMEDIA_ZERO_COPY_EXPECTED 0
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);          \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

// Same real, non-degenerate-content check hw_decode_test.c's own
// check_nv12_convert() already established, adapted to crtmedia_gpu_frame's
// own CPU-branch shape (no color_range/color_space fields -- left
// unspecified, matching crtmedia_gpu_frame_create_cpu()'s own established
// behavior).
static int check_cpu_gpu_frame_convert(const crtmedia_gpu_frame* frame) {
  int before = failures;
  crtmedia_frame src;
  memset(&src, 0, sizeof(src));
  src.format = frame->format;
  src.width = frame->width;
  src.height = frame->height;
  src.timestamp_us = frame->timestamp_us;
  src.plane_count = frame->plane_count;
  for (uint32_t i = 0; i < frame->plane_count; ++i) {
    src.planes[i] = frame->planes[i];
  }

  size_t stride = (size_t)frame->width * 4u;
  uint8_t* rgba = (uint8_t*)malloc(stride * frame->height);
  CHECK(rgba != NULL, "rgba scratch buffer allocated");
  if (rgba == NULL) {
    return 0;
  }
  crtmedia_frame dst;
  memset(&dst, 0, sizeof(dst));
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = frame->width;
  dst.height = frame->height;
  dst.planes[0].data = rgba;
  dst.planes[0].stride = (uint32_t)stride;

  crtmedia_result r = crtmedia_frame_convert_to_rgba(&src, &dst);
  CHECK(r == CRTMEDIA_OK, "crtmedia_frame_convert_to_rgba succeeds on a real dequeue_gpu_frame CPU-branch frame");
  if (r == CRTMEDIA_OK) {
    int saw_varying_pixel = 0;
    uint8_t first_r = rgba[0];
    for (uint32_t y = 0; y < frame->height && !saw_varying_pixel; ++y) {
      for (uint32_t x = 0; x < frame->width; ++x) {
        if (rgba[(size_t)y * stride + (size_t)x * 4u] != first_r) {
          saw_varying_pixel = 1;
          break;
        }
      }
    }
    CHECK(saw_varying_pixel, "converted RGBA frame has real, non-uniform image content");
  }
  free(rgba);
  return failures == before;
}

static crtmedia_extractor* open_fixture_video_track(crtmedia_format** out_video_format, int* out_video_track) {
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds on the real MP4 fixture");
  if (extractor == NULL) {
    return NULL;
  }

  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  int video_track = -1;
  crtmedia_format* video_format = NULL;
  for (uint32_t i = 0; i < track_count; ++i) {
    crtmedia_format* format = NULL;
    if (crtmedia_extractor_track_format(extractor, i, &format) != CRTMEDIA_OK || format == NULL) {
      continue;
    }
    const char* mime = NULL;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    if (mime != NULL && strncmp(mime, "video/", 6) == 0 && video_track < 0) {
      video_track = (int)i;
      video_format = format;
    } else {
      crtmedia_format_release(format);
    }
  }
  CHECK(video_track >= 0, "a real video track was found");
  if (video_track < 0) {
    crtmedia_extractor_release(extractor);
    return NULL;
  }
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);
  *out_video_format = video_format;
  *out_video_track = video_track;
  return extractor;
}

// Decodes the whole fixture through crtmedia_codec_dequeue_gpu_frame(),
// counting frames and, for the *first* frame only, capturing whether it was
// delivered zero-copy (memory_kind == GPU) or CPU (checked/converted like
// hw_decode_test.c's own NV12 check). Mirrors that test's own drive loop
// exactly, swapped to the new dequeue function.
static void run_gpu_frame_decode(
    crtmedia_codec* video_codec, crtmedia_extractor* extractor, int video_track, uint32_t* out_frame_count,
    int* out_saw_gpu_frame, int* out_saw_cpu_frame, int* out_cpu_convert_ok) {
  uint32_t video_frame_count = 0;
  int extractor_eof = 0;
  int video_eof = 0;
  int saw_gpu_frame = 0;
  int saw_cpu_frame = 0;
  int cpu_convert_ok = -1;
  int checked_first_frame = 0;

  for (int iterations = 0; iterations < 20000 && !(extractor_eof && video_eof); ++iterations) {
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      crtmedia_result r = crtmedia_extractor_read_sample(extractor, &sample, &sample_eof);
      CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_read_sample succeeds");
      if (sample_eof) {
        extractor_eof = 1;
        crtmedia_codec_queue_input(
            video_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
      } else if ((int)sample.track_index == video_track) {
        crtmedia_result qr;
        for (;;) {
          qr = crtmedia_codec_queue_input(
              video_codec, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
          if (qr != CRTMEDIA_WOULD_BLOCK) {
            break;
          }
          crtmedia_gpu_frame frame;
          int frame_eof = 0;
          if (crtmedia_codec_dequeue_gpu_frame(video_codec, &frame, NULL, &frame_eof) == CRTMEDIA_OK && !frame_eof) {
            ++video_frame_count;
            crtmedia_gpu_frame_release(&frame);
          }
        }
        CHECK(qr == CRTMEDIA_OK, "crtmedia_codec_queue_input eventually succeeds");
        crtmedia_sample_release(&sample);
      } else {
        crtmedia_sample_release(&sample);
      }
    }

    for (;;) {
      crtmedia_gpu_frame frame;
      int frame_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_gpu_frame(video_codec, &frame, NULL, &frame_eof);
      if (dr == CRTMEDIA_WOULD_BLOCK) {
        break;
      }
      CHECK(dr == CRTMEDIA_OK, "crtmedia_codec_dequeue_gpu_frame succeeds");
      if (dr != CRTMEDIA_OK) {
        break;
      }
      if (frame_eof) {
        video_eof = 1;
        break;
      }
      CHECK(frame.width == 64 && frame.height == 64, "decoded video frame is 64x64");
      CHECK(frame.release != NULL, "decoded video frame owns its own storage");
      if (frame.memory_kind == CRTMEDIA_GPU_MEMORY_GPU) {
        saw_gpu_frame = 1;
        CHECK(frame.native_handle != NULL, "a real zero-copy frame has a non-null native_handle");
        CHECK(frame.plane_count == 0, "a real zero-copy frame has no CPU-addressable planes");
        CHECK(
            frame.format == CRTMEDIA_PIXEL_FORMAT_NV12,
            "a real zero-copy frame is reported as NV12, matching every real hardware H.264 decoder this project "
            "supports");
      } else {
        saw_cpu_frame = 1;
        CHECK(frame.native_handle == NULL, "a real CPU-branch frame has a null native_handle");
        CHECK(
            frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P || frame.format == CRTMEDIA_PIXEL_FORMAT_NV12,
            "decoded video frame is YUV420P (software) or NV12 (hardware CPU fallback)");
        CHECK(frame.plane_count == 2 || frame.plane_count == 3, "a real CPU-branch frame has real pixel planes");
        if (!checked_first_frame) {
          cpu_convert_ok = check_cpu_gpu_frame_convert(&frame);
          checked_first_frame = 1;
        }
      }
      ++video_frame_count;
      crtmedia_gpu_frame_release(&frame);
    }
  }

  CHECK(extractor_eof, "extractor eventually reports EOF");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(video_frame_count == 25, "decoded video frame count matches the fixture's real encoded frame count (25)");

  *out_frame_count = video_frame_count;
  *out_saw_gpu_frame = saw_gpu_frame;
  *out_saw_cpu_frame = saw_cpu_frame;
  *out_cpu_convert_ok = cpu_convert_ok;
}

// docs/crtmedia_zero_copy_decode_acceptance.md's own frozen acceptance
// gate: on a host with a real zero-copy path implemented, a hardware-
// preferring decoder must actually produce at least one memory_kind == GPU
// frame (the real point of this whole tranche), and the public/private
// zero-copy diagnostics must agree with that.
static void run_hardware_preferring_decode(void) {
  crtmedia_format* video_format = NULL;
  int video_track = -1;
  crtmedia_extractor* extractor = open_fixture_video_track(&video_format, &video_track);
  if (extractor == NULL) {
    return;
  }

  int32_t prefer_hw = 1;
  crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, prefer_hw);

  crtmedia_codec* video_codec = NULL;
  crtmedia_result r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds (real hardware or a graceful software fallback)");
  crtmedia_format_release(video_format);
  if (video_codec == NULL) {
    crtmedia_extractor_release(extractor);
    return;
  }

  uint32_t video_frame_count = 0;
  int saw_gpu_frame = 0;
  int saw_cpu_frame = 0;
  int cpu_convert_ok = -1;
  run_gpu_frame_decode(video_codec, extractor, video_track, &video_frame_count, &saw_gpu_frame, &saw_cpu_frame,
                       &cpu_convert_ok);

  int is_hardware = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds");

  crtmedia_codec_hw_diagnostics diagnostics;
  memset(&diagnostics, 0, sizeof(diagnostics));
  CHECK(crtmedia_codec_test_get_hw_diagnostics(video_codec, &diagnostics) == CRTMEDIA_OK,
        "crtmedia_codec_test_get_hw_diagnostics succeeds");
  CHECK(diagnostics.hw_frame_transferred == is_hardware,
        "private hw_frame_transferred diagnostic agrees with the public hardware_accelerated value");

  if (is_hardware) {
    CHECK(diagnostics.hw_zero_copy_delivered == CRTMEDIA_ZERO_COPY_EXPECTED,
          "hw_zero_copy_delivered matches this host's real, documented zero-copy support");
    if (CRTMEDIA_ZERO_COPY_EXPECTED) {
      CHECK(saw_gpu_frame, "a real hardware frame was actually delivered zero-copy on a host that supports it");
    } else {
      CHECK(!saw_gpu_frame, "no zero-copy frame is reported on a host with no real zero-copy path yet");
      CHECK(cpu_convert_ok != 0, "the hardware CPU-fallback path still produces real, convertible pixel data");
    }
  } else {
    CHECK(!saw_gpu_frame, "a software-only decode never reports a zero-copy frame");
  }

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);

  fprintf(
      stderr,
      "crtmedia_zero_copy_test: RESULT backend=%s hw_requested=%s hardware_accelerated=%s zero_copy_expected=%s "
      "zero_copy_delivered=%s saw_gpu_frame=%s saw_cpu_frame=%s frame_count=%u\n",
      CRTMEDIA_HW_BACKEND_NAME, diagnostics.hw_requested ? "yes" : "no", is_hardware ? "yes" : "no",
      CRTMEDIA_ZERO_COPY_EXPECTED ? "yes" : "no", diagnostics.hw_zero_copy_delivered ? "yes" : "no",
      saw_gpu_frame ? "yes" : "no", saw_cpu_frame ? "yes" : "no", video_frame_count);
}

// A software-only decoder (no PREFER_HARDWARE_DECODE) must never produce a
// zero-copy frame through crtmedia_codec_dequeue_gpu_frame(), on any host --
// the new API's own CPU-fallback branch is the only one it can ever take.
static void run_software_only_decode(void) {
  crtmedia_format* video_format = NULL;
  int video_track = -1;
  crtmedia_extractor* extractor = open_fixture_video_track(&video_format, &video_track);
  if (extractor == NULL) {
    return;
  }

  crtmedia_codec* video_codec = NULL;
  crtmedia_result r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds for a plain software-only decoder");
  crtmedia_format_release(video_format);
  if (video_codec == NULL) {
    crtmedia_extractor_release(extractor);
    return;
  }

  uint32_t video_frame_count = 0;
  int saw_gpu_frame = 0;
  int saw_cpu_frame = 0;
  int cpu_convert_ok = -1;
  run_gpu_frame_decode(video_codec, extractor, video_track, &video_frame_count, &saw_gpu_frame, &saw_cpu_frame,
                       &cpu_convert_ok);
  CHECK(!saw_gpu_frame, "a software-only decoder never produces a zero-copy frame through dequeue_gpu_frame");
  CHECK(saw_cpu_frame, "a software-only decoder produces real CPU frames through dequeue_gpu_frame");

  int is_hardware = 1;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds for a software-only decoder");
  CHECK(!is_hardware, "hardware_accelerated stays false for a decoder that never opted into hardware decode");

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);
}

int main(void) {
  run_hardware_preferring_decode();
  run_software_only_decode();

  if (failures != 0) {
    fprintf(stderr, "crtmedia_zero_copy_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_zero_copy_test: ok\n");
  return 0;
}
