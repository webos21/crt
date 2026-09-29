/* Networking & Streaming Tranche 1 acceptance (docs/crtmedia_networking_
 * acceptance.md): deterministic proof of the bounded transport queue's
 * contract -- fully resource-free, no socket/curl/TLS dependency, real
 * pthread producer/consumer threads driving the actual concurrency paths
 * rather than single-threaded simulation. */

#include "transport_queue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(condition, message)                                    \
  do {                                                                \
    if (!(condition)) {                                               \
      fprintf(stderr, "crtmedia_transport_queue_test: %s\n", (message)); \
      return 1;                                                       \
    }                                                                 \
  } while (0)

static int test_would_block_and_high_low_watermark(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(64, 64, 32, &queue) == CRTMEDIA_OK, "create 64/64/32 queue");

  unsigned char data[64];
  memset(data, 0xAB, sizeof(data));
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, data, 64, -1, &written) == CRTMEDIA_OK && written == 64,
        "fill to exactly the high watermark");

  size_t extra_written = 123; /* poisoned, must be reset to 0 on WOULD_BLOCK */
  CHECK(crtmedia_transport_queue_write(queue, data, 1, 0, &extra_written) == CRTMEDIA_WOULD_BLOCK,
        "write at/above high watermark with timeout_ms=0 returns WOULD_BLOCK");
  CHECK(extra_written == 0, "WOULD_BLOCK write reports zero bytes written");

  unsigned char drain[40];
  size_t read_count = 0;
  int eof = 0;
  CHECK(crtmedia_transport_queue_read(queue, drain, sizeof(drain), 0, &read_count, &eof) == CRTMEDIA_OK &&
            read_count == 40 && eof == 0,
        "drain 40 bytes, dropping buffered count to 24 (<= low watermark 32)");

  CHECK(crtmedia_transport_queue_write(queue, data, 10, 0, &written) == CRTMEDIA_OK && written == 10,
        "write below the high watermark succeeds without blocking");

  crtmedia_transport_queue_release(queue);
  return 0;
}

static int test_timeout(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(8, 0, 0, &queue) == CRTMEDIA_OK, "create 8-byte queue (default watermarks)");

  unsigned char data[8] = {0};
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, data, 8, -1, &written) == CRTMEDIA_OK && written == 8,
        "fill the queue completely");

  CHECK(crtmedia_transport_queue_write(queue, data, 1, 50, &written) == CRTMEDIA_ERROR_TIMEOUT,
        "a bounded wait with nobody draining times out rather than blocking forever");

  unsigned char one_byte;
  size_t read_count = 123; /* poisoned, must be reset to 0 */
  int eof = 1; /* poisoned, must be reset to 0 */
  CHECK(crtmedia_transport_queue_read(queue, &one_byte, 0, 0, &read_count, &eof) == CRTMEDIA_OK && read_count == 0 &&
            eof == 0,
        "a zero-capacity read is a harmless no-op, matching POSIX read()'s own count=0 semantics");

  crtmedia_transport_queue_release(queue);
  return 0;
}

static int test_eof_drains_before_reporting(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(32, 0, 0, &queue) == CRTMEDIA_OK, "create 32-byte queue");

  unsigned char data[10];
  for (int i = 0; i < 10; ++i) data[i] = (unsigned char)i;
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, data, 10, -1, &written) == CRTMEDIA_OK && written == 10,
        "write 10 bytes");
  CHECK(crtmedia_transport_queue_write_eof(queue) == CRTMEDIA_OK, "signal EOF");

  unsigned char writing_after_eof = 0xFF;
  CHECK(crtmedia_transport_queue_write(queue, &writing_after_eof, 1, 0, &written) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "writing after EOF is a programming error, not a transport condition");

  unsigned char drain[32];
  size_t read_count = 0;
  int eof = 0;
  CHECK(crtmedia_transport_queue_read(queue, drain, sizeof(drain), 0, &read_count, &eof) == CRTMEDIA_OK &&
            read_count == 10 && eof == 0 && memcmp(drain, data, 10) == 0,
        "already-buffered bytes are delivered before EOF is reported");

  CHECK(crtmedia_transport_queue_read(queue, drain, sizeof(drain), 0, &read_count, &eof) == CRTMEDIA_OK &&
            read_count == 0 && eof == 1,
        "EOF is reported only once the buffer is fully drained");

  crtmedia_transport_queue_release(queue);
  return 0;
}

static int test_sticky_error_drains_before_reporting(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(32, 0, 0, &queue) == CRTMEDIA_OK, "create 32-byte queue");

  unsigned char data[5] = {1, 2, 3, 4, 5};
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, data, 5, -1, &written) == CRTMEDIA_OK && written == 5,
        "write 5 bytes");
  CHECK(crtmedia_transport_queue_write_error(queue, CRTMEDIA_ERROR_IO) == CRTMEDIA_OK, "signal a producer error");

  CHECK(crtmedia_transport_queue_write_eof(queue) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "write_eof() after write_error() is rejected -- the two terminal states are mutually exclusive");

  unsigned char writing_after_error = 0xFF;
  CHECK(crtmedia_transport_queue_write(queue, &writing_after_error, 1, 0, &written) == CRTMEDIA_ERROR_INVALID_ARGUMENT,
        "writing after a sticky error is a programming error, same as writing after EOF");

  unsigned char drain[32];
  size_t read_count = 0;
  int eof = 0;
  CHECK(crtmedia_transport_queue_read(queue, drain, sizeof(drain), 0, &read_count, &eof) == CRTMEDIA_OK &&
            read_count == 5 && eof == 0 && memcmp(drain, data, 5) == 0,
        "already-buffered bytes are delivered before the sticky error is reported");

  CHECK(crtmedia_transport_queue_read(queue, drain, sizeof(drain), 0, &read_count, &eof) == CRTMEDIA_ERROR_IO &&
            read_count == 0 && eof == 0,
        "the sticky error itself is returned (not CRTMEDIA_OK/eof=1) once the buffer is drained");

  crtmedia_transport_queue_release(queue);
  return 0;
}

/* Real concurrent stress: N bytes of a deterministic pattern, written in
 * small chunks by one real pthread and read back by another, through a
 * queue much smaller than the total transfer -- forces many real block/
 * wake cycles in both directions, not just a single watermark crossing. */
#define STRESS_TOTAL_BYTES (256 * 1024)
#define STRESS_QUEUE_CAPACITY 4096
#define STRESS_CHUNK_BYTES 777 /* deliberately not a divisor of the capacity */

typedef struct producer_args {
  crtmedia_transport_queue* queue;
  int slow; /* sleeps briefly per chunk, forcing the "consumer faster" case */
} producer_args;

static void sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static void* producer_thread_main(void* argument) {
  producer_args* args = (producer_args*)argument;
  unsigned char chunk[STRESS_CHUNK_BYTES];
  size_t produced = 0;
  int failed = 0;
  while (produced < STRESS_TOTAL_BYTES && !failed) {
    size_t remaining = STRESS_TOTAL_BYTES - produced;
    size_t this_chunk = remaining < STRESS_CHUNK_BYTES ? remaining : STRESS_CHUNK_BYTES;
    for (size_t i = 0; i < this_chunk; ++i) {
      chunk[i] = (unsigned char)((produced + i) & 0xFF);
    }
    size_t offset = 0;
    while (offset < this_chunk) {
      size_t written = 0;
      crtmedia_result r = crtmedia_transport_queue_write(args->queue, chunk + offset, this_chunk - offset, -1, &written);
      if (r != CRTMEDIA_OK) {
        failed = 1;
        break;
      }
      offset += written;
    }
    produced += this_chunk;
    if (args->slow) {
      sleep_ms(1);
    }
  }
  if (!failed) {
    crtmedia_transport_queue_write_eof(args->queue);
  }
  return (void*)(intptr_t)(failed ? 1 : 0);
}

typedef struct consumer_result {
  int ok;
  int slow;
  crtmedia_transport_queue* queue;
} consumer_result;

static void* consumer_thread_main(void* argument) {
  consumer_result* result = (consumer_result*)argument;
  unsigned char chunk[STRESS_CHUNK_BYTES + 37]; /* deliberately not chunk-aligned */
  size_t consumed = 0;
  int mismatch = 0;
  for (;;) {
    size_t read_count = 0;
    int eof = 0;
    crtmedia_result r = crtmedia_transport_queue_read(result->queue, chunk, sizeof(chunk), -1, &read_count, &eof);
    if (r != CRTMEDIA_OK) {
      result->ok = 0;
      return NULL;
    }
    if (eof) {
      break;
    }
    for (size_t i = 0; i < read_count; ++i) {
      if (chunk[i] != (unsigned char)((consumed + i) & 0xFF)) {
        mismatch = 1;
      }
    }
    consumed += read_count;
    if (result->slow) {
      sleep_ms(1);
    }
  }
  result->ok = !mismatch && consumed == STRESS_TOTAL_BYTES;
  return NULL;
}

static int run_stress(int slow_producer, int slow_consumer) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(STRESS_QUEUE_CAPACITY, 0, 0, &queue) == CRTMEDIA_OK, "create stress queue");

  producer_args producer_arg = {queue, slow_producer};
  consumer_result consumer_res = {0, slow_consumer, queue};
  pthread_t producer_thread, consumer_thread;
  CHECK(pthread_create(&producer_thread, NULL, producer_thread_main, &producer_arg) == 0, "spawn producer thread");
  CHECK(pthread_create(&consumer_thread, NULL, consumer_thread_main, &consumer_res) == 0, "spawn consumer thread");

  void* producer_status = NULL;
  pthread_join(producer_thread, &producer_status);
  pthread_join(consumer_thread, NULL);
  CHECK(producer_status == NULL, "producer thread reported no failure");
  CHECK(consumer_res.ok, "consumer received every byte, in order, followed by EOF");

  crtmedia_transport_queue_release(queue);
  return 0;
}

typedef struct blocked_call_result {
  crtmedia_transport_queue* queue;
  crtmedia_result outcome;
} blocked_call_result;

static void* blocked_writer_thread_main(void* argument) {
  blocked_call_result* result = (blocked_call_result*)argument;
  unsigned char byte = 0;
  size_t written = 0;
  result->outcome = crtmedia_transport_queue_write(result->queue, &byte, 1, -1, &written);
  return NULL;
}

static void* blocked_reader_thread_main(void* argument) {
  blocked_call_result* result = (blocked_call_result*)argument;
  unsigned char byte;
  size_t read_count = 0;
  int eof = 0;
  result->outcome = crtmedia_transport_queue_read(result->queue, &byte, 1, -1, &read_count, &eof);
  return NULL;
}

static int test_cancel_wakes_blocked_writer(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(4, 0, 0, &queue) == CRTMEDIA_OK, "create 4-byte queue");
  unsigned char fill[4] = {0};
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, fill, 4, -1, &written) == CRTMEDIA_OK, "fill the queue completely");

  blocked_call_result result = {queue, CRTMEDIA_OK};
  pthread_t writer_thread;
  CHECK(pthread_create(&writer_thread, NULL, blocked_writer_thread_main, &result) == 0, "spawn blocked writer");
  sleep_ms(50); /* give the writer time to actually reach its blocking wait */
  CHECK(crtmedia_transport_queue_cancel(queue) == CRTMEDIA_OK, "cancel the queue");
  pthread_join(writer_thread, NULL);
  CHECK(result.outcome == CRTMEDIA_ERROR_CANCELLED, "the blocked writer woke with CANCELLED");

  crtmedia_transport_queue_release(queue);
  return 0;
}

static int test_cancel_wakes_blocked_reader(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(4, 0, 0, &queue) == CRTMEDIA_OK, "create empty 4-byte queue");

  blocked_call_result result = {queue, CRTMEDIA_OK};
  pthread_t reader_thread;
  CHECK(pthread_create(&reader_thread, NULL, blocked_reader_thread_main, &result) == 0, "spawn blocked reader");
  sleep_ms(50);
  CHECK(crtmedia_transport_queue_cancel(queue) == CRTMEDIA_OK, "cancel the queue");
  pthread_join(reader_thread, NULL);
  CHECK(result.outcome == CRTMEDIA_ERROR_CANCELLED, "the blocked reader woke with CANCELLED");

  crtmedia_transport_queue_release(queue);
  return 0;
}

static int test_release_wakes_every_waiter(void) {
  crtmedia_transport_queue* queue = NULL;
  CHECK(crtmedia_transport_queue_create(4, 0, 0, &queue) == CRTMEDIA_OK, "create 4-byte queue");
  unsigned char fill[4] = {0};
  size_t written = 0;
  CHECK(crtmedia_transport_queue_write(queue, fill, 4, -1, &written) == CRTMEDIA_OK, "fill the queue completely");

  blocked_call_result result_a = {queue, CRTMEDIA_OK};
  blocked_call_result result_b = {queue, CRTMEDIA_OK};
  pthread_t writer_a, writer_b;
  CHECK(pthread_create(&writer_a, NULL, blocked_writer_thread_main, &result_a) == 0, "spawn blocked writer A");
  CHECK(pthread_create(&writer_b, NULL, blocked_writer_thread_main, &result_b) == 0, "spawn blocked writer B");
  sleep_ms(50);

  /* release() itself must cancel, wake both waiters, wait for them to
   * actually leave their own wait loop, and only then free the queue --
   * proven here by the fact that joining both threads below never hangs
   * and each reports CANCELLED, not by inspecting freed memory. */
  crtmedia_transport_queue_release(queue);

  pthread_join(writer_a, NULL);
  pthread_join(writer_b, NULL);
  CHECK(result_a.outcome == CRTMEDIA_ERROR_CANCELLED, "writer A woke with CANCELLED");
  CHECK(result_b.outcome == CRTMEDIA_ERROR_CANCELLED, "writer B woke with CANCELLED");
  return 0;
}

int main(void) {
  if (test_would_block_and_high_low_watermark() != 0) return 1;
  if (test_timeout() != 0) return 1;
  if (test_eof_drains_before_reporting() != 0) return 1;
  if (test_sticky_error_drains_before_reporting() != 0) return 1;
  if (run_stress(/*slow_producer=*/0, /*slow_consumer=*/1) != 0) return 1; /* producer faster */
  if (run_stress(/*slow_producer=*/1, /*slow_consumer=*/0) != 0) return 1; /* consumer faster */
  if (test_cancel_wakes_blocked_writer() != 0) return 1;
  if (test_cancel_wakes_blocked_reader() != 0) return 1;
  if (test_release_wakes_every_waiter() != 0) return 1;

  printf("crtmedia_transport_queue_test: ok watermark=pass timeout=pass eof=pass sticky_error=pass "
         "stress_producer_faster=pass stress_consumer_faster=pass cancel_writer=pass "
         "cancel_reader=pass release_wakes_all=pass\n");
  return 0;
}
