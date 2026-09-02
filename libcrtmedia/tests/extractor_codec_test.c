// Real end-to-end coverage for docs/libcrtmedia_api_policy.md's decided
// core -- crtmedia_extractor (crtmedia/extractor.h) + crtmedia_codec
// (crtmedia/codec.h) driven together against the same real, tiny,
// project-authored MP4 fixture demux_decode_video_test.c already
// verifies through the older, still-supported crtmedia_demuxer_*
// convenience API (libcrtmedia/assets/test_video.mp4, see assets/
// README.md) -- proving the new async buffer-queue core decodes the
// exact same real content correctly, not just that it compiles.
//
// CRTMEDIA_TEST_VIDEO_PATH is a compile-time -D define (libcrtmedia/
// CMakeLists.txt), matching every other demux_decode_*_test.c's own
// convention.

#include "crtmedia/codec.h"
#include "crtmedia/extractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

// Drains every video frame currently ready from `codec` (stops at the
// first CRTMEDIA_WOULD_BLOCK -- "nothing more ready yet", not an error).
// Sets *eof once the codec itself reports end-of-stream.
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
    CHECK(frame.release != NULL, "decoded video frame owns its own storage");
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
    CHECK(buffer.data != NULL, "decoded audio buffer has real data");
    CHECK(buffer.release != NULL, "decoded audio buffer owns its own storage");
    *total_samples += buffer.frame_count;
    crtmedia_audio_buffer_release(&buffer);
  }
}

int main(void) {
  crtmedia_extractor* extractor = NULL;
  crtmedia_result r = crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor);
  CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_create succeeds on the real MP4 fixture");
  CHECK(extractor != NULL, "crtmedia_extractor_create produces a real extractor");
  if (extractor == NULL) {
    fprintf(stderr, "crtmedia_extractor_codec_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t track_count = crtmedia_extractor_track_count(extractor);
  CHECK(track_count == 2, "MP4 fixture has exactly one video and one audio track");

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
      int32_t width = 0;
      int32_t height = 0;
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_WIDTH, &width);
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_HEIGHT, &height);
      CHECK(width == 64, "video track format width == 64");
      CHECK(height == 64, "video track format height == 64");
      const void* csd = NULL;
      size_t csd_size = 0;
      CHECK(crtmedia_format_get_buffer(format, CRTMEDIA_FORMAT_KEY_CSD, &csd, &csd_size) == CRTMEDIA_OK && csd_size > 0,
            "video track format carries real H.264 SPS/PPS csd-0");
    } else if (mime != NULL && strncmp(mime, "audio/", 6) == 0) {
      audio_track = (int)i;
      audio_format = format;
      int32_t sample_rate = 0;
      int32_t channels = 0;
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_SAMPLE_RATE, &sample_rate);
      crtmedia_format_get_int32(format, CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT, &channels);
      CHECK(sample_rate == 44100, "audio track format sample_rate == 44100");
      CHECK(channels == 1, "audio track format channels == 1");
    } else {
      crtmedia_format_release(format);
    }
  }
  CHECK(video_track >= 0, "a real video track was found");
  CHECK(audio_track >= 0, "a real audio track was found");
  if (video_track < 0 || audio_track < 0) {
    crtmedia_extractor_release(extractor);
    fprintf(stderr, "crtmedia_extractor_codec_test: %d failure(s)\n", failures);
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
      CHECK(r == CRTMEDIA_OK, "crtmedia_extractor_read_sample succeeds");
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
          // Real backpressure -- drain this codec's own output to make
          // room, then retry the exact same sample (queue_input's own
          // documented contract: nothing was lost on CRTMEDIA_WOULD_BLOCK).
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

  CHECK(extractor_eof, "extractor eventually reports EOF");
  CHECK(video_eof, "video codec eventually drains to EOF");
  CHECK(audio_eof, "audio codec eventually drains to EOF");
  CHECK(video_frame_count == 25, "decoded video frame count matches the fixture's real encoded frame count (25)");
  CHECK(total_audio_samples > 30000 && total_audio_samples < 60000,
        "total decoded audio samples are in the real ~1-second range for a 44100 Hz stream");

  crtmedia_codec_release(video_codec);
  crtmedia_codec_release(audio_codec);
  crtmedia_extractor_release(extractor);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_extractor_codec_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_extractor_codec_test: ok\n");
  return 0;
}
