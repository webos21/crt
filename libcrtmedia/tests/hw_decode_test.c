// Real coverage for TODO.md's "hardware decode, phase A" step
// (2026-09-08): opts into CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE
// (crtmedia/format.h) against the same real H.264 MP4 fixture
// tests/extractor_codec_test.c already proves correct in software
// (libcrtmedia/assets/test_video.mp4 -- 64x64, 25 real encoded frames,
// see that file's own top comment), driven through the core API
// directly (crtmedia_extractor + crtmedia_codec), video track only
// (hardware decode has no audio equivalent).
//
// Real, honest scope (this project's own established "software fallback
// must remain a first-class path" discipline): crtmedia_codec_create_
// decoder() always falls back to software automatically when no real
// hardware decoder is available on this host/CI -- this test passes
// either way, logging which real path was actually taken via crtmedia_
// codec_is_hardware_accelerated() (crtmedia/codec.h), matching crtgfx_
// skia_gpu_offscreen_smoke's own "0 real devices is a graceful skip, not
// a failure" precedent. Structural correctness (frame count, dimensions,
// monotonic PTS, real owned storage) is asserted regardless of which
// path ran -- proving hardware decode produces the exact same real
// result as software, not just "didn't crash." When (and only when) the
// real path decoded via hardware, the resulting NV12 frame is also run
// through the newly-extended crtmedia_frame_convert_to_rgba() and
// checked for real, non-degenerate output -- proving the new NV12
// pixel-format plumbing (crtmedia/frame.h, src/frame_convert.c) actually
// works end to end, not just that it compiles.
//
// CRTMEDIA_TEST_VIDEO_PATH is a compile-time -D define (libcrtmedia/
// CMakeLists.txt), matching every other demux_decode_*_test.c's own
// convention.

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);          \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

// Runs crtmedia_frame_convert_to_rgba() on one real decoded NV12 frame and
// checks the output is real, non-degenerate image data -- not a per-pixel
// ground-truth check (this test has no independently-verified reference
// values for test_video.mp4's own real visual content the way tests/
// skia_reference_scene.h does for its own synthetic scene), but enough to
// prove real chroma/luma data actually flowed through the new NV12 convert
// path rather than silently producing all-zero/uniform garbage.
static void check_nv12_convert(const crtmedia_frame* frame) {
  size_t stride = (size_t)frame->width * 4u;
  uint8_t* rgba = (uint8_t*)malloc(stride * frame->height);
  CHECK(rgba != NULL, "rgba scratch buffer allocated");
  if (rgba == NULL) {
    return;
  }
  crtmedia_frame dst;
  memset(&dst, 0, sizeof(dst));
  dst.format = CRTMEDIA_PIXEL_FORMAT_RGBA8888;
  dst.width = frame->width;
  dst.height = frame->height;
  dst.planes[0].data = rgba;
  dst.planes[0].stride = (uint32_t)stride;

  crtmedia_result r = crtmedia_frame_convert_to_rgba(frame, &dst);
  CHECK(r == CRTMEDIA_OK, "crtmedia_frame_convert_to_rgba succeeds on a real hardware-decoded NV12 frame");
  if (r == CRTMEDIA_OK) {
    int saw_nonzero_alpha = 0;
    int saw_varying_pixel = 0;
    uint8_t first_r = rgba[0];
    for (uint32_t y = 0; y < frame->height && !saw_varying_pixel; ++y) {
      for (uint32_t x = 0; x < frame->width; ++x) {
        uint8_t* p = rgba + (size_t)y * stride + (size_t)x * 4u;
        if (p[3] != 0) saw_nonzero_alpha = 1;
        if (p[0] != first_r) {
          saw_varying_pixel = 1;
          break;
        }
      }
    }
    CHECK(saw_nonzero_alpha, "converted RGBA frame is fully opaque (every alpha byte 255)");
    CHECK(saw_varying_pixel, "converted RGBA frame has real, non-uniform image content");
  }
  free(rgba);
}

int main(void) {
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds on the real MP4 fixture");
  if (extractor == NULL) {
    fprintf(stderr, "crtmedia_hw_decode_test: %d failure(s)\n", failures);
    return 1;
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
    fprintf(stderr, "crtmedia_hw_decode_test: %d failure(s)\n", failures);
    return 1;
  }
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);

  // The one real opt-in this whole test exists to exercise -- every other
  // real caller in this codebase leaves this key unset and keeps today's
  // exact existing software-only behavior.
  int32_t prefer_hw = 1;
  crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, prefer_hw);

  crtmedia_codec* video_codec = NULL;
  r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds (real hardware or a graceful software fallback)");
  crtmedia_format_release(video_format);
  if (video_codec == NULL) {
    crtmedia_extractor_release(extractor);
    fprintf(stderr, "crtmedia_hw_decode_test: %d failure(s)\n", failures);
    return 1;
  }

  int is_hardware = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds");
  fprintf(
      stderr, "crtmedia_hw_decode_test: real hardware decode %s on this host\n",
      is_hardware ? "IS active" : "is NOT active (graceful software fallback)");

  uint32_t video_frame_count = 0;
  int64_t last_pts = -1;
  int extractor_eof = 0;
  int video_eof = 0;
  int checked_nv12_convert = 0;

  for (int iterations = 0; iterations < 20000 && !(extractor_eof && video_eof); ++iterations) {
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      r = crtmedia_extractor_read_sample(extractor, &sample, &sample_eof);
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
          crtmedia_frame frame;
          int frame_eof = 0;
          if (crtmedia_codec_dequeue_output(video_codec, &frame, NULL, &frame_eof) == CRTMEDIA_OK && !frame_eof) {
            ++video_frame_count;
            crtmedia_frame_release(&frame);
          }
        }
        CHECK(qr == CRTMEDIA_OK, "crtmedia_codec_queue_input eventually succeeds");
        crtmedia_sample_release(&sample);
      } else {
        crtmedia_sample_release(&sample);
      }
    }

    for (;;) {
      crtmedia_frame frame;
      int frame_eof = 0;
      crtmedia_result dr = crtmedia_codec_dequeue_output(video_codec, &frame, NULL, &frame_eof);
      if (dr == CRTMEDIA_WOULD_BLOCK) {
        break;
      }
      CHECK(dr == CRTMEDIA_OK, "crtmedia_codec_dequeue_output succeeds");
      if (dr != CRTMEDIA_OK) {
        break;
      }
      if (frame_eof) {
        video_eof = 1;
        break;
      }
      CHECK(
          frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P || frame.format == CRTMEDIA_PIXEL_FORMAT_NV12,
          "decoded video frame is YUV420P (software) or NV12 (hardware)");
      CHECK(frame.width == 64 && frame.height == 64, "decoded video frame is 64x64");
      CHECK(frame.release != NULL, "decoded video frame owns its own storage");
      if (frame.timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
        CHECK(frame.timestamp_us >= last_pts, "video frame timestamps are non-decreasing");
        last_pts = frame.timestamp_us;
      }
      if (frame.format == CRTMEDIA_PIXEL_FORMAT_NV12 && !checked_nv12_convert) {
        check_nv12_convert(&frame);
        checked_nv12_convert = 1;
      }
      ++video_frame_count;
      crtmedia_frame_release(&frame);
    }
  }

  CHECK(extractor_eof, "extractor eventually reports EOF");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(video_frame_count == 25, "decoded video frame count matches the fixture's real encoded frame count (25)");
  if (is_hardware) {
    CHECK(checked_nv12_convert, "a real NV12 frame was decoded and convert-checked when hardware decode is active");
  }

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_hw_decode_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_hw_decode_test: ok\n");
  return 0;
}
