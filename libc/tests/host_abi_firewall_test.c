/* host_abi_firewall_test -- TODO.md's "Allocator baseline validation
 * before Upper Runtime" tranche 7 ("freeze the Host ABI firewall before
 * hardware decode"), the positive half. See docs/host_abi_firewall.md's
 * "Mechanical detection" section for the full design and host_abi_
 * firewall_fake_host.c for the synthetic host library this drives.
 *
 * Simulates a real CRT adapter's own discipline: only ever stores/passes
 * the opaque fake_host_object pointer and calls the host's own retain/
 * release API -- never reinterprets it, never calls this project's own
 * malloc()/free() on it directly (that violation is host_abi_firewall_
 * fault_test.c's own job to prove gets caught). Exercises both a plain
 * create-then-release cycle and a retain/release pair on top of it (a
 * real adapter often retains a handle it did not itself create, e.g. a
 * shared buffer referenced from two internal call sites), then asserts
 * the host's own create/destroy counters came out exactly balanced. */

#include "host_abi_firewall_fake_host.h"

#include <stdio.h>

#define OBJECT_COUNT 1000u

static int fail(const char* message) {
  fprintf(stderr, "host_abi_firewall_test: %s\n", message);
  return 1;
}

int main(void) {
  fake_host_object* objects[OBJECT_COUNT];
  unsigned i;

  for (i = 0; i < OBJECT_COUNT; ++i) {
    objects[i] = fake_host_create();
    if (objects[i] == 0) {
      return fail("fake_host_create");
    }
  }

  for (i = 0; i < OBJECT_COUNT; ++i) {
    fake_host_retain(objects[i]);
  }
  for (i = 0; i < OBJECT_COUNT; ++i) {
    fake_host_release(objects[i]); /* drops the retain() above only */
  }
  if (fake_host_live_count() != OBJECT_COUNT) {
    return fail("unexpected live count after a balanced retain/release pair");
  }

  for (i = 0; i < OBJECT_COUNT; ++i) {
    fake_host_release(objects[i]); /* drops the original create() reference */
  }

  if (fake_host_create_count() != OBJECT_COUNT) {
    return fail("create_count mismatch");
  }
  if (fake_host_destroy_count() != OBJECT_COUNT) {
    return fail("destroy_count mismatch");
  }
  if (fake_host_live_count() != 0) {
    return fail("live_count did not reach zero");
  }

  printf("host_abi_firewall_test: ok\n");
  return 0;
}
