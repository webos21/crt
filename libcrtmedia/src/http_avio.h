#pragma once

/* Networking & Streaming Tranche 2 (docs/crtmedia_networking_acceptance.md)
 * -- a private FFmpeg custom AVIOContext wrapping http_transport.h. This is
 * the seam between the CRT-owned HTTP transport and FFmpeg's own demuxer:
 * everything below this file only ever deals in raw bytes
 * (crtmedia_http_transport); everything above it (crtmedia_extractor) only
 * ever deals in a plain AVIOContext*, exactly like the existing file-based
 * open path's own plain filename. Private header (libcrtmedia/src/), never
 * installed.
 *
 * Seek-via-reconnect: HTTP has no in-place seek on an open connection, so
 * crtmedia_http_avio_open()'s own AVIOContext seek callback closes the
 * current transport and opens a brand new one at the target byte offset
 * (http_transport.h's own validated-Range-response contract -- never a
 * silent, unvalidated jump). Only wired in when the initial open proved
 * the source seekable (a validated 206 response); a non-seekable source's
 * AVIOContext gets no seek callback at all (NULL), matching FFmpeg's own
 * documented "seek not supported" convention and this project's own
 * source/sink capability contract (docs/crtmedia_networking_acceptance.md
 * -- "no public API may imply that a non-seekable stream supports
 * arbitrary seek"). */

#include "crtmedia/frame.h"

#include <libavformat/avio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtmedia_http_avio crtmedia_http_avio;

typedef struct crtmedia_http_avio_info {
  int seekable;
  int64_t size; /* -1 if unknown */
} crtmedia_http_avio_info;

crtmedia_result crtmedia_http_avio_open(
    const char* url, crtmedia_http_avio** out_avio, crtmedia_http_avio_info* out_info);

/* Borrowed pointer, valid until crtmedia_http_avio_close(). The caller
 * (crtmedia_extractor.c) assigns this directly to AVFormatContext::pb. */
AVIOContext* crtmedia_http_avio_context(crtmedia_http_avio* avio);

void crtmedia_http_avio_close(crtmedia_http_avio* avio);

#ifdef __cplusplus
}
#endif
