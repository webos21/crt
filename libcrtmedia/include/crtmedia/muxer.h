#pragma once

#include <stdint.h>

#include "crtmedia/codec.h"
#include "crtmedia/format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Container writer shaped after AMediaMuxer without exposing FFmpeg or host
 * SDK types. MPEG-4 is the first accepted output; the track API is already
 * multi-track so audio can be added without changing the ownership/timing
 * contract. */
typedef struct crtmedia_muxer crtmedia_muxer;

typedef enum crtmedia_muxer_output_format {
  CRTMEDIA_MUXER_OUTPUT_MPEG_4 = 1,
  /* Fragmented MP4 (moof/mdat pairs interleaved throughout the stream
   * instead of one big index atom at the end) -- a real forward-only
   * reader (Networking & Streaming Tranche 2's own non-seekable HTTP
   * custom AVIO, docs/crtmedia_networking_acceptance.md) can start
   * demuxing without ever seeking, unlike CRTMEDIA_MUXER_OUTPUT_MPEG_4's
   * plain moov-at-the-end layout. Also the output shape Tranche 3's own
   * non-seekable HTTP upload sink needs (a live upload stream cannot seek
   * back to patch a header the way a local-file "faststart" rewrite
   * would). Added now, for Tranche 2's own fixture generation need, since
   * both tranches need the identical muxer capability. */
  CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED = 2,
} crtmedia_muxer_output_format;

typedef enum crtmedia_sink_capability {
  CRTMEDIA_SINK_WRITABLE = 1 << 1,
  CRTMEDIA_SINK_SEEKABLE = 1 << 2,
  CRTMEDIA_SINK_SIZE_KNOWN = 1 << 3,
} crtmedia_sink_capability;

crtmedia_result crtmedia_muxer_create(
    const char* path, crtmedia_muxer_output_format output_format,
    crtmedia_muxer** out_muxer);

/* Networking & Streaming Tranche 3: creates a forward-only HTTP PUT sink.
 * Only CRTMEDIA_MUXER_OUTPUT_MPEG_4_FRAGMENTED is accepted because a live
 * upload cannot seek back to rewrite a regular MP4 header. `queue_capacity`
 * is the hard upper bound for bytes waiting between FFmpeg and libcurl; 0
 * selects the implementation default. A full queue applies synchronous
 * back-pressure to crtmedia_muxer_write_sample() -- it never grows without
 * bound and never silently drops an encoded sample. The URL sink reports
 * CRTMEDIA_SINK_WRITABLE only; curl, FFmpeg, and host socket types remain
 * private implementation details. HTTP upload redirects and automatic
 * resume are deliberately unsupported in this tranche. */
crtmedia_result crtmedia_muxer_create_for_url(
    const char* url, crtmedia_muxer_output_format output_format,
    uint32_t queue_capacity, crtmedia_muxer** out_muxer);

/* Local-file muxers report WRITABLE | SEEKABLE. URL muxers report WRITABLE
 * only. Returns 0 for NULL. */
uint32_t crtmedia_muxer_get_capabilities(const crtmedia_muxer* muxer);
void crtmedia_muxer_release(crtmedia_muxer* muxer);

/* Adds one encoder output format before start and returns its stable track
 * index. The format is copied into the muxer's own FFmpeg-independent public
 * boundary; the caller may release it immediately after this call. */
crtmedia_result crtmedia_muxer_add_track(
    crtmedia_muxer* muxer, const crtmedia_format* format,
    uint32_t* out_track_index);

crtmedia_result crtmedia_muxer_start(crtmedia_muxer* muxer);
crtmedia_result crtmedia_muxer_write_sample(
    crtmedia_muxer* muxer, uint32_t track_index,
    const crtmedia_encoded_sample* sample);

/* Writes the container trailer and closes the output. Calling finish twice is
 * harmless. A failed finish still leaves release() safe to call. */
crtmedia_result crtmedia_muxer_finish(crtmedia_muxer* muxer);

#ifdef __cplusplus
}
#endif
