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
} crtmedia_muxer_output_format;

crtmedia_result crtmedia_muxer_create(
    const char* path, crtmedia_muxer_output_format output_format,
    crtmedia_muxer** out_muxer);
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
