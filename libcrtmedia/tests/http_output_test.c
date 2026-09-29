/* Networking & Streaming Tranche 3 acceptance:
 * deterministic software encode -> fragmented MP4 -> bounded HTTP PUT ->
 * slow repository-owned receiver -> reopen/decode-back. */

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"
#include "http_upload_test_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FRAME_COUNT 90
#define FRAME_WIDTH 160
#define FRAME_HEIGHT 120
#define FRAME_INTERVAL_US 33333
#define UPLOAD_QUEUE_CAPACITY 4096

static int contains_box(const unsigned char* data, size_t size, const char name[4]) {
  for (size_t i = 4; i + 4 <= size; ++i) {
    if (memcmp(data + i, name, 4) == 0) return 1;
  }
  return 0;
}

static int64_t monotonic_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int drain_encoder(
    crtmedia_codec* encoder, crtmedia_muxer* muxer, uint32_t track,
    int* out_eof, int* packet_count) {
  for (;;) {
    crtmedia_encoded_sample sample;
    int eof = 0;
    crtmedia_result result = crtmedia_codec_dequeue_encoded_output(encoder, &sample, &eof);
    if (result == CRTMEDIA_WOULD_BLOCK) return 0;
    if (result != CRTMEDIA_OK) return -1;
    if (eof) {
      *out_eof = 1;
      return 0;
    }
    result = crtmedia_muxer_write_sample(muxer, track, &sample);
    crtmedia_encoded_sample_release(&sample);
    if (result != CRTMEDIA_OK) return -1;
    ++*packet_count;
  }
}

static int drain_decoder(crtmedia_codec* decoder, int* out_eof, int* decoded_count) {
  for (;;) {
    crtmedia_frame frame;
    int eof = 0;
    crtmedia_result result = crtmedia_codec_dequeue_output(decoder, &frame, NULL, &eof);
    if (result == CRTMEDIA_WOULD_BLOCK) return 0;
    if (result != CRTMEDIA_OK) return -1;
    if (eof) {
      *out_eof = 1;
      return 0;
    }
    int index = *decoded_count;
    if (frame.timestamp_us != (int64_t)index * FRAME_INTERVAL_US ||
        frame.width != FRAME_WIDTH || frame.height != FRAME_HEIGHT ||
        frame.format != CRTMEDIA_PIXEL_FORMAT_YUV420P || frame.plane_count < 3) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    if (index == 0 || index == FRAME_COUNT - 1) {
      int expected = 32 + ((index * 3) % 180);
      int actual = ((const unsigned char*)frame.planes[0].data)[0];
      int delta = actual - expected;
      if (delta < 0) delta = -delta;
      if (delta > 35) {
        crtmedia_frame_release(&frame);
        return -1;
      }
    }
    ++*decoded_count;
    crtmedia_frame_release(&frame);
  }
}

int main(void) {
  int failed = 0;
  http_upload_test_server* server = NULL;
  crtmedia_format* input_format = NULL;
  crtmedia_format* output_format = NULL;
  crtmedia_codec* encoder = NULL;
  crtmedia_muxer* muxer = NULL;
  crtmedia_extractor* extractor = NULL;
  crtmedia_codec* decoder = NULL;
  unsigned char* y = NULL;
  unsigned char* u = NULL;
  unsigned char* v = NULL;
  void* uploaded_data = NULL;
  size_t uploaded_size = 0;
  int used_chunked = 0;
  const char* captured_path = CRTMEDIA_TEST_HTTP_OUTPUT_PATH;
  remove(captured_path);

#define REQUIRE(condition, message)                                    \
  do {                                                                 \
    if (!(condition)) {                                                \
      fprintf(stderr, "crtmedia_http_output_test: %s\n", (message)); \
      failed = 1;                                                      \
      goto cleanup;                                                    \
    }                                                                  \
  } while (0)

  int port = 0;
  REQUIRE(http_upload_test_server_start(&server, &port) == 0, "start slow loopback upload receiver");
  char url[128];
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/upload.mp4", port);

  REQUIRE(crtmedia_format_create(&input_format) == CRTMEDIA_OK, "create encoder format");
  REQUIRE(crtmedia_format_set_string(input_format, CRTMEDIA_FORMAT_KEY_MIME, "video/mp4v-es") == CRTMEDIA_OK,
          "set encoder mime");
  REQUIRE(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_WIDTH, FRAME_WIDTH) == CRTMEDIA_OK,
          "set width");
  REQUIRE(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_HEIGHT, FRAME_HEIGHT) == CRTMEDIA_OK,
          "set height");
  REQUIRE(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT,
                                    CRTMEDIA_PIXEL_FORMAT_YUV420P) == CRTMEDIA_OK,
          "set pixel format");
  REQUIRE(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE, 30) == CRTMEDIA_OK,
          "set frame rate");
  REQUIRE(crtmedia_format_set_int32(input_format, CRTMEDIA_FORMAT_KEY_BIT_RATE, 1000000) == CRTMEDIA_OK,
          "set bitrate");
  REQUIRE(crtmedia_codec_create_encoder(input_format, &encoder) == CRTMEDIA_OK, "create software encoder");
  crtmedia_format_release(input_format);
  input_format = NULL;
  REQUIRE(crtmedia_codec_get_output_format(encoder, &output_format) == CRTMEDIA_OK, "get encoder output format");
  REQUIRE(crtmedia_muxer_create_for_url(
              url, CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED, UPLOAD_QUEUE_CAPACITY, &muxer) == CRTMEDIA_OK,
          "create bounded HTTP fragmented-MP4 sink");
  REQUIRE(crtmedia_muxer_get_capabilities(muxer) == CRTMEDIA_SINK_WRITABLE,
          "HTTP sink is writable and honestly non-seekable/size-unknown");
  uint32_t track = 0;
  REQUIRE(crtmedia_muxer_add_track(muxer, output_format, &track) == CRTMEDIA_OK, "add stream track");
  crtmedia_format_release(output_format);
  output_format = NULL;
  REQUIRE(crtmedia_muxer_start(muxer) == CRTMEDIA_OK, "start fragmented MP4 stream");

  y = (unsigned char*)malloc(FRAME_WIDTH * FRAME_HEIGHT);
  u = (unsigned char*)malloc((FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2));
  v = (unsigned char*)malloc((FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2));
  REQUIRE(y != NULL && u != NULL && v != NULL, "allocate deterministic frame planes");
  int encoded_packets = 0;
  int encoder_eof = 0;
  int64_t started_ms = monotonic_ms();
  for (int index = 0; index < FRAME_COUNT; ++index) {
    for (int row = 0; row < FRAME_HEIGHT; ++row) {
      for (int column = 0; column < FRAME_WIDTH; ++column) {
        y[row * FRAME_WIDTH + column] =
            (unsigned char)(32 + ((column * 7 + row * 11 + index * 3) % 180));
      }
    }
    memset(u, 96 + index % 16, (FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2));
    memset(v, 160 - index % 16, (FRAME_WIDTH / 2) * (FRAME_HEIGHT / 2));
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
      crtmedia_result result = crtmedia_codec_queue_frame(encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (result == CRTMEDIA_OK) break;
      REQUIRE(result == CRTMEDIA_WOULD_BLOCK, "encoder applies explicit queue back-pressure");
      REQUIRE(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
              "drain encoder under upload back-pressure");
    }
    REQUIRE(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
            "stream encoded samples");
  }
  for (;;) {
    crtmedia_result result = crtmedia_codec_queue_frame(
        encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
    if (result == CRTMEDIA_OK) break;
    REQUIRE(result == CRTMEDIA_WOULD_BLOCK, "encoder EOS back-pressure");
    REQUIRE(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
            "drain before encoder EOS");
  }
  while (!encoder_eof) {
    REQUIRE(drain_encoder(encoder, muxer, track, &encoder_eof, &encoded_packets) == 0,
            "drain encoder EOS");
  }
  REQUIRE(encoded_packets == FRAME_COUNT, "encoder produced every sample exactly once");
  REQUIRE(crtmedia_muxer_finish(muxer) == CRTMEDIA_OK, "finish HTTP upload and receive 2xx");
  int64_t elapsed_ms = monotonic_ms() - started_ms;
  crtmedia_muxer_release(muxer);
  muxer = NULL;
  crtmedia_codec_release(encoder);
  encoder = NULL;
  free(y); y = NULL;
  free(u); u = NULL;
  free(v); v = NULL;

  REQUIRE(http_upload_test_server_wait_and_copy(
              server, &uploaded_data, &uploaded_size, &used_chunked) == 0,
          "receive exact uploaded body");
  REQUIRE(used_chunked, "HTTP request used forward-only chunked transfer encoding");
  REQUIRE(uploaded_size > UPLOAD_QUEUE_CAPACITY, "uploaded body exceeded the hard queue capacity");
  REQUIRE(elapsed_ms >= 30, "slow receiver exerted measurable end-to-end back-pressure");
  REQUIRE(contains_box((const unsigned char*)uploaded_data, uploaded_size, "ftyp") &&
              contains_box((const unsigned char*)uploaded_data, uploaded_size, "moof") &&
              contains_box((const unsigned char*)uploaded_data, uploaded_size, "mdat"),
          "uploaded body is a real fragmented MP4");

  FILE* captured = fopen(captured_path, "wb");
  REQUIRE(captured != NULL, "open captured upload for decode-back");
  REQUIRE(fwrite(uploaded_data, 1, uploaded_size, captured) == uploaded_size, "persist captured upload");
  REQUIRE(fclose(captured) == 0, "close captured upload");

  REQUIRE(crtmedia_extractor_create(captured_path, &extractor) == CRTMEDIA_OK, "reopen uploaded fragmented MP4");
  REQUIRE(crtmedia_extractor_track_count(extractor) == 1, "uploaded MP4 has one track");
  crtmedia_format* decode_format = NULL;
  REQUIRE(crtmedia_extractor_track_format(extractor, 0, &decode_format) == CRTMEDIA_OK,
          "read uploaded track format");
  REQUIRE(crtmedia_codec_create_decoder(decode_format, &decoder) == CRTMEDIA_OK,
          "create uploaded-stream decoder");
  crtmedia_format_release(decode_format);
  REQUIRE(crtmedia_extractor_select_track(extractor, 0) == CRTMEDIA_OK, "select uploaded track");
  int decoded_count = 0;
  int decoder_eof = 0;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    REQUIRE(crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) == CRTMEDIA_OK,
            "read uploaded sample");
    if (extractor_eof) break;
    for (;;) {
      crtmedia_result result = crtmedia_codec_queue_input(
          decoder, sample.data, sample.size, sample.pts_us, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (result == CRTMEDIA_OK) break;
      REQUIRE(result == CRTMEDIA_WOULD_BLOCK, "decoder input back-pressure");
      REQUIRE(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain decoder back-pressure");
    }
    crtmedia_sample_release(&sample);
    REQUIRE(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain uploaded sample");
  }
  REQUIRE(crtmedia_codec_queue_input(
              decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) == CRTMEDIA_OK,
          "queue uploaded decoder EOS");
  while (!decoder_eof) {
    REQUIRE(drain_decoder(decoder, &decoder_eof, &decoded_count) == 0, "drain uploaded decoder EOS");
  }
  REQUIRE(decoded_count == FRAME_COUNT, "decode-back produced every frame exactly once");
  printf("crtmedia_http_output_test: ok samples=%d frames=%d bytes=%zu queue_capacity=%d slow_receiver_ms=%lld\n",
         encoded_packets, decoded_count, uploaded_size, UPLOAD_QUEUE_CAPACITY, (long long)elapsed_ms);

cleanup:
  if (decoder != NULL) crtmedia_codec_release(decoder);
  if (extractor != NULL) crtmedia_extractor_release(extractor);
  if (muxer != NULL) crtmedia_muxer_release(muxer);
  if (encoder != NULL) crtmedia_codec_release(encoder);
  if (output_format != NULL) crtmedia_format_release(output_format);
  if (input_format != NULL) crtmedia_format_release(input_format);
  free(y);
  free(u);
  free(v);
  free(uploaded_data);
  http_upload_test_server_stop(server);
  remove(captured_path);
  return failed;
}
