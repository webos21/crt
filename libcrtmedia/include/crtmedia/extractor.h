#pragma once

#include <stdint.h>

#include "crtmedia/format.h"
#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Demux-only container reader, the second piece of docs/libcrtmedia_
 * api_policy.md's decided core (TODO.md's "Separate extractor and codec"
 * item). Shaped after AMediaExtractor's own design -- open a container,
 * list tracks, select which ones to read, pull raw (still encoded)
 * samples -- deliberately NOT the literal `AMediaExtractor`/
 * `AMediaExtractor_*` symbols (see that policy doc's own Decision/Non-
 * Goals). No decode happens here at all: crtmedia_extractor never touches
 * an AVCodecContext, only AVFormatContext -- decode is crtmedia_codec.h's
 * own, separate job, matching the whole point of splitting this out of
 * the existing combined crtmedia_demuxer_* (crtmedia/demux.h, kept as a
 * convenience API layered over this core, not replaced).
 *
 * A real FFmpeg container can have more streams than this pass's own
 * codec set eventually decodes (subtitles, data streams, an unsupported
 * codec) -- crtmedia_extractor reports every real stream via track_
 * count()/track_format() regardless (matching AMediaExtractor's own
 * behavior: extraction is codec-agnostic, decodability is crtmedia_
 * codec.h's own separate concern), and read_sample() only ever returns
 * data for a track the caller has explicitly selected. */

typedef struct crtmedia_extractor crtmedia_extractor;

/* One raw, still-encoded sample read from a selected track -- owns its
 * own storage (release is never NULL once a real sample is produced),
 * mirroring crtmedia_frame/crtmedia_audio_buffer's own established
 * ownership model (crtmedia/frame.h, crtmedia/audio.h). Forward-declared
 * (the typedef'd name, not `struct crtmedia_sample` -- this project's own
 * public headers never mix the two spellings for the same type) so
 * crtmedia_sample_release_fn can name it before the full definition below. */
typedef struct crtmedia_sample crtmedia_sample;
typedef void (*crtmedia_sample_release_fn)(crtmedia_sample* sample, void* release_context);

typedef enum crtmedia_sample_flags {
  CRTMEDIA_SAMPLE_FLAG_NONE = 0,
  CRTMEDIA_SAMPLE_FLAG_KEY_FRAME = 1 << 0,
} crtmedia_sample_flags;

struct crtmedia_sample {
  void* data;
  uint32_t size;
  uint32_t track_index;
  int64_t pts_us; /* CRTMEDIA_FRAME_TIMESTAMP_NONE (crtmedia/frame.h) if unknown */
  uint32_t flags; /* crtmedia_sample_flags, OR'd together */
  crtmedia_sample_release_fn release;
  void* release_context;
};

void crtmedia_sample_release(crtmedia_sample* sample);

/* Opens `path` (a plain local filesystem path -- matching crtmedia_
 * demuxer_open()'s own file-only scope) and probes every real stream's
 * container-level parameters. No track is selected yet -- read_sample()
 * produces nothing until at least one crtmedia_extractor_select_track()
 * call. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null path/
 * out_extractor, CRTMEDIA_ERROR_UNSUPPORTED if the file cannot be opened
 * or demuxed at all. */
crtmedia_result crtmedia_extractor_create(const char* path, crtmedia_extractor** out_extractor);

void crtmedia_extractor_release(crtmedia_extractor* extractor);

/* Real track count in the container, including any track this pass's
 * own codec set cannot eventually decode (see this file's own top
 * comment). 0 for a null extractor. */
uint32_t crtmedia_extractor_track_count(const crtmedia_extractor* extractor);

/* Fills `*out_format` with a new crtmedia_format (caller-owned -- release
 * with crtmedia_format_release() once done, matching AMediaExtractor_
 * getTrackFormat()'s own "returns a new object each call" convention)
 * describing track_index's real container-level parameters: at least
 * CRTMEDIA_FORMAT_KEY_MIME always, plus CRTMEDIA_FORMAT_KEY_WIDTH/HEIGHT
 * for a video track or CRTMEDIA_FORMAT_KEY_SAMPLE_RATE/CHANNEL_COUNT for
 * an audio track. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null
 * extractor/out_format or an out-of-range track_index. */
crtmedia_result crtmedia_extractor_track_format(
    const crtmedia_extractor* extractor, uint32_t track_index, crtmedia_format** out_format);

/* Marks track_index as one read_sample() should return samples for.
 * Selecting an already-selected track is a harmless no-op. Returns
 * CRTMEDIA_ERROR_INVALID_ARGUMENT for a null extractor or an out-of-range
 * track_index. */
crtmedia_result crtmedia_extractor_select_track(crtmedia_extractor* extractor, uint32_t track_index);

/* Reverses crtmedia_extractor_select_track() -- read_sample() stops
 * returning samples for this track (any already-selected sample already
 * handed out is unaffected, matching crtmedia_sample's own independent-
 * ownership model). Unselecting an already-unselected track is a
 * harmless no-op. Same argument-validation contract as select_track(). */
crtmedia_result crtmedia_extractor_unselect_track(crtmedia_extractor* extractor, uint32_t track_index);

/* Demuxes forward until the next sample belonging to a selected track
 * (any not-selected track's own packets are read and discarded
 * internally, matching AMediaExtractor's own behavior), filling
 * `*out_sample` and clearing `*out_eof` to 0 -- or, once the container is
 * exhausted, leaves `*out_sample` untouched and sets `*out_eof` to 1.
 * Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null extractor/
 * out_sample/out_eof, CRTMEDIA_ERROR_UNSUPPORTED if the container itself
 * hits a real read error partway through. */
crtmedia_result crtmedia_extractor_read_sample(crtmedia_extractor* extractor, crtmedia_sample* out_sample, int* out_eof);

/* Seeks every selected track to the first real sync/key sample at or
 * before `seek_pos_us`, matching AMediaExtractor_seekTo()'s own default
 * SEEK_PREVIOUS_SYNC mode -- the only mode this first pass supports (see
 * extractor.c's own top comment). Returns CRTMEDIA_ERROR_INVALID_ARGUMENT
 * for a null extractor, CRTMEDIA_ERROR_UNSUPPORTED if the container does
 * not support seeking at all. */
crtmedia_result crtmedia_extractor_seek_to(crtmedia_extractor* extractor, int64_t seek_pos_us);

#ifdef __cplusplus
}
#endif
