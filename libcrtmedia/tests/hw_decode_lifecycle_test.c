// "Hardware video decode" Tranche 2, item 7 (2026-09-18,
// docs/crtmedia_hardware_decode_acceptance.md): bounded repeated
// create/decode/release lifecycle coverage on real VideoToolbox hardware.
// A deliberately small, fixed iteration count (15, within the plan's own
// 10-20 range) -- this is a CTest correctness regression, not a stress
// benchmark; longer endurance/leak investigation stays out of routine
// CTest unless a real failure here shows it is actually needed.
//
// Exists specifically to catch two real classes of regression a single
// create/decode/release pass (hw_decode_test.c) cannot: a stale hardware
// context surviving decoder destruction and leaking into (or being
// reused incorrectly by) the next instance, and any intermittent hang in
// the codec-registration path -- this project's own real, previously-
// fixed pthread_once ABI-mismatch deadlock (2026-09-18, "Hardware video
// decode" Tranche 1) would have shown up as exactly this shape (the very
// first crtmedia_codec_create_decoder() call hanging, or an inconsistent
// hardware_accelerated result appearing only on a later iteration).
//
// Same graceful-software-fallback discipline as hw_decode_test.c: passes
// whether or not real hardware is available. This file requires every
// iteration to agree with the very first one on whether hardware ended up
// active, rather than hard-requiring hardware=true unconditionally --
// remains valid evidence on a host with no real GPU (every iteration
// software) while still catching a real regression that made hardware
// availability flicker across iterations on a host that does have one.

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "codec_test_control.h"

#include <stdio.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

#define CRTMEDIA_HW_LIFECYCLE_ITERATIONS 15

static int failures = 0;

#define CHECK(cond, msg)                                            \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                 \
  } while (0)

// Runs one full create-extractor -> create-decoder -> decode-to-EOS ->
// release-everything cycle. Returns 1 iff hardware_accelerated was true
// by the end of this cycle, 0 otherwise (software fallback or a real
// failure already recorded via CHECK).
static int run_one_lifecycle_iteration(int iteration) {
  int before_failures = failures;
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds");
  if (extractor == NULL) {
    return 0;
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
    return 0;
  }
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);
  crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, 1);

  crtmedia_codec* video_codec = NULL;
  r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds");
  crtmedia_format_release(video_format);
  if (video_codec == NULL) {
    crtmedia_extractor_release(extractor);
    return 0;
  }

  uint32_t frame_count = 0;
  int extractor_eof = 0;
  int video_eof = 0;

  for (int it = 0; it < 20000 && !(extractor_eof && video_eof); ++it) {
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
            ++frame_count;
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
      ++frame_count;
      crtmedia_frame_release(&frame);
    }
  }

  CHECK(extractor_eof, "extractor eventually reports EOF");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(frame_count == 25, "decoded video frame count matches the fixture's real encoded frame count (25)");

  int is_hardware = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds");

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);

  if (failures != before_failures) {
    fprintf(stderr, "crtmedia_hw_decode_lifecycle_test: iteration %d recorded a real failure above\n", iteration);
  }
  return is_hardware;
}

int main(void) {
  int first_iteration_hardware = -1;
  int hardware_iterations = 0;

  for (int i = 0; i < CRTMEDIA_HW_LIFECYCLE_ITERATIONS; ++i) {
    int is_hardware = run_one_lifecycle_iteration(i);
    if (first_iteration_hardware < 0) {
      first_iteration_hardware = is_hardware;
    } else {
      CHECK(
          is_hardware == first_iteration_hardware,
          "every lifecycle iteration agrees with the first on whether hardware decode was active "
          "(a flip would mean a stale hardware context leaked across create/release cycles)");
    }
    if (is_hardware) {
      ++hardware_iterations;
    }
  }

  fprintf(
      stderr,
      "crtmedia_hw_decode_lifecycle_test: RESULT iterations=%d hardware_iterations=%d\n",
      CRTMEDIA_HW_LIFECYCLE_ITERATIONS, hardware_iterations);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_hw_decode_lifecycle_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_hw_decode_lifecycle_test: ok\n");
  return 0;
}
