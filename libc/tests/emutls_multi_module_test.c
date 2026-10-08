/* One emulated-TLS runtime must serve every Windows image in the process.
 * Module A exports one control object and module B imports it.  If either DLL
 * carries a private compiler-rt emutls runtime, B reads its own per-DLL slot
 * instead of A's value and the first assertion fails.  Four private controls
 * also prove that A and B draw from one collision-free index sequence. */
#include "emutls_multi_module.h"

#include <pthread.h>
#include <stdio.h>

static int unique_nonzero(uintptr_t* values, int count) {
  int i;
  int j;
  for (i = 0; i < count; ++i) {
    if (values[i] == 0) return 0;
    for (j = i + 1; j < count; ++j) {
      if (values[i] == values[j]) return 0;
    }
  }
  return 1;
}

static void* check_new_thread(void* unused) {
  (void)unused;
  if (emutls_a_shared_get() != 11 || emutls_b_shared_get() != 11) return (void*)1;
  if (emutls_a_compiled_get() != 301 || emutls_b_compiled_get() != 401) return (void*)2;
  emutls_b_shared_set(777);
  emutls_a_compiled_set(778);
  emutls_b_compiled_set(779);
  if (emutls_a_shared_get() != 777 || emutls_b_shared_get() != 777) return (void*)3;
  return 0;
}

int main(void) {
  uintptr_t indexes[5];
  pthread_t thread;
  void* thread_result = 0;

  emutls_a_shared_set(123);
  if (emutls_b_shared_get() != 123) {
    fprintf(stderr, "emutls_multi_module_test: module B did not see module A's value\n");
    return 1;
  }
  emutls_b_shared_set(456);
  if (emutls_a_shared_get() != 456) {
    fprintf(stderr, "emutls_multi_module_test: module A did not see module B's value\n");
    return 1;
  }

  emutls_a_touch_private();
  emutls_b_touch_private();
  indexes[0] = emutls_shared_index();
  indexes[1] = emutls_a_private_index(0);
  indexes[2] = emutls_a_private_index(1);
  indexes[3] = emutls_b_private_index(0);
  indexes[4] = emutls_b_private_index(1);
  if (!unique_nonzero(indexes, 5)) {
    fprintf(stderr, "emutls_multi_module_test: module control indexes collided\n");
    return 1;
  }

  emutls_a_compiled_set(501);
  emutls_b_compiled_set(601);
  if (pthread_create(&thread, 0, check_new_thread, 0) != 0 ||
      pthread_join(thread, &thread_result) != 0 || thread_result != 0) {
    fprintf(stderr, "emutls_multi_module_test: new-thread isolation failed (%p)\n",
            thread_result);
    return 1;
  }
  if (emutls_a_shared_get() != 456 || emutls_b_shared_get() != 456 ||
      emutls_a_compiled_get() != 501 || emutls_b_compiled_get() != 601) {
    fprintf(stderr, "emutls_multi_module_test: worker overwrote the main thread\n");
    return 1;
  }

  printf("emutls_multi_module_test: ok\n");
  return 0;
}
