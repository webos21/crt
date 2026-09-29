#pragma once

/* Repository-owned loopback HTTP/1.1 PUT receiver for Networking &
 * Streaming Tranche 3. It deliberately consumes request-body bytes slowly,
 * so the production upload path must cross its bounded queue/back-pressure
 * boundary instead of succeeding only against an infinitely fast sink. */

#include <stddef.h>

typedef struct http_upload_test_server http_upload_test_server;

int http_upload_test_server_start(http_upload_test_server** out_server, int* out_port);

/* Waits for the one accepted upload and copies the exact received request
 * body. The caller owns *out_data and frees it with free(). */
int http_upload_test_server_wait_and_copy(
    http_upload_test_server* server, void** out_data, size_t* out_size,
    int* out_used_chunked_encoding);

void http_upload_test_server_stop(http_upload_test_server* server);
