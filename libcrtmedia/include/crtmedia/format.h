#pragma once

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Key-value format description, the first piece of docs/libcrtmedia_
 * api_policy.md's decided core (TODO.md's "Separate extractor and codec"
 * item). Shaped after AMediaFormat's own design -- a generic key-value
 * store instead of one fixed struct per container/codec -- deliberately
 * NOT the literal `AMediaFormat`/`AMediaFormat_*` symbols (see that
 * policy doc's own Decision/Non-Goals on why exact NDK source
 * compatibility is a later, optional adapter, not this core).
 *
 * `crtmedia_extractor_track_format()` (crtmedia/extractor.h) fills one of
 * these describing what a real container track actually is; `crtmedia_
 * codec_create_decoder()` (crtmedia/codec.h) reads one to configure a
 * decoder. Values are copied in and out -- a crtmedia_format never aliases
 * caller-owned memory, so it has no release-callback ownership model the
 * way crtmedia_frame/crtmedia_audio_buffer/crtmedia_sample do. */

typedef struct crtmedia_format crtmedia_format;

/* Well-known keys. String literals, not an enum, matching AMediaFormat's
 * own real design intent (a format can carry keys this contract doesn't
 * predefine at all -- codec-specific extradata, for instance -- without
 * needing a schema change) -- shaped after AMediaFormat's own key names
 * where the semantics are identical, not a promise of exact string
 * compatibility with any real Android build. */
#define CRTMEDIA_FORMAT_KEY_MIME "mime" /* string, e.g. "video/avc", "audio/mp4a-latm" */
#define CRTMEDIA_FORMAT_KEY_WIDTH "width" /* int32, video only */
#define CRTMEDIA_FORMAT_KEY_HEIGHT "height" /* int32, video only */
#define CRTMEDIA_FORMAT_KEY_SAMPLE_RATE "sample-rate" /* int32, audio only */
#define CRTMEDIA_FORMAT_KEY_CHANNEL_COUNT "channel-count" /* int32, audio only */
#define CRTMEDIA_FORMAT_KEY_DURATION_US "duration-us" /* int64, either */
/* buffer -- codec-specific config data a decoder needs before it can
 * decode any real sample at all (H.264's SPS/PPS in avcC form, AAC's
 * AudioSpecificConfig, ...; FFmpeg calls this "extradata", real Android
 * calls it "csd-0" -- named to match AMediaFormat's own real key exactly,
 * a real, useful point of shape alignment even without a full
 * compatibility claim, see docs/libcrtmedia_api_policy.md). Not every
 * track has one (PCM/MP3 need none) -- crtmedia_format_get_buffer()
 * returning CRTMEDIA_ERROR_UNSUPPORTED for a track with no real config
 * data is expected, not a bug. */
#define CRTMEDIA_FORMAT_KEY_CSD "csd-0"

/* Creates an empty format. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a
 * null out_format. */
crtmedia_result crtmedia_format_create(crtmedia_format** out_format);

/* Releases `format` and every key/value it owns. A NULL format is a
 * no-op. */
void crtmedia_format_release(crtmedia_format* format);

/* Sets `key` to `value`, replacing any existing value for that key
 * (regardless of that existing value's own type -- a later _get_int64()
 * on a key last set via _set_int32() is well-defined to fail with
 * CRTMEDIA_ERROR_UNSUPPORTED, not read garbage). Returns CRTMEDIA_ERROR_
 * INVALID_ARGUMENT for a null format/key, CRTMEDIA_ERROR_UNSUPPORTED if
 * this format already holds as many distinct keys as it supports (a
 * real, small, fixed limit -- see format.c's own top comment for the
 * exact number and why). */
crtmedia_result crtmedia_format_set_int32(crtmedia_format* format, const char* key, int32_t value);
crtmedia_result crtmedia_format_set_int64(crtmedia_format* format, const char* key, int64_t value);
/* `value` is copied in full -- format does not alias or take ownership
 * of the caller's own string. */
crtmedia_result crtmedia_format_set_string(crtmedia_format* format, const char* key, const char* value);
/* `data` (`size` bytes) is copied in full -- same non-aliasing contract
 * as _set_string(). CRTMEDIA_ERROR_UNSUPPORTED if `size` exceeds this
 * format's own fixed per-buffer-value limit (format.c's own top comment
 * has the exact number -- generous for real SPS/PPS/AudioSpecificConfig
 * codec config data, not for arbitrary large payloads). */
crtmedia_result crtmedia_format_set_buffer(crtmedia_format* format, const char* key, const void* data, size_t size);

/* Fills `*out_value` from `key`. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT
 * for a null format/key/out_value, CRTMEDIA_ERROR_UNSUPPORTED if `key` was
 * never set or was last set with a different value type than this call
 * asks for. */
crtmedia_result crtmedia_format_get_int32(const crtmedia_format* format, const char* key, int32_t* out_value);
crtmedia_result crtmedia_format_get_int64(const crtmedia_format* format, const char* key, int64_t* out_value);
/* `*out_value` is set to point directly at this format's own internal
 * copy of the string -- valid only until the next call that mutates this
 * same format (a _set_*() call for any key, or crtmedia_format_release())
 * -- copy it out immediately if it needs to outlive that. */
crtmedia_result crtmedia_format_get_string(const crtmedia_format* format, const char* key, const char** out_value);
/* `*out_data`/`*out_size` are set to point directly at this format's own
 * internal copy -- same "valid only until the next mutating call" rule
 * as _get_string(). */
crtmedia_result crtmedia_format_get_buffer(
    const crtmedia_format* format, const char* key, const void** out_data, size_t* out_size);

#ifdef __cplusplus
}
#endif
