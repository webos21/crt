// Real coverage for crtmedia/audio_sink.h's Windows (WASAPI) backend --
// argument validation, then a real open/write/get_position/close cycle
// against the real default host audio device when one is actually
// available. Matches crtgfx_window_smoke's own established graceful-
// degradation pattern (libcrtgfx/tests/window_smoke.c) for a headless/
// no-device environment: CRTMEDIA_ERROR_UNSUPPORTED from crtmedia_audio_
// sink_open() is treated as a real, valid, non-failing outcome here, not
// a test failure -- this project's own CI/WSL dev loop genuinely has no
// audio device at all.

#include "crtmedia/audio_sink.h"

#include <stdio.h>
#include <time.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                     \
  do {                                                                                        \
    if (!(cond)) {                                                                            \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                           \
      ++failures;                                                                             \
    }                                                                                          \
  } while (0)

static void sleep_us(long microseconds) {
  struct timespec ts;
  ts.tv_sec = microseconds / 1000000;
  ts.tv_nsec = (microseconds % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

int main(void) {
  crtmedia_audio_sink_desc desc;
  desc.format = CRTMEDIA_SAMPLE_FORMAT_S16;
  desc.sample_rate = 44100;
  desc.channels = 2;

  crtmedia_audio_sink* sink = NULL;

  // Argument validation -- no real device needed for any of these.
  CHECK(
      crtmedia_audio_sink_open(NULL, &sink) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "open(NULL desc, ...) fails cleanly");
  CHECK(
      crtmedia_audio_sink_open(&desc, NULL) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "open(..., NULL out_sink) fails cleanly");
  crtmedia_audio_sink_desc bad_rate = desc;
  bad_rate.sample_rate = 0;
  CHECK(
      crtmedia_audio_sink_open(&bad_rate, &sink) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "open() rejects a 0 sample_rate");
  crtmedia_audio_sink_desc bad_channels = desc;
  bad_channels.channels = 0;
  CHECK(
      crtmedia_audio_sink_open(&bad_channels, &sink) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "open() rejects 0 channels");
  crtmedia_audio_sink_desc bad_format = desc;
  bad_format.format = (crtmedia_sample_format)0;
  CHECK(
      crtmedia_audio_sink_open(&bad_format, &sink) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "open() rejects an unrecognized format");

  CHECK(
      crtmedia_audio_sink_write(NULL, "x", 1) == (int64_t)CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "write(NULL sink, ...) fails cleanly");
  uint64_t position = 0;
  CHECK(
      crtmedia_audio_sink_get_position_frames(NULL, &position) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "get_position_frames(NULL sink, ...) fails cleanly");
  CHECK(
      crtmedia_audio_sink_get_position_frames(NULL, NULL) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
      "get_position_frames(NULL, NULL) fails cleanly");

  crtmedia_audio_sink_close(NULL); // must be a real, safe no-op

  // The real open/write/close cycle -- CRTMEDIA_ERROR_UNSUPPORTED (no real
  // host audio device in this environment) is a real, accepted outcome,
  // not a failure; anything else must be CRTMEDIA_OK.
  sink = NULL;
  crtmedia_result open_result = crtmedia_audio_sink_open(&desc, &sink);
  CHECK(
      open_result == CRTMEDIA_OK || open_result == CRTMEDIA_ERROR_UNSUPPORTED,
      "open() with a valid desc either succeeds or reports CRTMEDIA_ERROR_UNSUPPORTED, nothing else");

  if (open_result == CRTMEDIA_OK) {
    CHECK(sink != NULL, "a successful open() produces a real sink");

    // 100ms of real interleaved S16 silence at 44100Hz/2ch.
    enum { kFrameCount = 4410 };
    static int16_t silence[kFrameCount * 2];
    for (int i = 0; i < kFrameCount * 2; ++i) {
      silence[i] = 0;
    }

    int64_t written = crtmedia_audio_sink_write(sink, silence, kFrameCount);
    CHECK(written >= 0, "write() of real silence does not report a device failure");
    CHECK((uint32_t)written <= kFrameCount, "write() never reports accepting more than it was given");

    uint64_t pos_after_first_write = 0;
    CHECK(
        crtmedia_audio_sink_get_position_frames(sink, &pos_after_first_write) == CRTMEDIA_OK,
        "get_position_frames() succeeds on a real, open sink");

    // A second write, after real playback has had a moment to advance --
    // exercises the real GetCurrentPadding()-paced backpressure path.
    sleep_us(20000);
    int64_t written2 = crtmedia_audio_sink_write(sink, silence, kFrameCount);
    CHECK(written2 >= 0, "a second write() also does not report a device failure");

    uint64_t pos_after_second_write = 0;
    CHECK(
        crtmedia_audio_sink_get_position_frames(sink, &pos_after_second_write) == CRTMEDIA_OK,
        "get_position_frames() still succeeds after a second write()");
    CHECK(
        pos_after_second_write >= pos_after_first_write,
        "real playback position never moves backwards between two get_position_frames() calls");

    // A few more real writes -- proves this is a real loop, not just a
    // one-off two-call path, while staying safely inside this project's
    // own actually-verified range: on real WSL, the WSLg PulseAudio
    // bridge's own real sink was confirmed (both through this backend and
    // independently through a real reference libpulse client) to stop
    // granting new write credit at all after roughly one real second of
    // continuously, tightly-written audio -- a genuine, external RDP-
    // audio-bridge limit on this exact dev machine, not something this
    // sink (or this test) can or should try to paper over. Half a real
    // second of additional silence (well under that real limit) is
    // enough to prove sustained writing works without chasing full
    // coverage of an external constraint outside this project's control.
    {
      enum { kExtraWrites = 4 }; // 4 * 100ms = 400ms more real audio
      int sustained_ok = 1;
      for (int i = 0; i < kExtraWrites; ++i) {
        int64_t n = crtmedia_audio_sink_write(sink, silence, kFrameCount);
        if (n < 0) {
          sustained_ok = 0;
          break;
        }
      }
      CHECK(sustained_ok, "a few more real writes in a row never report a device failure");
    }

    crtmedia_audio_sink_close(sink); // a real drain -- expected to block briefly, not hang
  }

  if (failures != 0) {
    fprintf(stderr, "crtmedia_audio_sink_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_audio_sink_test: ok\n");
  return 0;
}
