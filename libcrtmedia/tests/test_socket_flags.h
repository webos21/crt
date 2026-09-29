#pragma once

/* send() flags for the loopback test fixtures (http_test_server.c,
 * http_upload_test_server.c, tls_test_server.c).
 *
 * These fixtures deliberately talk to peers that abort mid-stream (a client
 * refusing a certificate, a dropped connection), so a fixture write can land
 * on a closed socket. On Linux that raises SIGPIPE and silently kills the
 * whole test process (exit 141, no output) -- found running Networking &
 * Streaming Tranche 5's crtmedia_https_test on the physical Linux host;
 * Windows has no SIGPIPE. macOS has no MSG_NOSIGNAL at all and does hit it
 * (found replaying Networking & Streaming Tranche 6's crtmedia_http_
 * lifecycle_test on macOS: transport cancel cycles abandon an 8 MiB body
 * mid-send, exit 141, no output), so there the fixtures ignore SIGPIPE
 * process-wide instead (crtmedia_test_send() below). libcurl itself already
 * suppresses SIGPIPE on its own sends, so only the fixtures were exposed.
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

/* crtmedia_test_send(): send() with SIGPIPE suppressed on every host.
 * Linux: MSG_NOSIGNAL. macOS: no such flag, so SIGPIPE is set to SIG_IGN once
 * (fixture-only, a test process never wants the default kill). Windows: no
 * SIGPIPE, plain send(). */
#if defined(CRT_TARGET_OS_MACOS)
#include <signal.h>
#endif
static inline ssize_t crtmedia_test_send(int fd, const void* data, size_t size) {
#if defined(CRT_TARGET_OS_MACOS)
  static int sigpipe_ignored;
  if (!sigpipe_ignored) {
    signal(SIGPIPE, SIG_IGN);
    sigpipe_ignored = 1;
  }
#endif
  return send(fd, data, size, CRTMEDIA_TEST_SEND_FLAGS);
}
