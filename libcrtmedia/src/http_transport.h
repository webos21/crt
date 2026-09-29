#pragma once

/* Networking & Streaming Tranche 2 (docs/crtmedia_networking_acceptance.md)
 * -- a libcurl-backed HTTP GET producer feeding a private crtmedia_
 * transport_queue (transport_queue.h). This is the only file in
 * libcrtmedia that #includes <curl/curl.h>; everything above this layer
 * (http_avio.h, crtmedia_extractor) only ever sees crtmedia types. Private
 * header (libcrtmedia/src/), never installed.
 *
 * Always issues a byte-range request, `Range: bytes=<offset>-`, even for
 * offset 0 -- this is the one way to *validate* Range support rather than
 * assume it from an Accept-Ranges header claim (docs/crtmedia_networking_
 * acceptance.md's own source/sink capability section: "HTTP with
 * validated Range response -> seekable"), and it is exactly the same rule
 * Tranche 4's own reconnect contract needs later, exercised here in
 * miniature for capability detection. crtmedia_http_transport_open()
 * blocks until the response headers for the *final* hop (after any
 * redirects) are known, populating *out_info, then returns while the body
 * keeps streaming into the queue in the background:
 *
 *   206 Partial Content, Content-Range start == offset  -> success,
 *     out_info->seekable = 1 (this server can be re-opened at an
 *     arbitrary offset later, e.g. for Tranche 4's reconnect or a seek)
 *   200 OK (server ignored the Range header), offset == 0 -> success,
 *     out_info->seekable = 0 (a legitimate, expected outcome for a
 *     chunked/non-seekable source -- Tranche 2's own fixture B)
 *   200 OK, offset != 0 -> CRTMEDIA_ERROR_PROTOCOL (the server cannot
 *     actually give us the byte range we asked for -- never silently
 *     substitute the wrong bytes)
 *   any other status, or a real connection failure before headers ever
 *     arrived -> a real error (CRTMEDIA_ERROR_PROTOCOL / _IO / _TIMEOUT),
 *     never a hang: the background worker thread signals open()'s own
 *     wait even when curl_easy_perform() fails before any header line was
 *     seen.
 *
 * A transfer that fails or the connection drops *after* headers already
 * validated marks the queue with crtmedia_transport_queue_write_error()
 * (not a clean EOF) -- see that header's own top comment for why this
 * distinction matters for a caller reading the tail of the stream. */

#include <stddef.h>
#include <stdint.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtmedia_http_transport crtmedia_http_transport;

typedef struct crtmedia_http_transport_info {
  int seekable;      /* 1 if this exact request got a validated 206 */
  int64_t size;      /* total resource size in bytes if known, else -1 */
  long status_code;  /* the final (post-redirect) HTTP status */
} crtmedia_http_transport_info;

/* queue_capacity is the internal bounded transport_queue's own capacity
 * (transport_queue.h) -- the same host-neutral primitive Tranche 1
 * already proved, unmodified here except for the new sticky-error path
 * that header's own top comment documents. */
crtmedia_result crtmedia_http_transport_open(
    const char* url, int64_t offset, size_t queue_capacity, crtmedia_http_transport** out_transport,
    crtmedia_http_transport_info* out_info);

/* Thin passthrough to the internal transport_queue's own read() -- see
 * transport_queue.h for the full timeout_ms/out_eof/sticky-error
 * contract. */
crtmedia_result crtmedia_http_transport_read(
    crtmedia_http_transport* transport, void* data, size_t capacity, int timeout_ms, size_t* out_read, int* out_eof);

/* Cancels the in-flight request (if any), joins the background worker
 * thread, then frees everything. Safe to call at any point after a
 * successful open(), even mid-transfer. NULL is a no-op. */
void crtmedia_http_transport_close(crtmedia_http_transport* transport);

#ifdef __cplusplus
}
#endif
