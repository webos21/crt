#pragma once

/* Test-only loopback TLS terminator for Networking & Streaming Tranche 5
 * (docs/crtmedia_networking_acceptance.md, "TLS trust policy").
 *
 * Generates a fresh PKI in memory on every start (never a checked-in
 * certificate with a hardcoded validity window): an ECDSA P-256 CA, a
 * second unrelated CA, and a server certificate signed by the first CA. It
 * then accepts TLS 1.2 connections with mbedTLS (the same TLS stack the
 * production libcurl port uses) over this project's own CRT sockets and
 * relays the decrypted bytes to a plain HTTP test server already listening
 * on 127.0.0.1:<backend_port> (tests/http_test_server.h or
 * tests/http_upload_test_server.h). The relay only connects to that
 * backend after a TLS handshake succeeded, so a client that refuses the
 * certificate never causes a backend connection at all. */

#include <stddef.h>

typedef struct tls_test_proxy tls_test_proxy;

typedef enum tls_test_san {
  TLS_TEST_SAN_LOOPBACK, /* server cert covers IP:127.0.0.1 */
  TLS_TEST_SAN_WRONG_IP, /* server cert covers only IP:10.255.255.1 */
} tls_test_san;

/* On success: *out_port is the TLS listening port; *out_ca_pem is the PEM of
 * the CA that signed the server certificate; *out_other_ca_pem is the PEM of
 * an unrelated CA. Both PEM strings are NUL-terminated and owned by the
 * proxy (valid until tls_test_proxy_stop()). Returns 0 on success. */
int tls_test_proxy_start(
    int backend_port, tls_test_san san, tls_test_proxy** out_proxy, int* out_port,
    const char** out_ca_pem, const char** out_other_ca_pem);

/* Number of TLS handshakes that completed successfully. */
int tls_test_proxy_handshakes_completed(tls_test_proxy* proxy);

void tls_test_proxy_stop(tls_test_proxy* proxy);
