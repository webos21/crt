/* malloc_baseline_test -- the deterministic allocator workload and
 * machine-readable result format for TODO.md's "Allocator baseline
 * validation before Upper Runtime", tranche 1 ("Build a deterministic
 * workload and result format").
 *
 * Run with no arguments (the shape ctest uses), this exercises a small,
 * fixed-size, fully deterministic workload purely for correctness -- a
 * bounded regression, fast enough for routine CTest, printing the usual
 * "<name>: ok" line tranche 3 asks routine CTest to keep.
 *
 * Run with --ops/--seed/--json (the shape an opt-in host-side baseline
 * runner drives), the same workload logic scales to whatever operation
 * count is requested and prints one machine-readable JSON line instead:
 * the workload parameters, elapsed time, throughput, an approximate
 * latency distribution, and live/peak allocation counters, all captured
 * from THIS run so a failure or regression can be replayed exactly by
 * re-running with the same --ops/--seed. tools/run_allocator_baseline.py
 * drives this binary at the 10^3/10^4/10^5/10^6 tiers this tranche calls
 * for and collects the resulting JSON lines.
 *
 * The workload itself (tranche 2, "cover realistic allocation shapes"):
 * a bounded-size live set undergoing random churn -- new allocations
 * (malloc/calloc/posix_memalign, weighted, across small/medium/large/
 * occasional mapping-sized requests) once the live set has room, and
 * otherwise a free or a grow/shrink realloc of a randomly chosen live
 * entry. Every live entry's payload is filled with a known byte pattern
 * and reverified before any operation that reads, moves, or releases it
 * -- malloc_test.c's own "fill, then verify before consuming" discipline,
 * just applied continuously across a long, randomized run instead of a
 * handful of fixed cases. A corrupted or unexpectedly-failing allocation
 * is reported and fails the run explicitly; this harness never traps
 * (unlike CRT_DEBUG_MALLOC's canary/owner checks in malloc.c) since it is
 * meant to run in ordinary, non-diagnostic builds too.
 *
 * The live-set array and the latency-sample buffer are both fixed-size
 * static storage, not heap-allocated: this harness measures malloc.c's
 * own allocator, and must not let its own bookkeeping compete for the
 * same heap it is timing. */

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* malloc.c's own private, already-existing fork-support accessors (see
 * that file's own CRT_MALLOC_MAX_OS_REGIONS comment) -- not declared in
 * any public header, but already real, exported libc symbols. Exposed
 * here only to help diagnose a real, currently-unexplained gap this
 * harness found between its own in-process elapsed_ns and the host-
 * observed wall time at the 10^5/10^6 tiers (see run_allocator_baseline.py
 * output): if that gap tracks os_regions rather than op count, it points
 * at the allocator's own "mmap once, never munmap" design (every
 * append_chunk() call is permanent) rather than at anything this harness
 * or the CLOCK_MONOTONIC PAL implementation is doing wrong. */
extern int __crt_malloc_os_region_count(void);

#define MALLOC_BASELINE_DEFAULT_OPS 2000u
#define MALLOC_BASELINE_DEFAULT_SEED UINT64_C(0x4d616c6c6f634254) /* "MallocBT" */
#define MALLOC_BASELINE_MAX_LIVE 4096u
/* Covers the largest tier tranche 1 calls for (10^6) without ever growing
 * dynamically; extra operations beyond this many simply stop contributing
 * new latency samples; earlier samples are enough to compute a stable
 * distribution. */
#define MALLOC_BASELINE_MAX_LATENCY_SAMPLES (1u << 20)

typedef struct {
  unsigned char* ptr;
  size_t size;
  size_t alignment; /* 0 for plain malloc/calloc, else posix_memalign's own */
  unsigned char fill;
} live_entry;

static live_entry g_live[MALLOC_BASELINE_MAX_LIVE];
static uint32_t g_live_count;

static uint32_t g_latency_ns[MALLOC_BASELINE_MAX_LATENCY_SAMPLES];
static uint32_t g_latency_count;

static uint64_t g_malloc_count;
static uint64_t g_calloc_count;
static uint64_t g_aligned_count;
static uint64_t g_free_count;
static uint64_t g_realloc_grow_count;
static uint64_t g_realloc_shrink_count;

static uint64_t g_live_bytes;
static uint64_t g_peak_live_bytes;
static uint32_t g_peak_live_count;

static uint64_t g_rng_state;

static int fail(const char* message) {
  fprintf(stderr, "malloc_baseline_test: %s\n", message);
  return 1;
}

/* xorshift64* (Vigna): small, fast, fully deterministic from a 64-bit
 * seed -- the same seed always reproduces the same operation sequence,
 * satisfying the "replay a failure or regression exactly" requirement. */
static uint64_t next_rand(void) {
  uint64_t x = g_rng_state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  g_rng_state = x;
  return x * UINT64_C(0x2545F4914F6CDD1D);
}

static uint32_t rand_below(uint32_t bound) {
  return bound == 0 ? 0 : (uint32_t)(next_rand() % (uint64_t)bound);
}

static uint64_t monotonic_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static void record_latency(uint64_t start_ns, uint64_t end_ns) {
  if (g_latency_count < MALLOC_BASELINE_MAX_LATENCY_SAMPLES) {
    uint64_t delta = end_ns - start_ns;
    g_latency_ns[g_latency_count++] = delta > UINT32_MAX ? UINT32_MAX : (uint32_t)delta;
  }
}

static void track_live_added(size_t size) {
  g_live_bytes += size;
  if (g_live_bytes > g_peak_live_bytes) {
    g_peak_live_bytes = g_live_bytes;
  }
  if (g_live_count > g_peak_live_count) {
    g_peak_live_count = g_live_count;
  }
}

static void track_live_removed(size_t size) {
  g_live_bytes -= size;
}

static void track_live_resized(size_t old_size, size_t new_size) {
  g_live_bytes = g_live_bytes - old_size + new_size;
  if (g_live_bytes > g_peak_live_bytes) {
    g_peak_live_bytes = g_live_bytes;
  }
}

/* tranche 2's size-class mix: mostly small, a good share of medium, a
 * meaningful tail of large, and a rare mapping-sized outlier -- weights
 * chosen to resemble a real application's allocation profile rather than
 * a uniform distribution across an unrealistic range. */
static size_t pick_size(void) {
  uint32_t roll = rand_below(1000);

  if (roll < 600) {
    return 16 + rand_below(113); /* 16..128 */
  }
  if (roll < 900) {
    return 129 + rand_below(3968); /* 129..4096 */
  }
  if (roll < 998) {
    return 4097 + rand_below(61440); /* 4097..65536 */
  }
  return 1048576 + rand_below(3u * 1048576u); /* ~1..4 MiB, mapping-sized */
}

static size_t pick_alignment(void) {
  static const size_t alignments[] = {16, 32, 64, 128, 256, 4096};
  return alignments[rand_below((uint32_t)(sizeof(alignments) / sizeof(alignments[0])))];
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

static int do_allocate(void) {
  uint32_t kind;
  size_t size;
  unsigned char* ptr;
  unsigned char fill;
  size_t alignment = 0;
  uint64_t start_ns, end_ns;

  if (g_live_count >= MALLOC_BASELINE_MAX_LIVE) {
    return 1; /* live set full -- caller falls back to a churn op instead */
  }

  size = pick_size();
  kind = rand_below(100);
  fill = (unsigned char)(next_rand() & 0xFFu);

  if (kind < 10) {
    /* calloc: verify the zero-fill contract itself before this harness
     * overwrites it with its own tracked fill pattern. */
    start_ns = monotonic_ns();
    ptr = (unsigned char*)calloc(1, size);
    end_ns = monotonic_ns();
    if (ptr == 0) {
      return fail("calloc returned NULL");
    }
    if (!verify_fill(ptr, size, 0)) {
      return fail("calloc did not zero-fill");
    }
    record_latency(start_ns, end_ns);
    g_calloc_count++;
  } else if (kind < 20) {
    alignment = pick_alignment();
    start_ns = monotonic_ns();
    if (posix_memalign((void**)&ptr, alignment, size) != 0) {
      return fail("posix_memalign failed");
    }
    end_ns = monotonic_ns();
    if (((uintptr_t)ptr & (alignment - 1)) != 0) {
      return fail("posix_memalign result misaligned");
    }
    record_latency(start_ns, end_ns);
    g_aligned_count++;
  } else {
    start_ns = monotonic_ns();
    ptr = (unsigned char*)malloc(size);
    end_ns = monotonic_ns();
    if (ptr == 0) {
      return fail("malloc returned NULL");
    }
    record_latency(start_ns, end_ns);
    g_malloc_count++;
  }

  memset(ptr, fill, size);
  g_live[g_live_count].ptr = ptr;
  g_live[g_live_count].size = size;
  g_live[g_live_count].alignment = alignment;
  g_live[g_live_count].fill = fill;
  g_live_count++;
  track_live_added(size);
  return 1;
}

static int do_free(uint32_t index) {
  live_entry entry = g_live[index];
  uint64_t start_ns, end_ns;

  if (!verify_fill(entry.ptr, entry.size, entry.fill)) {
    return fail("live block content corrupted before free");
  }
  start_ns = monotonic_ns();
  free(entry.ptr);
  end_ns = monotonic_ns();
  record_latency(start_ns, end_ns);
  g_free_count++;
  track_live_removed(entry.size);

  g_live[index] = g_live[g_live_count - 1];
  g_live_count--;
  return 1;
}

static int do_realloc(uint32_t index) {
  live_entry* entry = &g_live[index];
  size_t old_size = entry->size;
  size_t new_size;
  size_t verify_size;
  unsigned char* new_ptr;
  unsigned char new_fill;
  uint64_t start_ns, end_ns;
  int growing;

  if (!verify_fill(entry->ptr, entry->size, entry->fill)) {
    return fail("live block content corrupted before realloc");
  }

  growing = rand_below(100) < 60;
  if (growing) {
    new_size = old_size + 16 + rand_below((uint32_t)(old_size < UINT32_MAX ? old_size + 1 : UINT32_MAX));
  } else {
    new_size = 1 + rand_below((uint32_t)(old_size));
  }

  start_ns = monotonic_ns();
  new_ptr = (unsigned char*)realloc(entry->ptr, new_size);
  end_ns = monotonic_ns();
  if (new_ptr == 0) {
    return fail("realloc returned NULL");
  }
  record_latency(start_ns, end_ns);
  if (growing) {
    g_realloc_grow_count++;
  } else {
    g_realloc_shrink_count++;
  }

  verify_size = old_size < new_size ? old_size : new_size;
  if (!verify_fill(new_ptr, verify_size, entry->fill)) {
    return fail("realloc did not preserve content");
  }
  if (entry->alignment != 0 && ((uintptr_t)new_ptr & (entry->alignment - 1)) != 0) {
    return fail("realloc did not preserve alignment");
  }

  new_fill = (unsigned char)(next_rand() & 0xFFu);
  memset(new_ptr, new_fill, new_size);
  track_live_resized(old_size, new_size);
  entry->ptr = new_ptr;
  entry->size = new_size;
  entry->fill = new_fill;
  return 1;
}

static int run_workload(uint64_t seed, uint64_t ops, uint64_t* out_elapsed_ns) {
  uint64_t i;
  uint64_t start_ns, end_ns;

  g_rng_state = seed != 0 ? seed : 1; /* xorshift64* needs a nonzero state */
  g_live_count = 0;
  g_latency_count = 0;
  g_malloc_count = g_calloc_count = g_aligned_count = 0;
  g_free_count = g_realloc_grow_count = g_realloc_shrink_count = 0;
  g_live_bytes = g_peak_live_bytes = 0;
  g_peak_live_count = 0;

  start_ns = monotonic_ns();
  for (i = 0; i < ops; ++i) {
    uint32_t roll;

    if (g_live_count == 0 || rand_below(100) < 30) {
      if (!do_allocate()) {
        return 0;
      }
      continue;
    }

    roll = rand_below(100);
    if (roll < 50) {
      if (!do_free(rand_below(g_live_count))) {
        return 0;
      }
    } else {
      if (!do_realloc(rand_below(g_live_count))) {
        return 0;
      }
    }
  }

  /* Drain: every surviving live entry gets one final content
   * verification, then is freed, leaving a clean heap and full op
   * coverage regardless of how the random walk above ended. */
  while (g_live_count > 0) {
    if (!do_free(g_live_count - 1)) {
      return 0;
    }
  }
  end_ns = monotonic_ns();

  *out_elapsed_ns = end_ns - start_ns;
  return 1;
}

static int compare_u32(const void* a, const void* b) {
  uint32_t lhs = *(const uint32_t*)a;
  uint32_t rhs = *(const uint32_t*)b;
  return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static uint32_t percentile(uint32_t p) {
  uint32_t index;
  if (g_latency_count == 0) {
    return 0;
  }
  index = (uint32_t)(((uint64_t)p * (g_latency_count - 1)) / 100);
  return g_latency_ns[index];
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

static void print_json_result(uint64_t seed, uint64_t ops, uint64_t elapsed_ns, uint64_t process_ns) {
  double throughput = elapsed_ns == 0 ? 0.0 : (double)ops * 1e9 / (double)elapsed_ns;
  uint64_t latency_sum = 0;
  double latency_avg = 0.0;
  uint32_t i;
  int os_regions = __crt_malloc_os_region_count();

  qsort(g_latency_ns, g_latency_count, sizeof(g_latency_ns[0]), compare_u32);
  for (i = 0; i < g_latency_count; ++i) {
    latency_sum += g_latency_ns[i];
  }
  if (g_latency_count != 0) {
    latency_avg = (double)latency_sum / (double)g_latency_count;
  }

  printf(
      "{"
      "\"tool\":\"malloc_baseline\","
      "\"seed\":%llu,"
      "\"ops\":%llu,"
      "\"arch\":\"%s\","
      "\"os\":\"%s\","
      "\"elapsed_ns\":%llu,"
      "\"process_ns\":%llu,"
      "\"throughput_ops_per_sec\":%.2f,"
      "\"latency_ns\":{\"min\":%u,\"p50\":%u,\"p99\":%u,\"max\":%u,\"avg\":%.2f,\"samples\":%u},"
      "\"live\":{\"count\":%u,\"peak_count\":%u,\"bytes\":%llu,\"peak_bytes\":%llu},"
      "\"op_counts\":{\"malloc\":%llu,\"calloc\":%llu,\"aligned\":%llu,\"free\":%llu,"
      "\"realloc_grow\":%llu,\"realloc_shrink\":%llu},"
      /* os_regions: malloc.c's own heap_os_region_count -- the total
       * number of distinct mmap()/VirtualAlloc() regions this process has
       * ever mapped, since append_chunk() never releases one back to the
       * OS. Included specifically to help attribute any gap between
       * elapsed_ns/process_ns (this harness's own in-process clock) and a
       * caller's own externally-observed wall time: if that gap tracks
       * this count rather than ops, it points at OS-level cost from the
       * sheer number of live mappings (e.g. address-space teardown at
       * process exit), not at the workload loop itself. */
      "\"os_regions\":%d,"
      "\"status\":\"ok\""
      "}\n",
      (unsigned long long)seed, (unsigned long long)ops, target_arch(), target_os(),
      (unsigned long long)elapsed_ns, (unsigned long long)process_ns, throughput,
      g_latency_count != 0 ? g_latency_ns[0] : 0u, percentile(50), percentile(99),
      g_latency_count != 0 ? g_latency_ns[g_latency_count - 1] : 0u, latency_avg, g_latency_count,
      /* g_live_count/g_live_bytes are always 0 here -- run_workload()'s own
       * drain pass (above) frees every surviving entry before returning, so
       * the "live" object below reports the peak the workload actually
       * reached, not a snapshot at some arbitrary stopping point. */
      g_live_count, g_peak_live_count, (unsigned long long)g_live_bytes,
      (unsigned long long)g_peak_live_bytes, (unsigned long long)g_malloc_count,
      (unsigned long long)g_calloc_count, (unsigned long long)g_aligned_count,
      (unsigned long long)g_free_count, (unsigned long long)g_realloc_grow_count,
      (unsigned long long)g_realloc_shrink_count, os_regions);
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

int main(int argc, char** argv) {
  uint64_t seed = MALLOC_BASELINE_DEFAULT_SEED;
  uint64_t ops = MALLOC_BASELINE_DEFAULT_OPS;
  int json = 0;
  int i;
  uint64_t elapsed_ns = 0;
  uint64_t process_start_ns = monotonic_ns();

  for (i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--ops=", 6) == 0) {
      ops = parse_u64(argv[i] + 6, ops);
    } else if (strcmp(argv[i], "--ops") == 0 && i + 1 < argc) {
      ops = parse_u64(argv[++i], ops);
    } else if (strncmp(argv[i], "--seed=", 7) == 0) {
      seed = parse_u64(argv[i] + 7, seed);
    } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = parse_u64(argv[++i], seed);
    } else if (strcmp(argv[i], "--json") == 0) {
      json = 1;
    }
  }

  if (ops > (uint64_t)UINT32_MAX) {
    return fail("--ops exceeds this harness's own supported range");
  }

  if (!run_workload(seed, ops, &elapsed_ns)) {
    return 1;
  }

  if (json) {
    /* Measured right before printing, not after: the qsort()/printf()
     * inside print_json_result() itself should stay cheap and roughly
     * constant-ish relative to ops, so this is effectively "time spent in
     * main() up through the workload," the same span a caller's own
     * externally-observed wall time can be compared against to localize
     * any gap to before main() even starts, inside main() but outside the
     * timed workload, or strictly after main() returns (process exit). */
    print_json_result(seed, ops, elapsed_ns, monotonic_ns() - process_start_ns);
  } else {
    printf("malloc_baseline_test: ok\n");
  }
  return 0;
}
