#pragma once

/* Networking & Streaming Tranche 5 (docs/acceptance/crtmedia_networking_acceptance.md,
 * "TLS trust policy"). https:// URLs are authenticated by default:
 * certificate chain and host name/IP verification are always on, and CRT
 * vendors no CA store, so the caller supplies the trust anchors. No libcurl,
 * mbedTLS, or host TLS type appears in this header. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtmedia_tls_options {
  /* NUL-terminated PEM text holding one or more CA certificates that the
   * server certificate must chain to. Copied by the callee; may be freed
   * as soon as the create call returns. NULL means "no caller-supplied
   * trust anchors": with verification on (the default) every https://
   * connection then fails, because CRT has no system store to fall back
   * on. Ignored for http:// URLs. */
  const char* ca_pem;

  /* Deliberately loud, non-default opt-out of certificate and host
   * verification, for local development only. Any nonzero value disables
   * ALL server authentication for this stream: the peer can be anyone.
   * Never set this in shipping code. */
  int insecure_skip_verify_for_local_development;
} crtmedia_tls_options;

#ifdef __cplusplus
}
#endif
