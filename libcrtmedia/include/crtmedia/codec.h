#pragma once

#include <stdint.h>

#include "crtmedia/audio.h"
#include "crtmedia/format.h"
#include "crtmedia/frame.h"
#include "crtmedia/gpu_frame.h"

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

/* GPU-frame sibling of crtmedia_codec_dequeue_output() above (2026-09-23,
 * "Zero-copy decoded textures" Tranche 0/1 --
 * docs/crtmedia_zero_copy_decode_acceptance.md has the full frozen
 * contract). Same calling convention, same CRTMEDIA_WOULD_BLOCK/EOF
 * behavior, and freely interchangeable with dequeue_output() frame-by-
 * frame on the same codec instance -- this only changes how the *next*
 * already-decoded frame is packaged, not which frame comes next.
 *
 * Fills `*out_video_frame` (crtmedia/gpu_frame.h) as one of:
 *   - memory_kind == CRTMEDIA_GPU_MEMORY_GPU, native_handle a real
 *     per-host native surface handle, plane_count == 0 -- only when a
 *     real hardware-resident frame was observed and this host can hand its
 *     native surface to the optional graphics bridge. This guarantees no
 *     CPU download in this function; it does not by itself claim end-to-end
 *     zero-copy, because a downstream bridge may require a GPU copy
 *     (Windows D3D11VA -> D3D12).
 *   - memory_kind == CRTMEDIA_GPU_MEMORY_CPU, native_handle == NULL, real
 *     pixel planes otherwise -- a hardware-resident frame was observed
 *     but this frame has no GPU-frame handoff path (falls back to the same
 *     av_hwframe_transfer_data() download dequeue_output() itself uses),
 *     or the frame was decoded in software to begin with.
 * A caller must check memory_kind before touching native_handle, exactly
 * like crtmedia_gpu_frame's own existing contract already requires.
 *
 * Broadens (does not redefine) crtmedia_codec_is_hardware_accelerated():
 * it becomes true from either this function's GPU-frame delivery or
 * dequeue_output()'s CPU-transfer delivery -- a caller using only
 * dequeue_output() observes byte-identical behavior to before this
 * function existed. */
crtmedia_result crtmedia_codec_dequeue_gpu_frame(
    crtmedia_codec* codec, crtmedia_gpu_frame* out_video_frame, crtmedia_audio_buffer* out_audio_buffer,
    int* out_eof);

/* Discards every buffered input/output and resets end-of-stream state --
 * matching AMediaCodec_flush()'s own real use case (a real seek: the
 * caller is about to queue input from a new position and any already-
 * queued/pending output is now stale). A NULL codec is a no-op. */
crtmedia_result crtmedia_codec_flush(crtmedia_codec* codec);

/* Real, honest report (2026-09-08, "hardware decode, phase A"; semantics
 * corrected 2026-09-18, Tranche 2; broadened, not redefined, 2026-09-23,
 * "Zero-copy decoded textures" -- docs/crtmedia_zero_copy_decode_
 * acceptance.md) of whether `codec` has actually decoded at least one
 * real hardware-backed frame and successfully delivered it to the caller
 * on this host -- only ever true for a video codec created with
 * CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE set (crtmedia/format.h).
 * Becomes true once a real hardware-resident `AVFrame` has actually been
 * observed and either (a) crtmedia_codec_dequeue_output()'s CPU download
 * (`av_hwframe_transfer_data()`) has actually succeeded, or (b)
 * crtmedia_codec_dequeue_gpu_frame()'s GPU-resident handoff has actually
 * succeeded -- creating the hardware device and opening the codec
 * successfully are both deliberately *not* enough on their own (a
 * decoder can open cleanly with hardware attached and still never decode
 * a single frame through it), and crtmedia_codec_create_decoder() always
 * falls back to software silently when hardware isn't usable at all,
 * matching this project's own "software fallback must remain a
 * first-class path" precedent. A caller using only dequeue_output()
 * observes byte-identical behavior to before path (b) existed. Once
 * true, stays true for the rest of this decoder instance's lifetime,
 * including across crtmedia_codec_flush() -- this answers whether the
 * instance has ever used hardware, not whether the immediately previous
 * frame did. Never a caller-side guess, this reflects what really
 * happened. Returns CRTMEDIA_ERROR_INVALID_ARGUMENT for a null codec/
 * out_is_hardware, CRTMEDIA_OK with `*out_is_hardware` set to 0 or 1
 * otherwise. */
crtmedia_result crtmedia_codec_is_hardware_accelerated(const crtmedia_codec* codec, int* out_is_hardware);

#ifdef __cplusplus
}
#endif
