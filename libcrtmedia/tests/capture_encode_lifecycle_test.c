/* Encode & Capture Tranche 6 acceptance: repeated create/destroy lifecycle
 * stress for both halves of this pipeline in one process -- real device
 * capture (open/start/dequeue/stop/release) and real hardware H.264 encode
 * (create/queue/drain/EOS/destroy) -- run back to back for 15 iterations,
 * each followed by a real mux -> reopen -> decode-back check.
 *
 * Modeled directly on the Windows zero-copy Tranche 5/6 lifecycle test
 * (libcrtgfx/tests/skia_media_lifecycle_test.cc) precedent: a *single* pass
 * through a device/encoder API rarely exercises real create/destroy
 * resource-lifetime bugs (that test's own history is a real
 * DXGI_ERROR_DEVICE_REMOVED found only on the *second* of 15 decoder
 * cycles, from a process-wide cache tied to the first device -- HISTORY.md,
 * 2026-09-28). This test is the crtmedia-side equivalent for capture
 * devices and hardware encoders instead of GPU decode textures.
 *
 * Host-generic, unmodified across all three hosts: crtmedia_capture_
 * enumerate()/open() and crtmedia_codec_create_encoder("video/avc") already
 * dispatch to whichever real backend (V4L2/VA-API, AVFoundation/
 * VideoToolbox, Media Foundation) this host's own recipe enables. Each half
 * is independently, honestly skippable (no camera; no hardware H.264
 * encoder) -- this test needs at least one of the two to be real hardware
 * evidence, matching every other Tranche 2-5 test's own skip discipline;
 * skipping both halves is the only case classified as a CTest skip. */

#include "crtmedia/capture.h"
#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 15
#define CAPTURE_FRAME_COUNT 30
#define HW_FRAME_COUNT 100
#define HW_FRAME_WIDTH 64
#define HW_FRAME_HEIGHT 48
#define HW_FRAME_INTERVAL_US 33333
#define DEQUEUE_TIMEOUT_MS 3000

#define CHECK(condition, message)                                          \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "crtmedia_capture_encode_lifecycle_test: %s\n", (message)); \
      return 1;                                                            \
    }                                                                      \
  } while (0)

static int drain_encoder(
    crtmedia_codec* encoder, crtmedia_muxer* muxer, uint32_t track, int* eof, int* packet_count) {
  for (;;) {
    crtmedia_encoded_sample sample;
    int sample_eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_encoded_output(encoder, &sample, &sample_eof);
    if (r == CRTMEDIA_WOULD_BLOCK) return 0;
    if (r != CRTMEDIA_OK) return -1;
    if (sample_eof) {
      *eof = 1;
      return 0;
    }
    if (crtmedia_muxer_write_sample(muxer, track, &sample) != CRTMEDIA_OK) {
      crtmedia_encoded_sample_release(&sample);
      return -1;
    }
    ++*packet_count;
    crtmedia_encoded_sample_release(&sample);
  }
}

static int drain_decoder(crtmedia_codec* decoder, int* eof, int* count, int64_t* last_pts) {
  for (;;) {
    crtmedia_frame frame;
    int frame_eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_output(decoder, &frame, NULL, &frame_eof);
    if (r == CRTMEDIA_WOULD_BLOCK) return 0;
    if (r != CRTMEDIA_OK) return -1;
    if (frame_eof) {
      *eof = 1;
      return 0;
    }
    if ((*count != 0 && frame.timestamp_us <= *last_pts) || frame.format != CRTMEDIA_PIXEL_FORMAT_YUV420P) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    *last_pts = frame.timestamp_us;
    ++*count;
    crtmedia_frame_release(&frame);
  }
}

/* One full real-device capture -> owned YUV420P -> software mp4v-es encode
 * -> MP4 mux -> reopen -> decode-back cycle. Returns 0 on a real pass, 1 on
 * a real "no usable capture device" skip, -1 on any check failure. Adds to
 * frames_captured, frames_released, and decoded_total on a pass. */
static int run_capture_cycle(
    const char* output_path, int64_t* frames_captured, int64_t* frames_released, int64_t* decoded_total) {
  crtmedia_capture_device_info devices[16];
  size_t device_count = 0;
  crtmedia_capture* capture = NULL;
  crtmedia_capture_config requested = {640, 480, 30, CRTMEDIA_PIXEL_FORMAT_YUV420P};
  crtmedia_capture_config actual;

  if (crtmedia_capture_enumerate(devices, 16, &device_count) != CRTMEDIA_OK) return -1;
  if (device_count == 0) return 1;
  size_t candidate_count = device_count < 16 ? device_count : 16;
  for (size_t index = 0; index < candidate_count && capture == NULL; ++index) {
    crtmedia_capture_open(devices[index].id, &requested, &capture, &actual);
  }
  if (capture == NULL) return 1;

  if (crtmedia_capture_start(capture) != CRTMEDIA_OK) {
    crtmedia_capture_release(capture);
    return -1;
  }

  remove(output_path);
  crtmedia_format* encoder_format = NULL;
  if (crtmedia_format_create(&encoder_format) != CRTMEDIA_OK ||
      crtmedia_format_set_string(encoder_format, CRTMEDIA_FORMAT_KEY_MIME, "video/mp4v-es") != CRTMEDIA_OK ||
      crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_WIDTH, (int32_t)actual.width) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_HEIGHT, (int32_t)actual.height) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, (int32_t)actual.frame_rate) !=
          CRTMEDIA_OK ||
      crtmedia_format_set_int32(
          encoder_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT, CRTMEDIA_PIXEL_FORMAT_YUV420P) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 1000000) != CRTMEDIA_OK) {
    if (encoder_format != NULL) crtmedia_format_release(encoder_format);
    crtmedia_capture_stop(capture);
    crtmedia_capture_release(capture);
    return -1;
  }
  crtmedia_codec* encoder = NULL;
  crtmedia_result encoder_create = crtmedia_codec_create_encoder(encoder_format, &encoder);
  crtmedia_format_release(encoder_format);
  if (encoder_create != CRTMEDIA_OK) {
    crtmedia_capture_stop(capture);
    crtmedia_capture_release(capture);
    return -1;
  }
  crtmedia_format* output_format = NULL;
  crtmedia_muxer* muxer = NULL;
  uint32_t track = 0;
  if (crtmedia_codec_get_output_format(encoder, &output_format) != CRTMEDIA_OK ||
      crtmedia_muxer_create(output_path, CRTMEDIA_MUXER_OUTPUT_MPEG_4, &muxer) != CRTMEDIA_OK ||
      crtmedia_muxer_add_track(muxer, output_format, &track) != CRTMEDIA_OK) {
    if (output_format != NULL) crtmedia_format_release(output_format);
    if (muxer != NULL) crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    crtmedia_capture_stop(capture);
    crtmedia_capture_release(capture);
    return -1;
  }
  crtmedia_format_release(output_format);
  if (crtmedia_muxer_start(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    crtmedia_capture_stop(capture);
    crtmedia_capture_release(capture);
    return -1;
  }

  int encoder_eof = 0;
  int encoded_packets = 0;
  int64_t last_capture_pts = -1;
  int failed = 0;
  for (int index = 0; index < CAPTURE_FRAME_COUNT && !failed; ++index) {
    crtmedia_frame frame;
    crtmedia_result r = crtmedia_capture_dequeue_frame(capture, DEQUEUE_TIMEOUT_MS, &frame);
    if (r != CRTMEDIA_OK || frame.timestamp_us <= last_capture_pts ||
        frame.format != CRTMEDIA_PIXEL_FORMAT_YUV420P || frame.release == NULL) {
      failed = 1;
      break;
    }
    last_capture_pts = frame.timestamp_us;
    ++*frames_captured;
    for (;;) {
      r = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    crtmedia_frame_release(&frame);
    ++*frames_released;
    if (!failed && drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) failed = 1;
  }
  CHECK(crtmedia_capture_stop(capture) == CRTMEDIA_OK, "stop capture");
  crtmedia_capture_release(capture);
  if (!failed) {
    crtmedia_result eos_result;
    while ((eos_result = crtmedia_codec_queue_frame(
                encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) == CRTMEDIA_WOULD_BLOCK) {
      if (drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    if (!failed && eos_result != CRTMEDIA_OK) failed = 1;
    while (!failed && !encoder_eof) {
      if (drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) failed = 1;
    }
  }
  if (failed || encoded_packets != CAPTURE_FRAME_COUNT || crtmedia_muxer_finish(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  crtmedia_extractor* extractor = NULL;
  crtmedia_format* decode_format = NULL;
  crtmedia_codec* decoder = NULL;
  if (crtmedia_extractor_create(output_path, &extractor) != CRTMEDIA_OK) return -1;
  if (crtmedia_extractor_track_format(extractor, 0, &decode_format) != CRTMEDIA_OK ||
      crtmedia_codec_create_decoder(decode_format, &decoder) != CRTMEDIA_OK) {
    if (decode_format != NULL) crtmedia_format_release(decode_format);
    crtmedia_extractor_release(extractor);
    return -1;
  }
  crtmedia_format_release(decode_format);
  if (crtmedia_extractor_select_track(extractor, 0) != CRTMEDIA_OK) {
    crtmedia_codec_release(decoder);
    crtmedia_extractor_release(extractor);
    return -1;
  }
  int decoded_count = 0;
  int decoder_eof = 0;
  int64_t last_decode_pts = -1;
  failed = 0;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    if (crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) != CRTMEDIA_OK) {
      failed = 1;
      break;
    }
    if (extractor_eof) break;
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_input(
          decoder, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_decoder(decoder, &decoder_eof, &decoded_count, &last_decode_pts) != 0) {
        failed = 1;
        break;
      }
    }
    crtmedia_sample_release(&sample);
    if (failed || drain_decoder(decoder, &decoder_eof, &decoded_count, &last_decode_pts) != 0) {
      failed = 1;
      break;
    }
  }
  if (!failed) {
    if (crtmedia_codec_queue_input(decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) != CRTMEDIA_OK) {
      failed = 1;
    }
    while (!failed && !decoder_eof) {
      if (drain_decoder(decoder, &decoder_eof, &decoded_count, &last_decode_pts) != 0) failed = 1;
    }
  }
  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  if (failed || decoded_count != CAPTURE_FRAME_COUNT) return -1;
  *decoded_total += decoded_count;
  return 0;
}

/* One full synthetic-frame -> real hardware H.264 encode -> MP4 mux ->
 * reopen -> decode-back cycle. Returns 0 on a real pass, 1 on a real "no
 * hardware H.264 encoder available" skip, -1 on any check failure. */
static int run_hw_encode_cycle(
    const char* output_path, int64_t* samples_encoded, int64_t* samples_released, int64_t* decoded_total) {
  crtmedia_format* input_format = NULL;
  if (crtmedia_format_create(&input_format) != CRTMEDIA_OK) return -1;
  if (crtmedia_format_set_string(input_format, CRTMEDIA_FORMAT_KEY_MIME, "video/avc") != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_WIDTH, HW_FRAME_WIDTH) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_HEIGHT, HW_FRAME_HEIGHT) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT, CRTMEDIA_PIXEL_FORMAT_YUV420P) !=
          CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, 30) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 1000000) != CRTMEDIA_OK) {
    crtmedia_format_release(input_format);
    return -1;
  }
  crtmedia_codec* encoder = NULL;
  crtmedia_result create_result = crtmedia_codec_create_encoder(input_format, &encoder);
  crtmedia_format_release(input_format);
  if (create_result != CRTMEDIA_OK) {
    return create_result == CRTMEDIA_ERROR_UNSUPPORTED ? 1 : -1;
  }

  remove(output_path);
  crtmedia_format* output_format = NULL;
  crtmedia_muxer* muxer = NULL;
  uint32_t track = 0;
  if (crtmedia_codec_get_output_format(encoder, &output_format) != CRTMEDIA_OK ||
      crtmedia_muxer_create(output_path, CRTMEDIA_MUXER_OUTPUT_MPEG_4, &muxer) != CRTMEDIA_OK ||
      crtmedia_muxer_add_track(muxer, output_format, &track) != CRTMEDIA_OK) {
    if (output_format != NULL) crtmedia_format_release(output_format);
    if (muxer != NULL) crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_format_release(output_format);
  if (crtmedia_muxer_start(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }

  unsigned char y[HW_FRAME_WIDTH * HW_FRAME_HEIGHT];
  unsigned char u[(HW_FRAME_WIDTH / 2) * (HW_FRAME_HEIGHT / 2)];
  unsigned char v[(HW_FRAME_WIDTH / 2) * (HW_FRAME_HEIGHT / 2)];
  int encoded_packets = 0;
  int encoder_eof = 0;
  int failed = 0;
  for (int index = 0; index < HW_FRAME_COUNT && !failed; ++index) {
    for (int row = 0; row < HW_FRAME_HEIGHT; ++row) {
      for (int column = 0; column < HW_FRAME_WIDTH; ++column) {
        y[row * HW_FRAME_WIDTH + column] = (unsigned char)(32 + ((column + row + index * 3) % 180));
      }
    }
    memset(u, 96 + index % 16, sizeof(u));
    memset(v, 160 - index % 16, sizeof(v));
    crtmedia_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
    frame.width = HW_FRAME_WIDTH;
    frame.height = HW_FRAME_HEIGHT;
    frame.color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
    frame.color_space = CRTMEDIA_COLOR_SPACE_BT709;
    frame.timestamp_us = (int64_t)index * HW_FRAME_INTERVAL_US;
    frame.plane_count = 3;
    frame.planes[0] = (crtmedia_frame_plane){y, HW_FRAME_WIDTH, HW_FRAME_WIDTH, HW_FRAME_HEIGHT};
    frame.planes[1] = (crtmedia_frame_plane){u, HW_FRAME_WIDTH / 2, HW_FRAME_WIDTH / 2, HW_FRAME_HEIGHT / 2};
    frame.planes[2] = (crtmedia_frame_plane){v, HW_FRAME_WIDTH / 2, HW_FRAME_WIDTH / 2, HW_FRAME_HEIGHT / 2};
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    ++*samples_released; /* queue_frame() copies planes before returning */
    if (!failed && drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) failed = 1;
  }
  if (!failed) {
    crtmedia_result eos_result;
    while ((eos_result = crtmedia_codec_queue_frame(
                encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) == CRTMEDIA_WOULD_BLOCK) {
      if (drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    if (!failed && eos_result != CRTMEDIA_OK) failed = 1;
    while (!failed && !encoder_eof) {
      if (drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) failed = 1;
    }
  }
  if (failed || encoded_packets != HW_FRAME_COUNT || crtmedia_muxer_finish(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }
  *samples_encoded += encoded_packets;
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  crtmedia_extractor* extractor = NULL;
  crtmedia_format* decode_format = NULL;
  crtmedia_codec* decoder = NULL;
  if (crtmedia_extractor_create(output_path, &extractor) != CRTMEDIA_OK) return -1;
  if (crtmedia_extractor_track_format(extractor, 0, &decode_format) != CRTMEDIA_OK ||
      crtmedia_codec_create_decoder(decode_format, &decoder) != CRTMEDIA_OK) {
    if (decode_format != NULL) crtmedia_format_release(decode_format);
    crtmedia_extractor_release(extractor);
    return -1;
  }
  crtmedia_format_release(decode_format);
  if (crtmedia_extractor_select_track(extractor, 0) != CRTMEDIA_OK) {
    crtmedia_codec_release(decoder);
    crtmedia_extractor_release(extractor);
    return -1;
  }
  int decoded_count = 0;
  int decoder_eof = 0;
  int64_t last_pts = -1;
  failed = 0;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    if (crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) != CRTMEDIA_OK) {
      failed = 1;
      break;
    }
    if (extractor_eof) break;
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_input(
          decoder, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_decoder(decoder, &decoder_eof, &decoded_count, &last_pts) != 0) {
        failed = 1;
        break;
      }
    }
    crtmedia_sample_release(&sample);
    if (failed || drain_decoder(decoder, &decoder_eof, &decoded_count, &last_pts) != 0) {
      failed = 1;
      break;
    }
  }
  if (!failed) {
    if (crtmedia_codec_queue_input(decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) != CRTMEDIA_OK) {
      failed = 1;
    }
    while (!failed && !decoder_eof) {
      if (drain_decoder(decoder, &decoder_eof, &decoded_count, &last_pts) != 0) failed = 1;
    }
  }
  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  if (failed || decoded_count != HW_FRAME_COUNT) return -1;
  *decoded_total += decoded_count;
  return 0;
}

int main(void) {
  const char* capture_output_path = CRTMEDIA_TEST_LIFECYCLE_CAPTURE_OUTPUT_PATH;
  const char* hw_output_path = CRTMEDIA_TEST_LIFECYCLE_HW_OUTPUT_PATH;

  int64_t capture_frames = 0, capture_releases = 0, capture_decoded = 0;
  int64_t hw_samples = 0, hw_releases = 0, hw_decoded = 0;
  int capture_iterations = 0, hw_iterations = 0;
  int capture_skip = 0, hw_skip = 0;

  for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
    if (!capture_skip) {
      int r = run_capture_cycle(capture_output_path, &capture_frames, &capture_releases, &capture_decoded);
      CHECK(r >= 0, "capture lifecycle cycle");
      if (r == 1) {
        capture_skip = 1;
      } else {
        ++capture_iterations;
      }
    }
    if (!hw_skip) {
      int r = run_hw_encode_cycle(hw_output_path, &hw_samples, &hw_releases, &hw_decoded);
      CHECK(r >= 0, "hardware encode lifecycle cycle");
      if (r == 1) {
        hw_skip = 1;
      } else {
        ++hw_iterations;
      }
    }
  }

  if (capture_iterations == 0 && hw_iterations == 0) {
    printf("crtmedia_capture_encode_lifecycle_test: skip reason=no-capture-device-and-no-hw-encoder\n");
    return 0;
  }
  CHECK(capture_iterations == 0 || capture_iterations == ITERATIONS,
        "capture half completed either zero or all iterations");
  CHECK(hw_iterations == 0 || hw_iterations == ITERATIONS,
        "hardware encode half completed either zero or all iterations");
  CHECK(capture_iterations == 0 || capture_frames == (int64_t)capture_iterations * CAPTURE_FRAME_COUNT,
        "capture frame count matches iterations * per-cycle frames");
  CHECK(capture_iterations == 0 || capture_releases == capture_frames,
        "every captured frame was released exactly once");
  CHECK(hw_iterations == 0 || hw_samples == (int64_t)hw_iterations * HW_FRAME_COUNT,
        "hardware encoded sample count matches iterations * per-cycle frames");
  CHECK(hw_iterations == 0 || hw_releases == hw_samples,
        "every hardware-encoded input frame was released exactly once");

  printf(
      "crtmedia_capture_encode_lifecycle_test: ok capture_iterations=%d capture_frames=%lld "
      "capture_releases=%lld capture_decoded=%lld hw_iterations=%d hw_samples=%lld hw_releases=%lld "
      "hw_decoded=%lld\n",
      capture_iterations, (long long)capture_frames, (long long)capture_releases, (long long)capture_decoded,
      hw_iterations, (long long)hw_samples, (long long)hw_releases, (long long)hw_decoded);
  return 0;
}
