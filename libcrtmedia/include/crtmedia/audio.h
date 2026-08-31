#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CPU audio buffer handoff contract -- the audio half of TODO.md's
 * "libcrtmedia CPU frame handoff contract" item (crtmedia/frame.h covers
 * video). Mirrors that file's own design language deliberately rather than
 * inventing a new shape: the same release-callback ownership model, the
 * same `_UNSPECIFIED = 0` zero-init-safe enum convention is not needed here
 * (no format is ever "not applicable" the way color_range/color_space are
 * for a packed-RGB video frame -- every audio buffer has a real sample
 * format), and the same `_NONE` timestamp sentinel idea.
 *
 * This is deliberately a pure data contract, decoder-agnostic -- see
 * crtmedia/demux.h for the actual FFmpeg-backed decode path that produces
 * these. */

/* Interleaved PCM sample layout. Both are the two natural outputs of this
 * project's own first FFmpeg decoder set (crtmedia/demux.h's own AAC/MP3/
 * PCM decode) -- FLT is libavcodec's own default internal sample format
 * for AAC/MP3 decode, S16 is pcm_s16le's native format and a common
 * explicit conversion target via libswresample. Not bit-depth-agnostic on
 * purpose: add a real value here only once a real decoder in this
 * project's own codec set needs it, matching crtmedia_pixel_format's own
 * established "no speculative values" convention. */
typedef enum crtmedia_sample_format {
  CRTMEDIA_SAMPLE_FORMAT_S16 = 1,
  CRTMEDIA_SAMPLE_FORMAT_FLT = 2,
} crtmedia_sample_format;

/* Same sentinel value and same reasoning as CRTMEDIA_FRAME_TIMESTAMP_NONE
 * (crtmedia/frame.h) -- kept as its own, separately-named constant (not a
 * shared header) so a future divergence in either contract's own timestamp
 * semantics does not silently couple the two. */
#define CRTMEDIA_AUDIO_TIMESTAMP_NONE INT64_MIN

typedef struct crtmedia_audio_buffer crtmedia_audio_buffer;

/* Called exactly once by crtmedia_audio_buffer_release() to free whatever
 * backing storage this buffer's own `data` points into. Same shape and
 * same ownership contract as crtmedia_frame_release_fn (crtmedia/frame.h):
 * `release == NULL` means a non-owning view over caller-managed storage. */
typedef void (*crtmedia_audio_release_fn)(crtmedia_audio_buffer* buffer, void* release_context);

/* One buffer of decoded PCM audio. `data` holds `frame_count` interleaved
 * samples per channel (i.e. `frame_count * channels` total samples, each
 * `sizeof(int16_t)` or `sizeof(float)` bytes wide per `format`) --
 * "frame_count" here matches libavcodec's own AVFrame::nb_samples naming
 * (samples per channel, not total sample count), not crtmedia_frame's
 * unrelated video frame concept. */
struct crtmedia_audio_buffer {
  crtmedia_sample_format format;
  uint32_t sample_rate;
  uint32_t channels;
  uint32_t frame_count;
  void* data;
  int64_t timestamp_us;
  crtmedia_audio_release_fn release;
  void* release_context;
};

/* Calls buffer->release(buffer, buffer->release_context) if release is
 * non-NULL, then zeroes *buffer (format becomes 0, not a valid
 * crtmedia_sample_format value -- matches crtmedia_frame_release()'s own
 * use-after-release-crashes-cleanly reasoning). A NULL buffer is a no-op. */
void crtmedia_audio_buffer_release(crtmedia_audio_buffer* buffer);

#ifdef __cplusplus
}
#endif
