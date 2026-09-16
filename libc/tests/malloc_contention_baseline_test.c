/* malloc_contention_baseline_test -- TODO.md's "Allocator baseline
 * validation before Upper Runtime", tranche 3 ("Scale contention
 * deliberately"): the same deterministic-workload/machine-readable-result
 * discipline tranche 1's malloc_baseline_test.c established, extended
 * across threads and two distinct allocation access patterns.
 *
 * "private" pattern: each thread owns a private, bounded live-set slice
 * (a plain array, no locking of its own) and only ever allocates, grows/
 * shrinks, or frees pointers it itself allocated -- the same shape
 * malloc_contention_test.c already exercises, just with the fuller
 * size-class mix and content/alignment verification tranche 1's workload
 * already established. Every thread still contends on malloc.c's own
 * single global heap_lock (this allocator has no per-thread arena/pool of
 * its own to be genuinely "private" at the allocator level -- see
 * TODO.md's own Scudo-tranche writeup), so this pattern measures
 * throughput/latency under N threads hammering that one lock with
 * allocation patterns that never cross threads.
 *
 * "shared" pattern: ALL threads draw from and return to one shared,
 * mutex-guarded live-set table, so a pointer one thread allocates is
 * routinely grown, shrunk, or freed by a *different* thread -- a real
 * remote-free/remote-realloc workload, not just concurrent-but-disjoint
 * allocation. This adds a second, project-owned lock (g_shared_mutex)
 * layered on top of the allocator's own heap_lock; the interesting
 * comparison against the private pattern is not "which has less locking"
 * (both are fully serialized by heap_lock regardless) but whether cross-
 * thread ownership hand-off changes throughput/latency or -- more
 * importantly -- ever produces a correctness failure, since nothing about
 * malloc.c's own design assumes a block is freed by the thread that
 * allocated it, but this is the first test in this project that actually
 * exercises that assumption under real concurrent load.
 *
 * Every piece of bookkeeping state a "shared" run touches (the live-entry
 * array, its own live count, live/peak byte and count counters) is
 * referenced through a pool_ref of POINTERS to ONE shared set of globals,
 * not per-thread copies -- an earlier draft of this file gave each
 * thread_ctx its own live_count field even in the shared pattern, which
 * is a real bug, not just an inaccurate stat: two threads could then
 * compute the same "next free slot" index independently and each write a
 * live live_entry into it, silently clobbering the other's pointer (a
 * genuine leak/corruption risk, not merely a miscount) despite each
 * individual step already being mutex-protected -- the mutex serializes
 * *execution* of a step, but does nothing to make an ordinary per-struct
 * field shared state across threads. pool_ref exists specifically to
 * close that gap: the private pattern points every field at a thread's
 * own private storage, and the shared pattern points every field at the
 * one real shared global instead, so "which count/array does this op
 * touch" is decided once, at pool_ref construction, not re-derived by
 * each op.
 *
 * Run with no arguments (the shape CTest uses): a small, fixed 4-thread,
 * bounded-op correctness pass through BOTH patterns in turn, matching
 * malloc_contention_test.c's own thread count and tranche 1's "bounded
 * correctness variant in routine CTest" discipline. Run with
 * --threads/--ops/--pattern/--seed/--json (the shape an opt-in host-side
 * runner drives) for the larger, timed 1/8/16/32-thread sweep tranche 3
 * calls for -- see tools/run_allocator_contention_baseline.py. */

#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MCB_MAX_THREADS 32u
#define MCB_DEFAULT_THREADS 4u
#define MCB_DEFAULT_OPS_PER_THREAD 300u
#define MCB_PRIVATE_MAX_LIVE_PER_THREAD 256u
#define MCB_SHARED_MAX_LIVE 4096u
/* Per thread; 32 threads * 65536 samples * 4 bytes = 8 MiB static storage,
 * covering a substantial opt-in run without needing dynamic growth -- see
 * malloc_baseline_test.c's own matching rationale for why this must be
 * static, not heap-allocated. */
#define MCB_MAX_LATENCY_SAMPLES_PER_THREAD (1u << 16)

typedef struct {
  unsigned char* ptr;
  size_t size;
  size_t alignment;
  unsigned char fill;
} live_entry;

/* Every field here is a pointer so the private and shared patterns can
 * share the exact same do_allocate()/do_free()/do_realloc() bodies while
 * pointing at entirely different backing storage -- see this file's own
 * top comment for why that indirection is load-bearing, not just style. */
typedef struct {
  live_entry* live;
  uint32_t* live_count;
  uint32_t live_capacity;
  uint64_t* live_bytes;
  uint64_t* peak_live_bytes;
  uint32_t* peak_live_count;
  pthread_mutex_t* live_mutex; /* NULL for the private pattern */
} pool_ref;

typedef struct {
  uint64_t rng_state;
  pool_ref pool;

  /* Private-pattern backing storage for the pool_ref fields above.
   * Unused (left zeroed) when this context runs the shared pattern
   * instead, where pool points at the g_shared_* globals below. */
  live_entry private_live[MCB_PRIVATE_MAX_LIVE_PER_THREAD];
  uint32_t private_live_count;
  uint64_t private_live_bytes;
  uint64_t private_peak_live_bytes;
  uint32_t private_peak_live_count;

  uint32_t* latency_ns; /* NULL: do not record (used for the post-join shared drain pass) */
  uint32_t latency_count;
  uint32_t ops;
  uint64_t malloc_count, calloc_count, aligned_count;
  uint64_t free_count, realloc_grow_count, realloc_shrink_count;
  int failed;
  char fail_reason[128];
} thread_ctx;

static live_entry g_shared_live[MCB_SHARED_MAX_LIVE];
static uint32_t g_shared_live_count;
static uint64_t g_shared_live_bytes;
static uint64_t g_shared_peak_live_bytes;
static uint32_t g_shared_peak_live_count;
static pthread_mutex_t g_shared_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t g_latency_storage[MCB_MAX_THREADS][MCB_MAX_LATENCY_SAMPLES_PER_THREAD];

static uint64_t next_rand(uint64_t* state) {
  uint64_t x = *state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *state = x;
  return x * UINT64_C(0x2545F4914F6CDD1D);
}

static uint32_t rand_below(uint64_t* state, uint32_t bound) {
  return bound == 0 ? 0 : (uint32_t)(next_rand(state) % (uint64_t)bound);
}

static uint64_t monotonic_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static void record_latency(thread_ctx* ctx, uint64_t start_ns, uint64_t end_ns) {
  if (ctx->latency_ns != 0 && ctx->latency_count < MCB_MAX_LATENCY_SAMPLES_PER_THREAD) {
    uint64_t delta = end_ns - start_ns;
    ctx->latency_ns[ctx->latency_count++] = delta > UINT32_MAX ? UINT32_MAX : (uint32_t)delta;
  }
}

static void record_fail(thread_ctx* ctx, const char* reason) {
  ctx->failed = 1;
  strncpy(ctx->fail_reason, reason, sizeof(ctx->fail_reason) - 1);
  ctx->fail_reason[sizeof(ctx->fail_reason) - 1] = 0;
}

/* Same weighted mix malloc_baseline_test.c's own pick_size() uses --
 * small/medium/large/occasional mapping-sized -- kept modest at the top
 * end (256KiB instead of 4MiB) since up to 32 threads may hold a peak
 * live set concurrently and this test must stay well clear of realistic
 * CI memory limits even at the largest thread count. */
static size_t pick_size(uint64_t* rng) {
  uint32_t roll = rand_below(rng, 1000);
  if (roll < 600) {
    return 16 + rand_below(rng, 113);
  }
  if (roll < 900) {
    return 129 + rand_below(rng, 3968);
  }
  if (roll < 998) {
    return 4097 + rand_below(rng, 61440);
  }
  return 65536 + rand_below(rng, 196608);
}

static size_t pick_alignment(uint64_t* rng) {
  static const size_t alignments[] = {16, 32, 64, 128, 256};
  return alignments[rand_below(rng, (uint32_t)(sizeof(alignments) / sizeof(alignments[0])))];
}

static int verify_fill(const unsigned char* ptr, size_t size, unsigned char fill) {
  size_t i;
  for (i = 0; i < size; ++i) {
    if (ptr[i] != fill) {
      return 0;
    }
  }
  return 1;
}

static void track_added(pool_ref* pool, size_t size) {
  *pool->live_bytes += size;
  if (*pool->live_bytes > *pool->peak_live_bytes) {
    *pool->peak_live_bytes = *pool->live_bytes;
  }
  if (*pool->live_count > *pool->peak_live_count) {
    *pool->peak_live_count = *pool->live_count;
  }
}

static void track_removed(pool_ref* pool, size_t size) {
  *pool->live_bytes -= size;
}

static void track_resized(pool_ref* pool, size_t old_size, size_t new_size) {
  *pool->live_bytes = *pool->live_bytes - old_size + new_size;
  if (*pool->live_bytes > *pool->peak_live_bytes) {
    *pool->peak_live_bytes = *pool->live_bytes;
  }
}

/* do_allocate()/do_free()/do_realloc() all assume the caller already
 * holds ctx->pool.live_mutex (when non-NULL, i.e. the shared pattern) for
 * the whole operation -- see this file's own top comment for why holding
 * it across the actual malloc()/free()/realloc() call, not just the
 * bookkeeping, is the simpler and still-correct choice here. */

static int do_allocate(thread_ctx* ctx) {
  pool_ref* pool = &ctx->pool;
  uint32_t kind;
  size_t size;
  unsigned char* ptr;
  unsigned char fill;
  size_t alignment = 0;
  uint64_t start_ns, end_ns;

  if (*pool->live_count >= pool->live_capacity) {
    return 1;
  }

  size = pick_size(&ctx->rng_state);
  kind = rand_below(&ctx->rng_state, 100);
  fill = (unsigned char)(next_rand(&ctx->rng_state) & 0xFFu);

  if (kind < 10) {
    start_ns = monotonic_ns();
    ptr = (unsigned char*)calloc(1, size);
    end_ns = monotonic_ns();
    if (ptr == 0) {
      record_fail(ctx, "calloc returned NULL");
      return 0;
    }
    if (!verify_fill(ptr, size, 0)) {
      record_fail(ctx, "calloc did not zero-fill");
      return 0;
    }
    record_latency(ctx, start_ns, end_ns);
    ctx->calloc_count++;
  } else if (kind < 20) {
    alignment = pick_alignment(&ctx->rng_state);
    start_ns = monotonic_ns();
    if (posix_memalign((void**)&ptr, alignment, size) != 0) {
      record_fail(ctx, "posix_memalign failed");
      return 0;
    }
    end_ns = monotonic_ns();
    if (((uintptr_t)ptr & (alignment - 1)) != 0) {
      record_fail(ctx, "posix_memalign result misaligned");
      return 0;
    }
    record_latency(ctx, start_ns, end_ns);
    ctx->aligned_count++;
  } else {
    start_ns = monotonic_ns();
    ptr = (unsigned char*)malloc(size);
    end_ns = monotonic_ns();
    if (ptr == 0) {
      record_fail(ctx, "malloc returned NULL");
      return 0;
    }
    record_latency(ctx, start_ns, end_ns);
    ctx->malloc_count++;
  }

  memset(ptr, fill, size);
  pool->live[*pool->live_count].ptr = ptr;
  pool->live[*pool->live_count].size = size;
  pool->live[*pool->live_count].alignment = alignment;
  pool->live[*pool->live_count].fill = fill;
  (*pool->live_count)++;
  track_added(pool, size);
  return 1;
}

static int do_free(thread_ctx* ctx, uint32_t index) {
  pool_ref* pool = &ctx->pool;
  live_entry entry = pool->live[index];
  uint64_t start_ns, end_ns;

  if (!verify_fill(entry.ptr, entry.size, entry.fill)) {
    record_fail(ctx, "live block content corrupted before free");
    return 0;
  }
  start_ns = monotonic_ns();
  free(entry.ptr);
  end_ns = monotonic_ns();
  record_latency(ctx, start_ns, end_ns);
  ctx->free_count++;
  track_removed(pool, entry.size);

  pool->live[index] = pool->live[*pool->live_count - 1];
  (*pool->live_count)--;
  return 1;
}

static int do_realloc(thread_ctx* ctx, uint32_t index) {
  pool_ref* pool = &ctx->pool;
  live_entry* entry = &pool->live[index];
  size_t old_size = entry->size;
  size_t new_size;
  size_t verify_size;
  unsigned char* new_ptr;
  unsigned char new_fill;
  uint64_t start_ns, end_ns;
  int growing;

  if (!verify_fill(entry->ptr, entry->size, entry->fill)) {
    record_fail(ctx, "live block content corrupted before realloc");
    return 0;
  }

  growing = rand_below(&ctx->rng_state, 100) < 60;
  if (growing) {
    new_size = old_size + 16 + rand_below(&ctx->rng_state, (uint32_t)(old_size + 1));
  } else {
    new_size = 1 + rand_below(&ctx->rng_state, (uint32_t)old_size);
  }

  start_ns = monotonic_ns();
  new_ptr = (unsigned char*)realloc(entry->ptr, new_size);
  end_ns = monotonic_ns();
  if (new_ptr == 0) {
    record_fail(ctx, "realloc returned NULL");
    return 0;
  }
  record_latency(ctx, start_ns, end_ns);
  if (growing) {
    ctx->realloc_grow_count++;
  } else {
    ctx->realloc_shrink_count++;
  }

  verify_size = old_size < new_size ? old_size : new_size;
  if (!verify_fill(new_ptr, verify_size, entry->fill)) {
    record_fail(ctx, "realloc did not preserve content");
    return 0;
  }
  if (entry->alignment != 0 && ((uintptr_t)new_ptr & (entry->alignment - 1)) != 0) {
    record_fail(ctx, "realloc did not preserve alignment");
    return 0;
  }

  new_fill = (unsigned char)(next_rand(&ctx->rng_state) & 0xFFu);
  memset(new_ptr, new_fill, new_size);
  track_resized(pool, old_size, new_size);
  entry->ptr = new_ptr;
  entry->size = new_size;
  entry->fill = new_fill;
  return 1;
}

static int step_locked(thread_ctx* ctx) {
  if (*ctx->pool.live_count == 0 || rand_below(&ctx->rng_state, 100) < 30) {
    return do_allocate(ctx);
  }
  if (rand_below(&ctx->rng_state, 100) < 50) {
    return do_free(ctx, rand_below(&ctx->rng_state, *ctx->pool.live_count));
  }
  return do_realloc(ctx, rand_below(&ctx->rng_state, *ctx->pool.live_count));
}

static int step(thread_ctx* ctx) {
  int ok;
  if (ctx->pool.live_mutex != 0) {
    pthread_mutex_lock(ctx->pool.live_mutex);
    ok = step_locked(ctx);
    pthread_mutex_unlock(ctx->pool.live_mutex);
  } else {
    ok = step_locked(ctx);
  }
  return ok;
}

static void* worker(void* arg) {
  thread_ctx* ctx = (thread_ctx*)arg;
  uint32_t i;

  for (i = 0; i < ctx->ops; ++i) {
    if (!step(ctx)) {
      return 0;
    }
  }

  if (ctx->pool.live_mutex == 0) {
    /* Private pattern: drain this thread's own live set here, while its
     * own worker is still running -- nothing else can touch it. */
    while (*ctx->pool.live_count > 0) {
      if (!do_free(ctx, *ctx->pool.live_count - 1)) {
        return 0;
      }
    }
  }
  /* Shared pattern: draining is done once, single-threaded, after every
   * worker has joined -- see run()'s own comment. */

  return arg;
}

static void bind_private_pool(thread_ctx* ctx) {
  ctx->pool.live = ctx->private_live;
  ctx->pool.live_count = &ctx->private_live_count;
  ctx->pool.live_capacity = MCB_PRIVATE_MAX_LIVE_PER_THREAD;
  ctx->pool.live_bytes = &ctx->private_live_bytes;
  ctx->pool.peak_live_bytes = &ctx->private_peak_live_bytes;
  ctx->pool.peak_live_count = &ctx->private_peak_live_count;
  ctx->pool.live_mutex = 0;
}

static void bind_shared_pool(thread_ctx* ctx) {
  ctx->pool.live = g_shared_live;
  ctx->pool.live_count = &g_shared_live_count;
  ctx->pool.live_capacity = MCB_SHARED_MAX_LIVE;
  ctx->pool.live_bytes = &g_shared_live_bytes;
  ctx->pool.peak_live_bytes = &g_shared_peak_live_bytes;
  ctx->pool.peak_live_count = &g_shared_peak_live_count;
  ctx->pool.live_mutex = &g_shared_mutex;
}

static int run(uint32_t thread_count, uint32_t ops_per_thread, uint64_t seed, int shared_pattern,
                thread_ctx* contexts, uint64_t* out_elapsed_ns) {
  pthread_t threads[MCB_MAX_THREADS];
  uint32_t i;
  uint64_t start_ns, end_ns;
  int ok = 1;

  memset(g_shared_live, 0, sizeof(g_shared_live));
  g_shared_live_count = 0;
  g_shared_live_bytes = 0;
  g_shared_peak_live_bytes = 0;
  g_shared_peak_live_count = 0;

  for (i = 0; i < thread_count; ++i) {
    thread_ctx* ctx = &contexts[i];
    memset(ctx, 0, sizeof(*ctx));
    ctx->rng_state = seed ^ (UINT64_C(0x9E3779B97F4A7C15) * (uint64_t)(i + 1));
    if (ctx->rng_state == 0) {
      ctx->rng_state = 1;
    }
    ctx->ops = ops_per_thread;
    ctx->latency_ns = g_latency_storage[i];
    if (shared_pattern) {
      bind_shared_pool(ctx);
    } else {
      bind_private_pool(ctx);
    }
  }

  start_ns = monotonic_ns();
  for (i = 0; i < thread_count; ++i) {
    if (pthread_create(&threads[i], 0, worker, &contexts[i]) != 0) {
      ok = 0;
    }
  }
  for (i = 0; i < thread_count; ++i) {
    void* result = 0;
    if (pthread_join(threads[i], &result) != 0) {
      ok = 0;
    }
  }

  if (shared_pattern) {
    /* Single-threaded now (every worker has joined): drain whatever the
     * shared pool still holds, the same "verify then free" pass the
     * private pattern's own workers already did for themselves inline.
     * latency_ns is left NULL here deliberately -- this cleanup pass is
     * not part of the timed concurrent workload, so its own free() calls
     * must not dilute the reported latency distribution. */
    thread_ctx drain_ctx;
    memset(&drain_ctx, 0, sizeof(drain_ctx));
    bind_shared_pool(&drain_ctx);
    while (*drain_ctx.pool.live_count > 0) {
      if (!do_free(&drain_ctx, *drain_ctx.pool.live_count - 1)) {
        ok = 0;
        break;
      }
    }
    if (drain_ctx.failed) {
      fprintf(stderr, "malloc_contention_baseline_test: drain: %s\n", drain_ctx.fail_reason);
      ok = 0;
    }
  }
  end_ns = monotonic_ns();

  for (i = 0; i < thread_count; ++i) {
    if (contexts[i].failed) {
      fprintf(stderr, "malloc_contention_baseline_test: thread %u: %s\n", i, contexts[i].fail_reason);
      ok = 0;
    }
  }

  *out_elapsed_ns = end_ns - start_ns;
  return ok;
}

static const char* target_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#else
  return "unknown";
#endif
}

static const char* target_os(void) {
#if defined(CRT_TARGET_OS_WINDOWS)
  return "windows";
#elif defined(CRT_TARGET_OS_MACOS)
  return "macos";
#elif defined(CRT_TARGET_OS_LINUX)
  return "linux";
#else
  return "unknown";
#endif
}

static int compare_u32(const void* a, const void* b) {
  uint32_t lhs = *(const uint32_t*)a;
  uint32_t rhs = *(const uint32_t*)b;
  return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static uint32_t g_merged_latency[MCB_MAX_THREADS * MCB_MAX_LATENCY_SAMPLES_PER_THREAD];

static void print_json_result(uint32_t thread_count, uint32_t ops_per_thread, uint64_t seed, int shared_pattern,
                               thread_ctx* contexts, uint64_t elapsed_ns) {
  uint32_t merged_count = 0;
  uint64_t total_ops = 0;
  uint64_t malloc_count = 0, calloc_count = 0, aligned_count = 0;
  uint64_t free_count = 0, realloc_grow_count = 0, realloc_shrink_count = 0;
  uint64_t peak_live_bytes = 0;
  uint32_t peak_live_count = 0;
  uint64_t latency_sum = 0;
  double latency_avg = 0.0;
  double throughput;
  uint32_t i, j;

  for (i = 0; i < thread_count; ++i) {
    thread_ctx* ctx = &contexts[i];
    for (j = 0; j < ctx->latency_count && merged_count < (MCB_MAX_THREADS * MCB_MAX_LATENCY_SAMPLES_PER_THREAD); ++j) {
      g_merged_latency[merged_count++] = ctx->latency_ns[j];
    }
    total_ops += ctx->ops;
    malloc_count += ctx->malloc_count;
    calloc_count += ctx->calloc_count;
    aligned_count += ctx->aligned_count;
    free_count += ctx->free_count;
    realloc_grow_count += ctx->realloc_grow_count;
    realloc_shrink_count += ctx->realloc_shrink_count;
    if (*ctx->pool.peak_live_bytes > peak_live_bytes) {
      peak_live_bytes = *ctx->pool.peak_live_bytes;
    }
    if (*ctx->pool.peak_live_count > peak_live_count) {
      peak_live_count = *ctx->pool.peak_live_count;
    }
  }

  qsort(g_merged_latency, merged_count, sizeof(g_merged_latency[0]), compare_u32);
  for (i = 0; i < merged_count; ++i) {
    latency_sum += g_merged_latency[i];
  }
  if (merged_count != 0) {
    latency_avg = (double)latency_sum / (double)merged_count;
  }
  throughput = elapsed_ns == 0 ? 0.0 : (double)total_ops * 1e9 / (double)elapsed_ns;

  printf(
      "{"
      "\"tool\":\"malloc_contention_baseline\","
      "\"seed\":%llu,"
      "\"threads\":%u,"
      "\"ops_per_thread\":%u,"
      "\"pattern\":\"%s\","
      "\"arch\":\"%s\","
      "\"os\":\"%s\","
      "\"elapsed_ns\":%llu,"
      "\"throughput_ops_per_sec\":%.2f,"
      "\"latency_ns\":{\"min\":%u,\"p50\":%u,\"p99\":%u,\"max\":%u,\"avg\":%.2f,\"samples\":%u},"
      "\"live\":{\"peak_count\":%u,\"peak_bytes\":%llu},"
      "\"op_counts\":{\"malloc\":%llu,\"calloc\":%llu,\"aligned\":%llu,\"free\":%llu,"
      "\"realloc_grow\":%llu,\"realloc_shrink\":%llu},"
      "\"status\":\"ok\""
      "}\n",
      (unsigned long long)seed, thread_count, ops_per_thread, shared_pattern ? "shared" : "private",
      target_arch(), target_os(), (unsigned long long)elapsed_ns, throughput,
      merged_count != 0 ? g_merged_latency[0] : 0u,
      merged_count != 0 ? g_merged_latency[(uint32_t)(((uint64_t)50 * (merged_count - 1)) / 100)] : 0u,
      merged_count != 0 ? g_merged_latency[(uint32_t)(((uint64_t)99 * (merged_count - 1)) / 100)] : 0u,
      merged_count != 0 ? g_merged_latency[merged_count - 1] : 0u, latency_avg, merged_count,
      peak_live_count, (unsigned long long)peak_live_bytes, (unsigned long long)malloc_count,
      (unsigned long long)calloc_count, (unsigned long long)aligned_count, (unsigned long long)free_count,
      (unsigned long long)realloc_grow_count, (unsigned long long)realloc_shrink_count);
}

static uint64_t parse_u64(const char* text, uint64_t fallback) {
  char* end = 0;
  unsigned long long value;
  if (text == 0 || text[0] == '\0') {
    return fallback;
  }
  value = strtoull(text, &end, 10);
  if (end == text) {
    return fallback;
  }
  return (uint64_t)value;
}

static thread_ctx g_contexts[MCB_MAX_THREADS];

int main(int argc, char** argv) {
  uint64_t seed = UINT64_C(0x436f6e74656e7449); /* "ContentI" */
  uint32_t threads = MCB_DEFAULT_THREADS;
  uint32_t ops = MCB_DEFAULT_OPS_PER_THREAD;
  int json = 0;
  int pattern_arg = -1; /* -1: unused in CTest mode, which always runs both */
  int i;
  uint64_t elapsed_ns = 0;

  for (i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--threads=", 10) == 0) {
      threads = (uint32_t)parse_u64(argv[i] + 10, threads);
    } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      threads = (uint32_t)parse_u64(argv[++i], threads);
    } else if (strncmp(argv[i], "--ops=", 6) == 0) {
      ops = (uint32_t)parse_u64(argv[i] + 6, ops);
    } else if (strcmp(argv[i], "--ops") == 0 && i + 1 < argc) {
      ops = (uint32_t)parse_u64(argv[++i], ops);
    } else if (strncmp(argv[i], "--seed=", 7) == 0) {
      seed = parse_u64(argv[i] + 7, seed);
    } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = parse_u64(argv[++i], seed);
    } else if (strcmp(argv[i], "--json") == 0) {
      json = 1;
    } else if (strcmp(argv[i], "--pattern") == 0 && i + 1 < argc) {
      ++i;
      if (strcmp(argv[i], "private") == 0) {
        pattern_arg = 0;
      } else if (strcmp(argv[i], "shared") == 0) {
        pattern_arg = 1;
      } else {
        fprintf(stderr, "malloc_contention_baseline_test: --pattern must be 'private' or 'shared'\n");
        return 2;
      }
    }
  }

  if (threads == 0 || threads > MCB_MAX_THREADS) {
    fprintf(stderr, "malloc_contention_baseline_test: --threads must be 1..%u\n", MCB_MAX_THREADS);
    return 2;
  }

  if (json) {
    int shared_pattern = pattern_arg == 1;
    if (!run(threads, ops, seed, shared_pattern, g_contexts, &elapsed_ns)) {
      return 1;
    }
    print_json_result(threads, ops, seed, shared_pattern, g_contexts, elapsed_ns);
    return 0;
  }

  /* CTest mode: small, fixed thread count, both patterns in turn. */
  if (!run(threads, ops, seed, 0, g_contexts, &elapsed_ns)) {
    return 1;
  }
  if (!run(threads, ops, seed ^ UINT64_C(0x5368617265645f31) /* "Shared_1" */, 1, g_contexts, &elapsed_ns)) {
    return 1;
  }

  printf("malloc_contention_baseline_test: ok\n");
  return 0;
}
