#include "http_test_server.h"

#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

struct http_test_server {
  int listen_fd;
  pthread_t thread;
  int thread_started;
  volatile int stop_requested;

  const uint8_t* body;
  size_t body_size;
  http_test_server_mode mode;
};

static void send_all(int fd, const void* data, size_t size) {
  const uint8_t* p = (const uint8_t*)data;
  size_t sent = 0;
  while (sent < size) {
    ssize_t n = send(fd, p + sent, size - sent, 0);
    if (n <= 0) {
      return; /* best-effort: a real test client never legitimately fails mid-send here */
    }
    sent += (size_t)n;
  }
}

/* Case-insensitive search for a header named `name` (e.g. "Range:") at the
 * start of some line within `request` (a NUL-terminated buffer already
 * known to contain the full header block). Returns a pointer just past the
 * matched name (the header's own value, including any leading spaces), or
 * NULL if not present. */
static const char* find_header_value(const char* request, const char* name) {
  size_t name_len = strlen(name);
  const char* line = request;
  while (line != NULL && *line != '\0' && *line != '\r') {
    if (strncasecmp(line, name, name_len) == 0) {
      return line + name_len;
    }
    const char* next = strstr(line, "\r\n");
    if (next == NULL) {
      break;
    }
    line = next + 2;
  }
  return NULL;
}

/* Parses a real "Range: bytes=<start>-[<end>]" header value. Returns 1 and
 * fills *out_start and *out_end (out_end == -1 means "to the end of the
 * resource") on a well-formed value, 0 otherwise. */
static int parse_range_value(const char* value, int64_t* out_start, int64_t* out_end) {
  while (*value == ' ') ++value;
  if (strncmp(value, "bytes=", 6) != 0) {
    return 0;
  }
  value += 6;
  char* after_start = NULL;
  long long start = strtoll(value, &after_start, 10);
  if (after_start == value || *after_start != '-') {
    return 0;
  }
  const char* end_ptr = after_start + 1;
  long long end = -1;
  if (*end_ptr >= '0' && *end_ptr <= '9') {
    char* after_end = NULL;
    end = strtoll(end_ptr, &after_end, 10);
  }
  *out_start = start;
  *out_end = end;
  return 1;
}

static void send_chunked_response(http_test_server* server, int client_fd) {
  char header[256];
  int header_len = snprintf(
      header, sizeof(header),
      "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Connection: close\r\n"
      "\r\n");
  send_all(client_fd, header, (size_t)header_len);

  const size_t chunk_size = 4096;
  size_t offset = 0;
  while (offset < server->body_size) {
    size_t this_chunk = server->body_size - offset < chunk_size ? server->body_size - offset : chunk_size;
    char chunk_header[32];
    int chunk_header_len = snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", this_chunk);
    send_all(client_fd, chunk_header, (size_t)chunk_header_len);
    send_all(client_fd, server->body + offset, this_chunk);
    send_all(client_fd, "\r\n", 2);
    offset += this_chunk;
  }
  send_all(client_fd, "0\r\n\r\n", 5);
}

static void send_range_capable_response(http_test_server* server, int client_fd, const char* request) {
  const char* range_value = find_header_value(request, "Range:");
  int64_t start = -1;
  int64_t end = -1;
  int has_range = range_value != NULL && parse_range_value(range_value, &start, &end);

  if (!has_range) {
    char header[256];
    int header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %zu\r\n"
        "Accept-Ranges: bytes\r\n"
        "Connection: close\r\n"
        "\r\n",
        server->body_size);
    send_all(client_fd, header, (size_t)header_len);
    send_all(client_fd, server->body, server->body_size);
    return;
  }

  if (start < 0 || (size_t)start >= server->body_size) {
    char header[128];
    int header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 416 Range Not Satisfiable\r\n"
        "Content-Range: bytes */%zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        server->body_size);
    send_all(client_fd, header, (size_t)header_len);
    return;
  }
  int64_t clamped_end = (end < 0 || (size_t)end >= server->body_size) ? (int64_t)server->body_size - 1 : end;
  size_t length = (size_t)(clamped_end - start + 1);

  char header[256];
  int header_len = snprintf(
      header, sizeof(header),
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes %lld-%lld/%zu\r\n"
      "Content-Length: %zu\r\n"
      "Accept-Ranges: bytes\r\n"
      "Connection: close\r\n"
      "\r\n",
      (long long)start, (long long)clamped_end, server->body_size, length);
  send_all(client_fd, header, (size_t)header_len);
  send_all(client_fd, server->body + start, length);
}

static void handle_connection(http_test_server* server, int client_fd) {
  char request[8192];
  size_t total_read = 0;
  for (;;) {
    if (total_read >= sizeof(request) - 1) {
      return; /* header block too large for this test fixture -- give up */
    }
    ssize_t n = recv(client_fd, request + total_read, sizeof(request) - 1 - total_read, 0);
    if (n <= 0) {
      return; /* client closed before sending a full request */
    }
    total_read += (size_t)n;
    request[total_read] = '\0';
    if (strstr(request, "\r\n\r\n") != NULL) {
      break;
    }
  }

  if (server->mode == HTTP_TEST_SERVER_CHUNKED_NO_RANGE) {
    send_chunked_response(server, client_fd);
  } else {
    send_range_capable_response(server, client_fd, request);
  }
}

static void* server_thread_main(void* argument) {
  http_test_server* server = (http_test_server*)argument;
  while (!server->stop_requested) {
    struct pollfd pfd;
    pfd.fd = server->listen_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    /* A short, repeated poll timeout so this loop notices stop_requested
     * promptly rather than blocking in accept() indefinitely. */
    if (poll(&pfd, 1, 200) <= 0) {
      continue;
    }
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
      continue;
    }
    handle_connection(server, client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }
  return NULL;
}

int http_test_server_start(
    const void* body, size_t body_size, http_test_server_mode mode, http_test_server** out_server, int* out_port) {
  if (body == NULL || out_server == NULL || out_port == NULL) {
    return -1;
  }
  *out_server = NULL;
  *out_port = 0;

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    return -1;
  }
  int reuse = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(listen_fd);
    return -1;
  }
  if (listen(listen_fd, 4) != 0) {
    close(listen_fd);
    return -1;
  }
  struct sockaddr_in bound;
  socklen_t bound_len = sizeof(bound);
  if (getsockname(listen_fd, (struct sockaddr*)&bound, &bound_len) != 0) {
    close(listen_fd);
    return -1;
  }

  http_test_server* server = (http_test_server*)calloc(1, sizeof(*server));
  if (server == NULL) {
    close(listen_fd);
    return -1;
  }
  server->listen_fd = listen_fd;
  server->body = (const uint8_t*)body;
  server->body_size = body_size;
  server->mode = mode;

  if (pthread_create(&server->thread, NULL, server_thread_main, server) != 0) {
    close(listen_fd);
    free(server);
    return -1;
  }
  server->thread_started = 1;

  *out_server = server;
  *out_port = (int)ntohs(bound.sin_port);
  return 0;
}

void http_test_server_stop(http_test_server* server) {
  if (server == NULL) {
    return;
  }
  server->stop_requested = 1;
  if (server->thread_started) {
    pthread_join(server->thread, NULL);
  }
  close(server->listen_fd);
  free(server);
}
