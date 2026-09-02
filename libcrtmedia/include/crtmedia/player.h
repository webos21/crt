#pragma once

#include <stdint.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Software playback session (TODO.md's upper-runtime roadmap "Build the
 * software player" step): a real, monotonic master clock, play/pause/
 * stop/seek state, and the A/V synchronization decision a render loop
 * needs to decide what to do with each decoded video frame. Host-
 * independent -- no FFmpeg type, no host audio API type (`WASAPI`/
 * `CoreAudio`/`ALSA`/...), no `crtgfx` type -- matching this project's
 * established "no host/upstream SDK type in a public header" policy.
 * `crtmedia_audio_sink.h` (the real per-host audio output backend this
 * clock is meant to be driven by) and the actual demux/decode/render
 * loop that ties this, `crtmedia_extractor`/`crtmedia_codec`
 * (crtmedia/extractor.h, crtmedia/codec.h), an audio sink, and a caller-
 * owned render surface together are each their own, separate piece --
 * `crtmedia_player` itself only owns the clock and the play/pause/stop
 * state, not the actual media pipeline.
 *
 * Audio is this player's own reference clock once a real audio track is
 * present, matching real media players' own established practice
 * (audible glitches from clock drift are far more noticeable than an
 * occasional dropped/duplicated video frame) -- crtmedia_player_update_
 * audio_clock() is how whatever component is actually writing real
 * decoded audio to a host sink reports genuine playback position back
 * here. A video-only stream instead free-runs on real host wall-clock
 * time alone (never calling update_audio_clock() at all is a fully
 * supported, real use shape, not a degraded one). */

typedef struct crtmedia_player crtmedia_player;

typedef enum crtmedia_player_state {
  /* Never started, or explicitly crtmedia_player_stop()'d -- the master
   * clock reads 0 in both cases. */
  CRTMEDIA_PLAYER_STATE_IDLE = 0,
  CRTMEDIA_PLAYER_STATE_PLAYING = 1,
  /* Master clock is frozen at exactly the value it held the instant
   * crtmedia_player_pause() was called -- crtmedia_player_play() resumes
   * from there, unlike crtmedia_player_stop() which resets to 0. */
  CRTMEDIA_PLAYER_STATE_PAUSED = 2,
  CRTMEDIA_PLAYER_STATE_STOPPED = 3,
} crtmedia_player_state;

crtmedia_result crtmedia_player_create(crtmedia_player** out_player);
void crtmedia_player_release(crtmedia_player* player);

/* Starts or resumes playback -- from CRTMEDIA_PLAYER_STATE_IDLE/
 * _STOPPED, the master clock starts fresh at 0; from _PAUSED, it resumes
 * exactly where it was frozen. A no-op (not an error) if already
 * CRTMEDIA_PLAYER_STATE_PLAYING. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT
 * for a null player. */
crtmedia_result crtmedia_player_play(crtmedia_player* player);

/* Freezes the master clock at its current real value. A no-op (not an
 * error) unless the player is currently CRTMEDIA_PLAYER_STATE_PLAYING.
 * Same argument-validation contract as crtmedia_player_play(). */
crtmedia_result crtmedia_player_pause(crtmedia_player* player);

/* Stops playback and resets the master clock to 0 -- unlike
 * crtmedia_player_pause(), position is not preserved; a subsequent
 * crtmedia_player_play() starts over from 0. Valid (and a real state
 * change, not a no-op) from any state. Same argument-validation contract
 * as crtmedia_player_play(). */
crtmedia_result crtmedia_player_stop(crtmedia_player* player);

/* Re-anchors the master clock to `position_us` immediately, without
 * changing crtmedia_player_get_state()'s own current state (a real seek
 * while playing keeps playing from the new position; a seek while paused
 * stays paused there) -- this call only re-anchors the clock itself, the
 * caller is responsible for actually re-positioning whatever real
 * crtmedia_extractor/crtmedia_codec instances are feeding it (crtmedia_
 * extractor_seek_to(), crtmedia_codec_flush()). Same argument-validation
 * contract as crtmedia_player_play(). */
crtmedia_result crtmedia_player_seek(crtmedia_player* player, int64_t position_us);

crtmedia_player_state crtmedia_player_get_state(const crtmedia_player* player);

/* Real, monotonic master clock, in microseconds -- advances in real host
 * time while CRTMEDIA_PLAYER_STATE_PLAYING (corrected by any
 * crtmedia_player_update_audio_clock() call since play() started), and
 * otherwise holds steady at whatever value it was last anchored to: 0
 * initially or immediately after crtmedia_player_stop() (a real, positive
 * reset, not a special "not playing" sentinel), or wherever crtmedia_
 * player_seek()/crtmedia_player_update_audio_clock() last placed it --
 * both are meaningful and take effect regardless of the current
 * crtmedia_player_get_state(), including CRTMEDIA_PLAYER_STATE_IDLE/
 * _STOPPED, so a caller can pre-seek to a real starting position before
 * the first crtmedia_player_play() call. 0 for a null player. */
int64_t crtmedia_player_get_clock_us(const crtmedia_player* player);

/* Reports genuine playback position (pts_us) real audio hardware has
 * actually reached, immediately re-anchoring the master clock to it --
 * this is what actually paces crtmedia_player_get_clock_us() during real
 * playback with an audio track (see this file's own top comment on why
 * audio is the reference clock, not video). Meaningful regardless of
 * crtmedia_player_get_state()'s own current value (a real correction
 * that arrived slightly before/after a play()/pause() transition is not
 * an error). Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null player. */
crtmedia_result crtmedia_player_update_audio_clock(crtmedia_player* player, int64_t pts_us);

/* A decoded video frame is considered too far behind the master clock to
 * still matter (crtmedia_player_plan_video_frame() reports
 * CRTMEDIA_PLAYER_VIDEO_DROP rather than _PRESENT_NOW) once it is more
 * than this many microseconds late -- a real, deliberate, documented
 * threshold (100ms), in the same range real media players commonly use
 * for this same decision, not an arbitrary placeholder. */
#define CRTMEDIA_PLAYER_LATE_DROP_THRESHOLD_US 100000

typedef enum crtmedia_player_video_action {
  /* frame_pts_us <= current clock, and not late enough to drop --
   * present it right now. */
  CRTMEDIA_PLAYER_VIDEO_PRESENT_NOW = 0,
  /* frame_pts_us is still ahead of the current clock -- wait
   * `*out_wait_us` microseconds (a real, computed value, never negative),
   * then present it. */
  CRTMEDIA_PLAYER_VIDEO_WAIT = 1,
  /* frame_pts_us is more than CRTMEDIA_PLAYER_LATE_DROP_THRESHOLD_US
   * behind the current clock -- too late to still matter; do not present
   * it at all (the caller should still release the frame's own storage,
   * this function never does that itself). */
  CRTMEDIA_PLAYER_VIDEO_DROP = 2,
} crtmedia_player_video_action;

/* Real A/V sync decision for one decoded video frame's own presentation
 * timestamp (`frame_pts_us`, matching crtmedia_frame's own timestamp_us
 * field -- crtmedia/frame.h) against the current master clock. Returns
 * CRTMEDIA_ERROR_INVALID_ARGUMENT for a null player/out_action/
 * out_wait_us. */
crtmedia_result crtmedia_player_plan_video_frame(
    const crtmedia_player* player, int64_t frame_pts_us, crtmedia_player_video_action* out_action,
    int64_t* out_wait_us);

#ifdef __cplusplus
}
#endif
