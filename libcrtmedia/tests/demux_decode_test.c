// Real FFmpeg demux/decode coverage for crtmedia/demux.h -- opens a real,
// tiny, project-authored WAV fixture (libcrtmedia/assets/test_tone.wav,
// see that directory's own README.md) and checks real stream info and
// decoded PCM sample counts, not just that the calls succeed without
// crashing. Deterministic and headless: a local file, no real audio
// device, no network.
//
// CRTMEDIA_TEST_WAV_PATH is a compile-time -D define (libcrtmedia/
// CMakeLists.txt), an absolute path to the fixture -- matches this
// project's own established compile-time-test-asset-path convention
// (e.g. porting/tests/freetype_glyph_test.c's own CRT_TEST_FONT_PATH).

#include "crtmedia/demux.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef CRTMEDIA_TEST_WAV_PATH
#error "CRTMEDIA_TEST_WAV_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
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
  crtmedia_result r = crtmedia_demuxer_open(CRTMEDIA_TEST_WAV_PATH, &demuxer);
  CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_open succeeds on the real WAV fixture");
  CHECK(demuxer != NULL, "crtmedia_demuxer_open produces a real demuxer");
  if (demuxer == NULL) {
    fprintf(stderr, "crtmedia_demux_test: %d failure(s)\n", failures);
    return 1;
  }

  uint32_t stream_count = crtmedia_demuxer_stream_count(demuxer);
  CHECK(stream_count == 1, "WAV fixture has exactly one stream");

  crtmedia_stream_info info;
  r = crtmedia_demuxer_stream_info(demuxer, 0, &info);
  CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_stream_info succeeds");
  CHECK(info.type == CRTMEDIA_STREAM_AUDIO, "stream 0 is audio");
  CHECK(info.sample_rate == 8000, "stream 0 sample_rate == 8000");
  CHECK(info.channels == 1, "stream 0 channels == 1");

  /* test_tone.wav is 0.25s @ 8000 Hz mono -- exactly 2000 real PCM
   * samples, generated directly by libcrtmedia/assets/README.md's own
   * documented Python wave-module script, not approximated. */
  const uint32_t expected_total_samples = 2000;
  uint32_t total_samples = 0;
  int buffer_count = 0;
  int saw_eof = 0;

  for (int iterations = 0; iterations < 10000; ++iterations) {
    crtmedia_read_status status;
    uint32_t stream_index;
    crtmedia_audio_buffer buffer;
    r = crtmedia_demuxer_read(demuxer, &status, &stream_index, NULL, &buffer);
    CHECK(r == CRTMEDIA_OK, "crtmedia_demuxer_read succeeds");
    if (r != CRTMEDIA_OK) {
      break;
    }
    if (status == CRTMEDIA_READ_EOF) {
      saw_eof = 1;
      break;
    }
    CHECK(status == CRTMEDIA_READ_AUDIO_BUFFER, "every read from an audio-only file is an audio buffer");
    CHECK(stream_index == 0, "audio buffer comes from stream 0");
    CHECK(buffer.sample_rate == 8000, "decoded buffer sample_rate == 8000");
    CHECK(buffer.channels == 1, "decoded buffer channels == 1");
    CHECK(buffer.data != NULL, "decoded buffer has real data");
    CHECK(buffer.release != NULL, "decoded buffer owns its own storage");
    total_samples += buffer.frame_count;
    ++buffer_count;
    crtmedia_audio_buffer_release(&buffer);
  }

  CHECK(saw_eof, "demuxer eventually reports EOF");
  CHECK(buffer_count > 0, "at least one real audio buffer was decoded");
  CHECK(total_samples == expected_total_samples, "total decoded samples matches the fixture's real sample count");

  crtmedia_demuxer_close(demuxer);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_demux_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_demux_test: ok\n");
  return 0;
}
