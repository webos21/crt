// Networking & Streaming Tranche 2 fixture B (docs/acceptance/crtmedia_networking_
// acceptance.md): a real fragmented MP4 (CRTMEDIA_MUXER_OUTPUT_MPEG_4_
// FRAGMENTED -- moof/mdat pairs interleaved, no seeking needed to decode
// it forward) served over a real repository-owned loopback HTTP/1.1
// server (tests/http_test_server.h) that ignores any Range header and
// sends the body with real HTTP/1.1 chunked transfer-encoding. Exercises
// the *non-seekable* custom-AVIO path end to end: capability detection
// (CRTMEDIA_SOURCE_SEEKABLE must be absent, CRTMEDIA_SOURCE_READABLE must
// be present -- the one reliable signal this contract actually promises,
// see the capability check's own comment below for why a behavioral seek
// attempt is deliberately not also asserted) and a full extractor+codec
// decode round trip.
//
// Deliberately separate from tests/http_input_range_test.c (fixture A) --
// see that file's own top comment for why conflating the two would make a
// real failure ambiguous between several unrelated bug classes.
//
// The fragmented MP4 fixture is generated at test time (not checked in as
// a binary asset, unlike libcrtmedia/assets/test_video.mp4) using this
// project's own already-accepted software encoder -- the same synthetic-
// frame generation shape tests/encode_mux_test.c already proves, just
// muxed with CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED instead of the plain
// CRTMEDIA_MUXER_OUTPUT_MPEG_4 that test uses.

#include "http_test_server.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                          \
  do {                                                                            \
    if (!(cond)) {                                                                \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);               \
      ++failures;                                                                 \
    }                                                                             \
  } while (0)

#define FIXTURE_FRAME_COUNT 20
#define FIXTURE_WIDTH 64
#define FIXTURE_HEIGHT 48
#define FIXTURE_INTERVAL_US 33333

static int drain_encoder(crtmedia_codec* encoder, crtmedia_muxer* muxer, uint32_t track, int* eof, int* packet_count) {
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

// Generates a real fragmented MP4 fixture at `path` -- deterministic
// synthetic frames through the already-accepted software mp4v-es encoder,
// same shape as tests/encode_mux_test.c, muxed fragmented instead of plain.
static int generate_fragmented_fixture(const char* path) {
  crtmedia_format* input_format = NULL;
  if (crtmedia_format_create(&input_format) != CRTMEDIA_OK) return -1;
  if (crtmedia_format_set_string(input_format, CRTMEDIA_FORMAT_KEY_MIME, "video/mp4v-es") != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_WIDTH, FIXTURE_WIDTH) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_HEIGHT, FIXTURE_HEIGHT) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT, CRTMEDIA_PIXEL_FORMAT_YUV420P) !=
          CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, 30) != CRTMEDIA_OK ||
      crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 200000) != CRTMEDIA_OK) {
    crtmedia_format_release(input_format);
    return -1;
  }
  crtmedia_codec* encoder = NULL;
  if (crtmedia_codec_create_encoder(input_format, &encoder) != CRTMEDIA_OK) {
    crtmedia_format_release(input_format);
    return -1;
  }
  crtmedia_format_release(input_format);

  crtmedia_format* output_format = NULL;
  if (crtmedia_codec_get_output_format(encoder, &output_format) != CRTMEDIA_OK) {
    crtmedia_codec_release(encoder);
    return -1;
  }
  crtmedia_muxer* muxer = NULL;
  if (crtmedia_muxer_create(path, CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED, &muxer) != CRTMEDIA_OK) {
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

  unsigned char y[FIXTURE_WIDTH * FIXTURE_HEIGHT];
  unsigned char u[(FIXTURE_WIDTH / 2) * (FIXTURE_HEIGHT / 2)];
  unsigned char v[(FIXTURE_WIDTH / 2) * (FIXTURE_HEIGHT / 2)];
  int encoded_packets = 0;
  int encoder_eof = 0;
  int failed = 0;
  for (int index = 0; index < FIXTURE_FRAME_COUNT && !failed; ++index) {
    for (int row = 0; row < FIXTURE_HEIGHT; ++row) {
      for (int column = 0; column < FIXTURE_WIDTH; ++column) {
        y[row * FIXTURE_WIDTH + column] = (unsigned char)(32 + ((column + row + index * 3) % 180));
      }
    }
    memset(u, 96 + index % 16, sizeof(u));
    memset(v, 160 - index % 16, sizeof(v));
    crtmedia_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.format = CRTMEDIA_PIXEL_FORMAT_YUV420P;
    frame.width = FIXTURE_WIDTH;
    frame.height = FIXTURE_HEIGHT;
    frame.color_range = CRTMEDIA_COLOR_RANGE_LIMITED;
    frame.color_space = CRTMEDIA_COLOR_SPACE_BT709;
    frame.timestamp_us = (int64_t)index * FIXTURE_INTERVAL_US;
    frame.plane_count = 3;
    frame.planes[0] = (crtmedia_frame_plane){y, FIXTURE_WIDTH, FIXTURE_WIDTH, FIXTURE_HEIGHT};
    frame.planes[1] = (crtmedia_frame_plane){u, FIXTURE_WIDTH / 2, FIXTURE_WIDTH / 2, FIXTURE_HEIGHT / 2};
    frame.planes[2] = (crtmedia_frame_plane){v, FIXTURE_WIDTH / 2, FIXTURE_WIDTH / 2, FIXTURE_HEIGHT / 2};
    for (;;) {
      crtmedia_result r = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (r == CRTMEDIA_OK) break;
      if (r != CRTMEDIA_WOULD_BLOCK || drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) {
        failed = 1;
        break;
      }
    }
    if (!failed && drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) != 0) failed = 1;
  }
  if (!failed) {
    crtmedia_result eos_result;
    while ((eos_result = crtmedia_codec_queue_frame(encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) ==
           CRTMEDIA_WOULD_BLOCK) {
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
  int ok = !failed && encoded_packets == FIXTURE_FRAME_COUNT && crtmedia_muxer_finish(muxer) == CRTMEDIA_OK;
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);
  return ok ? 0 : -1;
}

static void drain_video(crtmedia_codec* codec, uint32_t* frame_count, int64_t* last_pts, int* eof) {
  for (;;) {
    crtmedia_frame frame;
    int frame_eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_output(codec, &frame, NULL, &frame_eof);
    if (r == CRTMEDIA_WOULD_BLOCK) return;
    CHECK(r == CRTMEDIA_OK, "video crtmedia_codec_dequeue_output succeeds");
    if (r != CRTMEDIA_OK) return;
    if (frame_eof) {
      *eof = 1;
      return;
    }
    CHECK(frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P, "decoded video frame is YUV420P");
    if (frame.timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
      CHECK(frame.timestamp_us >= *last_pts, "video frame timestamps are non-decreasing");
      *last_pts = frame.timestamp_us;
    }
    ++(*frame_count);
    crtmedia_frame_release(&frame);
  }
}

static void* read_file(const char* path, size_t* out_size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (size < 0) {
    fclose(file);
    return NULL;
  }
  void* data = malloc((size_t)size);
  if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *out_size = (size_t)size;
  return data;
}

int main(void) {
  const char* fixture_path = CRTMEDIA_TEST_HTTP_CHUNKED_FIXTURE_PATH;
  remove(fixture_path);
  CHECK(generate_fragmented_fixture(fixture_path) == 0, "generated a real fragmented MP4 fixture");

  size_t body_size = 0;
  void* body = read_file(fixture_path, &body_size);
  CHECK(body != NULL, "loaded the generated fragmented MP4 fixture into memory");
  remove(fixture_path);
  if (body == NULL) {
    fprintf(stderr, "crtmedia_http_input_chunked_test: %d failure(s)\n", failures);
    return 1;
  }

  http_test_server* server = NULL;
  int port = 0;
  CHECK(http_test_server_start(body, body_size, HTTP_TEST_SERVER_CHUNKED_NO_RANGE, &server, &port) == 0,
        "started the chunked/no-Range loopback HTTP server");
  if (server == NULL) {
    free(body);
    fprintf(stderr, "crtmedia_http_input_chunked_test: %d failure(s)\n", failures);
    return 1;
  }

  char url[128];
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/test.mp4", port);

  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create_from_url(url, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create_from_url succeeds against the chunked server");
  CHECK(extractor != NULL, "crtmedia_extractor_create_from_url produces a real extractor");
  if (extractor == NULL) {
    http_test_server_stop(server);
    free(body);
    fprintf(stderr, "crtmedia_http_input_chunked_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t capabilities = crtmedia_extractor_get_capabilities(extractor);
  CHECK((capabilities & CRTMEDIA_SOURCE_READABLE) != 0, "URL extractor reports READABLE");
  CHECK((capabilities & CRTMEDIA_SOURCE_SEEKABLE) == 0,
        "URL extractor does NOT report SEEKABLE -- the server ignored Range, a legitimate outcome");

  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  CHECK(track_count == 1, "fragmented fixture has exactly one video track");
  int video_track = track_count > 0 ? 0 : -1;
  CHECK(video_track >= 0, "a real video track was found over chunked HTTP");
  if (video_track < 0) {
    crtmedia_extractor_release(extractor);
    http_test_server_stop(server);
    free(body);
    fprintf(stderr, "crtmedia_http_input_chunked_test: %d failure(s)\n", failures);
    return 1;
  }

  crtmedia_format* video_format = NULL;
  r = crtmedia_extractor_track_format(extractor, (uint32_t)video_track, &video_format);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_track_format succeeds over chunked HTTP");
  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);

  crtmedia_codec* video_codec = NULL;
  if (video_format != NULL) {
    r = crtmedia_codec_create_decoder(video_format, &video_codec);
    CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds over chunked HTTP");
    crtmedia_format_release(video_format);
  }

  uint32_t video_frame_count = 0;
  int64_t last_video_pts = -1;
  int extractor_eof = 0;
  int video_eof = 0;

  for (int iterations = 0; iterations < 20000 && video_codec != NULL && !(extractor_eof && video_eof);
       ++iterations) {
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      r = crtmedia_extractor_read_sample(extractor, &sample, &sample_eof);
      CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_read_sample succeeds over chunked HTTP");
      if (sample_eof) {
        extractor_eof = 1;
        crtmedia_codec_queue_input(video_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
                                    CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
      } else {
        crtmedia_result qr;
        for (;;) {
          qr = crtmedia_codec_queue_input(video_codec, sample.data, sample.size, sample.pts_us,
                                           CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
          if (qr != CRTMEDIA_WOULD_BLOCK) break;
          drain_video(video_codec, &video_frame_count, &last_video_pts, &video_eof);
        }
        CHECK(qr == CRTMEDIA_OK, "crtmedia_codec_queue_input eventually succeeds");
        crtmedia_sample_release(&sample);
      }
    }
    drain_video(video_codec, &video_frame_count, &last_video_pts, &video_eof);
  }

  CHECK(extractor_eof, "extractor eventually reports EOF over chunked HTTP");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(video_frame_count == FIXTURE_FRAME_COUNT,
        "decoded video frame count over chunked HTTP matches the fixture's real encoded frame count");

  // The real, honest capability check this fixture exists for: a non-
  // seekable source must never claim otherwise (docs/acceptance/crtmedia_networking_
  // acceptance.md's own capability contract -- "no public API may imply
  // that a non-seekable stream supports arbitrary seek"), verified above
  // via CRTMEDIA_SOURCE_SEEKABLE right after open(). A behavioral seek
  // attempt was deliberately *not* added here as a second check: real,
  // confirmed FFmpeg behavior (found running this exact test) makes a
  // seek's own success/failure an unreliable signal on its own --
  // av_seek_frame() on a fragmented MP4 can report success without ever
  // touching the AVIOContext (it only resets internal fragment-tracking
  // state), and AVIOContext's own internal read-ahead buffer can silently
  // satisfy a small seek from already-buffered memory regardless of the
  // underlying transport's real seekability. Neither is a contract
  // violation -- the capability bit is the one reliable, documented
  // signal callers can act on, and it is correct.

  crtmedia_codec_release(video_codec);
  crtmedia_extractor_release(extractor);
  http_test_server_stop(server);
  free(body);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_http_input_chunked_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_http_input_chunked_test: ok\n");
  return 0;
}
