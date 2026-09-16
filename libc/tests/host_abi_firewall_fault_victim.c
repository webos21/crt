/* host_abi_firewall_fault_victim -- deliberately violates the Host ABI
 * firewall rule (docs/host_abi_firewall.md): allocates a fake host
 * object through the host's own API, then frees it via this project's
 * OWN ordinary free() instead of fake_host_release() -- exactly the
 * class of bug the rule exists to prevent ("CRT adapter code must never
 * ... call CRT's own free()/malloc()-family functions on it"). Not a
 * ctest target itself -- it is EXPECTED to crash. host_abi_firewall_
 * fault_test.c spawns it and checks that it actually did, matching
 * tranche 6's malloc_fault_victim.c/malloc_fault_test.c shape exactly.
 *
 * This executable's own ordinary malloc()/free() are a direct-linked,
 * CRT_DEBUG_MALLOC-enabled instance (same recipe as malloc_debug_test/
 * malloc_fault_victim), while fake_host_create()/_release() operate
 * through a *different* instance (malloc_fault_instance_b.c's renamed
 * symbols, reused as-is from tranche 6) -- so the free() below is a
 * genuine cross-instance free, not merely a logic error, and CRT_DEBUG_
 * MALLOC's owner-mismatch check catches it the same way it would catch
 * any other cross-instance free. */

#include "host_abi_firewall_fake_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  fake_host_object* obj = fake_host_create();
  if (obj == 0) {
    fprintf(stderr, "host_abi_firewall_fault_victim: fake_host_create failed\n");
    _exit(65);
  }

  free(obj); /* the violation: expected to trap (cross-instance owner mismatch) */

  fprintf(stderr, "VICTIM DID NOT TRAP\n");
  return 66;
}
