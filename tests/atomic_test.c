#include <private/crt_atomic.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "atomic_test: %s\n", message);
  return 1;
}

int main(void) {
  crt_atomic_int atomic = CRT_ATOMIC_INT_INIT(1);
  crt_spinlock lock = CRT_SPINLOCK_INIT;
  crt_once once = CRT_ONCE_INIT;
  int expected;
  int once_count = 0;
  int guarded = 0;

  if (crt_atomic_load_relaxed(&atomic) != 1) {
    return fail("load relaxed");
  }
  crt_atomic_store_release(&atomic, 2);
  if (crt_atomic_load_acquire(&atomic) != 2) {
    return fail("store release");
  }
  if (crt_atomic_exchange_acquire(&atomic, 3) != 2 ||
      crt_atomic_load_acquire(&atomic) != 3) {
    return fail("exchange");
  }
  expected = 3;
  if (!crt_atomic_compare_exchange_acq_rel(&atomic, &expected, 4) ||
      expected != 3 || crt_atomic_load_acquire(&atomic) != 4) {
    return fail("compare exchange success");
  }
  expected = 3;
  if (crt_atomic_compare_exchange_acq_rel(&atomic, &expected, 5) ||
      expected != 4 || crt_atomic_load_acquire(&atomic) != 4) {
    return fail("compare exchange failure");
  }
  if (crt_atomic_fetch_add_acq_rel(&atomic, 5) != 4 ||
      crt_atomic_load_acquire(&atomic) != 9) {
    return fail("fetch add");
  }

  crt_spin_lock(&lock);
  guarded += 7;
  crt_spin_unlock(&lock);
  if (guarded != 7) {
    return fail("spinlock");
  }

  if (crt_once_begin(&once)) {
    ++once_count;
    crt_once_complete(&once);
  }
  if (crt_once_begin(&once)) {
    ++once_count;
    crt_once_complete(&once);
  }
  if (once_count != 1) {
    return fail("once");
  }

  printf("atomic_test: ok\n");
  return 0;
}
