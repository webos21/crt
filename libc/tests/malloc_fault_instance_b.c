/* A second, independently-instantiated copy of libc/src/malloc.c's
 * PUBLIC surface (its own private heap_head/heap_lock/free list), built
 * by #include-ing the real source after renaming every symbol it
 * actually exports at link scope. Every one of malloc.c's own internal
 * helpers and file-scope state (block_header, align_size(),
 * append_chunk(), heap_head, heap_lock, ...) is already declared
 * `static`, so it has no cross-translation-unit visibility at all and
 * needs no renaming; only the small set of symbols malloc.c genuinely
 * exports (malloc/free/calloc/realloc/posix_memalign/aligned_alloc/
 * malloc_usable_size, the Windows-only _aligned_malloc/_aligned_free, and
 * the __crt_malloc_* fork/region accessors) would otherwise collide with
 * the ordinary ones this test's own normal libc link already provides.
 *
 * This exists specifically for TODO.md's "Allocator baseline validation
 * before Upper Runtime" tranche 6 ("add expected-fault tests for ...
 * cross-instance owner mismatch"): malloc.c's own CRT_DEBUG_MALLOC
 * comment describes the real scenario this reproduces -- "a statically-
 * linked libc.a copy inside an executable, plus a separate copy inside
 * the shared libc.so any dynamically-loaded consumer ... resolves
 * through" -- without needing to actually build and dlopen() a second
 * shared library just to get two coexisting instances in one process;
 * the preprocessor rename below achieves the identical "two independent
 * allocator instances, one process" shape directly. See malloc_fault_
 * victim.c for the actual fault this second instance is used to trigger. */

#define malloc crt_fault_instance_b_malloc
#define free crt_fault_instance_b_free
#define calloc crt_fault_instance_b_calloc
#define realloc crt_fault_instance_b_realloc
#define posix_memalign crt_fault_instance_b_posix_memalign
#define aligned_alloc crt_fault_instance_b_aligned_alloc
#define malloc_usable_size crt_fault_instance_b_malloc_usable_size
#define _aligned_malloc crt_fault_instance_b__aligned_malloc
#define _aligned_free crt_fault_instance_b__aligned_free
#define __crt_malloc_after_fork_child crt_fault_instance_b_after_fork_child
#define __crt_malloc_os_region_count crt_fault_instance_b_os_region_count
#define __crt_malloc_os_region_base crt_fault_instance_b_os_region_base
#define __crt_malloc_os_region_size crt_fault_instance_b_os_region_size

#include "../src/malloc.c"
