/* Real curl round trip against real servers, not just a version-string or
 * link-only smoke check:
 *  1. A plain HTTP GET (proving the portable POSIX socket path this PAL
 *     implements -- socket/connect/send/recv/select/poll -- actually
 *     drives a real TCP connection and curl correctly parses a real HTTP
 *     response).
 *  2. An HTTPS GET (proving the mbedTLS backend this recipe wires up
 *     actually performs a real TLS handshake and decrypts the response
 *     correctly -- the response body could not come back intelligible
 *     otherwise).
 * Both requests target http(s)://example.com, an IANA-reserved domain
 * kept stable and minimal specifically for use in documentation/testing
 * like this (see RFC 2606) -- its response body has included the string
 * "Example Domain" for years, making it a reasonable stable known-answer
 * check for a regression test.
 *
 * CURLOPT_SSL_VERIFYPEER/VERIFYHOST are disabled for the HTTPS request:
 * this project doesn't vendor or maintain a CA trust bundle (see
 * curl.json's own notes on --without-ca-bundle/--without-ca-path), so
 * there is no certificate chain to validate against here. This does NOT
 * weaken what the test actually proves -- the TLS handshake and record
 * encryption/decryption still have to succeed for the response body to
 * decode into readable HTML at all, which is the mechanism this port
 * needs verified (mbedTLS's own certificate-parsing/verification logic
 * is already exercised and tested far more thoroughly upstream than a
 * single regression test here could add to). A real deployment consuming
 * this port is expected to supply its own CA bundle via
 * CURLOPT_CAINFO/CURLOPT_CAINFO_BLOB.
 */
#include <curl/curl.h>

#include <stdio.h>
#include <string.h>

struct response_buffer {
  char data[8192];
  size_t len;
};

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  struct response_buffer* buf = (struct response_buffer*)userdata;
  size_t chunk = size * nmemb;
  if (buf->len + chunk >= sizeof(buf->data)) {
    chunk = sizeof(buf->data) - buf->len - 1;
  }
  memcpy(buf->data + buf->len, ptr, chunk);
  buf->len += chunk;
  buf->data[buf->len] = '\0';
  return size * nmemb;
}

static int fetch(const char* url, int verify_tls, long* out_status, struct response_buffer* buf) {
  CURL* easy = curl_easy_init();
  CURLcode res;

  if (!easy) {
    fprintf(stderr, "curl_http_roundtrip_test: curl_easy_init failed\n");
    return -1;
  }

  buf->len = 0;
  buf->data[0] = '\0';

  curl_easy_setopt(easy, CURLOPT_URL, url);
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(easy, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(easy, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
  if (!verify_tls) {
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  res = curl_easy_perform(easy);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_http_roundtrip_test: %s failed: %s\n", url, curl_easy_strerror(res));
    curl_easy_cleanup(easy);
    return -1;
  }

  curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, out_status);
  curl_easy_cleanup(easy);
  return 0;
}

int main(void) {
  struct response_buffer buf;
  long status;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  /* HTTP round trip. */
  if (fetch("http://example.com/", 1, &status, &buf) != 0) {
    curl_global_cleanup();
    return 1;
  }
  if (status != 200) {
    fprintf(stderr, "curl_http_roundtrip_test: http status=%ld (want 200)\n", status);
    curl_global_cleanup();
    return 1;
  }
  if (strstr(buf.data, "Example Domain") == NULL) {
    fprintf(stderr, "curl_http_roundtrip_test: http body missing expected content\n");
    curl_global_cleanup();
    return 1;
  }

  /* HTTPS round trip (TLS handshake + decrypt via the mbedTLS backend). */
  if (fetch("https://example.com/", 0, &status, &buf) != 0) {
    curl_global_cleanup();
    return 1;
  }
  if (status != 200) {
    fprintf(stderr, "curl_http_roundtrip_test: https status=%ld (want 200)\n", status);
    curl_global_cleanup();
    return 1;
  }
  if (strstr(buf.data, "Example Domain") == NULL) {
    fprintf(stderr, "curl_http_roundtrip_test: https body missing expected content\n");
    curl_global_cleanup();
    return 1;
  }

  curl_global_cleanup();
  printf("curl_http_roundtrip_test: ok http=200 https=200\n");
  return 0;
}
