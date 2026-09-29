#include "transport_queue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct crtmedia_transport_queue {
  uint8_t* buffer;
  size_t capacity;
  size_t high_watermark;
  size_t low_watermark;

  size_t head; /* next byte to read */
  size_t count; /* valid buffered bytes */

  int eof_written;
  int cancelled;
  int has_error;
  crtmedia_result stored_error;

  /* Teardown safety: release() must not destroy lock/not_full/not_empty
   * while a producer or consumer thread is still inside its own
   * pthread_cond_wait()/timedwait() on them -- that is undefined behavior.
   * Every blocking wait increments active_waiters before waiting and
   * decrements it after, signaling drained when it reaches 0; release()
   * cancels, then waits on drained until active_waiters is 0 before
   * actually freeing anything. */
  int active_waiters;

  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
  pthread_cond_t drained;
};

crtmedia_result crtmedia_transport_queue_create(
    size_t capacity, size_t high_watermark, size_t low_watermark, crtmedia_transport_queue** out_queue) {
  if (capacity == 0 || out_queue == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  if (high_watermark == 0 || high_watermark > capacity) {
    high_watermark = capacity;
  }
  if (low_watermark == 0 || low_watermark > high_watermark) {
    low_watermark = high_watermark / 2;
    if (low_watermark == 0) {
      low_watermark = 1;
    }
  }

  crtmedia_transport_queue* queue = (crtmedia_transport_queue*)calloc(1, sizeof(*queue));
  if (queue == NULL) {
    return CRTMEDIA_ERROR_IO;
  }
  queue->buffer = (uint8_t*)malloc(capacity);
  if (queue->buffer == NULL) {
    free(queue);
    return CRTMEDIA_ERROR_IO;
  }
  queue->capacity = capacity;
  queue->high_watermark = high_watermark;
  queue->low_watermark = low_watermark;
  pthread_mutex_init(&queue->lock, NULL);
  pthread_cond_init(&queue->not_full, NULL);
  pthread_cond_init(&queue->not_empty, NULL);
  pthread_cond_init(&queue->drained, NULL);
  *out_queue = queue;
  return CRTMEDIA_OK;
}

/* Waits on `cond` (already holding queue->lock) for up to timeout_ms,
 * tracking active_waiters for release()'s own teardown safety. Returns 0 on
 * a real wake (spurious or signaled -- caller re-checks its own predicate),
 * or CRTMEDIA_ERROR_TIMEOUT if the deadline passed first. timeout_ms < 0
 * waits indefinitely and never times out. */
static crtmedia_result bounded_wait(crtmedia_transport_queue* queue, pthread_cond_t* cond, int timeout_ms) {
  crtmedia_result result;
  ++queue->active_waiters;
  if (timeout_ms < 0) {
    pthread_cond_wait(cond, &queue->lock);
    result = CRTMEDIA_OK;
  } else {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
      deadline.tv_nsec -= 1000000000L;
      deadline.tv_sec += 1;
    }
    result = pthread_cond_timedwait(cond, &queue->lock, &deadline) == 0 ? CRTMEDIA_OK : CRTMEDIA_ERROR_TIMEOUT;
  }
  --queue->active_waiters;
  if (queue->active_waiters == 0) {
    pthread_cond_signal(&queue->drained);
  }
  return result;
}

crtmedia_result crtmedia_transport_queue_write(
    crtmedia_transport_queue* queue, const void* data, size_t size, int timeout_ms, size_t* out_written) {
  if (queue == NULL || (data == NULL && size > 0) || out_written == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_written = 0;
  pthread_mutex_lock(&queue->lock);
  if (queue->cancelled) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_CANCELLED;
  }
  if (queue->eof_written || queue->has_error) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  while (queue->count >= queue->high_watermark) {
    if (timeout_ms == 0) {
      pthread_mutex_unlock(&queue->lock);
      return CRTMEDIA_WOULD_BLOCK;
    }
    crtmedia_result wait_result = bounded_wait(queue, &queue->not_full, timeout_ms);
    if (queue->cancelled) {
      pthread_mutex_unlock(&queue->lock);
      return CRTMEDIA_ERROR_CANCELLED;
    }
    if (wait_result != CRTMEDIA_OK) {
      pthread_mutex_unlock(&queue->lock);
      return wait_result; /* CRTMEDIA_ERROR_TIMEOUT */
    }
  }

  size_t space = queue->capacity - queue->count;
  size_t to_copy = size < space ? size : space;
  size_t tail = (queue->head + queue->count) % queue->capacity;
  size_t first_chunk = queue->capacity - tail;
  if (first_chunk > to_copy) {
    first_chunk = to_copy;
  }
  memcpy(queue->buffer + tail, data, first_chunk);
  if (to_copy > first_chunk) {
    memcpy(queue->buffer, (const uint8_t*)data + first_chunk, to_copy - first_chunk);
  }
  queue->count += to_copy;
  *out_written = to_copy;
  pthread_cond_signal(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_transport_queue_write_eof(crtmedia_transport_queue* queue) {
  if (queue == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  pthread_mutex_lock(&queue->lock);
  if (queue->cancelled) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_CANCELLED;
  }
  if (queue->has_error) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  queue->eof_written = 1;
  pthread_cond_broadcast(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_transport_queue_write_error(crtmedia_transport_queue* queue, crtmedia_result error) {
  if (queue == NULL || error == CRTMEDIA_OK || error == CRTMEDIA_WOULD_BLOCK || error == CRTMEDIA_ERROR_CANCELLED) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  pthread_mutex_lock(&queue->lock);
  if (queue->cancelled) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_CANCELLED;
  }
  if (!queue->has_error && !queue->eof_written) {
    queue->has_error = 1;
    queue->stored_error = error;
  }
  pthread_cond_broadcast(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_transport_queue_read(
    crtmedia_transport_queue* queue, void* data, size_t capacity, int timeout_ms, size_t* out_read, int* out_eof) {
  if (queue == NULL || (data == NULL && capacity > 0) || out_read == NULL || out_eof == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  *out_read = 0;
  *out_eof = 0;
  pthread_mutex_lock(&queue->lock);
  if (queue->cancelled) {
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_ERROR_CANCELLED;
  }
  while (queue->count == 0 && !queue->eof_written && !queue->has_error) {
    if (timeout_ms == 0) {
      pthread_mutex_unlock(&queue->lock);
      return CRTMEDIA_WOULD_BLOCK;
    }
    crtmedia_result wait_result = bounded_wait(queue, &queue->not_empty, timeout_ms);
    if (queue->cancelled) {
      pthread_mutex_unlock(&queue->lock);
      return CRTMEDIA_ERROR_CANCELLED;
    }
    if (wait_result != CRTMEDIA_OK) {
      pthread_mutex_unlock(&queue->lock);
      return wait_result; /* CRTMEDIA_ERROR_TIMEOUT */
    }
  }

  if (queue->count == 0) {
    if (queue->has_error) {
      /* Drained and the producer signaled a real failure: report the
       * stored error itself, not a clean EOF. */
      crtmedia_result stored_error = queue->stored_error;
      pthread_mutex_unlock(&queue->lock);
      return stored_error;
    }
    /* Drained and EOF was signaled: report EOF, not a zero-byte data read. */
    *out_eof = 1;
    pthread_mutex_unlock(&queue->lock);
    return CRTMEDIA_OK;
  }

  size_t to_copy = capacity < queue->count ? capacity : queue->count;
  size_t first_chunk = queue->capacity - queue->head;
  if (first_chunk > to_copy) {
    first_chunk = to_copy;
  }
  memcpy(data, queue->buffer + queue->head, first_chunk);
  if (to_copy > first_chunk) {
    memcpy((uint8_t*)data + first_chunk, queue->buffer, to_copy - first_chunk);
  }
  queue->head = (queue->head + to_copy) % queue->capacity;
  queue->count -= to_copy;
  *out_read = to_copy;
  if (queue->count <= queue->low_watermark) {
    pthread_cond_broadcast(&queue->not_full);
  }
  pthread_mutex_unlock(&queue->lock);
  return CRTMEDIA_OK;
}

crtmedia_result crtmedia_transport_queue_cancel(crtmedia_transport_queue* queue) {
  if (queue == NULL) {
    return CRTMEDIA_ERROR_INVALID_ARGUMENT;
  }
  pthread_mutex_lock(&queue->lock);
  queue->cancelled = 1;
  pthread_cond_broadcast(&queue->not_full);
  pthread_cond_broadcast(&queue->not_empty);
  pthread_mutex_unlock(&queue->lock);
  return CRTMEDIA_OK;
}

void crtmedia_transport_queue_release(crtmedia_transport_queue* queue) {
  if (queue == NULL) {
    return;
  }
  pthread_mutex_lock(&queue->lock);
  queue->cancelled = 1;
  pthread_cond_broadcast(&queue->not_full);
  pthread_cond_broadcast(&queue->not_empty);
  while (queue->active_waiters > 0) {
    pthread_cond_wait(&queue->drained, &queue->lock);
  }
  pthread_mutex_unlock(&queue->lock);

  pthread_cond_destroy(&queue->not_full);
  pthread_cond_destroy(&queue->not_empty);
  pthread_cond_destroy(&queue->drained);
  pthread_mutex_destroy(&queue->lock);
  free(queue->buffer);
  free(queue);
}
