/* Real implementation behind host_abi_firewall_fake_host.h -- see that
 * header's own top comment and docs/host_abi_firewall.md's "Mechanical
 * detection" section for the full design.
 *
 * Allocates through crt_fault_instance_b_malloc()/_free() (tranche 6's
 * malloc_fault_instance_b.c -- a second, independently-instantiated copy
 * of libc/src/malloc.c's public surface, reused here as-is) rather than
 * this translation unit's own ordinary malloc()/free(). That is the
 * whole point: it makes this "fake host library" a genuinely separate
 * allocator domain from whatever "CRT adapter" code links against, the
 * same way a real host library (linked as its own DSO, with its own
 * allocator) is a separate domain from this project's own statically-
 * linked CRT -- so a caller that wrongly calls its own ordinary free()
 * on a fake_host_object instead of fake_host_release() triggers a real,
 * mechanically-detectable cross-instance owner mismatch (CRT_DEBUG_
 * MALLOC, tranche 6), not just a hopefully-still-working accident. */

#include "host_abi_firewall_fake_host.h"

#include <stddef.h>

extern void* crt_fault_instance_b_malloc(size_t size);
extern void crt_fault_instance_b_free(void* ptr);

/* Real struct definition, private to this translation unit -- see this
 * file's own top comment for why callers never see this layout. */
struct fake_host_object {
  unsigned magic;
  unsigned refcount;
};

#define FAKE_HOST_MAGIC 0x484f5354u /* "HOST" */

static unsigned g_create_count;
static unsigned g_destroy_count;

fake_host_object* fake_host_create(void) {
  fake_host_object* obj = (fake_host_object*)crt_fault_instance_b_malloc(sizeof(fake_host_object));
  if (obj == 0) {
    return 0;
  }
  obj->magic = FAKE_HOST_MAGIC;
  obj->refcount = 1;
  g_create_count++;
  return obj;
}

void fake_host_retain(fake_host_object* obj) {
  if (obj == 0 || obj->magic != FAKE_HOST_MAGIC) {
    /* A real host library would be entitled to assume this can never
     * happen (its own API contract), and this project's own opaque
     * handles carry no such self-check at all -- this synthetic one adds
     * it purely so a CRT-side bug (passing something that never came
     * from fake_host_create() at all) fails loudly here too, not just
     * the deliberate free()-instead-of-release() violation this file's
     * own top comment is really about. */
    __builtin_trap();
  }
  obj->refcount++;
}

void fake_host_release(fake_host_object* obj) {
  if (obj == 0 || obj->magic != FAKE_HOST_MAGIC) {
    __builtin_trap();
  }
  obj->refcount--;
  if (obj->refcount == 0) {
    obj->magic = 0; /* poison before returning to this domain's own allocator */
    crt_fault_instance_b_free(obj);
    g_destroy_count++;
  }
}

unsigned fake_host_create_count(void) {
  return g_create_count;
}

unsigned fake_host_destroy_count(void) {
  return g_destroy_count;
}

unsigned fake_host_live_count(void) {
  return g_create_count - g_destroy_count;
}
