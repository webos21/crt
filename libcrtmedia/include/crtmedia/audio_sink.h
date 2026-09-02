#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/audio.h"
#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host audio output (TODO.md's upper-runtime roadmap "Build the software
 * player" step -- "host audio sinks: WASAPI, CoreAudio, and PipeWire/
 * ALSA"). Opaque, blocking-write-shaped -- no `IAudioClient`/
 * `AudioQueueRef`/`snd_pcm_t` (or any other host audio API type) ever
 * appears here, matching this project's established "no host/upstream
 * SDK type in a public header" policy (see docs/libcrtgfx_api_policy.md's
 * own Non-Goals for the same rule applied to libcrtgfx's own Win32/
 * Direct3D/Cocoa/Metal boundary). Each real host backend (`src/arch/
 * windows/audio_sink_wasapi.c`, `src/arch/linux/audio_sink_alsa.c`,
 * `src/arch/macos/audio_sink_coreaudio.c`) presents the identical simple
 * blocking-write contract below regardless of how differently each real
 * host API is actually shaped (WASAPI's own get-buffer/release-buffer
 * cycle, CoreAudio's own callback-driven `AudioQueue`, ALSA's own
 * naturally blocking `snd_pcm_writei()`) -- crtmedia_player.h (the master
 * clock/A/V-sync layer this sink is meant to feed) and a real render
 * loop built on top of both stay entirely host-independent either way.
 *
 * Matches `crtgfx_window_create()`'s own established "no usable host
 * backend in this environment" graceful-degradation contract (`libcrtgfx/
 * include/crtgfx/window.h`) rather than treating a real headless/no-
 * audio-device environment (CI, a container, this project's own WSL dev
 * loop) as fatal: crtmedia_audio_sink_open() returns CRTMEDIA_ERROR_
 * UNSUPPORTED there, not a crash or a hang, and a caller that has no
 * audio track to play (or does not care about hearing it) never needs to
 * open one at all -- decode/render can proceed on the master clock's own
 * free-running host-wall-clock mode instead (crtmedia/player.h). */

typedef struct crtmedia_audio_sink crtmedia_audio_sink;

typedef struct crtmedia_audio_sink_desc {
  crtmedia_sample_format format; /* crtmedia/audio.h -- S16 or FLT */
  uint32_t sample_rate;
  uint32_t channels;
} crtmedia_audio_sink_desc;

/* Opens the real, default host audio output device configured for
 * `desc`'s own exact format/sample_rate/channels -- this first pass does
 * not resample or reformat on this sink's own behalf (a caller decoding
 * through crtmedia_codec/crtmedia_demuxer_* already gets real PCM at
 * whatever rate/format the source actually is; matching that to what a
 * real device will accept, if a resample is ever genuinely needed, stays
 * the caller's own job via libswresample -- already linked into this
 * library for crtmedia_codec's own internal use -- not this sink's).
 * Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null desc/out_sink or an
 * invalid desc field (0 sample_rate/channels, an unrecognized format);
 * CRTMEDIA_ERROR_UNSUPPORTED if no usable host audio device/backend is
 * available in this environment at all, or if the real device genuinely
 * cannot be configured for the exact format requested. */
crtmedia_result crtmedia_audio_sink_open(const crtmedia_audio_sink_desc* desc, crtmedia_audio_sink** out_sink);

/* Closes `sink`, blocking until every already-written sample has finished
 * real playback (a real drain, not an abrupt cutoff) -- a caller that
 * wants an immediate stop instead should simply stop calling crtmedia_
 * audio_sink_write() and drop the sink without an explicit intent to
 * hear the tail end play out; this project's own crtmedia_player.h
 * (`crtmedia_player_stop()`) does not itself call this function; that
 * decision belongs to whatever component actually owns the sink. A NULL
 * sink is a no-op. */
void crtmedia_audio_sink_close(crtmedia_audio_sink* sink);

/* Writes `frame_count` real interleaved PCM sample frames from `data`
 * (matching `crtmedia_audio_buffer::frame_count`'s own "samples per
 * channel" meaning exactly -- crtmedia/audio.h -- so a caller can pass a
 * real, already-decoded crtmedia_audio_buffer's own `data`/`frame_count`
 * straight through), blocking until the host device has real room for at
 * least one full frame (genuine backpressure paced by real playback
 * time, not merely an internal buffer-full check) -- this is what makes
 * a real render loop built on this sink naturally self-pace to real
 * time without needing its own separate sleep/timing logic for audio.
 * Returns the real number of frames actually accepted (matching this
 * project's own established explicit-short-write convention, e.g.
 * `write()`/`fwrite()` -- 0 is a real, valid, non-error result if the
 * device could not accept anything on this particular call and the
 * caller should simply retry), or a negative `crtmedia_result` (cast to
 * `int64_t`) on a real device failure. `sink`/`data` must not be NULL. */
int64_t crtmedia_audio_sink_write(crtmedia_audio_sink* sink, const void* data, uint32_t frame_count);

/* Real playback position, in frames since crtmedia_audio_sink_open(),
 * that host audio hardware has actually, audibly reached so far -- the
 * real source crtmedia_player_update_audio_clock() (crtmedia/player.h)
 * should be fed from (converted to microseconds via the sink's own
 * sample_rate), deliberately distinct from "how many frames have been
 * written so far" (crtmedia_audio_sink_write()'s own return values summed
 * up): a real device buffers some real amount of audio ahead of what is
 * actually audible at any instant, and that amount is not fixed/
 * predictable in advance. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a
 * null sink/out_frames. */
crtmedia_result crtmedia_audio_sink_get_position_frames(const crtmedia_audio_sink* sink, uint64_t* out_frames);

#ifdef __cplusplus
}
#endif
