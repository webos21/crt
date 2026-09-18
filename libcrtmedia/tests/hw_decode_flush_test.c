// "Hardware video decode" Tranche 2, item 6 (2026-09-18,
// docs/crtmedia_hardware_decode_acceptance.md): flush()/reuse coverage on
// real VideoToolbox hardware, the first real exercise of crtmedia_
// extractor_seek_to() in this project's own test suite. Deliberately a
// separate file from hw_decode_test.c (matching demux_decode_test.c's own
// sibling-file-per-scenario convention in this same directory) rather than
// growing that file's already-substantial main() further.
//
// Real, honest scope: decode the fixture once to observe a real hardware
// frame and let hardware_accelerated latch true, seek the extractor back
// to the beginning, flush the same codec instance, and decode a second
// time through the same instance -- proving crtmedia_codec_flush()'s own
// documented "discards buffered input/output and resets end-of-stream
// state" contract composes correctly with a live hardware decode session,
// and that the public hardware flag stays true across it (it answers
// "has this instance ever used hardware," not "did the last frame").
// Same graceful-software-fallback discipline as hw_decode_test.c: this
// test passes whether or not real hardware is available, but only checks
// the hardware-specific requirements when it was.

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include "codec_test_control.h"

#include <stdio.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                            \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                 \
  } while (0)

// Decodes every remaining sample on `video_track` through `video_codec`
// until both the extractor and the codec report EOF, matching hw_decode_
// test.c's own main loop shape. Fills in frame_count/last_pts/saw_nv12 so
// the caller can check pass-specific expectations (25 frames, monotonic
// timestamps starting fresh after a seek, at least one real hardware
// frame observed).
static void run_decode_pass(
    crtmedia_extractor* extractor, int video_track, crtmedia_codec* video_codec, uint32_t* out_frame_count,
    int64_t* out_last_pts, int* out_saw_nv12) {
  uint32_t frame_count = 0;
  int64_t last_pts = -1;
  int extractor_eof = 0;
  int video_eof = 0;
  int saw_nv12 = 0;

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
          crtmedia_frame frame;
          int frame_eof = 0;
          if (crtmedia_codec_dequeue_output(video_codec, &frame, NULL, &frame_eof) == CRTMEDIA_OK && !frame_eof) {
            ++frame_count;
            if (frame.format == CRTMEDIA_PIXEL_FORMAT_NV12) {
              saw_nv12 = 1;
            }
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
      CHECK(frame.width == 64 && frame.height == 64, "decoded video frame is 64x64");
      if (frame.timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
        CHECK(frame.timestamp_us >= last_pts, "video frame timestamps are non-decreasing within this pass");
        last_pts = frame.timestamp_us;
      }
      if (frame.format == CRTMEDIA_PIXEL_FORMAT_NV12) {
        saw_nv12 = 1;
      }
      ++frame_count;
      crtmedia_frame_release(&frame);
    }
  }

  CHECK(extractor_eof, "extractor eventually reports EOF for this pass");
  CHECK(video_eof, "video codec eventually drains to EOF for this pass");
  *out_frame_count = frame_count;
  *out_last_pts = last_pts;
  *out_saw_nv12 = saw_nv12;
}

int main(void) {
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds on the real MP4 fixture");
  if (extractor == NULL) {
    fprintf(stderr, "crtmedia_hw_decode_flush_test: %d failure(s)\n", failures);
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
    fprintf(stderr, "crtmedia_hw_decode_flush_test: %d failure(s)\n", failures);
    return 1;
  }
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);

  crtmedia_format_set_int32(video_format, CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE, 1);
  crtmedia_codec* video_codec = NULL;
  r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds (real hardware or a graceful software fallback)");
  crtmedia_format_release(video_format);
  if (video_codec == NULL) {
    crtmedia_extractor_release(extractor);
    fprintf(stderr, "crtmedia_hw_decode_flush_test: %d failure(s)\n", failures);
    return 1;
  }

  // Pass 1: decode the whole fixture once, exactly like hw_decode_test.c.
  uint32_t frame_count_1 = 0;
  int64_t last_pts_1 = -1;
  int saw_nv12_1 = 0;
  run_decode_pass(extractor, video_track, video_codec, &frame_count_1, &last_pts_1, &saw_nv12_1);
  CHECK(frame_count_1 == 25, "pass 1 decodes the fixture's real encoded frame count (25)");

  int is_hardware = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds after pass 1");
  if (is_hardware) {
    CHECK(saw_nv12_1, "pass 1 observed at least one real hardware-backed NV12 frame");
  }

  // Seek the extractor back to the beginning (this project's own first
  // real exercise of crtmedia_extractor_seek_to() -- see this file's own
  // top comment) and flush the same codec instance before decoding again.
  r = crtmedia_extractor_seek_to(extractor, 0);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_seek_to(0) succeeds on the real MP4 fixture");
  r = crtmedia_codec_flush(video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_flush succeeds");

  // Tranche 2's own explicit flush requirement: flush must not reset the
  // public hardware flag -- it answers whether this instance has ever
  // used hardware, not whether the immediately previous frame did.
  int is_hardware_after_flush = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware_after_flush) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds immediately after flush");
  CHECK(
      is_hardware_after_flush == is_hardware,
      "crtmedia_codec_flush does not reset hardware_accelerated");

  // Pass 2: decode the same, freshly-seeked fixture again through the
  // same codec instance. A real EOF-state reset (crtmedia_codec_flush's
  // own contract) and no stale pre-flush frame both show up the same real
  // way: exactly 25 fresh frames, with timestamps starting over correctly
  // (run_decode_pass's own last_pts starts at -1 again for this pass, so
  // "non-decreasing" is checked relative to pass 2's own first frame, not
  // pass 1's last one -- a stale carried-over frame would instead surface
  // as a wrong frame count or a timestamp regression inside this pass).
  uint32_t frame_count_2 = 0;
  int64_t last_pts_2 = -1;
  int saw_nv12_2 = 0;
  run_decode_pass(extractor, video_track, video_codec, &frame_count_2, &last_pts_2, &saw_nv12_2);
  CHECK(frame_count_2 == 25, "pass 2 (post-flush) decodes the same real encoded frame count (25) again");

  int is_hardware_after_pass2 = 0;
  CHECK(crtmedia_codec_is_hardware_accelerated(video_codec, &is_hardware_after_pass2) == CRTMEDIA_OK,
        "crtmedia_codec_is_hardware_accelerated succeeds after pass 2");
  if (is_hardware) {
    CHECK(saw_nv12_2, "pass 2 (post-flush) observes real hardware-backed frames again, not a stale/software one");
    CHECK(is_hardware_after_pass2, "hardware_accelerated remains true through the flush/reuse cycle");
  }

  crtmedia_codec_hw_diagnostics diagnostics;
  memset(&diagnostics, 0, sizeof(diagnostics));
  CHECK(crtmedia_codec_test_get_hw_diagnostics(video_codec, &diagnostics) == CRTMEDIA_OK,
        "crtmedia_codec_test_get_hw_diagnostics succeeds after flush/reuse");
  fprintf(
      stderr,
      "crtmedia_hw_decode_flush_test: RESULT backend_hw_requested=%s pass1_frames=%u pass1_hw=%s "
      "pass2_frames=%u pass2_hw=%s flush_preserved_flag=%s\n",
      diagnostics.hw_requested ? "yes" : "no", frame_count_1, is_hardware ? "yes" : "no", frame_count_2,
      is_hardware_after_pass2 ? "yes" : "no", (is_hardware_after_flush == is_hardware) ? "yes" : "no");

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_hw_decode_flush_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_hw_decode_flush_test: ok\n");
  return 0;
}
