#include "http_avio.h"

#include "http_transport.h"

#include <libavutil/error.h>
#include <libavutil/mem.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CRTMEDIA_HTTP_AVIO_BUFFER_SIZE (64 * 1024)
#define CRTMEDIA_HTTP_QUEUE_CAPACITY (256 * 1024)

/* Tranche 4 reconnect policy: a lost connection is resumed at most this many
 * times in a row, with exponentially growing (bounded) backoff between
 * attempts. Progress -- any byte delivered after a resume -- resets the
 * count, so a long stream can survive several separate drops, but a server
 * that keeps failing without delivering anything is given up on quickly. */
#define CRTMEDIA_HTTP_MAX_CONSECUTIVE_RETRIES 3
#define CRTMEDIA_HTTP_RETRY_BACKOFF_BASE_MS 25

struct crtmedia_http_avio {
  char* url;
  crtmedia_http_transport* transport;
  AVIOContext* avio_ctx;
  int64_t position; /* current logical stream position */
  int64_t size;     /* -1 if unknown */
  int seekable;
  char validator[CRTMEDIA_HTTP_VALIDATOR_CAPACITY]; /* "" => no safe resume */
  int consecutive_retries;
  /* First hard read failure (never CRTMEDIA_OK once set). FFmpeg's demuxer
   * folds any read failure into a generic error or even EOF, so the
   * extractor asks this object for the real reason instead of guessing --
   * a lost connection must never look like a clean end of stream. */
  crtmedia_result last_error;
  /* Deep copy of the caller's TLS options, reused by every reconnect/seek. */
  char* ca_pem;
  crtmedia_tls_options tls;
};

static const crtmedia_tls_options* avio_tls(const crtmedia_http_avio* avio) {
  return &avio->tls;
}

static void sleep_ms(int ms) {
  struct timespec delay;
  delay.tv_sec = ms / 1000;
  delay.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&delay, NULL);
}

/* Reopens the byte stream at avio->position after a lost connection, only
 * when that is provably safe (a validated Range response, an entity
 * validator known from the original response, and the same validator again
 * on the resumed one -- http_transport.h's open_resume()). Returns 1 with
 * avio->transport replaced, or 0 with avio->transport left as the failed
 * one so the original error is reported. Never delivers a byte twice: the
 * resume offset is exactly the count of bytes already handed to FFmpeg.
 * Returns CRTMEDIA_OK once resumed, else the reason to surface. */
static crtmedia_result try_resume(crtmedia_http_avio* avio, crtmedia_result original_error) {
  crtmedia_result failure = original_error;
  if (!avio->seekable || avio->validator[0] == '\0') {
    return failure;
  }
  while (avio->consecutive_retries < CRTMEDIA_HTTP_MAX_CONSECUTIVE_RETRIES) {
    sleep_ms(CRTMEDIA_HTTP_RETRY_BACKOFF_BASE_MS << avio->consecutive_retries);
    ++avio->consecutive_retries;
    crtmedia_http_transport* resumed = NULL;
    crtmedia_http_transport_info info;
    crtmedia_result result = crtmedia_http_transport_open_resume(
        avio->url, avio->position, avio->validator, CRTMEDIA_HTTP_QUEUE_CAPACITY, avio_tls(avio), &resumed, &info);
    if (result == CRTMEDIA_OK) {
      crtmedia_http_transport_close(avio->transport);
      avio->transport = resumed;
      return CRTMEDIA_OK;
    }
    failure = result;
    if (result == CRTMEDIA_ERROR_PROTOCOL) {
      break; /* the resource changed or the server cannot resume: never retry into a splice */
    }
  }
  return failure;
}

static int avio_read_packet(void* opaque, uint8_t* buf, int buf_size) {
  crtmedia_http_avio* avio = (crtmedia_http_avio*)opaque;
  if (avio->last_error != CRTMEDIA_OK) {
    /* Sticky: once a stream failed for good, FFmpeg retrying the read must
     * not trigger fresh reconnect attempts. */
    return avio->last_error == CRTMEDIA_ERROR_CANCELLED ? AVERROR_EXIT : AVERROR_UNKNOWN;
  }
  for (;;) {
    size_t read_count = 0;
    int eof = 0;
    crtmedia_result result =
        crtmedia_http_transport_read(avio->transport, buf, (size_t)buf_size, -1, &read_count, &eof);
    if (result == CRTMEDIA_ERROR_IO || result == CRTMEDIA_ERROR_TIMEOUT) {
      /* Bytes still buffered were drained first (transport_queue's sticky
       * error semantics), so avio->position is exactly the next byte needed. */
      crtmedia_result resumed = try_resume(avio, result);
      if (resumed == CRTMEDIA_OK) {
        continue;
      }
      result = resumed;
    }
    if (result != CRTMEDIA_OK) {
      if (avio->last_error == CRTMEDIA_OK) {
        avio->last_error = result;
      }
      return result == CRTMEDIA_ERROR_CANCELLED ? AVERROR_EXIT : AVERROR_UNKNOWN;
    }
    if (eof) {
      return AVERROR_EOF;
    }
    if (read_count > 0) {
      avio->consecutive_retries = 0;
    }
    avio->position += (int64_t)read_count;
    return (int)read_count;
  }
}

static int64_t avio_seek_callback(void* opaque, int64_t offset, int whence) {
  crtmedia_http_avio* avio = (crtmedia_http_avio*)opaque;
  if ((whence & AVSEEK_SIZE) != 0) {
    return avio->size >= 0 ? avio->size : AVERROR(ENOSYS);
  }
  int real_whence = whence & ~AVSEEK_FORCE;
  int64_t target;
  if (real_whence == SEEK_SET) {
    target = offset;
  } else if (real_whence == SEEK_CUR) {
    target = avio->position + offset;
  } else if (real_whence == SEEK_END) {
    if (avio->size < 0) {
      return AVERROR(ENOSYS);
    }
    target = avio->size + offset;
  } else {
    return AVERROR(EINVAL);
  }
  if (target < 0) {
    return AVERROR(EINVAL);
  }
  if (target == avio->position) {
    return target;
  }

  crtmedia_http_transport* new_transport = NULL;
  crtmedia_http_transport_info info;
  /* A seek is also a resume of "the same resource": with a known validator,
   * insist on the same one so a changed resource is never silently mixed in. */
  crtmedia_result result =
      avio->validator[0] != '\0'
          ? crtmedia_http_transport_open_resume(
                avio->url, target, avio->validator, CRTMEDIA_HTTP_QUEUE_CAPACITY, avio_tls(avio),
                &new_transport, &info)
          : crtmedia_http_transport_open(
                avio->url, target, CRTMEDIA_HTTP_QUEUE_CAPACITY, avio_tls(avio), &new_transport, &info);
  if (result != CRTMEDIA_OK) {
    return AVERROR_UNKNOWN;
  }
  crtmedia_http_transport_close(avio->transport);
  avio->transport = new_transport;
  avio->position = target;
  return target;
}

static void free_avio_shell(crtmedia_http_avio* avio) {
  free(avio->ca_pem);
  free(avio->url);
  free(avio);
}

crtmedia_result crtmedia_http_avio_open(
    const char* url, const crtmedia_tls_options* tls, crtmedia_http_avio** out_avio,
    crtmedia_http_avio_info* out_info) {
  if (url == NULL || out_avio == NULL || out_info == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_avio = NULL;

  crtmedia_http_avio* avio = (crtmedia_http_avio*)calloc(1, sizeof(*avio));
  if (avio == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  avio->url = strdup(url);
  if (avio->url == NULL) {
    free(avio);
    return CRTMEDIA_ERROR_IO;
  }

  if (tls != NULL) {
    avio->tls.insecure_skip_verify_for_local_development = tls->insecure_skip_verify_for_local_development;
    if (tls->ca_pem != NULL) {
      avio->ca_pem = strdup(tls->ca_pem);
      if (avio->ca_pem == NULL) {
        free_avio_shell(avio);
        return CRTMEDIA_ERROR_IO;
      }
      avio->tls.ca_pem = avio->ca_pem;
    }
  }

  crtmedia_http_transport_info info;
  crtmedia_result result = crtmedia_http_transport_open(
      url, 0, CRTMEDIA_HTTP_QUEUE_CAPACITY, avio_tls(avio), &avio->transport, &info);
  if (result != CRTMEDIA_OK) {
    free_avio_shell(avio);
    return result;
  }
  avio->size = info.size;
  avio->seekable = info.seekable;
  memcpy(avio->validator, info.validator, sizeof(avio->validator));
  avio->position = 0;

  uint8_t* avio_buffer = (uint8_t*)av_malloc(CRTMEDIA_HTTP_AVIO_BUFFER_SIZE);
  if (avio_buffer == NULL) {
    crtmedia_http_transport_close(avio->transport);
    free_avio_shell(avio);
    return CRTMEDIA_ERROR_IO;
  }
  avio->avio_ctx = avio_alloc_context(
      avio_buffer, CRTMEDIA_HTTP_AVIO_BUFFER_SIZE, /*write_flag=*/0, avio, avio_read_packet,
      /*write_packet=*/NULL, avio->seekable ? avio_seek_callback : NULL);
  if (avio->avio_ctx == NULL) {
    av_free(avio_buffer);
    crtmedia_http_transport_close(avio->transport);
    free_avio_shell(avio);
    return CRTMEDIA_ERROR_IO;
  }
  avio->avio_ctx->seekable = avio->seekable ? AVIO_SEEKABLE_NORMAL : 0;

  out_info->seekable = avio->seekable;
  out_info->size = avio->size;
  *out_avio = avio;
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_http_avio_last_error(const crtmedia_http_avio* avio) {
  return avio != NULL ? avio->last_error : CRTMEDIA_OK;
}

AVIOContext* crtmedia_http_avio_context(crtmedia_http_avio* avio) {
  return avio != NULL ? avio->avio_ctx : NULL;
}

void crtmedia_http_avio_close(crtmedia_http_avio* avio) {
  if (avio == NULL) {
    return;
  }
  if (avio->avio_ctx != NULL) {
    /* avio_ctx->buffer may have been reallocated by FFmpeg's own AVIO
     * machinery since avio_alloc_context() -- free whatever it currently
     * points to, not the pointer originally passed in. */
    av_freep(&avio->avio_ctx->buffer);
    avio_context_free(&avio->avio_ctx);
  }
  crtmedia_http_transport_close(avio->transport);
  free_avio_shell(avio);
}
