#include "http_transport.h"

#include "transport_queue.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct crtmedia_http_transport {
  CURL* easy;
  struct curl_slist* request_headers;
  pthread_t thread;
  int thread_started;

  crtmedia_transport_queue* queue;

  pthread_mutex_t lock;
  pthread_cond_t ready_cond;
  int ready;

  int64_t requested_offset;
  int64_t content_length; /* -1 unknown */
  int64_t range_start;    /* -1 if no Content-Range seen this hop */
  long status_code;
  int seekable;
  crtmedia_result open_result;
};

static crtmedia_result map_curl_code(CURLcode code) {
  switch (code) {
    case CURLE_OK:
      return CRTMEDIA_OK;
    case CURLE_OPERATION_TIMEDOUT:
      return CRTMEDIA_ERROR_TIMEOUT;
    case CURLE_ABORTED_BY_CALLBACK:
      return CRTMEDIA_ERROR_CANCELLED;
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
    case CURLE_GOT_NOTHING:
      return CRTMEDIA_ERROR_IO;
    default:
      return CRTMEDIA_ERROR_PROTOCOL;
  }
}

/* Signals open()'s own blocking wait with a final outcome, exactly once --
 * called either from http_header_callback() once a non-redirect status
 * line's headers are fully seen, or from the worker thread itself if
 * curl_easy_perform() finishes (successfully or not) before that ever
 * happened (e.g. a real connection failure). Caller must hold
 * transport->lock. */
static void signal_ready_locked(crtmedia_http_transport* transport, crtmedia_result result) {
  if (transport->ready) {
    return;
  }
  transport->open_result = result;
  transport->ready = 1;
  pthread_cond_signal(&transport->ready_cond);
}

static size_t http_header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
  crtmedia_http_transport* transport = (crtmedia_http_transport*)userdata;
  size_t total = size * nitems;
  if (total >= 5 && strncmp(buffer, "HTTP/", 5) == 0) {
    /* A new hop's status line (the initial response or a redirect target):
     * reset this hop's own accumulated header fields. */
    transport->content_length = -1;
    transport->range_start = -1;
  } else if (total > 15 && strncasecmp(buffer, "Content-Length:", 15) == 0) {
    transport->content_length = strtoll(buffer + 15, NULL, 10);
  } else if (total > 14 && strncasecmp(buffer, "Content-Range:", 14) == 0) {
    /* "Content-Range: bytes <start>-<end>/<total-or-*>" */
    const char* space = strchr(buffer + 14, ' ');
    const char* dash = space != NULL ? strchr(space, '-') : NULL;
    const char* slash = dash != NULL ? strchr(dash, '/') : NULL;
    if (space != NULL) {
      transport->range_start = strtoll(space + 1, NULL, 10);
    }
    if (slash != NULL && slash[1] != '*') {
      transport->content_length = strtoll(slash + 1, NULL, 10);
    }
  } else if (total <= 2) {
    /* The blank line terminating one hop's headers. */
    long status = 0;
    curl_easy_getinfo(transport->easy, CURLINFO_RESPONSE_CODE, &status);
    if (status < 300 || status >= 400) {
      pthread_mutex_lock(&transport->lock);
      transport->status_code = status;
      if (status == 206 && transport->range_start == transport->requested_offset) {
        transport->seekable = 1;
      } else if (status == 200 && transport->requested_offset == 0) {
        transport->seekable = 0; /* server ignored Range; a legitimate outcome only at offset 0 */
      }
      crtmedia_result result = CRTMEDIA_OK;
      if (status == 200 && transport->requested_offset != 0) {
        /* The server cannot give us the byte range we asked for -- never
         * silently substitute the wrong bytes (docs/crtmedia_networking_
         * acceptance.md's own reconnect-safety rule). */
        result = CRTMEDIA_ERROR_PROTOCOL;
      } else if (status < 200 || (status >= 300 && status < 400) || status >= 400) {
        result = CRTMEDIA_ERROR_PROTOCOL;
      }
      signal_ready_locked(transport, result);
      pthread_mutex_unlock(&transport->lock);
    }
    /* A 3xx hop's own blank line: leave ready unset, curl will follow the
     * redirect and re-invoke this callback for the next hop's headers. */
  }
  return total;
}

static size_t http_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  crtmedia_http_transport* transport = (crtmedia_http_transport*)userdata;
  size_t total = size * nmemb;
  size_t offset = 0;
  while (offset < total) {
    size_t written = 0;
    crtmedia_result result = crtmedia_transport_queue_write(transport->queue, ptr + offset, total - offset, -1, &written);
    if (result != CRTMEDIA_OK) {
      /* A cancelled queue (crtmedia_http_transport_close() was called
       * mid-transfer) is the only way write() fails here -- returning
       * short tells curl to abort with CURLE_ABORTED_BY_CALLBACK. */
      return offset;
    }
    offset += written;
  }
  return total;
}

static void* http_worker_main(void* argument) {
  crtmedia_http_transport* transport = (crtmedia_http_transport*)argument;
  CURLcode code = curl_easy_perform(transport->easy);

  pthread_mutex_lock(&transport->lock);
  if (!transport->ready) {
    /* perform() finished before any header line was ever seen -- a real
     * connection failure (DNS, refused, timeout before response). */
    signal_ready_locked(transport, code == CURLE_OK ? CRTMEDIA_ERROR_IO : map_curl_code(code));
  }
  pthread_mutex_unlock(&transport->lock);

  if (code == CURLE_OK) {
    crtmedia_transport_queue_write_eof(transport->queue);
  } else if (code != CURLE_ABORTED_BY_CALLBACK) {
    /* CURLE_ABORTED_BY_CALLBACK means the queue was already cancelled by
     * crtmedia_http_transport_close(); write_error() on a cancelled queue
     * is a harmless no-op (transport_queue.h's own documented contract). */
    crtmedia_transport_queue_write_error(transport->queue, map_curl_code(code));
  }
  return NULL;
}

crtmedia_result crtmedia_http_transport_open(
    const char* url, int64_t offset, size_t queue_capacity, crtmedia_http_transport** out_transport,
    crtmedia_http_transport_info* out_info) {
  if (url == NULL || offset < 0 || out_transport == NULL || out_info == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_transport = NULL;

  crtmedia_http_transport* transport = (crtmedia_http_transport*)calloc(1, sizeof(*transport));
  if (transport == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  transport->requested_offset = offset;
  transport->content_length = -1;
  transport->range_start = -1;
  pthread_mutex_init(&transport->lock, NULL);
  pthread_cond_init(&transport->ready_cond, NULL);

  if (crtmedia_transport_queue_create(queue_capacity, 0, 0, &transport->queue) != CRTMEDIA_OK) {
    pthread_cond_destroy(&transport->ready_cond);
    pthread_mutex_destroy(&transport->lock);
    free(transport);
    return CRTMEDIA_ERROR_IO;
  }

  transport->easy = curl_easy_init();
  if (transport->easy == NULL) {
    crtmedia_transport_queue_release(transport->queue);
    pthread_cond_destroy(&transport->ready_cond);
    pthread_mutex_destroy(&transport->lock);
    free(transport);
    return CRTMEDIA_ERROR_IO;
  }

  char range_value[64];
  snprintf(range_value, sizeof(range_value), "%lld-", (long long)offset);
  curl_easy_setopt(transport->easy, CURLOPT_URL, url);
  curl_easy_setopt(transport->easy, CURLOPT_RANGE, range_value);
  curl_easy_setopt(transport->easy, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(transport->easy, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(transport->easy, CURLOPT_HEADERFUNCTION, http_header_callback);
  curl_easy_setopt(transport->easy, CURLOPT_HEADERDATA, transport);
  curl_easy_setopt(transport->easy, CURLOPT_WRITEFUNCTION, http_write_callback);
  curl_easy_setopt(transport->easy, CURLOPT_WRITEDATA, transport);
  curl_easy_setopt(transport->easy, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(transport->easy, CURLOPT_NOSIGNAL, 1L);

  if (pthread_create(&transport->thread, NULL, http_worker_main, transport) != 0) {
    curl_easy_cleanup(transport->easy);
    crtmedia_transport_queue_release(transport->queue);
    pthread_cond_destroy(&transport->ready_cond);
    pthread_mutex_destroy(&transport->lock);
    free(transport);
    return CRTMEDIA_ERROR_IO;
  }
  transport->thread_started = 1;

  pthread_mutex_lock(&transport->lock);
  while (!transport->ready) {
    pthread_cond_wait(&transport->ready_cond, &transport->lock);
  }
  crtmedia_result open_result = transport->open_result;
  out_info->seekable = transport->seekable;
  out_info->size = transport->content_length;
  out_info->status_code = transport->status_code;
  pthread_mutex_unlock(&transport->lock);

  if (open_result != CRTMEDIA_OK) {
    crtmedia_http_transport_close(transport);
    return open_result;
  }

  *out_transport = transport;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_http_transport_read(
    crtmedia_http_transport* transport, void* data, size_t capacity, int timeout_ms, size_t* out_read, int* out_eof) {
  if (transport == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  return crtmedia_transport_queue_read(transport->queue, data, capacity, timeout_ms, out_read, out_eof);
}

void crtmedia_http_transport_close(crtmedia_http_transport* transport) {
  if (transport == NULL) {
    return;
  }
  /* Cancel first: wakes http_write_callback()'s own blocked queue write
   * (if the worker thread is mid-transfer) so curl_easy_perform() returns
   * promptly instead of running to natural completion. */
  crtmedia_transport_queue_cancel(transport->queue);
  if (transport->thread_started) {
    pthread_join(transport->thread, NULL);
  }
  crtmedia_transport_queue_release(transport->queue);
  if (transport->easy != NULL) {
    curl_easy_cleanup(transport->easy);
  }
  if (transport->request_headers != NULL) {
    curl_slist_free_all(transport->request_headers);
  }
  pthread_cond_destroy(&transport->ready_cond);
  pthread_mutex_destroy(&transport->lock);
  free(transport);
}
