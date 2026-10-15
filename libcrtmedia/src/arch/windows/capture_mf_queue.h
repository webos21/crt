#pragma once

/* Private, OS-independent ownership bookkeeping for the MF frame ring.
 * Only [head, head + count) owns storage. Taking a frame transfers it to
 * the caller; later source teardown must never release that storage. */
#include <stdint.h>
#include <stdlib.h>

#define CRTMEDIA_MF_QUEUE_CAPACITY 4u

typedef struct crtmedia_mf_queued_frame {
  uint8_t* storage;
  uint32_t width;
  uint32_t height;
  int64_t timestamp_us;
} crtmedia_mf_queued_frame;

static inline crtmedia_mf_queued_frame crtmedia_mf_queue_take(
    crtmedia_mf_queued_frame* queue, uint32_t* head, uint32_t* count) {
  crtmedia_mf_queued_frame frame = queue[*head];
  queue[*head] = (crtmedia_mf_queued_frame){0};
  *head = (*head + 1u) % CRTMEDIA_MF_QUEUE_CAPACITY;
  --*count;
  return frame;
}

static inline void crtmedia_mf_queue_discard(
    crtmedia_mf_queued_frame* queue, uint32_t* head, uint32_t* count,
    void (*release_storage)(void*)) {
  while (*count != 0) {
    crtmedia_mf_queued_frame frame = crtmedia_mf_queue_take(queue, head, count);
    release_storage(frame.storage);
  }
}
