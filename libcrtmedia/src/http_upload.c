#include "http_upload.h"

#include "transport_queue.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define CRTMEDIA_HTTP_UPLOAD_DEFAULT_QUEUE_CAPACITY (64u * 1024u)

struct crtmedia_http_upload {
  CURL* easy;
  struct curl_slist* headers;
  crtmedia_transport_queue* queue;
  pthread_t thread;
  int thread_started;
  int eof_written;
  int joined;
  int finished;
  volatile int cancel_requested;
  CURLcode curl_result;
  long status_code;
};

static int upload_progress_callback(
    void* userdata, curl_off_t download_total, curl_off_t download_now,
    curl_off_t upload_total, curl_off_t upload_now) {
  (void)download_total;
  (void)download_now;
  (void)upload_total;
  (void)upload_now;
  return ((crtmedia_http_upload*)userdata)->cancel_requested ? 1 : 0;
}

static int append_header(crtmedia_http_upload* upload, const char* value) {
  struct curl_slist* updated = curl_slist_append(upload->headers, value);
  if (updated == NULL) return -1;
  upload->headers = updated;
  return 0;
}

static crtmedia_result map_curl_upload_code(CURLcode code) {
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

static size_t upload_read_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
  crtmedia_http_upload* upload = (crtmedia_http_upload*)userdata;
  size_t capacity = size * nitems;
  size_t read_count = 0;
  int eof = 0;
  crtmedia_result result = crtmedia_transport_queue_read(
      upload->queue, buffer, capacity, -1, &read_count, &eof);
  if (result != CRTMEDIA_OK) {
    return CURL_READFUNC_ABORT;
  }
  return eof ? 0 : read_count;
}

static size_t discard_response_callback(char* data, size_t size, size_t nitems, void* userdata) {
  (void)data;
  (void)userdata;
  return size * nitems;
}

static void* upload_worker_main(void* argument) {
  crtmedia_http_upload* upload = (crtmedia_http_upload*)argument;
  upload->curl_result = curl_easy_perform(upload->easy);
  curl_easy_getinfo(upload->easy, CURLINFO_RESPONSE_CODE, &upload->status_code);
  if (upload->curl_result != CURLE_OK || upload->status_code < 200 || upload->status_code >= 300) {
    /* Wake a producer blocked on a full queue. The public write path maps
     * this caller-visible cancellation to the worker's real final result. */
    crtmedia_transport_queue_cancel(upload->queue);
  }
  return NULL;
}

/* Same Tranche 5 policy as http_transport.c's apply_tls_policy(); kept as a
 * small private copy so this file stays independent of the input path. */
static int apply_upload_tls_policy(CURL* easy, const crtmedia_tls_options* tls) {
  curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https");
  if (tls != NULL && tls->insecure_skip_verify_for_local_development != 0) {
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
    return 0;
  }
  curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
  if (tls != NULL && tls->ca_pem != NULL) {
    struct curl_blob blob;
    blob.data = (void*)tls->ca_pem;
    blob.len = strlen(tls->ca_pem);
    blob.flags = CURL_BLOB_COPY;
    if (curl_easy_setopt(easy, CURLOPT_CAINFO_BLOB, &blob) != CURLE_OK) {
      return -1;
    }
  }
  return 0;
}

crtmedia_result crtmedia_http_upload_open(
    const char* url, size_t queue_capacity, const crtmedia_tls_options* tls,
    crtmedia_http_upload** out_upload) {
  if (url == NULL || out_upload == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_upload = NULL;
  if (queue_capacity == 0) {
    queue_capacity = CRTMEDIA_HTTP_UPLOAD_DEFAULT_QUEUE_CAPACITY;
  }

  crtmedia_http_upload* upload = (crtmedia_http_upload*)calloc(1, sizeof(*upload));
  if (upload == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  if (crtmedia_transport_queue_create(queue_capacity, 0, 0, &upload->queue) != CRTMEDIA_OK) {
    free(upload);
    return CRTMEDIA_ERROR_IO;
  }
  upload->easy = curl_easy_init();
  if (upload->easy == NULL) {
    crtmedia_transport_queue_release(upload->queue);
    free(upload);
    return CRTMEDIA_ERROR_IO;
  }

  /* Unknown-length PUT body: explicitly use HTTP/1.1 chunked framing. Empty
   * Expect disables curl's 100-continue delay, important for the deterministic
   * loopback server and harmless for real compliant HTTP/1.1 peers. */
  if (append_header(upload, "Transfer-Encoding: chunked") != 0 ||
      append_header(upload, "Content-Type: video/mp4") != 0 ||
      append_header(upload, "Expect:") != 0) {
    curl_slist_free_all(upload->headers);
    curl_easy_cleanup(upload->easy);
    crtmedia_transport_queue_release(upload->queue);
    free(upload);
    return CRTMEDIA_ERROR_IO;
  }
  curl_easy_setopt(upload->easy, CURLOPT_URL, url);
  curl_easy_setopt(upload->easy, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(upload->easy, CURLOPT_HTTPHEADER, upload->headers);
  curl_easy_setopt(upload->easy, CURLOPT_READFUNCTION, upload_read_callback);
  curl_easy_setopt(upload->easy, CURLOPT_READDATA, upload);
  curl_easy_setopt(upload->easy, CURLOPT_WRITEFUNCTION, discard_response_callback);
  curl_easy_setopt(upload->easy, CURLOPT_WRITEDATA, upload);
  curl_easy_setopt(upload->easy, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(upload->easy, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(upload->easy, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(upload->easy, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(upload->easy, CURLOPT_XFERINFOFUNCTION, upload_progress_callback);
  curl_easy_setopt(upload->easy, CURLOPT_XFERINFODATA, upload);
  if (apply_upload_tls_policy(upload->easy, tls) != 0) {
    curl_slist_free_all(upload->headers);
    curl_easy_cleanup(upload->easy);
    crtmedia_transport_queue_release(upload->queue);
    free(upload);
    return CRTMEDIA_ERROR_IO;
  }

  if (pthread_create(&upload->thread, NULL, upload_worker_main, upload) != 0) {
    curl_slist_free_all(upload->headers);
    curl_easy_cleanup(upload->easy);
    crtmedia_transport_queue_release(upload->queue);
    free(upload);
    return CRTMEDIA_ERROR_IO;
  }
  upload->thread_started = 1;
  *out_upload = upload;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_http_upload_write(
    crtmedia_http_upload* upload, const void* data, size_t size) {
  if (upload == NULL || (data == NULL && size > 0) || upload->eof_written) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  size_t offset = 0;
  while (offset < size) {
    size_t written = 0;
    crtmedia_result result = crtmedia_transport_queue_write(
        upload->queue, (const unsigned char*)data + offset, size - offset, -1, &written);
    if (result != CRTMEDIA_OK) {
      if (result == CRTMEDIA_ERROR_CANCELLED && upload->thread_started && !upload->joined) {
        pthread_join(upload->thread, NULL);
        upload->joined = 1;
        return upload->curl_result == CURLE_OK ? CRTMEDIA_ERROR_PROTOCOL : map_curl_upload_code(upload->curl_result);
      }
      return result;
    }
    offset += written;
  }
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_http_upload_finish(crtmedia_http_upload* upload) {
  if (upload == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (upload->finished) {
    return CRTMEDIA_OK;
  }
  if (!upload->eof_written) {
    crtmedia_result eof_result = crtmedia_transport_queue_write_eof(upload->queue);
    if (eof_result != CRTMEDIA_OK && eof_result != CRTMEDIA_ERROR_CANCELLED) {
      return eof_result;
    }
    upload->eof_written = 1;
  }
  if (upload->thread_started && !upload->joined) {
    pthread_join(upload->thread, NULL);
    upload->joined = 1;
  }
  if (upload->curl_result != CURLE_OK) {
    return map_curl_upload_code(upload->curl_result);
  }
  if (upload->status_code < 200 || upload->status_code >= 300) {
    return CRTMEDIA_ERROR_PROTOCOL;
  }
  upload->finished = 1;
  return CRTMEDIA_OK;
}

void crtmedia_http_upload_close(crtmedia_http_upload* upload) {
  if (upload == NULL) {
    return;
  }
  if (!upload->joined) {
    upload->cancel_requested = 1;
    crtmedia_transport_queue_cancel(upload->queue);
    if (upload->thread_started) {
      pthread_join(upload->thread, NULL);
    }
  }
  crtmedia_transport_queue_release(upload->queue);
  if (upload->headers != NULL) {
    curl_slist_free_all(upload->headers);
  }
  if (upload->easy != NULL) {
    curl_easy_cleanup(upload->easy);
  }
  free(upload);
}
