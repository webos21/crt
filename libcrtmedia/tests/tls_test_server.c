#include "tls_test_server.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>

#include "test_socket_flags.h"
#include <time.h>
#include <unistd.h>

#define PEM_CAPACITY 4096

struct tls_test_proxy {
  int listen_fd;
  int backend_port;
  pthread_t thread;
  int thread_started;
  volatile int stop_requested;
  volatile int handshakes_completed;

  char ca_pem[PEM_CAPACITY];
  char other_ca_pem[PEM_CAPACITY];
  char server_cert_pem[PEM_CAPACITY];

  mbedtls_ctr_drbg_context drbg;
  mbedtls_x509_crt server_chain;
  mbedtls_pk_context server_key;
  mbedtls_ssl_config conf;
};

static int entropy_from_crt(void* context, unsigned char* output, size_t length) {
  (void)context;
  size_t done = 0;
  while (done < length) {
    size_t chunk = length - done > 256 ? 256 : length - done;
    if (getentropy(output + done, chunk) != 0) {
      return -1;
    }
    done += chunk;
  }
  return 0;
}

static void format_time(time_t when, char out[15]) {
  struct tm parts;
  gmtime_r(&when, &parts);
  snprintf(out, 15, "%04d%02d%02d%02d%02d%02d", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
           parts.tm_hour, parts.tm_min, parts.tm_sec);
}

static int generate_key(mbedtls_pk_context* key, mbedtls_ctr_drbg_context* drbg) {
  mbedtls_pk_init(key);
  if (mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) {
    return -1;
  }
  return mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*key), mbedtls_ctr_drbg_random, drbg);
}

/* Issues a certificate valid from a day ago to a day from now (so neither
 * clock skew nor a slow CI host can expire it) and returns it as PEM. */
static int issue_certificate(
    mbedtls_ctr_drbg_context* drbg, int is_ca, const char* subject, mbedtls_pk_context* subject_key,
    const char* issuer, mbedtls_pk_context* issuer_key, unsigned char serial, const unsigned char* san_ipv4,
    char* pem, size_t pem_capacity) {
  mbedtls_x509write_cert crt;
  mbedtls_x509write_crt_init(&crt);
  char not_before[15];
  char not_after[15];
  time_t now = time(NULL);
  format_time(now - 24 * 3600, not_before);
  format_time(now + 24 * 3600, not_after);
  int rc = -1;
  mbedtls_x509_san_list san;
  memset(&san, 0, sizeof(san));

  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&crt, subject_key);
  mbedtls_x509write_crt_set_issuer_key(&crt, issuer_key);
  if (mbedtls_x509write_crt_set_subject_name(&crt, subject) != 0 ||
      mbedtls_x509write_crt_set_issuer_name(&crt, issuer) != 0 ||
      mbedtls_x509write_crt_set_serial_raw(&crt, (unsigned char*)&serial, 1) != 0 ||
      mbedtls_x509write_crt_set_validity(&crt, not_before, not_after) != 0 ||
      mbedtls_x509write_crt_set_basic_constraints(&crt, is_ca, is_ca ? 0 : -1) != 0) {
    goto done;
  }
  if (is_ca) {
    if (mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_KEY_CERT_SIGN) != 0) {
      goto done;
    }
  } else {
    if (mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_DIGITAL_SIGNATURE) != 0) {
      goto done;
    }
    san.node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
    san.node.san.unstructured_name.tag = MBEDTLS_ASN1_OCTET_STRING;
    san.node.san.unstructured_name.p = (unsigned char*)san_ipv4;
    san.node.san.unstructured_name.len = 4;
    if (mbedtls_x509write_crt_set_subject_alternative_name(&crt, &san) != 0) {
      goto done;
    }
  }
  if (mbedtls_x509write_crt_pem(&crt, (unsigned char*)pem, pem_capacity, mbedtls_ctr_drbg_random, drbg) != 0) {
    goto done;
  }
  rc = 0;
done:
  mbedtls_x509write_crt_free(&crt);
  return rc;
}

typedef struct fd_io {
  int fd;
} fd_io;

static int bio_send(void* context, const unsigned char* buffer, size_t length) {
  fd_io* io = (fd_io*)context;
  ssize_t n = send(io->fd, (const char*)buffer, length, CRTMEDIA_TEST_SEND_FLAGS);
  return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : (int)n;
}

static int bio_recv(void* context, unsigned char* buffer, size_t length) {
  fd_io* io = (fd_io*)context;
  struct pollfd pfd;
  pfd.fd = io->fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  /* Bounded: a client that goes silent mid-handshake must not hang the
   * single-threaded test proxy forever. */
  if (poll(&pfd, 1, 5000) <= 0) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  ssize_t n = recv(io->fd, (char*)buffer, length, 0);
  if (n == 0) {
    return MBEDTLS_ERR_SSL_CONN_EOF;
  }
  return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : (int)n;
}

static int connect_backend(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static int send_all_plain(int fd, const unsigned char* data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
    ssize_t n = send(fd, (const char*)data + sent, size - sent, CRTMEDIA_TEST_SEND_FLAGS);
    if (n <= 0) {
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

static int ssl_write_all(mbedtls_ssl_context* ssl, const unsigned char* data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
    int n = mbedtls_ssl_write(ssl, data + sent, size - sent);
    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) {
      continue;
    }
    if (n <= 0) {
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

static void serve_connection(tls_test_proxy* proxy, int client_fd) {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_init(&ssl);
  fd_io io;
  io.fd = client_fd;
  if (mbedtls_ssl_setup(&ssl, &proxy->conf) != 0) {
    mbedtls_ssl_free(&ssl);
    return;
  }
  mbedtls_ssl_set_bio(&ssl, &io, bio_send, bio_recv, NULL);

  int rc;
  while ((rc = mbedtls_ssl_handshake(&ssl)) != 0) {
    if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
      mbedtls_ssl_free(&ssl);
      return; /* client refused our certificate, or spoke garbage */
    }
  }
  ++proxy->handshakes_completed;

  int backend_fd = connect_backend(proxy->backend_port);
  if (backend_fd < 0) {
    mbedtls_ssl_free(&ssl);
    return;
  }

  int client_done = 0;
  unsigned char buffer[16384];
  while (!proxy->stop_requested) {
    int readable_client = 0;
    int readable_backend = 0;
    if (!client_done && mbedtls_ssl_get_bytes_avail(&ssl) > 0) {
      readable_client = 1;
    } else {
      struct pollfd pfds[2];
      pfds[0].fd = client_done ? -1 : client_fd;
      pfds[0].events = POLLIN;
      pfds[0].revents = 0;
      pfds[1].fd = backend_fd;
      pfds[1].events = POLLIN;
      pfds[1].revents = 0;
      if (poll(pfds, 2, 100) <= 0) {
        continue;
      }
      readable_client = pfds[0].revents != 0;
      readable_backend = pfds[1].revents != 0;
    }
    if (readable_client) {
      int n = mbedtls_ssl_read(&ssl, buffer, sizeof(buffer));
      if (n > 0) {
        if (send_all_plain(backend_fd, buffer, (size_t)n) != 0) {
          break;
        }
      } else if (n != MBEDTLS_ERR_SSL_WANT_READ && n != MBEDTLS_ERR_SSL_WANT_WRITE) {
        client_done = 1; /* close_notify or a dropped client: stop reading it */
        shutdown(backend_fd, SHUT_WR);
      }
    }
    if (readable_backend) {
      ssize_t n = recv(backend_fd, (char*)buffer, sizeof(buffer), 0);
      if (n <= 0) {
        break; /* backend finished its response */
      }
      if (ssl_write_all(&ssl, buffer, (size_t)n) != 0) {
        break;
      }
    }
  }
  mbedtls_ssl_close_notify(&ssl);
  close(backend_fd);
  mbedtls_ssl_free(&ssl);
}

static void* proxy_thread_main(void* argument) {
  tls_test_proxy* proxy = (tls_test_proxy*)argument;
  while (!proxy->stop_requested) {
    struct pollfd pfd;
    pfd.fd = proxy->listen_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 100) <= 0) {
      continue;
    }
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(proxy->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
      continue;
    }
    serve_connection(proxy, client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }
  return NULL;
}

int tls_test_proxy_start(
    int backend_port, tls_test_san san, tls_test_proxy** out_proxy, int* out_port, const char** out_ca_pem,
    const char** out_other_ca_pem) {
  if (out_proxy == NULL || out_port == NULL || out_ca_pem == NULL || out_other_ca_pem == NULL) {
    return -1;
  }
  *out_proxy = NULL;
  tls_test_proxy* proxy = (tls_test_proxy*)calloc(1, sizeof(*proxy));
  if (proxy == NULL) {
    return -1;
  }
  proxy->listen_fd = -1;
  proxy->backend_port = backend_port;
  mbedtls_ctr_drbg_init(&proxy->drbg);
  mbedtls_x509_crt_init(&proxy->server_chain);
  mbedtls_pk_init(&proxy->server_key);
  mbedtls_ssl_config_init(&proxy->conf);

  mbedtls_pk_context ca_key;
  mbedtls_pk_context other_ca_key;
  mbedtls_pk_init(&ca_key);
  mbedtls_pk_init(&other_ca_key);
  int ok = 0;

  static const unsigned char loopback_ip[4] = {127, 0, 0, 1};
  static const unsigned char wrong_ip[4] = {10, 255, 255, 1};

  if (mbedtls_ctr_drbg_seed(&proxy->drbg, entropy_from_crt, NULL, (const unsigned char*)"crt-tls", 7) != 0 ||
      generate_key(&ca_key, &proxy->drbg) != 0 || generate_key(&other_ca_key, &proxy->drbg) != 0 ||
      generate_key(&proxy->server_key, &proxy->drbg) != 0) {
    goto fail;
  }
  if (issue_certificate(&proxy->drbg, 1, "CN=CRT Test CA", &ca_key, "CN=CRT Test CA", &ca_key, 1, NULL,
                        proxy->ca_pem, sizeof(proxy->ca_pem)) != 0 ||
      issue_certificate(&proxy->drbg, 1, "CN=CRT Unrelated CA", &other_ca_key, "CN=CRT Unrelated CA",
                        &other_ca_key, 2, NULL, proxy->other_ca_pem, sizeof(proxy->other_ca_pem)) != 0 ||
      issue_certificate(&proxy->drbg, 0, "CN=localhost", &proxy->server_key, "CN=CRT Test CA", &ca_key, 3,
                        san == TLS_TEST_SAN_LOOPBACK ? loopback_ip : wrong_ip, proxy->server_cert_pem,
                        sizeof(proxy->server_cert_pem)) != 0) {
    goto fail;
  }
  if (mbedtls_x509_crt_parse(&proxy->server_chain, (const unsigned char*)proxy->server_cert_pem,
                             strlen(proxy->server_cert_pem) + 1) != 0) {
    goto fail;
  }
  if (mbedtls_ssl_config_defaults(&proxy->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    goto fail;
  }
  mbedtls_ssl_conf_rng(&proxy->conf, mbedtls_ctr_drbg_random, &proxy->drbg);
  /* TLS 1.2 keeps this fixture independent of PSA initialization; the
   * client-side verification policy under test is identical either way. */
  mbedtls_ssl_conf_max_tls_version(&proxy->conf, MBEDTLS_SSL_VERSION_TLS1_2);
  if (mbedtls_ssl_conf_own_cert(&proxy->conf, &proxy->server_chain, &proxy->server_key) != 0) {
    goto fail;
  }

  proxy->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (proxy->listen_fd < 0) {
    goto fail;
  }
  int reuse = 1;
  setsockopt(proxy->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  struct sockaddr_in bound;
  socklen_t bound_len = sizeof(bound);
  if (bind(proxy->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(proxy->listen_fd, 4) != 0 ||
      getsockname(proxy->listen_fd, (struct sockaddr*)&bound, &bound_len) != 0) {
    goto fail;
  }
  if (pthread_create(&proxy->thread, NULL, proxy_thread_main, proxy) != 0) {
    goto fail;
  }
  proxy->thread_started = 1;
  ok = 1;
  *out_proxy = proxy;
  *out_port = (int)ntohs(bound.sin_port);
  *out_ca_pem = proxy->ca_pem;
  *out_other_ca_pem = proxy->other_ca_pem;

fail:
  mbedtls_pk_free(&ca_key);
  mbedtls_pk_free(&other_ca_key);
  if (!ok) {
    if (proxy->listen_fd >= 0) {
      close(proxy->listen_fd);
    }
    mbedtls_ssl_config_free(&proxy->conf);
    mbedtls_pk_free(&proxy->server_key);
    mbedtls_x509_crt_free(&proxy->server_chain);
    mbedtls_ctr_drbg_free(&proxy->drbg);
    free(proxy);
    return -1;
  }
  return 0;
}

int tls_test_proxy_handshakes_completed(tls_test_proxy* proxy) {
  return proxy->handshakes_completed;
}

void tls_test_proxy_stop(tls_test_proxy* proxy) {
  if (proxy == NULL) {
    return;
  }
  proxy->stop_requested = 1;
  if (proxy->thread_started) {
    pthread_join(proxy->thread, NULL);
  }
  close(proxy->listen_fd);
  mbedtls_ssl_config_free(&proxy->conf);
  mbedtls_pk_free(&proxy->server_key);
  mbedtls_x509_crt_free(&proxy->server_chain);
  mbedtls_ctr_drbg_free(&proxy->drbg);
  free(proxy);
}
