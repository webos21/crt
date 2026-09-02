// Real FFmpeg H.264/AAC/MP4 demux+decode coverage for crtmedia/demux.h --
// opens a real, tiny, project-authored MP4 fixture (libcrtmedia/assets/
// test_video.mp4, see that directory's own README.md) with both a video
// and an audio stream, and checks real per-frame geometry/timestamp
// ordering and decoded PCM sample totals, not just that the calls succeed
// without crashing. Deterministic and headless: a local file, no real
// display/audio device, no network.
//
// This is the "verify threaded H.264 decode" / "PTS/DTS/duration
// ordering" / "EOF drain/flush" coverage TODO.md's upper-runtime roadmap
// step 1 calls for -- src/demux.c explicitly requests a real multi-thread
// H.264 decode (thread_count = 2), so a correct, complete, in-order frame
// sequence here is real evidence FFmpeg's internal decode-thread pool
// cooperates correctly with this project's own Bionic-style pthread
// implementation, not just that single-threaded decode works.
//
// CRTMEDIA_TEST_VIDEO_PATH is a compile-time -D define (libcrtmedia/
// CMakeLists.txt), matching demux_decode_test.c's own CRTMEDIA_TEST_WAV_
// PATH convention.

#include "crtmedia/demux.h"

#include <stdio.h>
#include <stdlib.h>

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

int main(void) {
  crtmedia_demuxer* demuxer = NULL;
  crtmedia_result r = crtmedia_demuxer_open(CRTMEDIA_TEST_VIDEO_PATH, &demuxer);
  CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_open succeeds on the real MP4 fixture");
  CHECK(demuxer != NULL, "crtmedia_demuxer_open produces a real demuxer");
  if (demuxer == NULL) {
    fprintf(stderr, "crtmedia_demux_video_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t stream_count = crtmedia_demuxer_stream_count(demuxer);
  CHECK(stream_count == 2, "MP4 fixture has exactly one video and one audio stream");

  int video_stream = -1;
  int audio_stream = -1;
  for (uint32_t i = 0; i < stream_count; ++i) {
    crtmedia_stream_info info;
    r = crtmedia_demuxer_stream_info(demuxer, i, &info);
    CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_stream_info succeeds for every real stream");
    if (info.type == CRTMEDIA_STREAM_VIDEO) {
      video_stream = (int)i;
      CHECK(info.width == 64, "video stream width == 64");
      CHECK(info.height == 64, "video stream height == 64");
    } else if (info.type == CRTMEDIA_STREAM_AUDIO) {
      audio_stream = (int)i;
      CHECK(info.sample_rate == 44100, "audio stream sample_rate == 44100");
      CHECK(info.channels == 1, "audio stream channels == 1");
    }
  }
  CHECK(video_stream >= 0, "a real video stream was found");
  CHECK(audio_stream >= 0, "a real audio stream was found");

  // test_video.mp4 is 25 fps @ 1s, encoded with bframes=0 (see assets/
  // README.md) -- decode order equals presentation order, so every video
  // frame's own timestamp must be non-decreasing across the whole
  // sequence, with no reordering to account for.
  const uint32_t expected_video_frames = 25;
  uint32_t video_frame_count = 0;
  uint32_t audio_buffer_count = 0;
  uint32_t total_audio_samples = 0;
  int64_t last_video_pts = -1;
  int saw_eof = 0;

  for (int iterations = 0; iterations < 10000; ++iterations) {
    crtmedia_read_status status;
    uint32_t stream_index;
    crtmedia_frame video_frame;
    crtmedia_audio_buffer audio_buffer;
    r = crtmedia_demuxer_read(demuxer, &status, &stream_index, &video_frame, &audio_buffer);
    CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_read succeeds");
    if (r != CRTMEDIA_OK) {
      break;
    }
    if (status == CRTMEDIA_READ_EOF) {
      saw_eof = 1;
      break;
    }
    if (status == CRTMEDIA_READ_VIDEO_FRAME) {
      CHECK((int)stream_index == video_stream, "video output comes from the real video stream index");
      CHECK(video_frame.format == CRTMEDIA_PIXEL_FORMAT_YUV420P, "decoded video frame is YUV420P");
      CHECK(video_frame.width == 64 && video_frame.height == 64, "decoded video frame is 64x64");
      CHECK(video_frame.plane_count == 3, "YUV420P decodes to 3 planes");
      CHECK(video_frame.planes[0].data != NULL, "decoded video frame has real luma plane data");
      CHECK(video_frame.release != NULL, "decoded video frame owns its own storage");
      if (video_frame.timestamp_us != CRTMEDIA_FRAME_TIMESTAMP_NONE) {
        CHECK(video_frame.timestamp_us >= last_video_pts,
              "video frame timestamps are non-decreasing (no B-frame reordering in this fixture)");
        last_video_pts = video_frame.timestamp_us;
      }
      ++video_frame_count;
      crtmedia_frame_release(&video_frame);
    } else if (status == CRTMEDIA_READ_AUDIO_BUFFER) {
      CHECK((int)stream_index == audio_stream, "audio output comes from the real audio stream index");
      CHECK(audio_buffer.sample_rate == 44100, "decoded audio buffer sample_rate == 44100");
      CHECK(audio_buffer.channels == 1, "decoded audio buffer channels == 1");
      CHECK(audio_buffer.data != NULL, "decoded audio buffer has real data");
      CHECK(audio_buffer.release != NULL, "decoded audio buffer owns its own storage");
      total_audio_samples += audio_buffer.frame_count;
      ++audio_buffer_count;
      crtmedia_audio_buffer_release(&audio_buffer);
    } else {
      CHECK(0, "crtmedia_demuxer_read produced an unexpected status");
    }
  }

  CHECK(saw_eof, "demuxer eventually reports EOF (real EOF-drain/flush path)");
  CHECK(video_frame_count == expected_video_frames,
        "decoded video frame count matches the fixture's real encoded frame count (25)");
  CHECK(audio_buffer_count > 0, "at least one real audio buffer was decoded");
  // AAC's own encoder priming/padding (a real, standard part of the codec,
  // not a bug) means the exact decoded sample count is not a clean 44100 --
  // a wide, deliberately generous tolerance band, not an exact-match
  // assertion the way the WAV fixture's own pcm_s16le check can afford to
  // be (see demux_decode_test.c) -- checks real decode actually happened
  // for approximately the fixture's real 1-second duration, not that a
  // specific encoder's own priming convention holds exactly.
  CHECK(total_audio_samples > 30000 && total_audio_samples < 60000,
        "total decoded audio samples are in the real ~1-second range for a 44100 Hz stream");

  crtmedia_demuxer_close(demuxer);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_demux_video_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_demux_video_test: ok\n");
  return 0;
}
