/* malloc_fault_victim -- deliberately triggers one of three real heap
 * bugs CRT_DEBUG_MALLOC (libc/src/malloc.c) is meant to catch, for
 * TODO.md's "Allocator baseline validation before Upper Runtime" tranche
 * 6 ("add expected-fault tests for double free, cross-instance owner
 * mismatch, and canary damage"). Not a ctest target itself -- it is
 * EXPECTED to crash -- malloc_fault_test.c spawns it (once per fault
 * kind) and checks that it actually did.
 *
 * Both malloc.c's usual public symbols (linked here the same way
 * malloc_debug_test.c already does: libc/src/malloc.c compiled directly
 * into this executable with CRT_DEBUG_MALLOC=1, so this direct object's
 * own malloc()/free()/... satisfy those symbols instead of the ordinary
 * archive's) and a second, independent instance (malloc_fault_instance_
 * b.c's own renamed crt_fault_instance_b_*() functions) are available
 * here, so run_owner_mismatch() below can genuinely allocate through one
 * real instance and free through a different one.
 *
 * Every fault path ends with __builtin_trap() firing inside malloc.c
 * (CRT_DEBUG_MALLOC's own detection code) -- this program should never
 * reach its own final "VICTIM DID NOT TRAP" line for any of the three
 * argv modes; if it does, the diagnostic missed the bug. */

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void* crt_fault_instance_b_malloc(size_t size);
extern void crt_fault_instance_b_free(void* ptr);

static void run_double_free(void) {
  void* p = malloc(16);
  if (p == 0) {
    fprintf(stderr, "malloc_fault_victim: malloc failed\n");
    _exit(65);
  }
  free(p);
  free(p); /* expected to trap here: double free */
}

static void run_canary_damage(void) {
  /* align_size() rounds every request up to CRT_MALLOC_ALIGNMENT (at
   * least 16 on every supported host), so a 1-byte request always
   * leaves real, canary-painted slack right after it. */
  unsigned char* p = (unsigned char*)malloc(1);
  if (p == 0) {
    fprintf(stderr, "malloc_fault_victim: malloc failed\n");
    _exit(65);
  }
  p[8] = 0xAA; /* a real 1-byte heap-buffer-overflow into that slack */
  free(p);     /* expected to trap here: canary damage */
}

static void run_owner_mismatch(void) {
  /* Allocated by this executable's own ordinary (CRT_DEBUG_MALLOC-
   * enabled) instance, then freed through malloc_fault_instance_b.c's
   * entirely separate instance -- a real cross-instance free, the exact
   * scenario malloc.c's own owner tag exists to catch. */
  void* p = malloc(16);
  if (p == 0) {
    fprintf(stderr, "malloc_fault_victim: malloc failed\n");
    _exit(65);
  }
  crt_fault_instance_b_free(p); /* expected to trap here: owner mismatch */
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s double-free|canary-damage|owner-mismatch\n",
            argc > 0 ? argv[0] : "malloc_fault_victim");
    return 2;
  }

  if (strcmp(argv[1], "double-free") == 0) {
    run_double_free();
  } else if (strcmp(argv[1], "canary-damage") == 0) {
    run_canary_damage();
  } else if (strcmp(argv[1], "owner-mismatch") == 0) {
    run_owner_mismatch();
  } else {
    fprintf(stderr, "malloc_fault_victim: unknown fault kind: %s\n", argv[1]);
    return 2;
  }

  /* Only reached if CRT_DEBUG_MALLOC failed to trap the fault above. */
  fprintf(stderr, "VICTIM DID NOT TRAP\n");
  return 66;
}
