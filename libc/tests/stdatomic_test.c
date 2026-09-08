#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int fail(const char* message) {
  fprintf(stderr, "stdatomic_test: %s\n", message);
  return 1;
}

int main(void) {
  atomic_int a;
  atomic_flag flag = ATOMIC_FLAG_INIT;
  atomic_size_t size_val;
  atomic_uintptr_t ptr_val;
  int expected;

  atomic_init(&a, 1);
  if (atomic_load(&a) != 1) {
    return fail("init/load");
  }

  atomic_store(&a, 2);
  if (atomic_load_explicit(&a, memory_order_acquire) != 2) {
    return fail("store/load_explicit");
  }

  if (atomic_exchange(&a, 3) != 2 || atomic_load(&a) != 3) {
    return fail("exchange");
  }

  expected = 3;
  if (!atomic_compare_exchange_strong(&a, &expected, 4) || expected != 3 ||
      atomic_load(&a) != 4) {
    return fail("compare_exchange_strong success");
  }
  expected = 3;
  if (atomic_compare_exchange_strong(&a, &expected, 5) || expected != 4 ||
      atomic_load(&a) != 4) {
    return fail("compare_exchange_strong failure");
  }

  /* compare_exchange_weak may spuriously fail per the standard, so loop
   * until it actually succeeds instead of asserting success on the first
   * try -- this is the correct, standard-mandated usage pattern. */
  expected = 4;
  while (!atomic_compare_exchange_weak(&a, &expected, 6)) {
    if (expected != 4) {
      return fail("compare_exchange_weak unexpected value");
    }
  }
  if (atomic_load(&a) != 6) {
    return fail("compare_exchange_weak result");
  }

  if (atomic_fetch_add(&a, 4) != 6 || atomic_load(&a) != 10) {
    return fail("fetch_add");
  }
  if (atomic_fetch_sub(&a, 3) != 10 || atomic_load(&a) != 7) {
    return fail("fetch_sub");
  }
  if (atomic_fetch_or(&a, 8) != 7 || atomic_load(&a) != 15) {
    return fail("fetch_or");
  }
  if (atomic_fetch_and(&a, 9) != 15 || atomic_load(&a) != 9) {
    return fail("fetch_and");
  }
  if (atomic_fetch_xor(&a, 1) != 9 || atomic_load(&a) != 8) {
    return fail("fetch_xor");
  }

  if (atomic_flag_test_and_set(&flag)) {
    return fail("flag should start clear");
  }
  if (!atomic_flag_test_and_set(&flag)) {
    return fail("flag should now be set");
  }
  atomic_flag_clear(&flag);
  if (atomic_flag_test_and_set(&flag)) {
    return fail("flag should be clear again after clear()");
  }

  atomic_init(&size_val, (size_t)0);
  atomic_store(&size_val, (size_t)123);
  if (atomic_load(&size_val) != (size_t)123) {
    return fail("atomic_size_t");
  }

  atomic_init(&ptr_val, (uintptr_t)0);
  atomic_store(&ptr_val, (uintptr_t)0x1000);
  if (atomic_load(&ptr_val) != (uintptr_t)0x1000) {
    return fail("atomic_uintptr_t");
  }

  atomic_thread_fence(memory_order_seq_cst);
  atomic_signal_fence(memory_order_seq_cst);

  if (!atomic_is_lock_free(&a)) {
    return fail("atomic_int should be lock-free on every host this project builds for");
  }

  if (ATOMIC_INT_LOCK_FREE == 0) {
    return fail("ATOMIC_INT_LOCK_FREE should be nonzero");
  }

  printf("stdatomic_test: ok\n");
  return 0;
}
