/* crtmedia/player.h -- see that header's own top comment for the design
 * reasoning. Real host time comes from this project's own libc
 * clock_gettime(CLOCK_MONOTONIC, ...) -- already the real, cross-platform
 * (Linux/macOS/Windows) monotonic time source this whole codebase's own
 * threading/synchronization code already relies on, so no new per-host
 * time API is needed here at all. */

#include "crtmedia/player.h"

#include <stdlib.h>
#include <time.h>

struct crtmedia_player {
  crtmedia_player_state state;
  /* The master clock's own real value (media time, microseconds) at the
   * instant host_base_us was last set -- crtmedia_player_get_clock_us()
   * while PLAYING adds real elapsed host time since then; while paused/
   * idle/stopped, this alone *is* the answer (frozen). */
  int64_t clock_base_us;
  /* Real host CLOCK_MONOTONIC time (microseconds) matching the instant
   * clock_base_us was last set -- only meaningful while PLAYING. */
  int64_t host_base_us;
};

static int64_t host_now_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0; /* Real clock_gettime() failure is not a realistic case for
               * CLOCK_MONOTONIC on any host this project targets -- 0 is
               * a safe, bounded fallback (a single lost tick), not a
               * crash. */
  }
  return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

crtmedia_result crtmedia_player_create(crtmedia_player** out_player) {
  if (out_player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  crtmedia_player* player = (crtmedia_player*)calloc(1, sizeof(crtmedia_player));
  if (player == NULL) {
    return CRTMEDIA_ERROR_UNSUPPORTED;
  }
  player->state = CRTMEDIA_PLAYER_STATE_IDLE;
  *out_player = player;
  return CRTMEDIA_OK;
}

void crtmedia_player_release(crtmedia_player* player) {
  free(player);
}

crtmedia_result crtmedia_player_play(crtmedia_player* player) {
  if (player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (player->state == CRTMEDIA_PLAYER_STATE_PLAYING) {
    return CRTMEDIA_OK;
  }
  /* clock_base_us is deliberately left untouched here regardless of the
   * state being left -- it already holds the right starting point in
   * every case: 0 from crtmedia_player_create()'s own calloc() or a real
   * crtmedia_player_stop(), the exact frozen value from a real crtmedia_
   * player_pause(), or a real position from a pre-play() crtmedia_
   * player_seek() (crtmedia/player.h's own documented "a caller can
   * pre-seek to a real starting position before the first play() call"
   * contract -- a real bug, found and fixed here: an earlier version of
   * this function force-reset clock_base_us to 0 whenever leaving IDLE/
   * STOPPED, silently discarding exactly that pre-seek). */
  player->host_base_us = host_now_us();
  player->state = CRTMEDIA_PLAYER_STATE_PLAYING;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_player_pause(crtmedia_player* player) {
  if (player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (player->state != CRTMEDIA_PLAYER_STATE_PLAYING) {
    return CRTMEDIA_OK;
  }
  player->clock_base_us = crtmedia_player_get_clock_us(player);
  player->state = CRTMEDIA_PLAYER_STATE_PAUSED;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_player_stop(crtmedia_player* player) {
  if (player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  player->state = CRTMEDIA_PLAYER_STATE_STOPPED;
  player->clock_base_us = 0;
  player->host_base_us = 0;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_player_seek(crtmedia_player* player, int64_t position_us) {
  if (player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  player->clock_base_us = position_us;
  player->host_base_us = host_now_us();
  return CRTMEDIA_OK;
}

crtmedia_player_state crtmedia_player_get_state(const crtmedia_player* player) {
  return player != NULL ? player->state : CRTMEDIA_PLAYER_STATE_IDLE;
}

int64_t crtmedia_player_get_clock_us(const crtmedia_player* player) {
  if (player == NULL) {
    return 0;
  }
  if (player->state != CRTMEDIA_PLAYER_STATE_PLAYING) {
    return player->clock_base_us;
  }
  return player->clock_base_us + (host_now_us() - player->host_base_us);
}

crtmedia_result crtmedia_player_update_audio_clock(crtmedia_player* player, int64_t pts_us) {
  if (player == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  /* A real correction -- re-anchor exactly like crtmedia_player_seek(),
   * but deliberately not implemented by just calling that function: a
   * future revision might want this path to apply smoothing/limits
   * (never jump by more than some bounded amount per call, say) that a
   * real, deliberate crtmedia_player_seek() call should never be subject
   * to -- kept as its own, separate real implementation from the start
   * rather than an alias that would need un-sharing later. */
  player->clock_base_us = pts_us;
  player->host_base_us = host_now_us();
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_player_plan_video_frame(
    const crtmedia_player* player, int64_t frame_pts_us, crtmedia_player_video_action* out_action,
    int64_t* out_wait_us) {
  if (player == NULL || out_action == NULL || out_wait_us == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  int64_t clock_us = crtmedia_player_get_clock_us(player);
  int64_t diff_us = frame_pts_us - clock_us;
  *out_wait_us = 0;
  if (diff_us > 0) {
    *out_action = CRTMEDIA_PLAYER_VIDEO_WAIT;
    *out_wait_us = diff_us;
  } else if (-diff_us > CRTMEDIA_PLAYER_LATE_DROP_THRESHOLD_US) {
    *out_action = CRTMEDIA_PLAYER_VIDEO_DROP;
  } else {
    *out_action = CRTMEDIA_PLAYER_VIDEO_PRESENT_NOW;
  }
  return CRTMEDIA_OK;
}
