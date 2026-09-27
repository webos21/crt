/* Encode & Capture Tranche 1 acceptance: 100 deterministic CPU frames ->
 * software MPEG-4 encoder -> MP4 muxer -> extractor/decoder -> 100 frames.
 * This is a generated-at-test-time round trip, not a checked-in fixture. */

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_COUNT 100
#define FRAME_WIDTH 64
#define FRAME_HEIGHT 48
#define FRAME_INTERVAL_US 33333

#define CHECK(condition, message)                                      \
  do {                                                                 \
    if (!(condition)) {                                                \
      fprintf(stderr, "crtmedia_encode_mux_test: %s\n", (message));   \
      return 1;                                                        \
    }                                                                  \
  } while (0)

static int drain_encoder(
    crtmedia_codec* encoder, crtmedia_muxer* muxer, uint32_t track,
    int* out_eof, int* packet_count) {
  for (;;) {
    crtmedia_encoded_sample sample;
    int eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_encoded_output(encoder, &sample, &eof);
    if (r == CRTMEDIA_WOULD_BLOCK) {
      return 0;
    }
    if (r != CRTMEDIA_OK) {
      return -1;
    }
    if (eof) {
      *out_eof = 1;
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

static int check_luma_near(const crtmedia_frame* frame, int expected) {
  if (frame->format != CRTMEDIA_PIXEL_FORMAT_YUV420P || frame->plane_count < 3 ||
      frame->planes[0].data == NULL) {
    return 0;
  }
  int actual = ((const unsigned char*)frame->planes[0].data)[0];
  int delta = actual - expected;
  if (delta < 0) {
    delta = -delta;
  }
  return delta <= 25;
}

static int drain_decoder(crtmedia_codec* decoder, int* out_eof, int* decoded_count) {
  for (;;) {
    crtmedia_frame frame;
    int eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_output(decoder, &frame, NULL, &eof);
    if (r == CRTMEDIA_WOULD_BLOCK) {
      return 0;
    }
    if (r != CRTMEDIA_OK) {
      return -1;
    }
    if (eof) {
      *out_eof = 1;
      return 0;
    }
    int expected_index = *decoded_count;
    if (frame.timestamp_us != (int64_t)expected_index * FRAME_INTERVAL_US ||
        frame.width != FRAME_WIDTH || frame.height != FRAME_HEIGHT) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    if ((expected_index == 0 && !check_luma_near(&frame, 32)) ||
        (expected_index == FRAME_COUNT - 1 && !check_luma_near(&frame, 149))) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    ++*decoded_count;
    crtmedia_frame_release(&frame);
  }
}

int main(void) {
  const char* output_path = CRTMEDIA_TEST_ENCODE_OUTPUT_PATH;
  remove(output_path);

  crtmedia_format* input_format = NULL;
  CHECK(crtmedia_format_create(&input_format) == CRTMEDIA_OK, "create input format");
  CHECK(crtmedia_format_set_string(input_format, CRTMEDIA_FORMAT_KEY_MIME, "video/mp4v-es") == CRTMEDIA_OK,
        "set encoder mime");
  CHECK(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_WIDTH, FRAME_WIDTH) == CRTMEDIA_OK,
        "set width");
  CHECK(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_HEIGHT, FRAME_HEIGHT) == CRTMEDIA_OK,
        "set height");
  CHECK(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT,
                                  CRTMEDIA_PIXEL_FORMAT_YUV420P) == CRTMEDIA_OK,
        "set pixel format");
  CHECK(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, 30) == CRTMEDIA_OK,
        "set frame rate");
  CHECK(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 200000) == CRTMEDIA_OK,
        "set bitrate");

  crtmedia_codec* encoder = NULL;
  CHECK(crtmedia_codec_create_encoder(input_format, &encoder) == CRTMEDIA_OK, "create software encoder");
  crtmedia_format_release(input_format);

  crtmedia_format* output_format = NULL;
  CHECK(crtmedia_codec_get_output_format(encoder, &output_format) == CRTMEDIA_OK,
        "get encoder output format");
  crtmedia_muxer* muxer = NULL;
  CHECK(crtmedia_muxer_create(output_path, CRTMEDIA_MUXER_OUTPUT_MPEG_4, &muxer) == CRTMEDIA_OK,
        "create MP4 muxer");
  uint32_t track = 0;
  CHECK(crtmedia_muxer_add_track(muxer, output_format, &track) == CRTMEDIA_OK, "add video track");
  crtmedia_format_release(output_format);
  CHECK(crtmedia_muxer_start(muxer) == CRTMEDIA_OK, "start MP4 muxer");

  unsigned char y[FRAME_WIDTH * FRAME_HEIGHT];
  unsigned char u[(FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2)];
  unsigned char v[(FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2)];
  int encoded_packets = 0;
  int encoder_eof = 0;
  for (int index = 0; index < FRAME_COUNT; ++index) {
    for (int row = 0; row < FRAME_HEIGHT; ++row) {
      for (int column = 0; column < FRAME_WIDTH; ++column) {
        y[row * FRAME_WIDTH + column] =
            (unsigned char)(32 + ((column + row + index * 3) % 180));
      }
    }
    memset(u, 96 + index % 16, sizeof(u));
    memset(v, 160 - index % 16, sizeof(v));
    crtmedia_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
    frame.width = FRAME_WIDTH;
    frame.height = FRAME_HEIGHT;
    frame.color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
    frame.color_space = CRTMEDIA_COLOR_SPACE_BT709;
    frame.timestamp_us = (int64_t)index * FRAME_INTERVAL_US;
    frame.plane_count = 3;
    frame.planes[0] = (crtmedia_frame_plane){y, FRAME_WIDTH, FRAME_WIDTH, FRAME_HEIGHT};
    frame.planes[1] = (crtmedia_frame_plane){u, FRAME_WIDTH / 2, FRAME_WIDTH / 2, FRAME_HEIGHT / 2};
    frame.planes[2] = (crtmedia_frame_plane){v, FRAME_WIDTH / 2, FRAME_WIDTH / 2, FRAME_HEIGHT / 2};
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) {
        break;
      }
      CHECK(r == CRTMEDIA_WOULD_BLOCK, "queue synthetic frame");
      CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
            "drain encoder backpressure");
    }
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
          "drain encoder");
  }
  crtmedia_result encoder_eos_result;
  while ((encoder_eos_result = crtmedia_codec_queue_frame(
              encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) == CRTMEDIA_WOULD_BLOCK) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
          "drain before encoder EOS");
  }
  CHECK(encoder_eos_result == CRTMEDIA_OK, "queue encoder EOS");
  while (!encoder_eof) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
          "drain encoder EOS");
  }
  CHECK(encoded_packets == FRAME_COUNT, "encoder produced exactly 100 packets");
  CHECK(crtmedia_muxer_finish(muxer) == CRTMEDIA_OK, "finish MP4 muxer");
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  crtmedia_extractor* extractor = NULL;
  CHECK(crtmedia_extractor_create(output_path, &extractor) == CRTMEDIA_OK, "reopen generated MP4");
  CHECK(crtmedia_extractor_track_count(extractor) == 1, "generated MP4 has one track");
  crtmedia_format* decode_format = NULL;
  CHECK(crtmedia_extractor_track_format(extractor, 0, &decode_format) == CRTMEDIA_OK,
        "read generated track format");
  int64_t duration_us = 0;
  CHECK(crtmedia_format_get_int64(decode_format, CRTMEDIA_FORMAT_KEY_DURATION_US, &duration_us) == CRTMEDIA_OK,
        "generated track has duration");
  CHECK(duration_us == (int64_t)FRAME_COUNT * FRAME_INTERVAL_US, "generated duration matches 100 frames");
  crtmedia_codec* decoder = NULL;
  CHECK(crtmedia_codec_create_decoder(decode_format, &decoder) == CRTMEDIA_OK, "create round-trip decoder");
  crtmedia_format_release(decode_format);
  CHECK(crtmedia_extractor_select_track(extractor, 0) == CRTMEDIA_OK, "select generated track");

  int decoded_count = 0;
  int decoder_eof = 0;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    CHECK(crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) == CRTMEDIA_OK,
          "read generated sample");
    if (extractor_eof) {
      break;
    }
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_input(
          decoder, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) {
        break;
      }
      CHECK(r == CRTMEDIA_WOULD_BLOCK, "queue generated sample");
      CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain decoder backpressure");
    }
    crtmedia_sample_release(&sample);
    CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain round-trip decoder");
  }
  CHECK(crtmedia_codec_queue_input(
            decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) == CRTMEDIA_OK,
        "queue decoder EOS");
  while (!decoder_eof) {
    CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain decoder EOS");
  }
  CHECK(decoded_count == FRAME_COUNT, "decoded exactly 100 frames");

  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  printf("crtmedia_encode_mux_test: ok frames=%d duration_us=%lld\n",
         decoded_count, (long long)duration_us);
  return 0;
}
