/* Device-gated Tranche 2 acceptance. With a supported /dev/video* node this
 * exercises V4L2 mmap capture -> owned YUV420P -> software encode -> MP4 ->
 * extractor/decode-back. A host with no camera reports a precise CTest skip. */
#include "crtmedia/capture.h"
#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"
#include "crtmedia/muxer.h"

#include <stdio.h>
#include <stdlib.h>

#define LIVE_FRAME_COUNT 30
#define CHECK(condition, message)                                      \
  do {                                                                 \
    if (!(condition)) {                                                \
      fprintf(stderr, "crtmedia_capture_v4l2_test: %s\n", message);    \
      return 1;                                                        \
    }                                                                  \
  } while (0)

static int drain_encoder(crtmedia_codec* encoder, crtmedia_muxer* muxer,
                         uint32_t track, int* eof, int* packet_count) {
  for (;;) {
    crtmedia_encoded_sample sample;
    int sample_eof = 0;
    crtmedia_result result =
        crtmedia_codec_dequeue_encoded_output(encoder, &sample, &sample_eof);
    if (result == CRTMEDIA_WOULD_BLOCK) return 0;
    if (result != CRTMEDIA_OK) return -1;
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

static int drain_decoder(crtmedia_codec* decoder, int* eof, int* count,
                         int64_t* last_pts) {
  for (;;) {
    crtmedia_frame frame;
    int frame_eof = 0;
    crtmedia_result result =
        crtmedia_codec_dequeue_output(decoder, &frame, NULL, &frame_eof);
    if (result == CRTMEDIA_WOULD_BLOCK) return 0;
    if (result != CRTMEDIA_OK) return -1;
    if (frame_eof) {
      *eof = 1;
      return 0;
    }
    if ((*count != 0 && frame.timestamp_us <= *last_pts) ||
        frame.format != CRTMEDIA_PIXEL_FORMAT_YUV420P) {
      crtmedia_frame_release(&frame);
      return -1;
    }
    *last_pts = frame.timestamp_us;
    ++*count;
    crtmedia_frame_release(&frame);
  }
}

int main(void) {
  crtmedia_capture_device_info devices[16];
  size_t device_count = 0;
  size_t candidate_count;
  size_t index;
  size_t opened_device_index = 0;
  crtmedia_capture* capture = NULL;
  crtmedia_capture_config requested = {640, 480, 30, CRTMEDIA_PIXEL_FORMAT_YUV420P};
  crtmedia_capture_config actual;
  const char* output_path = CRTMEDIA_TEST_CAPTURE_OUTPUT_PATH;

  CHECK(crtmedia_capture_enumerate(devices, 16, &device_count) == CRTMEDIA_OK,
        "enumerate V4L2 devices");
  if (device_count == 0) {
    printf("crtmedia_capture_v4l2_test: skip reason=no-v4l2-device\n");
    return 0;
  }
  candidate_count = device_count < 16 ? device_count : 16;
  for (index = 0; index < candidate_count; ++index) {
    if (crtmedia_capture_open(devices[index].id, &requested, &capture, &actual) ==
        CRTMEDIA_OK) {
      opened_device_index = index;
      break;
    }
  }
  if (capture == NULL) {
    printf("crtmedia_capture_v4l2_test: skip reason=no-supported-yuv-streaming-device\n");
    return 0;
  }

  remove(output_path);
  crtmedia_format* encoder_format = NULL;
  CHECK(crtmedia_format_create(&encoder_format) == CRTMEDIA_OK, "create encoder format");
  CHECK(crtmedia_format_set_string(encoder_format, CRTMEDIA_FORMAT_KEY_MIME,
                                   "video/mp4v-es") == CRTMEDIA_OK, "set mime");
  CHECK(crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_WIDTH,
                                  (int32_t)actual.width) == CRTMEDIA_OK, "set width");
  CHECK(crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_HEIGHT,
                                  (int32_t)actual.height) == CRTMEDIA_OK, "set height");
  CHECK(crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_FRAME_RATE,
                                  (int32_t)actual.frame_rate) == CRTMEDIA_OK, "set rate");
  CHECK(crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_PIXEL_FORMAT,
                                  CRTMEDIA_PIXEL_FORMAT_YUV420P) == CRTMEDIA_OK, "set pixel format");
  CHECK(crtmedia_format_set_int32(encoder_format, CRTMEDIA_FORMAT_KEY_BIT_RATE,
                                  1000000) == CRTMEDIA_OK, "set bitrate");
  crtmedia_codec* encoder = NULL;
  CHECK(crtmedia_codec_create_encoder(encoder_format, &encoder) == CRTMEDIA_OK,
        "create software encoder");
  crtmedia_format_release(encoder_format);
  crtmedia_format* output_format = NULL;
  CHECK(crtmedia_codec_get_output_format(encoder, &output_format) == CRTMEDIA_OK,
        "get encoder output format");
  crtmedia_muxer* muxer = NULL;
  uint32_t track = 0;
  CHECK(crtmedia_muxer_create(output_path, CRTMEDIA_MUXER_OUTPUT_MPEG_4, &muxer) ==
        CRTMEDIA_OK, "create muxer");
  CHECK(crtmedia_muxer_add_track(muxer, output_format, &track) == CRTMEDIA_OK,
        "add muxer track");
  crtmedia_format_release(output_format);
  CHECK(crtmedia_muxer_start(muxer) == CRTMEDIA_OK, "start muxer");
  CHECK(crtmedia_capture_start(capture) == CRTMEDIA_OK, "start capture");

  int encoder_eof = 0;
  int encoded_packets = 0;
  int64_t last_capture_pts = -1;
  for (index = 0; index < LIVE_FRAME_COUNT; ++index) {
    crtmedia_frame frame;
    crtmedia_result result = crtmedia_capture_dequeue_frame(capture, 3000, &frame);
    CHECK(result == CRTMEDIA_OK, "dequeue live frame");
    CHECK(frame.timestamp_us > last_capture_pts &&
          frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P && frame.release != NULL,
          "capture ownership/PTS contract");
    last_capture_pts = frame.timestamp_us;
    for (;;) {
      result = crtmedia_codec_queue_frame(
          encoder, &frame, CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (result == CRTMEDIA_OK) break;
      CHECK(result == CRTMEDIA_WOULD_BLOCK, "queue captured frame");
      CHECK(drain_encoder(encoder, muxer, track, &encoder_eof,
                          &encoded_packets) == 0, "drain encoder backpressure");
    }
    /* Encoder copied the planes, proving the capture frame can be released
     * independently while the native V4L2 buffer was already requeued. */
    crtmedia_frame_release(&frame);
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof,
                        &encoded_packets) == 0, "drain encoder");
  }
  CHECK(crtmedia_capture_stop(capture) == CRTMEDIA_OK, "stop capture");
  crtmedia_capture_release(capture);
  crtmedia_result encoder_eos_result;
  while ((encoder_eos_result = crtmedia_codec_queue_frame(
              encoder, NULL, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM)) ==
         CRTMEDIA_WOULD_BLOCK) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof,
                        &encoded_packets) == 0, "drain before EOS");
  }
  CHECK(encoder_eos_result == CRTMEDIA_OK, "queue encoder EOS");
  while (!encoder_eof) {
    CHECK(drain_encoder(encoder, muxer, track, &encoder_eof,
                        &encoded_packets) == 0, "drain encoder EOS");
  }
  CHECK(encoded_packets == LIVE_FRAME_COUNT, "encoded frame count");
  CHECK(crtmedia_muxer_finish(muxer) == CRTMEDIA_OK, "finish muxer");
  crtmedia_muxer_release(muxer);
  crtmedia_codec_release(encoder);

  crtmedia_extractor* extractor = NULL;
  crtmedia_format* decode_format = NULL;
  crtmedia_codec* decoder = NULL;
  CHECK(crtmedia_extractor_create(output_path, &extractor) == CRTMEDIA_OK,
        "reopen capture MP4");
  CHECK(crtmedia_extractor_track_format(extractor, 0, &decode_format) == CRTMEDIA_OK,
        "read capture track");
  CHECK(crtmedia_codec_create_decoder(decode_format, &decoder) == CRTMEDIA_OK,
        "create decode-back codec");
  crtmedia_format_release(decode_format);
  CHECK(crtmedia_extractor_select_track(extractor, 0) == CRTMEDIA_OK,
        "select capture track");
  int decoded_count = 0;
  int decoder_eof = 0;
  int64_t last_decode_pts = -1;
  for (;;) {
    crtmedia_sample sample;
    int extractor_eof = 0;
    CHECK(crtmedia_extractor_read_sample(extractor, &sample, &extractor_eof) ==
          CRTMEDIA_OK, "read capture sample");
    if (extractor_eof) break;
    for (;;) {
      crtmedia_result result = crtmedia_codec_queue_input(
          decoder, sample.data, sample.size, sample.pts_us,
          CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
      if (result == CRTMEDIA_OK) break;
      CHECK(result == CRTMEDIA_WOULD_BLOCK, "queue decode-back sample");
      CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count,
                          &last_decode_pts) == 0, "drain decoder backpressure");
    }
    crtmedia_sample_release(&sample);
    CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count,
                        &last_decode_pts) == 0, "drain decoder");
  }
  CHECK(crtmedia_codec_queue_input(
            decoder, NULL, 0, 0, CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM) ==
        CRTMEDIA_OK, "queue decoder EOS");
  while (!decoder_eof) {
    CHECK(drain_decoder(decoder, &decoder_eof, &decoded_count,
                        &last_decode_pts) == 0, "drain decoder EOS");
  }
  CHECK(decoded_count == LIVE_FRAME_COUNT, "decoded frame count");
  crtmedia_codec_release(decoder);
  crtmedia_extractor_release(extractor);
  remove(output_path);
  printf("crtmedia_capture_v4l2_test: ok frames=%d device=%s size=%ux%u fps=%u\n",
         decoded_count, devices[opened_device_index].id, actual.width, actual.height,
         actual.frame_rate);
  return 0;
}
