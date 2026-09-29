#include "http_upload_test_server.h"

#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

#include "test_socket_flags.h"
#include <time.h>
#include <unistd.h>

#define HTTP_UPLOAD_MAX_BODY (16u * 1024u * 1024u)

struct http_upload_test_server {
  int listen_fd;
  pthread_t thread;
  int thread_started;
  int joined;
  volatile int stop_requested;
  int result;
  int used_chunked_encoding;
  uint8_t* body;
  size_t body_size;
  size_t body_capacity;
  size_t bytes_since_pause;
  size_t drop_after_bytes; /* 0 = never drop */
  volatile int connections;
};

static int contains_case_insensitive(const char* haystack, const char* needle) {
  size_t needle_size = strlen(needle);
  for (const char* p = haystack; *p != '\0'; ++p) {
    if (strncasecmp(p, needle, needle_size) == 0) {
      return 1;
    }
  }
  return 0;
}

static int receive_exact(int fd, void* data, size_t size) {
  uint8_t* output = (uint8_t*)data;
  size_t received = 0;
  while (received < size) {
    ssize_t n = recv(fd, output + received, size - received, 0);
    if (n <= 0) return -1;
    received += (size_t)n;
  }
  return 0;
}

static int receive_body_slow(http_upload_test_server* server, int fd, void* data, size_t size) {
  uint8_t* output = (uint8_t*)data;
  size_t received = 0;
  while (received < size) {
    size_t request = size - received;
    if (request > 16384) request = 16384;
    ssize_t n = recv(fd, output + received, request, 0);
    if (n <= 0) return -1;
    received += (size_t)n;
    server->bytes_since_pause += (size_t)n;
    if (server->drop_after_bytes != 0 && server->body_size + received >= server->drop_after_bytes) {
      return -1;
    }
    if (server->bytes_since_pause >= 65536) {
      /* Deliberately slow but bounded. Pausing per 64 KiB keeps this test
       * quick even with Windows' coarse sleep clock, while a ~1 MiB body
       * still repeatedly meets real receiver pressure. */
      struct timespec delay = {0, 1000000L};
      nanosleep(&delay, NULL);
      server->bytes_since_pause = 0;
    }
  }
  return 0;
}

static int receive_line(int fd, char* line, size_t capacity) {
  size_t length = 0;
  while (length + 1 < capacity) {
    char c = 0;
    if (recv(fd, &c, 1, 0) != 1) return -1;
    line[length++] = c;
    if (length >= 2 && line[length - 2] == '\r' && line[length - 1] == '\n') {
      line[length] = '\0';
      return 0;
    }
  }
  return -1;
}

static int append_chunk(http_upload_test_server* server, int fd, size_t chunk_size) {
  if (chunk_size > HTTP_UPLOAD_MAX_BODY - server->body_size) return -1;
  size_t needed = server->body_size + chunk_size;
  if (needed > server->body_capacity) {
    size_t new_capacity = server->body_capacity == 0 ? 65536 : server->body_capacity;
    while (new_capacity < needed) new_capacity *= 2;
    if (new_capacity > HTTP_UPLOAD_MAX_BODY) new_capacity = HTTP_UPLOAD_MAX_BODY;
    uint8_t* resized = (uint8_t*)realloc(server->body, new_capacity);
    if (resized == NULL) return -1;
    server->body = resized;
    server->body_capacity = new_capacity;
  }
  if (receive_body_slow(server, fd, server->body + server->body_size, chunk_size) != 0) return -1;
  server->body_size += chunk_size;
  char crlf[2];
  return receive_exact(fd, crlf, sizeof(crlf)) == 0 && crlf[0] == '\r' && crlf[1] == '\n' ? 0 : -1;
}

static int receive_chunked_body(http_upload_test_server* server, int fd) {
  char line[128];
  for (;;) {
    if (receive_line(fd, line, sizeof(line)) != 0) return -1;
    char* after = NULL;
    unsigned long long chunk_size = strtoull(line, &after, 16);
    if (after == line || chunk_size > HTTP_UPLOAD_MAX_BODY) return -1;
    if (chunk_size == 0) {
      /* No trailers are emitted by this client; consume the terminating
       * empty line following the zero-sized chunk. */
      return receive_line(fd, line, sizeof(line));
    }
    if (append_chunk(server, fd, (size_t)chunk_size) != 0) return -1;
  }
}

static int handle_upload(http_upload_test_server* server, int client_fd) {
  char headers[8192];
  size_t length = 0;
  while (length + 1 < sizeof(headers)) {
    if (recv(client_fd, headers + length, 1, 0) != 1) return -1;
    ++length;
    headers[length] = '\0';
    if (length >= 4 && memcmp(headers + length - 4, "\r\n\r\n", 4) == 0) break;
  }
  if (strncmp(headers, "PUT ", 4) != 0 ||
      !contains_case_insensitive(headers, "Transfer-Encoding: chunked")) {
    return -1;
  }
  server->used_chunked_encoding = 1;
  if (receive_chunked_body(server, client_fd) != 0) return -1;
  static const char response[] =
      "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
  size_t sent = 0;
  while (sent < sizeof(response) - 1) {
    ssize_t n = crtmedia_test_send(client_fd, response + sent, sizeof(response) - 1 - sent);
    if (n <= 0) return -1;
    sent += (size_t)n;
  }
  return 0;
}

static void* upload_server_main(void* argument) {
  http_upload_test_server* server = (http_upload_test_server*)argument;
  server->result = -1;
  while (!server->stop_requested) {
    struct pollfd pfd = {server->listen_fd, POLLIN, 0};
    if (poll(&pfd, 1, 200) <= 0) continue;
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &client_size);
    if (client_fd < 0) continue;
    ++server->connections;
    server->result = handle_upload(server, client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    if (server->drop_after_bytes == 0) {
      break;
    }
  }
  return NULL;
}

static int upload_server_start(
    size_t drop_after_bytes, http_upload_test_server** out_server, int* out_port) {
  if (out_server == NULL || out_port == NULL) return -1;
  *out_server = NULL;
  *out_port = 0;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) return -1;
  int reuse = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(listen_fd, 1) != 0) {
    close(listen_fd);
    return -1;
  }
  struct sockaddr_in bound;
  socklen_t bound_size = sizeof(bound);
  if (getsockname(listen_fd, (struct sockaddr*)&bound, &bound_size) != 0) {
    close(listen_fd);
    return -1;
  }
  http_upload_test_server* server = (http_upload_test_server*)calloc(1, sizeof(*server));
  if (server == NULL) {
    close(listen_fd);
    return -1;
  }
  server->listen_fd = listen_fd;
  server->drop_after_bytes = drop_after_bytes;
  if (pthread_create(&server->thread, NULL, upload_server_main, server) != 0) {
    close(listen_fd);
    free(server);
    return -1;
  }
  server->thread_started = 1;
  *out_server = server;
  *out_port = (int)ntohs(bound.sin_port);
  return 0;
}

int http_upload_test_server_start(http_upload_test_server** out_server, int* out_port) {
  return upload_server_start(0, out_server, out_port);
}

int http_upload_test_server_start_dropping(
    size_t drop_after_bytes, http_upload_test_server** out_server, int* out_port) {
  return drop_after_bytes == 0 ? -1 : upload_server_start(drop_after_bytes, out_server, out_port);
}

int http_upload_test_server_connection_count(http_upload_test_server* server) {
  return server->connections;
}

int http_upload_test_server_wait_and_copy(
    http_upload_test_server* server, void** out_data, size_t* out_size,
    int* out_used_chunked_encoding) {
  if (server == NULL || out_data == NULL || out_size == NULL || out_used_chunked_encoding == NULL) return -1;
  *out_data = NULL;
  *out_size = 0;
  *out_used_chunked_encoding = 0;
  if (server->thread_started && !server->joined) {
    pthread_join(server->thread, NULL);
    server->joined = 1;
  }
  if (server->result != 0 || server->body_size == 0) return -1;
  void* copy = malloc(server->body_size);
  if (copy == NULL) return -1;
  memcpy(copy, server->body, server->body_size);
  *out_data = copy;
  *out_size = server->body_size;
  *out_used_chunked_encoding = server->used_chunked_encoding;
  return 0;
}

void http_upload_test_server_stop(http_upload_test_server* server) {
  if (server == NULL) return;
  server->stop_requested = 1;
  if (server->thread_started && !server->joined) pthread_join(server->thread, NULL);
  close(server->listen_fd);
  free(server->body);
  free(server);
}
