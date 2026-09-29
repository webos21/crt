#pragma once

/* send() flags for the loopback test fixtures (http_test_server.c,
 * http_upload_test_server.c, tls_test_server.c).
 *
 * These fixtures deliberately talk to peers that abort mid-stream (a client
 * refusing a certificate, a dropped connection), so a fixture write can land
 * on a closed socket. On Linux that raises SIGPIPE and silently kills the
 * whole test process (exit 141, no output) -- found running Networking &
 * Streaming Tranche 5's crtmedia_https_test on the physical Linux host;
 * Windows has no SIGPIPE and macOS did not hit it. libcurl itself already
 * passes MSG_NOSIGNAL on its own sends, so only the fixtures were exposed.
 * This project's public <sys/socket.h> does not currently expose MSG_NOSIGNAL,
 * so use the Linux ABI value (0x4000, asm-generic/socket.h) when the header
 * lacks it; other hosts keep flags 0. */
#include <sys/socket.h>

#ifdef MSG_NOSIGNAL
#define CRTMEDIA_TEST_SEND_FLAGS MSG_NOSIGNAL
#elif defined(CRT_TARGET_OS_LINUX)
#define CRTMEDIA_TEST_SEND_FLAGS 0x4000
#else
#define CRTMEDIA_TEST_SEND_FLAGS 0
#endif
