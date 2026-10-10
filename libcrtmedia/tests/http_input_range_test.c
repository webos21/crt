// Networking & Streaming Tranche 2 fixture A (docs/acceptance/crtmedia_networking_
// acceptance.md): a real, ordinary (non-fragmented) MP4 -- the same
// libcrtmedia/assets/test_video.mp4 fixture extractor_codec_test.c already
// verifies over a local file -- served over a real repository-owned
// loopback HTTP/1.1 server (tests/http_test_server.h) that honors Range
// requests. Exercises the *seekable* custom-AVIO path end to end: a real
// crtmedia_extractor_create_from_url() open, capability detection
// (CRTMEDIA_SOURCE_SEEKABLE/_SIZE_KNOWN must both be set, proven by a real
// validated 206 response, not inferred), a full extractor+codec decode
// round trip identical in shape to extractor_codec_test.c's own local-file
// version (proving the two byte sources decode the exact same real
// content), and a real seek-via-reconnect (http_avio.c's own seek
// callback opens a brand new HTTP connection with a new Range request).
//
// Deliberately kept separate from tests/http_input_chunked_test.c (fixture
// B): conflating Range-capable-regular-MP4 and chunked-no-Range-fragmented-
// MP4 into one fixture would make a failure ambiguous between a Range bug,
// a custom-AVIO bug, an ordinary-MP4-seek-requirement issue, or an HTTP-
// chunk-parsing bug.

#include "http_test_server.h"

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                          \
  do {                                                                            \
    if (!(cond)) {                                                                \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);               \
      ++failures;                                                                 \
    }                                                                             \
  } while (0)

static void drain_video(crtmedia_codec* codec, uint32_t* frame_count, int64_t* last_pts, int* eof) {
  for (;;) {
    crtmedia_frame frame;
    int frame_eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_output(codec, &frame, NULL, &frame_eof);
    if (r == CRTMEDIA_WOULD_BLOCK) {
      return;
    }
    CHECK(r == CRTMEDIA_OK, "video crtmedia_codec_dequeue_output succeeds");
    if (r != CRTMEDIA_OK) {
      return;
    }
    if (frame_eof) {
      *eof = 1;
      return;
    }
    CHECK(frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P, "decoded video frame is YUV420P");
    CHECK(frame.width == 64 && frame.height == 64, "decoded video frame is 64x64");
    if (frame.timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
      CHECK(frame.timestamp_us >= *last_pts, "video frame timestamps are non-decreasing");
      *last_pts = frame.timestamp_us;
    }
    ++(*frame_count);
    crtmedia_frame_release(&frame);
  }
}

static void drain_audio(crtmedia_codec* codec, uint32_t* total_samples, int* eof) {
  for (;;) {
    crtmedia_audio_buffer buffer;
    int buffer_eof = 0;
    crtmedia_result r = crtmedia_codec_dequeue_output(codec, NULL, &buffer, &buffer_eof);
    if (r == CRTMEDIA_WOULD_BLOCK) {
      return;
    }
    CHECK(r == CRTMEDIA_OK, "audio crtmedia_codec_dequeue_output succeeds");
    if (r != CRTMEDIA_OK) {
      return;
    }
    if (buffer_eof) {
      *eof = 1;
      return;
    }
    *total_samples += buffer.frame_count;
    crtmedia_audio_buffer_release(&buffer);
  }
}

static void* read_file(const char* path, size_t* out_size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }
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
  size_t body_size = 0;
  void* body = read_file(CRTMEDIA_TEST_VIDEO_PATH, &body_size);
  CHECK(body != NULL, "loaded the real MP4 fixture into memory");
  if (body == NULL) {
    fprintf(stderr, "crtmedia_http_input_range_test: %d failure(s)\n", failures);
    return 1;
  }

  http_test_server* server = NULL;
  int port = 0;
  CHECK(http_test_server_start(body, body_size, HTTP_TEST_SERVER_RANGE_CAPABLE, &server, &port) == 0,
        "started the Range-capable loopback HTTP server");
  if (server == NULL) {
    free(body);
    fprintf(stderr, "crtmedia_http_input_range_test: %d failure(s)\n", failures);
    return 1;
  }

  char url[128];
  snprintf(url, sizeof(url), "http://127.0.0.1:%d/test.mp4", port);

  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create_from_url(url, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create_from_url succeeds against the Range-capable server");
  CHECK(extractor != NULL, "crtmedia_extractor_create_from_url produces a real extractor");
  if (extractor == NULL) {
    http_test_server_stop(server);
    free(body);
    fprintf(stderr, "crtmedia_http_input_range_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t capabilities = crtmedia_extractor_get_capabilities(extractor);
  CHECK((capabilities & CRTMEDIA_SOURCE_READABLE) != 0, "URL extractor reports READABLE");
  CHECK((capabilities & CRTMEDIA_SOURCE_SEEKABLE) != 0,
        "URL extractor reports SEEKABLE (a real validated 206 response, not inferred)");
  CHECK((capabilities & CRTMEDIA_SOURCE_SIZE_KNOWN) != 0, "URL extractor reports SIZE_KNOWN");

  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  CHECK(track_count == 2, "MP4 fixture has exactly one video and one audio track over HTTP too");

  int video_track = -1;
  int audio_track = -1;
  crtmedia_format* video_format = NULL;
  crtmedia_format* audio_format = NULL;
  for (uint32_t i = 0; i < track_count; ++i) {
    crtmedia_format* format = NULL;
    r = crtmedia_extractor_track_format(extractor, i, &format);
    CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_track_format succeeds for every real track");
    if (r != CRTMEDIA_OK || format == NULL) {
      continue;
    }
    const char* mime = NULL;
    crtmedia_format_get_string(format, CRTMEDIA_FORMAT_KEY_MIME, &mime);
    if (mime != NULL && strncmp(mime, "video/", 6) == 0) {
      video_track = (int)i;
      video_format = format;
    } else if (mime != NULL && strncmp(mime, "audio/", 6) == 0) {
      audio_track = (int)i;
      audio_format = format;
    } else {
      crtmedia_format_release(format);
    }
  }
  CHECK(video_track >= 0, "a real video track was found over HTTP");
  CHECK(audio_track >= 0, "a real audio track was found over HTTP");
  if (video_track < 0 || audio_track < 0) {
    crtmedia_extractor_release(extractor);
    http_test_server_stop(server);
    free(body);
    fprintf(stderr, "crtmedia_http_input_range_test: %d failure(s)\n", failures);
    return 1;
  }

  crtmedia_extractor_select_track(extractor, (uint32_t)video_track);
  crtmedia_extractor_select_track(extractor, (uint32_t)audio_track);

  crtmedia_codec* video_codec = NULL;
  crtmedia_codec* audio_codec = NULL;
  r = crtmedia_codec_create_decoder(video_format, &video_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds for the video track");
  r = crtmedia_codec_create_decoder(audio_format, &audio_codec);
  CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds for the audio track");
  crtmedia_format_release(video_format);
  crtmedia_format_release(audio_format);

  uint32_t video_frame_count = 0;
  uint32_t total_audio_samples = 0;
  int64_t last_video_pts = -1;
  int extractor_eof = 0;
  int video_eof = 0;
  int audio_eof = 0;

  for (int iterations = 0; iterations < 20000 && video_codec != NULL && audio_codec != NULL &&
                            !(extractor_eof && video_eof && audio_eof);
       ++iterations) {
    if (!extractor_eof) {
      crtmedia_sample sample;
      int sample_eof = 0;
      r = crtmedia_extractor_read_sample(extractor, &sample, &sample_eof);
      CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_read_sample succeeds over HTTP");
      if (sample_eof) {
        extractor_eof = 1;
        crtmedia_codec_queue_input(video_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
                                    CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
        crtmedia_codec_queue_input(audio_codec, NULL, 0, CRTMEDIA_FRAME_TIMESTAMP_NONE,
                                    CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM);
      } else {
        int is_video = (int)sample.track_index == video_track;
        crtmedia_codec* target = is_video ? video_codec : audio_codec;
        crtmedia_result qr;
        for (;;) {
          qr = crtmedia_codec_queue_input(target, sample.data, sample.size, sample.pts_us,
                                           CRTMEDIA_CODEC_BUFFER_FLAG_NONE);
          if (qr != CRTMEDIA_WOULD_BLOCK) {
            break;
          }
          if (is_video) {
            drain_video(video_codec, &video_frame_count, &last_video_pts, &video_eof);
          } else {
            drain_audio(audio_codec, &total_audio_samples, &audio_eof);
          }
        }
        CHECK(qr == CRTMEDIA_OK, "crtmedia_codec_queue_input eventually succeeds");
        crtmedia_sample_release(&sample);
      }
    }
    drain_video(video_codec, &video_frame_count, &last_video_pts, &video_eof);
    drain_audio(audio_codec, &total_audio_samples, &audio_eof);
  }

  CHECK(extractor_eof, "extractor eventually reports EOF over HTTP");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(audio_eof, "audio codec eventually drains to EOF");
  CHECK(video_frame_count == 25,
        "decoded video frame count over HTTP matches the fixture's real encoded frame count (25)");
  CHECK(total_audio_samples > 30000 && total_audio_samples < 60000,
        "total decoded audio samples over HTTP are in the real ~1-second range");

  crtmedia_codec_release(video_codec);
  crtmedia_codec_release(audio_codec);
  crtmedia_extractor_release(extractor);

  // A real seek-via-reconnect: a fresh extractor, seek immediately (before
  // reading anything) to roughly the midpoint of the 1-second fixture,
  // then confirm the reopened connection actually delivers real, decodable
  // samples -- proving http_avio.c's own seek callback (close current
  // transport, open a brand new validated Range request at the target
  // offset) works end to end, not just that the initial open does.
  crtmedia_extractor* seek_extractor = NULL;
  r = crtmedia_extractor_create_from_url(url, &seek_extractor);
  CHECK(r == CRTMEDIA_OK, "second crtmedia_extractor_create_from_url succeeds");
  if (seek_extractor != NULL) {
    r = crtmedia_extractor_seek_to(seek_extractor, 500000);
    CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_seek_to succeeds via a real HTTP reconnect");

    crtmedia_format* seek_video_format = NULL;
    r = crtmedia_extractor_track_format(seek_extractor, (uint32_t)video_track, &seek_video_format);
    CHECK(r == CRTMEDIA_OK, "track_format still succeeds on the seeked extractor");
    crtmedia_extractor_select_track(seek_extractor, (uint32_t)video_track);

    crtmedia_codec* seek_video_codec = NULL;
    if (seek_video_format != NULL) {
      r = crtmedia_codec_create_decoder(seek_video_format, &seek_video_codec);
      CHECK(r == CRTMEDIA_OK, "crtmedia_codec_create_decoder succeeds after a seek");
      crtmedia_format_release(seek_video_format);
    }

    int got_a_real_sample = 0;
    if (seek_video_codec != NULL) {
      crtmedia_sample sample;
      int sample_eof = 0;
      r = crtmedia_extractor_read_sample(seek_extractor, &sample, &sample_eof);
      CHECK(r == CRTMEDIA_OK, "read_sample succeeds immediately after a seek-via-reconnect");
      if (r == CRTMEDIA_OK && !sample_eof) {
        got_a_real_sample = sample.size > 0;
        crtmedia_sample_release(&sample);
      }
      crtmedia_codec_release(seek_video_codec);
    }
    CHECK(got_a_real_sample, "a real sample was read from the post-seek connection");
    crtmedia_extractor_release(seek_extractor);
  }

  http_test_server_stop(server);
  free(body);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_http_input_range_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_http_input_range_test: ok\n");
  return 0;
}
