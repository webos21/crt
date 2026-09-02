#pragma once

#include <stdint.h>

#include "crtmedia/audio.h"
#include "crtmedia/format.h"
#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Async buffer-queue decoder, the third piece of docs/libcrtmedia_
 * api_policy.md's decided core (TODO.md's "Separate extractor and codec"
 * item). Shaped after AMediaCodec's own real dequeue/queue buffer-queue
 * model -- deliberately NOT the literal `AMediaCodec`/`AMediaCodec_*`
 * symbols, and deliberately NOT AMediaCodec's own raw indexed-buffer-pool
 * bookkeeping either (see docs/libcrtmedia_api_policy.md's own Decision):
 * that machinery exists in the real NDK API to support zero-copy
 * hand-off to real hardware/`ANativeWindow` buffer pools, which this
 * project's own software-only decode has no use for yet -- this crtmedia_
 * codec instead hands out an owned crtmedia_frame/crtmedia_audio_buffer
 * per decoded output, matching this project's own already-established
 * ownership idiom (crtmedia/frame.h, crtmedia/audio.h, crtmedia/
 * extractor.h's own crtmedia_sample) rather than introducing a second,
 * index-based one. A real hardware decode surface (TODO.md's own later
 * "hardware decode" steps) can still be added as a distinct, additional
 * output path from this same queue model later -- this is the same
 * asynchronous send/receive shape that would need either way.
 *
 * One crtmedia_codec decodes exactly one track's worth of samples --
 * crtmedia_extractor_read_sample()'s own output (crtmedia/extractor.h)
 * feeds crtmedia_codec_queue_input() directly, no adaptation needed. No
 * FFmpeg type (`AVCodecContext`, `AVPacket`, `AVFrame`, ...) ever appears
 * here, matching this project's established "no host/upstream SDK type
 * in a public header" policy. */

typedef struct crtmedia_codec crtmedia_codec;

typedef enum crtmedia_codec_buffer_flags {
  CRTMEDIA_CODEC_BUFFER_FLAG_NONE = 0,
  /* Signals "no more input will ever be queued" (AMediaCodec's own
   * BUFFER_FLAG_END_OF_STREAM) -- may be OR'd onto the last real queue_
   * input() call's own flags, or passed alone with data=NULL/size=0 as a
   * separate, final call. Either shape works; queue_input()'s own doc
   * comment below has the exact rule. */
  CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM = 1 << 0,
} crtmedia_codec_buffer_flags;

/* Creates a decoder configured from `format` (as produced by crtmedia_
 * extractor_track_format() -- reads CRTMEDIA_FORMAT_KEY_MIME to select a
 * real decoder from this pass's own narrow codec set (H.264 video; AAC/
 * MP3/PCM audio, matching crtmedia/demux.h's own scope) and CRTMEDIA_
 * FORMAT_KEY_WIDTH/HEIGHT or _SAMPLE_RATE/_CHANNEL_COUNT to configure
 * it). Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null format/
 * out_codec, CRTMEDIA_ERROR_UNSUPPORTED if the format's own MIME is
 * missing or outside this pass's own decode set. */
crtmedia_result crtmedia_codec_create_decoder(const crtmedia_format* format, crtmedia_codec** out_codec);

void crtmedia_codec_release(crtmedia_codec* codec);

/* Submits one encoded sample for decode -- `data`/`size` matching
 * crtmedia_sample's own fields directly (crtmedia/extractor.h), so a
 * caller normally passes read_sample()'s own output straight through.
 * `data`/`size` may both be 0/NULL when `flags` alone carries
 * CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_STREAM, matching AMediaCodec's own
 * "queue an empty EOS buffer" convention -- or that flag may be OR'd onto
 * a real, final sample's own flags instead; both are equivalent.
 *
 * Returns CRTMEDIA_OK once the sample is accepted, CRTMEDIA_WOULD_BLOCK
 * if this codec's own internal input queue is momentarily full (call
 * crtmedia_codec_dequeue_output() to make room, then retry the exact same
 * queue_input() call -- nothing was accepted or lost), or CRTMEDIA_ERROR_
 * UNSUPPORTED if the data itself is malformed enough to be a real decode
 * error. CRTMEDIA_ERROR_INVALID_ARGUMENT for a null codec, or for null
 * data with nonzero size (or vice versa). */
crtmedia_result crtmedia_codec_queue_input(crtmedia_codec* codec, const void* data, uint32_t size, int64_t pts_us, uint32_t flags);

/* Pulls one decoded output -- exactly one of `*out_video_frame`/
 * `*out_audio_buffer` is filled (whichever matches this codec's own real
 * type, fixed at crtmedia_codec_create_decoder() time), matching
 * crtmedia_demuxer_read()'s own established "caller passes both
 * pointers, only the relevant one is touched" shape; either may be NULL
 * if the caller has no interest in that codec's output type at all
 * (never true in practice today, since a codec is exactly one type, but
 * keeps the calling convention identical either way).
 *
 * Returns CRTMEDIA_OK with `*out_eof` cleared to 0 and a real decoded
 * frame/buffer filled, CRTMEDIA_WOULD_BLOCK if nothing is ready yet
 * (queue more input first, or -- after CRTMEDIA_CODEC_BUFFER_FLAG_END_OF_
 * STREAM has already been queued -- this cannot happen again; every
 * remaining buffered frame is guaranteed drained before CRTMEDIA_OK/
 * out_eof=1), or CRTMEDIA_OK with `*out_eof` set to 1 (neither output
 * pointer touched) once this codec is fully drained after end-of-stream.
 * CRTMEDIA_ERROR_INVALID_ARGUMENT for a null codec/out_eof. */
crtmedia_result crtmedia_codec_dequeue_output(
    crtmedia_codec* codec, crtmedia_frame* out_video_frame, crtmedia_audio_buffer* out_audio_buffer, int* out_eof);

/* Discards every buffered input/output and resets end-of-stream state --
 * matching AMediaCodec_flush()'s own real use case (a real seek: the
 * caller is about to queue input from a new position and any already-
 * queued/pending output is now stale). A NULL codec is a no-op. */
crtmedia_result crtmedia_codec_flush(crtmedia_codec* codec);

#ifdef __cplusplus
}
#endif
