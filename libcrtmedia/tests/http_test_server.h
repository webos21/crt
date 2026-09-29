#pragma once

/* A minimal, repository-owned loopback HTTP/1.1 server, test-only
 * infrastructure for Networking & Streaming Tranche 2 (docs/crtmedia_
 * networking_acceptance.md) -- no production coupling at all, built
 * entirely on this project's own <sys/socket.h>/<netinet/in.h> (the same
 * calling convention libc/tests/socket_network_test.c already proves),
 * never a host raw-socket API.
 *
 * Serves one fixed, caller-owned byte buffer for every GET request on any
 * path, in one of two modes matching the acceptance doc's own two-fixture
 * split (fixture A vs. fixture B -- kept deliberately separate rather than
 * conflated, so a failure points at a Range bug, a custom-AVIO bug, or an
 * HTTP-chunk-parsing bug instead of all three at once):
 *
 *   HTTP_TEST_SERVER_RANGE_CAPABLE: honors a real "Range: bytes=N-[M]"
 *     request header with a real "206 Partial Content"/"Content-Range"
 *     response and advertises "Accept-Ranges: bytes" on every response,
 *     including the initial unranged one.
 *   HTTP_TEST_SERVER_CHUNKED_NO_RANGE: ignores any Range header entirely
 *     (always a plain "200 OK"), sends the body with a real "Transfer-
 *     Encoding: chunked" framing and no Content-Length at all.
 *
 * Handles a bounded number of sequential connections, not just one --
 * fixture A's own seek-via-reconnect path (http_avio.c) opens a brand new
 * connection with a new Range request after a seek, so a real test needs
 * more than a single request/response round trip. */

#include <stddef.h>

typedef struct http_test_server http_test_server;

typedef enum http_test_server_mode {
  HTTP_TEST_SERVER_RANGE_CAPABLE,
  HTTP_TEST_SERVER_CHUNKED_NO_RANGE,
} http_test_server_mode;

/* Networking & Streaming Tranche 4 fault injection (all default to "off",
 * i.e. the plain Tranche 2 behavior, when the options struct is zeroed).
 * Only meaningful in HTTP_TEST_SERVER_RANGE_CAPABLE mode. */
typedef struct http_test_server_options {
  /* Entity validator sent as `ETag: "<etag>"` on every response; NULL = the
   * server offers no validator at all (so a client can never prove a resume
   * safe). */
  const char* etag;
  /* The first `truncate_connections` connections send the full headers
   * (Content-Length is the honest full length) but close after only
   * `truncate_after_bytes` body bytes -- a server closing mid-response. */
  int truncate_connections;
  size_t truncate_after_bytes;
  /* Every connection after the first accepts and immediately closes without
   * any response: a resumed request that never succeeds. */
  int refuse_after_first;
  /* From the second connection on, the resource "changed": ETag becomes
   * "<etag>-changed". With honor_if_range, a mismatching If-Range gets a
   * full 200 (RFC 9110 behavior); without it the server ignores If-Range and
   * answers 206 carrying the new ETag -- the two shapes a client must both
   * refuse to splice. */
  int change_etag_after_first;
  int honor_if_range;
} http_test_server_options;

typedef struct http_test_server_stats {
  int requests;                    /* every accepted connection with a parsed request */
  int ranged_requests;             /* requests carrying a Range header */
  int resume_requests;             /* Range with start > 0 */
  int resume_requests_with_if_range; /* of those, how many carried If-Range */
  int last_range_start;            /* start offset of the most recent Range, or -1 */
} http_test_server_stats;

/* Like http_test_server_start() with fault-injection options. */
int http_test_server_start_ex(
    const void* body, size_t body_size, http_test_server_mode mode, const http_test_server_options* options,
    http_test_server** out_server, int* out_port);

/* Snapshot of what the server has seen so far. */
void http_test_server_get_stats(http_test_server* server, http_test_server_stats* out_stats);

/* Starts the server on a background thread bound to 127.0.0.1 with an
 * OS-assigned port (*out_port). `body` must outlive the server (this
 * project's own established "caller owns the buffer" convention, matching
 * crtmedia_frame's own plane-ownership model) -- the server only ever
 * reads from it. Returns 0 on success, -1 on a real failure (out_port left
 * at 0 in that case). */
int http_test_server_start(
    const void* body, size_t body_size, http_test_server_mode mode, http_test_server** out_server, int* out_port);

/* Stops accepting new connections, lets any in-flight response finish
 * naturally, joins the background thread, and frees everything. NULL is a
 * no-op. */
void http_test_server_stop(http_test_server* server);
