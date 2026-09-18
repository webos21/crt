#pragma once

/* Private, non-installed diagnostic surface for hardware-decode acceptance
 * tests (docs/crtmedia_hardware_decode_acceptance.md, "Hardware video
 * decode" Tranche 2). Generic tests may query the fine-grained six-state
 * hardware-decode model through this header, but never see an FFmpeg type
 * (AVCodecContext/AVHWDeviceContext/AVFrame/...) or a concrete crtmedia_
 * codec layout -- mirrors libcrtgfx/src/gpu_test_control.h's own role and
 * scope for the GPU backend boundary.
 *
 * The public crtmedia_codec_is_hardware_accelerated() (crtmedia/codec.h)
 * only ever answers one question -- "has this instance actually used
 * hardware yet" -- and stays the sole public surface for it (this
 * tranche's own explicit ABI-unchanged constraint). This header exists
 * because a test verifying the *state machine itself* (device created vs.
 * pixel format offered vs. a real frame observed vs. transferred) needs
 * more resolution than that one boolean without turning the public API
 * into an FFmpeg-shaped diagnostic dump. */

#include "crtmedia/codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtmedia_codec_hw_diagnostics {
  /* CRTMEDIA_FORMAT_KEY_PREFER_HARDWARE_DECODE was set to a nonzero value
   * at crtmedia_codec_create_decoder() time. False for every audio codec
   * and every video codec that never opted in. */
  int hw_requested;
  /* av_hwdevice_ctx_create() and avcodec_open2() both succeeded with a
   * real per-host hardware device attached. Does NOT imply a frame was
   * ever actually decoded through it. */
  int hw_device_created;
  /* crtmedia_codec_get_format()'s own callback actually selected the
   * requested hardware pixel format at least once during decode
   * negotiation. A real decoder can legitimately never reach this. */
  int hw_pixfmt_offered;
  /* At least one decoded AVFrame's own format equaled hw_pix_fmt --
   * genuinely hardware-resident before any download was attempted. */
  int hw_frame_observed;
  /* Equal to crtmedia_codec_is_hardware_accelerated()'s own current
   * value: hw_frame_observed was true AND its av_hwframe_transfer_data()
   * CPU download also succeeded. Included here too so a caller can build
   * a complete diagnostic record from one query. */
  int hw_frame_transferred;
} crtmedia_codec_hw_diagnostics;

/* Snapshots every field above for `codec` right now. Every field is
 * sticky (sets to 1 and never back to 0 for that instance's own
 * lifetime, matching crtmedia_codec_is_hardware_accelerated()'s own
 * contract) except that the whole struct naturally reads all-zero for a
 * codec that never requested hardware at all. Returns CRTMEDIA_ERROR_
 * INVALID_ARGUMENT for a null codec/out_diagnostics, CRTMEDIA_OK
 * otherwise. */
crtmedia_result crtmedia_codec_test_get_hw_diagnostics(
    const crtmedia_codec* codec, crtmedia_codec_hw_diagnostics* out_diagnostics);

#ifdef __cplusplus
} /* extern "C" */
#endif
