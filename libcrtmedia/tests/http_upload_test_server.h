#pragma once

/* Repository-owned loopback HTTP/1.1 PUT receiver for Networking &
 * Streaming Tranche 3. It deliberately consumes request-body bytes slowly,
 * so the production upload path must cross its bounded queue/back-pressure
 * boundary instead of succeeding only against an infinitely fast sink. */

#include <stddef.h>

typedef struct http_upload_test_server http_upload_test_server;

int http_upload_test_server_start(http_upload_test_server** out_server, int* out_port);

/* Networking & Streaming Tranche 4 fault injection: the receiver closes the
 * connection with no response once `drop_after_bytes` request-body bytes have
 * arrived (a server dying mid-upload), and -- unlike the normal one-shot
 * receiver -- keeps accepting so the test can prove the client did NOT
 * silently open a second connection to resume. */
int http_upload_test_server_start_dropping(
    size_t drop_after_bytes, http_upload_test_server** out_server, int* out_port);

/* Number of connections accepted so far. */
int http_upload_test_server_connection_count(http_upload_test_server* server);

/* Networking & Streaming Tranche 4 fault injection: the receiver closes the
 * connection with no response once `drop_after_bytes` request-body bytes have
 * arrived (a server dying mid-upload), and -- unlike the normal one-shot
 * receiver -- keeps accepting so the test can prove the client did NOT
 * silently open a second connection to resume. */
int http_upload_test_server_start_dropping(
    size_t drop_after_bytes, http_upload_test_server** out_server, int* out_port);

/* Number of connections accepted so far. */
int http_upload_test_server_connection_count(http_upload_test_server* server);

/* Waits for the one accepted upload and copies the exact received request
 * body. The caller owns *out_data and frees it with free(). */
int http_upload_test_server_wait_and_copy(
    http_upload_test_server* server, void** out_data, size_t* out_size,
    int* out_used_chunked_encoding);

void http_upload_test_server_stop(http_upload_test_server* server);
