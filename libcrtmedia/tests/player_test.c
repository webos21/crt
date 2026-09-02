// Deterministic (within a generous real-time tolerance -- this test uses
// the real host clock on purpose, matching what crtmedia_player itself
// actually is) coverage for crtmedia/player.h: state transitions, the
// master clock's own real advance/freeze/reset/seek behavior, and the
// A/V sync frame-drop/wait decision. No host audio/video device needed.

#include "crtmedia/player.h"

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
  crtmedia_player* player = NULL;
  crtmedia_result r = crtmedia_player_create(&player);
  CHECK(r == CRTMEDIA_OK, "crtmedia_player_create succeeds");
  CHECK(player != NULL, "crtmedia_player_create produces a real player");
  if (player == NULL) {
    fprintf(stderr, "crtmedia_player_test: %d failure(s)\n", failures);
    return 1;
  }

  CHECK(crtmedia_player_get_state(player) == CRTMEDIA_PLAYER_STATE_IDLE, "a fresh player starts IDLE");
  CHECK(crtmedia_player_get_clock_us(player) == 0, "a fresh player's clock starts at 0");

  // Pre-seek before the first play() -- must take effect immediately,
  // even while IDLE (crtmedia/player.h's own documented contract).
  CHECK(crtmedia_player_seek(player, 5000000) == CRTMEDIA_OK, "seek while IDLE succeeds");
  CHECK(crtmedia_player_get_clock_us(player) == 5000000, "a pre-seek while IDLE takes effect immediately");

  // play(): clock should genuinely advance in real time from the seeked position.
  CHECK(crtmedia_player_play(player) == CRTMEDIA_OK, "play succeeds");
  CHECK(crtmedia_player_get_state(player) == CRTMEDIA_PLAYER_STATE_PLAYING, "state is PLAYING after play()");
  int64_t clock_a = crtmedia_player_get_clock_us(player);
  CHECK(clock_a >= 5000000, "clock resumes from the seeked position, not reset to 0");
  sleep_us(20000); // 20ms -- generous relative to real scheduler jitter
  int64_t clock_b = crtmedia_player_get_clock_us(player);
  CHECK(clock_b > clock_a, "clock genuinely advances in real time while PLAYING");
  CHECK(clock_b - clock_a >= 10000, "clock advances by a real, plausible amount (>=10ms after a 20ms real sleep)");

  // pause(): clock must freeze exactly, not merely slow down.
  CHECK(crtmedia_player_pause(player) == CRTMEDIA_OK, "pause succeeds");
  CHECK(crtmedia_player_get_state(player) == CRTMEDIA_PLAYER_STATE_PAUSED, "state is PAUSED after pause()");
  int64_t paused_clock = crtmedia_player_get_clock_us(player);
  sleep_us(20000);
  CHECK(crtmedia_player_get_clock_us(player) == paused_clock, "clock is genuinely frozen while PAUSED, not just slow");

  // play() again: resumes from the exact frozen position, not reset.
  CHECK(crtmedia_player_play(player) == CRTMEDIA_OK, "resuming play succeeds");
  CHECK(crtmedia_player_get_clock_us(player) >= paused_clock, "resumed clock continues from the paused position");

  // stop(): a real reset to 0, not merely "not playing".
  CHECK(crtmedia_player_stop(player) == CRTMEDIA_OK, "stop succeeds");
  CHECK(crtmedia_player_get_state(player) == CRTMEDIA_PLAYER_STATE_STOPPED, "state is STOPPED after stop()");
  CHECK(crtmedia_player_get_clock_us(player) == 0, "stop() resets the clock to 0");

  // update_audio_clock(): a real correction, takes effect immediately.
  CHECK(crtmedia_player_update_audio_clock(player, 9000000) == CRTMEDIA_OK, "update_audio_clock succeeds");
  CHECK(crtmedia_player_get_clock_us(player) == 9000000, "update_audio_clock re-anchors the clock immediately");

  // Null-argument handling.
  CHECK(crtmedia_player_play(NULL) == CRTMEDIA_ERROR_INVALID_ARGUMENT, "play(NULL) fails cleanly");
  CHECK(crtmedia_player_get_clock_us(NULL) == 0, "get_clock_us(NULL) returns 0, not a crash");
  CHECK(crtmedia_player_get_state(NULL) == CRTMEDIA_PLAYER_STATE_IDLE, "get_state(NULL) returns IDLE, not a crash");

  // crtmedia_player_plan_video_frame(): deterministic against a clock
  // just re-anchored via seek(), so real elapsed time since the anchor
  // is negligible and safely inside the assertions' own tolerance.
  CHECK(crtmedia_player_seek(player, 1000000) == CRTMEDIA_OK, "seek for the sync-decision checks succeeds");
  crtmedia_player_video_action action;
  int64_t wait_us = -1;

  // A frame well ahead of the clock -- must wait.
  CHECK(crtmedia_player_plan_video_frame(player, 1100000, &action, &wait_us) == CRTMEDIA_OK,
        "plan_video_frame succeeds (ahead case)");
  CHECK(action == CRTMEDIA_PLAYER_VIDEO_WAIT, "a frame ahead of the clock should wait");
  CHECK(wait_us > 0 && wait_us <= 100000, "the computed wait is a real, positive, plausible amount");

  // A frame right at (or a hair behind) the clock -- present now.
  CHECK(crtmedia_player_plan_video_frame(player, 1000000, &action, &wait_us) == CRTMEDIA_OK,
        "plan_video_frame succeeds (on-time case)");
  CHECK(action == CRTMEDIA_PLAYER_VIDEO_PRESENT_NOW, "a frame at the clock should present now");

  // A frame far behind the clock -- drop.
  CHECK(crtmedia_player_plan_video_frame(player, 1000000 - CRTMEDIA_PLAYER_LATE_DROP_THRESHOLD_US - 50000, &action,
                                          &wait_us) == CRTMEDIA_OK,
        "plan_video_frame succeeds (late case)");
  CHECK(action == CRTMEDIA_PLAYER_VIDEO_DROP, "a frame far behind the clock should be dropped");

  CHECK(crtmedia_player_plan_video_frame(NULL, 0, &action, &wait_us) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "plan_video_frame(NULL, ...) fails cleanly");

  crtmedia_player_release(player);
  crtmedia_player_release(NULL); // must be a real, safe no-op

  if (failures != 0) {
    fprintf(stderr, "crtmedia_player_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtmedia_player_test: ok\n");
  return 0;
}
