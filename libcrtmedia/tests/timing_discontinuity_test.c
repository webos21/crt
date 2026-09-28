/* Encode & Capture Tranche 6 acceptance: deterministic, irregularly spaced
 * synthetic PTS (including two real timeline gaps, as if frames were
 * dropped upstream) through software MPEG-4 encode -> MP4 mux -> reopen ->
 * decode-back. This freezes the timing half of Tranche 0's contract before
 * networking/transport work begins (TODO.md's runtime roadmap): a real
 * discontinuity must survive byte-for-byte, never get filled in with an
 * evenly-spaced synthetic frame and never get rounded away.
 *
 * This is possible to assert with *exact* equality, not just "close
 * enough", because of two real properties confirmed by reading the source
 * directly, not assumed: (1) libcrtmedia/src/codec.c's encoder keeps an
 * explicit encode_pts_queue FIFO of the caller's own submitted
 * frame.timestamp_us and returns exactly that value as the encoded
 * sample's pts_us/dts_us -- it never derives output timestamps from
 * frame_rate cadence. (2) libcrtmedia/src/muxer.c sets the MP4 stream's own
 * time_base to AV_TIME_BASE_Q (1,000,000 Hz, i.e. microseconds), so
 * av_rescale_q() at both mux and demux time is an exact identity
 * conversion, not a lossy rescale to some coarser codec time_base. Neither
 * property is guaranteed by the public crtmedia_codec.h/muxer.h contract in
 * writing, but both are what Tranche 0's contract actually promises
 * ("Public timestamps are signed microseconds ... A backend must convert
 * its native clock at its boundary") and this test is what pins them down
 * as regression-checked behavior. */

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_WIDTH 64
#define FRAME_HEIGHT 48

#define CHECK(condition, message)                                       \
  do {                                                                   \
    if (!(condition)) {                                                  \
      fprintf(stderr, "crtmedia_timing_discontinuity_test: %s\n", (message)); \
      return 1;                                                          \
    }                                                                    \
  } while (0)

/* A normal ~30fps cadence (33333us) broken by two real gaps: one where a
 * single frame was dropped (a ~2x interval), one much larger (a ~10x
 * interval, e.g. a stall). Monotonically increasing throughout -- this
 * project's codec/mux layer promises exact timestamp passthrough, not
 * reordering or gap-filling. */
static const int64_t kTimestampsUs[] = {
    0,       33333,   66666,                 /* normal cadence */
    133332,  166665,                         /* one dropped frame (2x gap) */
    199998,  233331,  266664,  299997,       /* normal cadence resumes */
    333330,  366663,                         /* skipped: 400000..866662 */
    866663,  899996,  933329,  966662,       /* large stall (~10x gap), then resumes */
    999995,  1033328, 1066661, 1099994, 1133327,
};
#define FRAME_COUNT ((int)(sizeof(kTimestampsUs) / sizeof(kTimestampsUs[0])))

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
    /* Encoded-sample-level check: the mux layer must see exactly the
     * timestamp the encoder was fed, before mux/demux even happens. */
    if (*packet_count >= FRAME_COUNT || sample.pts_us != kTimestampsUs[*packet_count] ||
        sample.dts_us != kTimestampsUs[*packet_count]) {
      crtmedia_encoded_sample_release(&sample);
      return -1;
    }
    if (crtmedia_muxer_write_sample(muxer, track, &sample) != CRTMEDIA_OK) {
      crtmedia_encoded_sample_release(&sample);
      return -1;
    }
    ++*packet_count;
    crtmedia_encoded_sample_release(&sample);
  }
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
    /* The real assertion this whole test exists for: the decoded timestamp
     * exactly matches the original, discontinuous input sequence -- no
     * frame was synthesized to fill either gap, and no frame's real
     * timestamp was quantized away. */
    if (*decoded_count >= FRAME_COUNT || frame.timestamp_us != kTimestampsUs[*decoded_count] ||
        frame.width != FRAME_WIDTH || frame.height != FRAME_HEIGHT) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    ++*decoded_count;
    crtmedia_frame_release(&frame);
  }
}

int main(void) {
  const char* output_path = CRTMEDIA_TEST_TIMING_DISCONTINUITY_OUTPUT_PATH;
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
        y[row * FRAME_WIDTH + column] = (unsigned char)(32 + ((column + row + index * 3) % 180));
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
    frame.timestamp_us = kTimestampsUs[index];
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
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0, "drain encoder");
  }
  crtmedia_result encoder_eos_result;
  while ((encoder_eos_result = crtmedia_codec_queue_frame(
              encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) == CRTMEDIA_WOULD_BLOCK) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
          "drain before encoder EOS");
  }
  CHECK(encoder_eos_result == CRTMEDIA_OK, "queue encoder EOS");
  while (!encoder_eof) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0, "drain encoder EOS");
  }
  CHECK(encoded_packets == FRAME_COUNT, "encoder produced exactly one packet per input frame");
  CHECK(crtmedia_muxer_finish(muxer) == CRTMEDIA_OK, "finish MP4 muxer");
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  crtmedia_extractor* extractor = NULL;
  CHECK(crtmedia_extractor_create(output_path, &extractor) == CRTMEDIA_OK, "reopen generated MP4");
  crtmedia_format* decode_format = NULL;
  CHECK(crtmedia_extractor_track_format(extractor, 0, &decode_format) == CRTMEDIA_OK,
        "read generated track format");
  crtmedia_codec* decoder = NULL;
  CHECK(crtmedia_codec_create_decoder(decode_format, &decoder) == CRTMEDIA_OK, "create round-trip decoder");
  crtmedia_format_release(decode_format);
  CHECK(crtmedia_extractor_select_track(extractor, 0) == CRTMEDIA_OK, "select generated track");

  int decoded_count = 0;
  int decoder_eof = 0;
  int sample_index = 0;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    CHECK(crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) == CRTMEDIA_OK,
          "read generated sample");
    if (extractor_eof) {
      break;
    }
    /* Container-level check: the MP4 sample table itself preserved the
     * exact original timestamp, not just the in-memory encoded_sample. */
    CHECK(sample_index < FRAME_COUNT && sample.pts_us == kTimestampsUs[sample_index],
          "muxed sample PTS matches original discontinuous timeline");
    ++sample_index;
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
  CHECK(crtmedia_codec_queue_input(decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) == CRTMEDIA_OK,
        "queue decoder EOS");
  while (!decoder_eof) {
    CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain decoder EOS");
  }
  CHECK(decoded_count == FRAME_COUNT, "decoded exactly one frame per input frame, none synthesized");

  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  printf(
      "crtmedia_timing_discontinuity_test: ok frames=%d gap1_us=%lld gap2_us=%lld\n", decoded_count,
      (long long)(kTimestampsUs[3] - kTimestampsUs[2]), (long long)(kTimestampsUs[11] - kTimestampsUs[10]));
  return 0;
}
