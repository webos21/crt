#pragma once

/* Private libcurl HTTP PUT consumer for Networking & Streaming Tranche 3.
 * FFmpeg writes fragmented MP4 bytes into this object's bounded transport
 * queue; libcurl's background worker reads the same queue. No curl or host
 * socket type crosses this header or any installed public header. */

#include <stddef.h>

#include "crtmedia/frame.h"

typedef struct crtmedia_http_upload crtmedia_http_upload;

crtmedia_result crtmedia_http_upload_open(
    const char* url, size_t queue_capacity, crtmedia_http_upload** out_upload);

/* Blocks under sustained receiver back-pressure. On success every requested
 * byte has entered the bounded queue, not necessarily the network socket. */
crtmedia_result crtmedia_http_upload_write(
    crtmedia_http_upload* upload, const void* data, size_t size);

/* Signals clean request-body EOF, waits for the HTTP response, and requires a
 * 2xx status. Idempotent after a successful finish. */
crtmedia_result crtmedia_http_upload_finish(crtmedia_http_upload* upload);

/* Cancels an unfinished upload, joins its worker, and releases it. */
void crtmedia_http_upload_close(crtmedia_http_upload* upload);
