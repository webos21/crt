/* Encode & Capture Tranche 4B acceptance (docs/acceptance/crtmedia_encode_capture_
 * acceptance.md): 100 deterministic CPU frames -> VideoToolbox H.264
 * hardware encoder -> MP4 muxer -> extractor/software decoder -> 100
 * frames, compared against the same round trip through the existing
 * mp4v-es software encoder (Tranche 1) run back to back on the identical
 * synthetic source. Deliberately a byte-for-byte structural mirror of
 * tests/encode_vaapi_test.c (Linux Tranche 3) -- same synthetic source,
 * same round-trip shape, same skip/pass contract -- so the two hardware
 * H.264 encode backends are held to the identical acceptance bar. A host
 * with no usable VideoToolbox H.264 encode session reports a precise
 * CTest skip -- crtmedia_codec_create_encoder("video/avc") fails honestly
 * rather than silently falling back to a different codec, so reaching
 * CRTMEDIA_OK from that call is itself the "actually used VideoToolbox,
 * not inferred" evidence this tranche's own acceptance gate requires. */

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRAME_COUNT 100
#define FRAME_WIDTH 64
#define FRAME_HEIGHT 48
#define FRAME_INTERVAL_US 33333

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "crtmedia_encode_videotoolbox_test: %s\n", (message));   \
      return 1;                                                                \
    }                                                                          \
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

static int drain_decoder(crtmedia_codec* decoder, int* out_eof, int* decoded_count, int64_t* last_pts) {
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
    if ((*decoded_count != 0 && frame.timestamp_us <= *last_pts) ||
        frame.format != CRTMEDIA_PIXEL_FORMAT_YUV420P || frame.width != FRAME_WIDTH ||
        frame.height != FRAME_HEIGHT) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    *last_pts = frame.timestamp_us;
    ++*decoded_count;
    crtmedia_frame_release(&frame);
  }
}

/* One full generate -> encode -> mux -> reopen -> decode-back round trip,
 * shared by both the VideoToolbox and the software-fallback comparison run
 * below so the two really do exercise the identical synthetic source and
 * gate. Returns 0 and fills out_decoded_count/out_encode_ms/out_decode_ms
 * on a real pass; returns 1 on a real, distinguishable "no VideoToolbox
 * H.264 encode available" skip; returns -1 on any check failure. */
static int run_round_trip(
    const char* mime, const char* output_path, int* out_decoded_count,
    double* out_encode_ms, double* out_decode_ms) {
  crtmedia_format* input_format = NULL;
  if (crtmedia_format_create(&input_format) != CRTMEDIA_OK) return -1;
  if (crtmedia_format_set_string(input_format, CRTMEDIA_FORMAT_KEY_MIME, mime) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_WIDTH, FRAME_WIDTH) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_HEIGHT, FRAME_HEIGHT) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT,
                                CRTMEDIA_PIXEL_FORMAT_YUV420P) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, 30) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 1000000) != CRTMEDIA_OK) {
    crtmedia_format_release(input_format);
    return -1;
  }

  crtmedia_codec* encoder = NULL;
  crtmedia_result create_result = crtmedia_codec_create_encoder(input_format, &encoder);
  crtmedia_format_release(input_format);
  if (create_result != CRTMEDIA_OK) {
    return create_result == CRTMEDIA_ERROR_UNSUPPORTED ? 1 /* real, distinguishable skip */ : -1;
  }

  remove(output_path);
  crtmedia_format* output_format = NULL;
  if (crtmedia_codec_get_output_format(encoder, &output_format) != CRTMEDIA_OK) {
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_muxer* muxer = NULL;
  if (crtmedia_muxer_create(output_path, CRTMEDIA_MUXER_OUTPUT_MPEG_4, &muxer) != CRTMEDIA_OK) {
    crtmedia_format_release(output_format);
    crtmedia_codec_release(encoder);
    return -1;
  }
  uint32_t track = 0;
  if (crtmedia_muxer_add_track(muxer, output_format, &track) != CRTMEDIA_OK) {
    crtmedia_format_release(output_format);
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_format_release(output_format);
  if (crtmedia_muxer_start(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }

  struct timespec encode_start, encode_end, decode_start, decode_end;
  clock_gettime(CLOCK_MONOTONIC, &encode_start);

  unsigned char y[FRAME_WIDTH * FRAME_HEIGHT];
  unsigned char u[(FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2)];
  unsigned char v[(FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2)];
  int encoded_packets = 0;
  int encoder_eof = 0;
  int failed = 0;
  for (int index = 0; index < FRAME_COUNT && !failed; ++index) {
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
    frame.timestamp_us = (int64_t)index * FRAME_INTERVAL_US;
    frame.plane_count = 3;
    frame.planes[0] = (crtmedia_frame_plane){y, FRAME_WIDTH, FRAME_WIDTH, FRAME_HEIGHT};
    frame.planes[1] = (crtmedia_frame_plane){u, FRAME_WIDTH / 2, FRAME_WIDTH / 2, FRAME_HEIGHT / 2};
    frame.planes[2] = (crtmedia_frame_plane){v, FRAME_WIDTH / 2, FRAME_WIDTH / 2, FRAME_HEIGHT / 2};
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    if (!failed && drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
      failed = 1;
    }
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
  clock_gettime(CLOCK_MONOTONIC, &encode_end);
  if (failed || encoded_packets != FRAME_COUNT || crtmedia_muxer_finish(muxer) != CRTMEDIA_OK) {
    crtmedia_muxer_release(muxer);
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  clock_gettime(CLOCK_MONOTONIC, &decode_start);
  crtmedia_extractor* extractor = NULL;
  if (crtmedia_extractor_create(output_path, &extractor) != CRTMEDIA_OK) return -1;
  crtmedia_format* decode_format = NULL;
  if (crtmedia_extractor_track_format(extractor, 0, &decode_format) != CRTMEDIA_OK) {
    crtmedia_extractor_release(extractor);
    return -1;
  }
  crtmedia_codec* decoder = NULL;
  /* No CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE: decode-back deliberately
   * stays on the plain software H.264 decoder, matching encode_vaapi_
   * test.c's own "verify what the muxer actually wrote" precedent,
   * independent of this host's own decode-side hardware-acceleration/
   * device-affinity story. */
  crtmedia_result decoder_create = crtmedia_codec_create_decoder(decode_format, &decoder);
  crtmedia_format_release(decode_format);
  if (decoder_create != CRTMEDIA_OK) {
    crtmedia_extractor_release(extractor);
    return -1;
  }
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
      if (r != CRTMEDIA_WOULD_BLOCK ||
          drain_decoder(decoder, &decoder_eof, &decoded_count, &last_pts) != 0) {
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
    if (crtmedia_codec_queue_input(decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) !=
        CRTMEDIA_OK) {
      failed = 1;
    }
    while (!failed && !decoder_eof) {
      if (drain_decoder(decoder, &decoder_eof, &decoded_count, &last_pts) != 0) failed = 1;
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &decode_end);
  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  if (failed || decoded_count != FRAME_COUNT) {
    return -1;
  }
  *out_decoded_count = decoded_count;
  *out_encode_ms = (encode_end.tv_sec - encode_start.tv_sec) * 1000.0 +
                   (encode_end.tv_nsec - encode_start.tv_nsec) / 1e6;
  *out_decode_ms = (decode_end.tv_sec - decode_start.tv_sec) * 1000.0 +
                   (decode_end.tv_nsec - decode_start.tv_nsec) / 1e6;
  return 0;
}

int main(void) {
  const char* videotoolbox_output_path = CRTMEDIA_TEST_ENCODE_VIDEOTOOLBOX_OUTPUT_PATH;
  const char* software_output_path = CRTMEDIA_TEST_ENCODE_VIDEOTOOLBOX_FALLBACK_OUTPUT_PATH;

  int videotoolbox_frames = 0;
  double videotoolbox_encode_ms = 0.0, videotoolbox_decode_ms = 0.0;
  int videotoolbox_result = run_round_trip(
      "video/avc", videotoolbox_output_path, &videotoolbox_frames, &videotoolbox_encode_ms,
      &videotoolbox_decode_ms);
  if (videotoolbox_result == 1) {
    printf("crtmedia_encode_videotoolbox_test: skip reason=no-videotoolbox-h264-encode-session\n");
    return 0;
  }
  CHECK(videotoolbox_result == 0, "VideoToolbox H.264 encode/mux/decode-back round trip");
  CHECK(videotoolbox_frames == FRAME_COUNT, "VideoToolbox round trip decoded exactly 100 frames");

  /* The software fallback comparison (docs/acceptance/crtmedia_encode_capture_
   * acceptance.md Tranche 4B's own "compare timing/ownership with the
   * software fallback" requirement, mirroring Tranche 3's own VA-API
   * test) -- the existing, already-accepted mp4v-es path (Tranche 1), run
   * back to back on the identical synthetic source and gate. This is
   * never expected to skip: it needs no VideoToolbox session at all. */
  int software_frames = 0;
  double software_encode_ms = 0.0, software_decode_ms = 0.0;
  int software_result = run_round_trip(
      "video/mp4v-es", software_output_path, &software_frames, &software_encode_ms, &software_decode_ms);
  CHECK(software_result == 0, "software mp4v-es fallback encode/mux/decode-back round trip");
  CHECK(software_frames == FRAME_COUNT, "software fallback round trip decoded exactly 100 frames");

  /* Reaching here on the VideoToolbox side only ever happens via crtmedia_
   * codec_create_encoder("video/avc") returning CRTMEDIA_OK, which only
   * this host's real h264_videotoolbox encoder path can produce (crtmedia_
   * codec.c has no other real or simulated way to satisfy that mime on
   * macOS) -- so "path=videotoolbox" below is the actually-used path, not
   * a capability inference. */
  printf(
      "crtmedia_encode_videotoolbox_test: ok path=videotoolbox frames=%d encode_ms=%.1f decode_ms=%.1f "
      "fallback_frames=%d fallback_encode_ms=%.1f fallback_decode_ms=%.1f\n",
      videotoolbox_frames, videotoolbox_encode_ms, videotoolbox_decode_ms, software_frames,
      software_encode_ms, software_decode_ms);
  return 0;
}
