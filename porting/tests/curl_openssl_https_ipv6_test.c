/* HTTPS over IPv6 through the OpenSSL-backed libcurl (porting/recipes/curl-openssl.json). The server
 * thread listens on ::1 (CRT libc's AF_INET6, not an Apple framework or host socket shape); curl,
 * with CURLOPT_IPRESOLVE forced to IPv6, fetches https://[::1]:<port>/ and, where the libc resolves
 * names to IPv6 (macOS: the host resolver, translated), https://localhost:<port>/ too. The test
 * requires libcurl to report IPv6 support, the body, the OpenSSL 3 backend, the SSL_CTX hook, and
 * that the connection really went to ::1 (CURLINFO_PRIMARY_IP). */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

static int listener = -1;
static EVP_PKEY* server_key;
static X509* server_cert;
static int ctx_callback_calls;
static int expected_connections = 1;
static int ctx_was_ssl_ctx;

static int make_credentials(void) {
  EVP_PKEY_CTX* key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  X509_NAME* name;
  if (!key_context || EVP_PKEY_keygen_init(key_context) <= 0 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(key_context, NID_X9_62_prime256v1) <= 0 ||
      EVP_PKEY_keygen(key_context, &server_key) <= 0) {
    return 0;
  }
  EVP_PKEY_CTX_free(key_context);
  server_cert = X509_new();
  X509_set_version(server_cert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(server_cert), 7);
  X509_gmtime_adj(X509_getm_notBefore(server_cert), 0);
  X509_gmtime_adj(X509_getm_notAfter(server_cert), 3600);
  X509_set_pubkey(server_cert, server_key);
  name = X509_get_subject_name(server_cert);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"::1", -1, -1, 0);
  X509_set_issuer_name(server_cert, name);
  return X509_sign(server_cert, server_key, EVP_sha256()) != 0;
}

static void* serve(void* unused) {
  SSL_CTX* context = SSL_CTX_new(TLS_server_method());
  int client;
  (void)unused;
  SSL_CTX_use_certificate(context, server_cert);
  SSL_CTX_use_PrivateKey(context, server_key);
  for (int served = 0; served < expected_connections; ++served) {
    client = accept(listener, NULL, NULL);
    if (client < 0) {
      break;
    }
    {
    SSL* ssl = SSL_new(context);
    char request[2048];
    static const char response[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\nConnection: close\r\n\r\ncrt-https-ok";
    SSL_set_fd(ssl, client);
    if (SSL_accept(ssl) == 1) {
      SSL_read(ssl, request, sizeof request);
      SSL_write(ssl, response, (int)sizeof response - 1);
      SSL_shutdown(ssl);
    }
    SSL_free(ssl);
    close(client);
    }
  }
  SSL_CTX_free(context);
  return NULL;
}

static size_t collect(char* data, size_t size, size_t count, void* user) {
  strncat((char*)user, data, size * count < 63 ? size * count : 63);
  return size * count;
}

static CURLcode on_ssl_ctx(CURL* curl, void* ssl_ctx, void* user) {
  (void)curl;
  (void)user;
  ++ctx_callback_calls;
  /* A real SSL_CTX: its verify mode can be read and a callback installed. */
  ctx_was_ssl_ctx = ssl_ctx != NULL && SSL_CTX_get_verify_mode((SSL_CTX*)ssl_ctx) >= 0;
  return CURLE_OK;
}

static int fetch(const char* url, const char* expect_ip, const char* label) {
  CURL* curl = curl_easy_init();
  char body[64] = "";
  char* primary_ip = NULL;
  CURLcode code;

  ctx_callback_calls = 0;
  ctx_was_ssl_ctx = 0;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, (long)CURL_IPRESOLVE_V6);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, on_ssl_ctx);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  code = curl_easy_perform(curl);
  if (code == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primary_ip);
  }
  if (code != CURLE_OK) {
    fprintf(stderr, "curl_openssl_https_ipv6_test: %s: curl failed: %s\n", label, curl_easy_strerror(code));
    curl_easy_cleanup(curl);
    return 0;
  }
  if (strcmp(body, "crt-https-ok") != 0 || ctx_callback_calls != 1 || !ctx_was_ssl_ctx ||
      primary_ip == NULL || strcmp(primary_ip, expect_ip) != 0) {
    fprintf(stderr, "curl_openssl_https_ipv6_test: %s: body '%s', ssl ctx %d/%d, ip '%s'\n", label, body,
            ctx_callback_calls, ctx_was_ssl_ctx, primary_ip ? primary_ip : "(none)");
    curl_easy_cleanup(curl);
    return 0;
  }
  curl_easy_cleanup(curl);
  return 1;
}

int main(void) {
  struct sockaddr_in6 address;
  socklen_t length = sizeof address;
  pthread_t thread;
  char url[64];
  curl_version_info_data* version;
  int ok;

  alarm(90); /* a failed first fetch would leave the server thread waiting for the second */
  if (!make_credentials()) {
    fprintf(stderr, "curl_openssl_https_ipv6_test: credentials\n");
    return 1;
  }
  listener = socket(AF_INET6, SOCK_STREAM, 0);
  if (listener < 0) {
    printf("curl_openssl_https_ipv6_test: ok (no IPv6 sockets here)\n");
    return 0;
  }
  memset(&address, 0, sizeof address);
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_loopback;
  if (bind(listener, (struct sockaddr*)&address, sizeof address) != 0 || listen(listener, 2) != 0 ||
      getsockname(listener, (struct sockaddr*)&address, &length) != 0) {
#if defined(__APPLE__)
    fprintf(stderr, "curl_openssl_https_ipv6_test: cannot listen on ::1\n");
    return 1;
#else
    printf("curl_openssl_https_ipv6_test: ok (no ::1 here)\n");
    return 0;
#endif
  }
#if defined(__APPLE__)
  expected_connections = 2;
#endif
  pthread_create(&thread, NULL, serve, NULL);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  version = curl_version_info(CURLVERSION_NOW);
  if (version->ssl_version == NULL || strncmp(version->ssl_version, "OpenSSL/3", 9) != 0) {
    fprintf(stderr, "curl_openssl_https_ipv6_test: SSL backend is %s\n",
            version->ssl_version ? version->ssl_version : "none");
    return 1;
  }
  if ((version->features & CURL_VERSION_IPV6) == 0) {
    fprintf(stderr, "curl_openssl_https_ipv6_test: libcurl was built without IPv6\n");
    return 1;
  }
  snprintf(url, sizeof url, "https://[::1]:%u/", (unsigned)ntohs(address.sin6_port));
  ok = fetch(url, "::1", "literal [::1]");
#if defined(__APPLE__)
  snprintf(url, sizeof url, "https://localhost:%u/", (unsigned)ntohs(address.sin6_port));
  ok = fetch(url, "::1", "name localhost") && ok;
#endif
  pthread_join(thread, NULL);
  if (!ok) {
    return 1;
  }
  printf("curl_openssl_https_ipv6_test: ok (%s, IPv6)\n", version->ssl_version);
  return 0;
}
