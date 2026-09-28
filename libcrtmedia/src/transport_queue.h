#pragma once

/* Networking & Streaming Tranche 1 (docs/crtmedia_networking_acceptance.md)
 * -- a fixed-capacity byte queue with explicit high/low watermarks,
 * cancellation, timeout, and sticky EOF, shared by a producer thread and a
 * consumer thread. Deliberately has no socket, curl, or TLS dependency at
 * all: this is the private, host-neutral primitive that a later HTTP
 * transport (Tranche 2+) sits behind, not the transport itself. This
 * header is private (libcrtmedia/src/, matching codec_test_control.h's own
 * "internal component, tested directly through its own private header"
 * precedent), never installed.
 *
 * Hysteresis: a blocked writer only wakes once the buffered byte count has
 * drained to at or below the low watermark (not merely "one byte freed"),
 * avoiding mutex/condvar thrashing right at the capacity boundary under
 * sustained back-pressure.
 *
 * EOF is sticky and one-directional: crtmedia_transport_queue_write_eof()
 * marks the producer done; every already-buffered byte is still delivered
 * to the consumer first (crtmedia_transport_queue_read() keeps returning
 * CRTMEDIA_OK with data as long as bytes remain), and EOF is only reported
 * (*out_eof = 1, *out_read = 0) once the buffer is fully drained. Writing
 * after EOF has been signaled is a programming error
 * (CRTMEDIA_ERROR_INVALID_ARGUMENT), not a transport condition.
 *
 * Cancellation is caller-driven only (crtmedia_transport_queue_cancel()),
 * never a spontaneous decision by this queue itself. Once cancelled, every
 * call -- including ones already blocked -- returns CRTMEDIA_ERROR_CANCELLED
 * immediately; the queue must not be used again afterward, only released.
 *
 * timeout_ms on read()/write() follows this project's own existing
 * crtmedia_capture_dequeue_frame()-style convention: negative blocks
 * indefinitely, zero never blocks at all (CRTMEDIA_WOULD_BLOCK if no
 * progress is immediately possible), positive is a bounded wait that
 * reports CRTMEDIA_ERROR_TIMEOUT if it elapses with no progress -- the
 * same WOULD_BLOCK/TIMEOUT distinction docs/crtmedia_networking_
 * acceptance.md's own Tranche 0 error-model section freezes. */

#include <stddef.h>

#include "crtmedia/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct crtmedia_transport_queue crtmedia_transport_queue;

/* capacity must be > 0. high_watermark defaults to capacity and
 * low_watermark to capacity/2 when either is 0; otherwise both are used
 * verbatim (0 < low_watermark <= high_watermark <= capacity is the
 * caller's responsibility -- an out-of-range pair is corrected to the
 * defaults rather than accepted silently miscalibrated). */
crtmedia_result crtmedia_transport_queue_create(
    size_t capacity, size_t high_watermark, size_t low_watermark, crtmedia_transport_queue** out_queue);

/* Copies up to `size` bytes from `data` into the queue, blocking per
 * timeout_ms while the buffered count is at or above the high watermark.
 * *out_written may be less than `size` on a partial write (the queue had
 * room for some but not all of it before the wait would have exceeded
 * timeout_ms) -- never 0 on CRTMEDIA_OK. Returns CRTMEDIA_ERROR_INVALID_
 * ARGUMENT if EOF was already signaled on this queue. */
crtmedia_result crtmedia_transport_queue_write(
    crtmedia_transport_queue* queue, const void* data, size_t size, int timeout_ms, size_t* out_written);

/* Marks the producer side done. Idempotent. Wakes any blocked reader so it
 * can drain the remaining buffered bytes and then observe EOF. */
crtmedia_result crtmedia_transport_queue_write_eof(crtmedia_transport_queue* queue);

/* Copies up to `capacity` bytes out of the queue into `data`, blocking per
 * timeout_ms while the queue is empty and EOF has not been signaled yet.
 * On CRTMEDIA_OK: *out_read > 0 and *out_eof = 0 means data; *out_read == 0
 * and *out_eof = 1 means the queue is drained and the producer signaled
 * EOF; *out_read is never 0 with *out_eof 0. */
crtmedia_result crtmedia_transport_queue_read(
    crtmedia_transport_queue* queue, void* data, size_t capacity, int timeout_ms, size_t* out_read, int* out_eof);

/* Cancels the queue: every waiter already blocked in read()/write() wakes
 * immediately with CRTMEDIA_ERROR_CANCELLED, and every call made after this
 * returns CRTMEDIA_ERROR_CANCELLED without blocking. Idempotent. The queue
 * must be released, not reused, afterward. */
crtmedia_result crtmedia_transport_queue_cancel(crtmedia_transport_queue* queue);

/* Cancels the queue (if not already) so every current waiter wakes, waits
 * for them to actually leave their own wait loop, then destroys it. Safe
 * to call with producer/consumer threads still mid-call: they observe
 * CRTMEDIA_ERROR_CANCELLED and return before this function frees anything.
 * The caller is still responsible for joining its own producer/consumer
 * threads; this only guarantees no thread is inside this queue's own wait
 * when its memory is freed. NULL is a no-op. */
void crtmedia_transport_queue_release(crtmedia_transport_queue* queue);

#ifdef __cplusplus
}
#endif
