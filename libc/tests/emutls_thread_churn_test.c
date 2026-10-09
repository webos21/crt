/* Bound Windows emulated-TLS growth across short-lived CRT pthreads.
 *
 * Windows lowers this __thread object through libc's process-wide
 * __emutls_get_address().  Two equal, sequential batches make the useful
 * distinction: allocator warm-up may grow the process during the first batch,
 * but a missing pthread-exit cleanup grows it by another ~32 MiB in the second.
 * Keep this bounded so it remains suitable for ordinary CTest.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#if defined(CRT_TARGET_OS_WINDOWS)

#define CRT_WINAPI
#define EMUTLS_BYTES (256U * 1024U)
#define WARMUP_THREADS 8
#define BATCH_THREADS 128
#define SECOND_BATCH_LIMIT (8U * 1024U * 1024U)
#define TOTAL_GROWTH_LIMIT (16U * 1024U * 1024U)

typedef unsigned long DWORD;
typedef int BOOL;
typedef void* HANDLE;

typedef struct {
  DWORD cb;
  DWORD page_fault_count;
  size_t peak_working_set_size;
  size_t working_set_size;
  size_t quota_peak_paged_pool_usage;
  size_t quota_paged_pool_usage;
  size_t quota_peak_nonpaged_pool_usage;
  size_t quota_nonpaged_pool_usage;
  size_t pagefile_usage;
  size_t peak_pagefile_usage;
  size_t private_usage;
} process_memory_counters_ex;

__declspec(dllimport) HANDLE CRT_WINAPI GetCurrentProcess(void);
__declspec(dllimport) BOOL CRT_WINAPI K32GetProcessMemoryInfo(
    HANDLE process, process_memory_counters_ex* counters, DWORD size);

static __thread unsigned char emutls_payload[EMUTLS_BYTES];

static void* touch_emutls(void* argument) {
  uintptr_t seed = (uintptr_t)argument;
  size_t offset;

  for (offset = 0; offset < sizeof(emutls_payload); offset += 4096) {
    emutls_payload[offset] = (unsigned char)(seed + offset / 4096);
  }
  if ((seed & 1U) == 0) {
    pthread_exit((void*)(uintptr_t)emutls_payload[0]);
  }
  return (void*)(uintptr_t)emutls_payload[0];
}

static int run_threads(int count) {
  int index;

  for (index = 0; index < count; ++index) {
    pthread_t thread;
    if (pthread_create(&thread, 0, touch_emutls, (void*)(uintptr_t)(index + 1)) != 0 ||
        pthread_join(thread, 0) != 0) {
      fprintf(stderr, "emutls_thread_churn_test: thread %d failed\n", index);
      return 1;
    }
  }
  return 0;
}

static size_t private_bytes(void) {
  process_memory_counters_ex counters = {0};

  counters.cb = (DWORD)sizeof(counters);
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb)) {
    return 0;
  }
  return counters.private_usage;
}

static size_t positive_delta(size_t after, size_t before) {
  return after > before ? after - before : 0;
}

int main(void) {
  size_t baseline;
  size_t after_first;
  size_t after_second;
  size_t second_growth;
  size_t total_growth;

  if (run_threads(WARMUP_THREADS) != 0) return 1;
  baseline = private_bytes();
  if (baseline == 0 || run_threads(BATCH_THREADS) != 0) return 1;
  after_first = private_bytes();
  if (run_threads(BATCH_THREADS) != 0) return 1;
  after_second = private_bytes();

  second_growth = positive_delta(after_second, after_first);
  total_growth = positive_delta(after_second, baseline);
  printf("emutls_thread_churn_test: private baseline=%zu first=%zu second=%zu "
         "second_growth=%zu total_growth=%zu\n",
         baseline, after_first, after_second, second_growth, total_growth);
  if (second_growth > SECOND_BATCH_LIMIT || total_growth > TOTAL_GROWTH_LIMIT) {
    fprintf(stderr,
            "emutls_thread_churn_test: unbounded per-thread growth "
            "(limits second=%u total=%u)\n",
            SECOND_BATCH_LIMIT, TOTAL_GROWTH_LIMIT);
    return 1;
  }
  printf("emutls_thread_churn_test: ok\n");
  return 0;
}

#else

int main(void) {
  printf("emutls_thread_churn_test: ok (skipped: Windows emulated TLS only)\n");
  return 0;
}

#endif
