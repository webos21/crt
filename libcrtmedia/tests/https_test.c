// Networking & Streaming Tranche 5 acceptance (docs/crtmedia_networking_
// acceptance.md, "TLS trust policy"): HTTPS *authentication*, not merely
// "the TLS handshake and decrypt work". A repository-owned loopback TLS
// terminator (tests/tls_test_server.h, mbedTLS over CRT sockets, PKI
// generated in memory at test time) fronts the same plain HTTP fixture
// servers used by Tranches 2-4. The required matrix:
//
//   correct CA               -> connect succeeds, content byte-exact
//   wrong/unrelated CA       -> connect fails (verification failure)
//   correct CA, wrong SAN/IP -> connect fails (IP mismatch)
//   no trust anchors at all  -> connect fails (verify is ON by default)
//   explicit insecure opt-in -> connects (proves the opt-in is the only way)
//
// and the same policy for the upload sink, where a refused certificate must
// also mean the request body was never sent to any peer.

#include "http_test_server.h"
#include "http_upload.h"
#include "http_upload_test_server.h"
#include "tls_test_server.h"

#include "crtmedia/extractor.h"
#include "crtmedia/tls.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CRTMEDIA_TEST_VIDEO_PATH
#error "CRTMEDIA_TEST_VIDEO_PATH must be defined (see libcrtmedia/CMakeLists.txt)"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

typedef struct extraction_summary {
  crtmedia_result open_result;
  crtmedia_result end_result;
  uint32_t samples;
  uint64_t hash;
} extraction_summary;

static uint64_t fnv1a(uint64_t hash, const void* data, size_t size) {
  const unsigned char* bytes = (const unsigned char*)data;
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
  return hash;
}

static void extract_all(crtmedia_extractor* extractor, extraction_summary* out) {
  uint32_t tracks = crtmedia_extractor_track_count(extractor);
  for (uint32_t i = 0; i < tracks; ++i) {
    crtmedia_extractor_select_track(extractor, i);
  }
  out->hash = 1469598103934665603ull;
  for (;;) {
    crtmedia_sample sample;
    int eof = 0;
    crtmedia_result r = crtmedia_extractor_read_sample(extractor, &sample, &eof);
    if (r != CRTMEDIA_OK) {
      out->end_result = r;
      return;
    }
    if (eof) {
      out->end_result = CRTMEDIA_OK;
      return;
    }
    out->hash = fnv1a(out->hash, &sample.track_index, sizeof(sample.track_index));
    out->hash = fnv1a(out->hash, &sample.pts_us, sizeof(sample.pts_us));
    out->hash = fnv1a(out->hash, &sample.size, sizeof(sample.size));
    out->hash = fnv1a(out->hash, sample.data, sample.size);
    ++out->samples;
    crtmedia_sample_release(&sample);
  }
}

static void extract_https(const char* url, const crtmedia_tls_options* tls, extraction_summary* out) {
  memset(out, 0, sizeof(*out));
  crtmedia_extractor* extractor = NULL;
  out->open_result = crtmedia_extractor_create_from_url_with_tls(url, tls, &extractor);
  if (out->open_result != CRTMEDIA_OK) {
    return;
  }
  extract_all(extractor, out);
  crtmedia_extractor_release(extractor);
}

static void* read_file(const char* path, size_t* out_size) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  void* data = size >= 0 ? malloc((size_t)size) : NULL;
  if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *out_size = (size_t)size;
  return data;
}

static int refused(const extraction_summary* s) {
  return s->open_result == CRTMEDIA_ERROR_PROTOCOL;
}

int main(void) {
  size_t body_size = 0;
  unsigned char* body = (unsigned char*)read_file(CRTMEDIA_TEST_VIDEO_PATH, &body_size);
  if (body == NULL) {
    fprintf(stderr, "crtmedia_https_test: cannot load fixture\n");
    return 1;
  }

  extraction_summary local;
  memset(&local, 0, sizeof(local));
  {
    crtmedia_extractor* extractor = NULL;
    CHECK(crtmedia_extractor_create(CRTMEDIA_TEST_VIDEO_PATH, &extractor) == CRTMEDIA_OK, "open local reference");
    if (extractor == NULL) {
      free(body);
      return 1;
    }
    extract_all(extractor, &local);
    crtmedia_extractor_release(extractor);
  }

  http_test_server* backend = NULL;
  int backend_port = 0;
  CHECK(http_test_server_start(body, body_size, HTTP_TEST_SERVER_RANGE_CAPABLE, &backend, &backend_port) == 0,
        "start plain HTTP backend");
  tls_test_proxy* good = NULL;
  tls_test_proxy* wrong_san = NULL;
  int good_port = 0;
  int wrong_san_port = 0;
  const char* ca_pem = NULL;
  const char* other_ca_pem = NULL;
  const char* unused_a = NULL;
  const char* unused_b = NULL;
  CHECK(tls_test_proxy_start(backend_port, TLS_TEST_SAN_LOOPBACK, &good, &good_port, &ca_pem, &other_ca_pem) == 0,
        "start TLS terminator with a 127.0.0.1 SAN");
  CHECK(tls_test_proxy_start(backend_port, TLS_TEST_SAN_WRONG_IP, &wrong_san, &wrong_san_port, &unused_a,
                             &unused_b) == 0,
        "start TLS terminator with a mismatching SAN");
  if (backend == NULL || good == NULL || wrong_san == NULL) {
    free(body);
    return 1;
  }
  char url[128];
  char wrong_san_url[128];
  snprintf(url, sizeof(url), "https://127.0.0.1:%d/test.mp4", good_port);
  snprintf(wrong_san_url, sizeof(wrong_san_url), "https://127.0.0.1:%d/test.mp4", wrong_san_port);

  crtmedia_tls_options tls;
  extraction_summary result;

  /* Correct CA: authenticated, and the bytes over TLS are the real media. */
  memset(&tls, 0, sizeof(tls));
  tls.ca_pem = ca_pem;
  extract_https(url, &tls, &result);
  CHECK(result.open_result == CRTMEDIA_OK && result.end_result == CRTMEDIA_OK, "correct CA: extraction succeeds");
  CHECK(result.samples == local.samples && result.hash == local.hash, "correct CA: samples identical to local file");
  CHECK(tls_test_proxy_handshakes_completed(good) >= 1, "correct CA: a real TLS handshake completed");

  /* Wrong (unrelated) CA. */
  const int handshakes_before = tls_test_proxy_handshakes_completed(good);
  memset(&tls, 0, sizeof(tls));
  tls.ca_pem = other_ca_pem;
  extract_https(url, &tls, &result);
  CHECK(refused(&result), "wrong CA: refused with CRTMEDIA_ERROR_PROTOCOL");
  CHECK(tls_test_proxy_handshakes_completed(good) == handshakes_before,
        "wrong CA: client aborted the handshake before any application data");

  /* Correct CA, but the server certificate does not cover 127.0.0.1. */
  memset(&tls, 0, sizeof(tls));
  tls.ca_pem = ca_pem;
  extract_https(wrong_san_url, &tls, &result);
  CHECK(refused(&result), "wrong SAN/IP: refused with CRTMEDIA_ERROR_PROTOCOL");
  CHECK(tls_test_proxy_handshakes_completed(wrong_san) == 0, "wrong SAN/IP: no handshake completed");

  /* No trust anchors at all, both spellings: verification is ON by default. */
  extract_https(url, NULL, &result);
  CHECK(refused(&result), "default (NULL options): refused, verification is on by default");
  memset(&tls, 0, sizeof(tls));
  extract_https(url, &tls, &result);
  CHECK(refused(&result), "empty options: refused, there is no implicit trust store");

  /* The explicit, loudly named opt-in is the only way to skip verification. */
  memset(&tls, 0, sizeof(tls));
  tls.insecure_skip_verify_for_local_development = 1;
  extract_https(wrong_san_url, &tls, &result);
  CHECK(result.open_result == CRTMEDIA_OK && result.hash == local.hash,
        "explicit insecure opt-in connects even to a mismatching certificate");

  /* Plain http:// is unaffected by TLS options (no accidental https-only). */
  {
    char plain_url[128];
    snprintf(plain_url, sizeof(plain_url), "http://127.0.0.1:%d/test.mp4", backend_port);
    memset(&tls, 0, sizeof(tls));
    extract_https(plain_url, &tls, &result);
    CHECK(result.open_result == CRTMEDIA_OK && result.hash == local.hash, "http:// still works with TLS options set");
  }

  tls_test_proxy_stop(good);
  tls_test_proxy_stop(wrong_san);
  http_test_server_stop(backend);

  /* Upload sink: same policy, and a refused certificate never leaks the body. */
  enum { UPLOAD_BYTES = 200 * 1024 };
  unsigned char* payload = (unsigned char*)malloc(UPLOAD_BYTES);
  for (int i = 0; i < UPLOAD_BYTES; ++i) {
    payload[i] = (unsigned char)(i * 31 + 7);
  }
  {
    http_upload_test_server* upload_backend = NULL;
    int upload_backend_port = 0;
    CHECK(http_upload_test_server_start(&upload_backend, &upload_backend_port) == 0, "start upload backend");
    tls_test_proxy* upload_proxy = NULL;
    int upload_port = 0;
    CHECK(tls_test_proxy_start(upload_backend_port, TLS_TEST_SAN_LOOPBACK, &upload_proxy, &upload_port, &ca_pem,
                               &other_ca_pem) == 0,
          "start TLS terminator for upload");
    char upload_url[128];
    snprintf(upload_url, sizeof(upload_url), "https://127.0.0.1:%d/upload.bin", upload_port);

    /* Wrong CA first: refused, and the backend never sees a connection. */
    memset(&tls, 0, sizeof(tls));
    tls.ca_pem = other_ca_pem;
    crtmedia_http_upload* upload = NULL;
    CHECK(crtmedia_http_upload_open(upload_url, 4096, &tls, &upload) == CRTMEDIA_OK, "open upload (wrong CA)");
    crtmedia_result write_result = CRTMEDIA_OK;
    for (int offset = 0; offset < UPLOAD_BYTES && write_result == CRTMEDIA_OK; offset += 8192) {
      write_result = crtmedia_http_upload_write(upload, payload + offset, 8192);
    }
    crtmedia_result finish_result = crtmedia_http_upload_finish(upload);
    CHECK(write_result == CRTMEDIA_ERROR_PROTOCOL || finish_result == CRTMEDIA_ERROR_PROTOCOL,
          "upload with the wrong CA is refused with CRTMEDIA_ERROR_PROTOCOL");
    crtmedia_http_upload_close(upload);
    CHECK(http_upload_test_server_connection_count(upload_backend) == 0,
          "refused certificate: the body never reached any server");

    /* Correct CA: uploaded byte-exact through TLS. */
    memset(&tls, 0, sizeof(tls));
    tls.ca_pem = ca_pem;
    upload = NULL;
    CHECK(crtmedia_http_upload_open(upload_url, 4096, &tls, &upload) == CRTMEDIA_OK, "open upload (correct CA)");
    write_result = CRTMEDIA_OK;
    for (int offset = 0; offset < UPLOAD_BYTES && write_result == CRTMEDIA_OK; offset += 8192) {
      write_result = crtmedia_http_upload_write(upload, payload + offset, 8192);
    }
    CHECK(write_result == CRTMEDIA_OK, "upload over TLS with the correct CA: writes accepted");
    CHECK(crtmedia_http_upload_finish(upload) == CRTMEDIA_OK, "upload over TLS with the correct CA: 2xx");
    crtmedia_http_upload_close(upload);
    void* received = NULL;
    size_t received_size = 0;
    int used_chunked = 0;
    CHECK(http_upload_test_server_wait_and_copy(upload_backend, &received, &received_size, &used_chunked) == 0,
          "backend received the upload");
    CHECK(received_size == UPLOAD_BYTES && received != NULL && memcmp(received, payload, UPLOAD_BYTES) == 0,
          "uploaded body is byte-exact after TLS");
    free(received);
    tls_test_proxy_stop(upload_proxy);
    http_upload_test_server_stop(upload_backend);
  }
  free(payload);
  free(body);

  if (failures != 0) {
    fprintf(stderr, "crtmedia_https_test: %d failure(s)\n", failures);
    return 1;
  }
  printf(
      "crtmedia_https_test: ok correct_ca=pass wrong_ca=protocol wrong_san=protocol default=protocol "
      "insecure_opt_in=pass upload_correct_ca=pass upload_wrong_ca=protocol samples=%u\n",
      local.samples);
  return 0;
}
