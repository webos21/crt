#include "http_avio.h"

#include "http_transport.h"

#include <libavutil/error.h>
#include <libavutil/mem.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CRTMEDIA_HTTP_AVIO_BUFFER_SIZE (64 * 1024)
#define CRTMEDIA_HTTP_QUEUE_CAPACITY (256 * 1024)

struct crtmedia_http_avio {
  char* url;
  crtmedia_http_transport* transport;
  AVIOContext* avio_ctx;
  int64_t position; /* current logical stream position */
  int64_t size;     /* -1 if unknown */
  int seekable;
};

static int avio_read_packet(void* opaque, uint8_t* buf, int buf_size) {
  crtmedia_http_avio* avio = (crtmedia_http_avio*)opaque;
  size_t read_count = 0;
  int eof = 0;
  crtmedia_result result =
      crtmedia_http_transport_read(avio->transport, buf, (size_t)buf_size, -1, &read_count, &eof);
  if (result != CRTMEDIA_OK) {
    return result == CRTMEDIA_ERROR_CANCELLED ? AVERROR_EXIT : AVERROR_UNKNOWN;
  }
  if (eof) {
    return AVERROR_EOF;
  }
  avio->position += (int64_t)read_count;
  return (int)read_count;
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
  crtmedia_result result =
      crtmedia_http_transport_open(avio->url, target, CRTMEDIA_HTTP_QUEUE_CAPACITY, &new_transport, &info);
  if (result != CRTMEDIA_OK) {
    return AVERROR_UNKNOWN;
  }
  crtmedia_http_transport_close(avio->transport);
  avio->transport = new_transport;
  avio->position = target;
  return target;
}

crtmedia_result crtmedia_http_avio_open(
    const char* url, crtmedia_http_avio** out_avio, crtmedia_http_avio_info* out_info) {
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

  crtmedia_http_transport_info info;
  crtmedia_result result =
      crtmedia_http_transport_open(url, 0, CRTMEDIA_HTTP_QUEUE_CAPACITY, &avio->transport, &info);
  if (result != CRTMEDIA_OK) {
    free(avio->url);
    free(avio);
    return result;
  }
  avio->size = info.size;
  avio->seekable = info.seekable;
  avio->position = 0;

  uint8_t* avio_buffer = (uint8_t*)av_malloc(CRTMEDIA_HTTP_AVIO_BUFFER_SIZE);
  if (avio_buffer == NULL) {
    crtmedia_http_transport_close(avio->transport);
    free(avio->url);
    free(avio);
    return CRTMEDIA_ERROR_IO;
  }
  avio->avio_ctx = avio_alloc_context(
      avio_buffer, CRTMEDIA_HTTP_AVIO_BUFFER_SIZE, /*write_flag=*/0, avio, avio_read_packet,
      /*write_packet=*/NULL, avio->seekable ? avio_seek_callback : NULL);
  if (avio->avio_ctx == NULL) {
    av_free(avio_buffer);
    crtmedia_http_transport_close(avio->transport);
    free(avio->url);
    free(avio);
    return CRTMEDIA_ERROR_IO;
  }
  avio->avio_ctx->seekable = avio->seekable ? AVIO_SEEKABLE_NORMAL : 0;

  out_info->seekable = avio->seekable;
  out_info->size = avio->size;
  *out_avio = avio;
  return CRTMEDIA_OK;
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
  free(avio->url);
  free(avio);
}
