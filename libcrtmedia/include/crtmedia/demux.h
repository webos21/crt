#pragma once

#include <stdint.h>

#include "crtmedia/audio.h"
#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* File demux + software decode, the FFmpeg-backed bridge TODO.md's
 * libcrtmedia item calls out ("FFmpeg demux/software decode -> the CPU
 * frame contract -> audio buffer handoff"). Deliberately narrow first pass
 * (HISTORY.md's matching entry has the full scope reasoning): local file
 * only (no network protocols yet), one container (MOV/MP4/M4A), H.264
 * video, AAC/MP3/PCM audio.
 *
 * `crtmedia_demuxer` is opaque -- no FFmpeg type (`AVFormatContext`,
 * `AVCodecContext`, ...) ever appears in this public header, matching this
 * project's established "no host/upstream SDK type in a public header"
 * policy (see docs/libcrtgfx_api_policy.md's own Non-Goals for the same
 * rule applied to libcrtgfx's own Win32/Direct3D/Cocoa/Metal boundary).
 * Decoded video lands in the *existing* crtmedia_frame contract
 * (crtmedia/frame.h) as CRTMEDIA_PIXEL_FORMAT_YUV420P -- H.264's native
 * decode output, and exactly the format crtmedia_frame_convert_to_rgba()
 * already knows how to turn into something Skia can draw. Decoded audio
 * lands in crtmedia_audio_buffer (crtmedia/audio.h). */

typedef struct crtmedia_demuxer crtmedia_demuxer;

/* CRTMEDIA_STREAM_UNKNOWN (0, the zero-init default) covers any stream
 * this contract does not decode (subtitles, data streams, a codec outside
 * this pass's own narrow enable list) -- crtmedia_demuxer_stream_info()
 * still reports its presence (so a caller can see the real stream count
 * without every stream being decodable), just with no width/height/
 * sample_rate/channels filled in, and crtmedia_demuxer_read() never
 * produces output for it. */
typedef enum crtmedia_stream_type {
  CRTMEDIA_STREAM_UNKNOWN = 0,
  CRTMEDIA_STREAM_VIDEO = 1,
  CRTMEDIA_STREAM_AUDIO = 2,
} crtmedia_stream_type;

typedef struct crtmedia_stream_info {
  crtmedia_stream_type type;
  /* Video only (0 for an audio/unknown stream): */
  uint32_t width;
  uint32_t height;
  /* Audio only (0 for a video/unknown stream): */
  uint32_t sample_rate;
  uint32_t channels;
} crtmedia_stream_info;

/* Opens `path` (a plain local filesystem path -- see this file's own top
 * comment on the file-only scope of this first pass), demuxes its
 * container, and probes every stream's codec parameters. Every stream
 * whose codec is within this pass's own decode set (H.264 video; AAC/MP3/
 * PCM audio) is opened and ready to decode via crtmedia_demuxer_read()
 * immediately -- there is no separate "select stream" step; every
 * decodable stream is active from the start, matching this narrow first
 * pass's own single-video/single-or-few-audio-track scope (a real stream-
 * selection API is future work once a real multi-track use case demands
 * it). Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null path/out_demuxer,
 * CRTMEDIA_ERROR_UNSUPPORTED if the file cannot be opened, demuxed, or has
 * no stream this pass's own codec set can decode. */
crtmedia_result crtmedia_demuxer_open(const char* path, crtmedia_demuxer** out_demuxer);

/* Real stream count in the container (including any stream this pass
 * cannot decode -- see crtmedia_stream_type's own CRTMEDIA_STREAM_UNKNOWN
 * comment). 0 for a null demuxer. */
uint32_t crtmedia_demuxer_stream_count(const crtmedia_demuxer* demuxer);

/* Fills `*out_info` for `stream_index` (< crtmedia_demuxer_stream_count()'s
 * own return value). Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null
 * demuxer/out_info or an out-of-range stream_index. */
crtmedia_result crtmedia_demuxer_stream_info(
    const crtmedia_demuxer* demuxer, uint32_t stream_index, crtmedia_stream_info* out_info);

/* What crtmedia_demuxer_read() actually produced this call. Exactly one of
 * out_video_frame/out_audio_buffer is filled, matching whichever status is
 * returned -- CRTMEDIA_READ_EOF fills neither. */
typedef enum crtmedia_read_status {
  CRTMEDIA_READ_VIDEO_FRAME = 1,
  CRTMEDIA_READ_AUDIO_BUFFER = 2,
  CRTMEDIA_READ_EOF = 3,
} crtmedia_read_status;

/* Demuxes and decodes forward by exactly one output unit (one decoded
 * video frame, or one decoded audio buffer -- never both in the same
 * call), filling `*out_status`/`*out_stream_index` and exactly one of
 * `*out_video_frame`/`*out_audio_buffer` (the other is left untouched --
 * callers should only read the one `*out_status` says was filled).
 * `out_video_frame`/`out_audio_buffer` may each be NULL if the caller has
 * no interest in that stream type at all (matching stream-selective
 * decode without a separate enable/disable API this narrow first pass
 * does not need); crtmedia_demuxer_read() still demuxes/decodes
 * internally and simply drops output for a NULL destination pointer that
 * type would have gone to.
 *
 * Every crtmedia_frame/crtmedia_audio_buffer this fills owns its own real
 * backing storage (release is never NULL) -- the caller must eventually
 * call crtmedia_frame_release()/crtmedia_audio_buffer_release() on it
 * regardless of what it does with the pixel/sample data first. Returns
 * CRTMEDIA_ERROR_INVALID_ARGUMENT for a null demuxer/out_status/
 * out_stream_index, or CRTMEDIA_ERROR_UNSUPPORTED if decoding hits a real
 * error partway through the stream (corrupt data, an unsupported profile
 * within an otherwise-decodable codec, ...) -- *out_status is left
 * unspecified in that case; a caller that wants best-effort decode
 * despite isolated stream errors is not supported by this first pass. */
crtmedia_result crtmedia_demuxer_read(
    crtmedia_demuxer* demuxer,
    crtmedia_read_status* out_status,
    uint32_t* out_stream_index,
    crtmedia_frame* out_video_frame,
    crtmedia_audio_buffer* out_audio_buffer);

/* Closes `demuxer` and frees everything it owns. Any crtmedia_frame/
 * crtmedia_audio_buffer already produced by crtmedia_demuxer_read() is
 * unaffected (each owns its own storage independently, per that
 * function's own comment) and must still be released on its own. A NULL
 * demuxer is a no-op. */
void crtmedia_demuxer_close(crtmedia_demuxer* demuxer);

#ifdef __cplusplus
}
#endif
