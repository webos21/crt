/* malloc_fork_regions_test -- TODO.md's "Allocator baseline validation
 * before Upper Runtime", tranche 5 ("exercise fork interaction and
 * allocator regions"): a many-region, fragmented-heap fork regression.
 *
 * No existing fork test (fork_test.c, fork_runtime_reset_test.c,
 * fork_signal_test.c) touches the allocator's own heap state at all --
 * they all fork with a small, simple, single-region heap. That leaves a
 * real gap specific to this project: on Windows, fork() has no native OS
 * support at all, so libc/src/arch/windows/{x86_64,aarch64}/fork_
 * memcopy.c instead walks malloc.c's own __crt_malloc_os_region_count()/
 * _base()/_size() accessors and manually copies each OS region's bytes
 * into the child's address space -- a mechanism this project wrote itself
 * (docs/windows_fork_emulation.md) and that has never been exercised here
 * against a heap spanning more than a handful of regions. Linux/macOS
 * fork() is the real OS syscall (copy-on-write), needing no such
 * per-region bookkeeping -- but running the identical test there too
 * confirms this project's own __crt_malloc_after_fork_child() heap_lock
 * reset (libc/src/process.c's __crt_atfork_child()) is itself correct
 * under a genuinely fragmented, multi-region heap, not just a trivial one.
 *
 * Scenario: allocate NUM_BLOCKS blocks, each comfortably larger than a
 * single heap chunk so each is virtually guaranteed to force its own
 * fresh OS region (append_chunk() in malloc.c) rather than being
 * satisfied by leftover free space -- confirmed, not assumed:
 * __crt_malloc_os_region_count() is checked directly before forking.
 * Every block is filled with a distinct, known byte pattern. A quarter of
 * the blocks are freed before forking, deliberately leaving holes in the
 * free list alongside the remaining live blocks -- a genuinely fragmented
 * multi-region heap, not just "many regions, all still tidy."
 *
 * After fork(): the CHILD verifies every surviving block's inherited
 * content still matches its own known pattern (proves the OS-level copy,
 * native COW or this project's own memory-copy fork, reached every live
 * region correctly), then mutates all of them with a different pattern
 * and allocates a brand-new block of its own (proves the child's heap is
 * independently writable and its post-fork allocator state is actually
 * functional, not just readable). The PARENT, after the child reports
 * success and exits cleanly, re-verifies its OWN blocks still show the
 * ORIGINAL pattern -- if the child's mutations leaked back into the
 * parent's own memory, that would mean the fork was aliasing shared
 * memory instead of giving the child a true independent copy, exactly
 * the class of bug a many-region memory-copy fork implementation could
 * plausibly have (e.g. a region copied to the wrong destination address,
 * or two regions accidentally mapped to the same backing pages). */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_BLOCKS 12u
/* Comfortably larger than any plausible heap chunk granularity on any
 * supported host, so each block below forces its own fresh OS region
 * rather than reusing space left over by an earlier one. Varied per
 * index so no two blocks are identically sized (a realistic, not
 * artificially uniform, fragmentation pattern). */
#define BASE_BLOCK_SIZE (256u * 1024u)
#define FREE_EVERY_NTH 3u

/* malloc.c's own private, already-existing fork-support accessor (see
 * that file's own CRT_MALLOC_MAX_OS_REGIONS comment) -- not declared in
 * any public header, but already a real, exported libc symbol. Used here
 * only to confirm this test's own scenario genuinely spans many distinct
 * OS regions before forking, not to test the accessor itself. */
extern int __crt_malloc_os_region_count(void);

typedef struct {
  unsigned char* ptr;
  size_t size;
  unsigned char fill;
  int freed_before_fork;
} block;

static int fail(const char* message) {
  fprintf(stderr, "malloc_fork_regions_test: %s\n", message);
  return 1;
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

static void free_survivors(block* blocks) {
  uint32_t i;
  for (i = 0; i < NUM_BLOCKS; ++i) {
    if (!blocks[i].freed_before_fork) {
      free(blocks[i].ptr);
    }
  }
}

static int child_main(block* blocks, int write_fd) {
  uint32_t i;
  int ok = 1;
  unsigned char* extra;
  unsigned char result;

  for (i = 0; ok && i < NUM_BLOCKS; ++i) {
    if (blocks[i].freed_before_fork) {
      continue;
    }
    if (!verify_fill(blocks[i].ptr, blocks[i].size, blocks[i].fill)) {
      ok = 0;
    }
  }

  if (ok) {
    /* Mutate independently -- see this file's own top comment for why
     * this is the setup for the parent's own critical check below. */
    for (i = 0; i < NUM_BLOCKS; ++i) {
      if (!blocks[i].freed_before_fork) {
        memset(blocks[i].ptr, (unsigned char)(blocks[i].fill ^ 0xFFu), blocks[i].size);
      }
    }
  }

  if (ok) {
    /* Allocate independently -- proves the child's own post-fork
     * heap_lock/region bookkeeping is actually functional, not just that
     * raw memory content happened to survive the copy. */
    extra = (unsigned char*)malloc(4096);
    if (extra == 0) {
      ok = 0;
    } else {
      memset(extra, 0x77, 4096);
      if (!verify_fill(extra, 4096, 0x77)) {
        ok = 0;
      }
      free(extra);
    }
  }

  result = ok ? 1 : 0;
  if (write(write_fd, &result, 1) != 1) {
    _exit(1);
  }
  close(write_fd);
  free_survivors(blocks);
  return ok ? 0 : 1;
}

int main(void) {
  block blocks[NUM_BLOCKS];
  uint32_t i;
  int pipefd[2];
  pid_t pid;
  unsigned char child_reported_ok = 0;
  int child_status = 0;

  for (i = 0; i < NUM_BLOCKS; ++i) {
    blocks[i].size = BASE_BLOCK_SIZE + (size_t)i * 4096u;
    blocks[i].ptr = (unsigned char*)malloc(blocks[i].size);
    if (blocks[i].ptr == 0) {
      return fail("malloc");
    }
    blocks[i].fill = (unsigned char)(0x40u + i);
    memset(blocks[i].ptr, blocks[i].fill, blocks[i].size);
    blocks[i].freed_before_fork = 0;
  }

  for (i = 0; i < NUM_BLOCKS; i += FREE_EVERY_NTH) {
    free(blocks[i].ptr);
    blocks[i].freed_before_fork = 1;
  }

  if (__crt_malloc_os_region_count() < 4) {
    /* A sanity check on this test's OWN scenario, not on the allocator
     * under test: if this ever fires, the size/count constants above no
     * longer force enough distinct regions on whatever host is running
     * this (e.g. a much larger heap chunk granularity than expected), and
     * the rest of this test would silently stop exercising "many
     * regions" the way tranche 5 actually calls for. */
    free_survivors(blocks);
    return fail("scenario did not create enough distinct OS regions");
  }

  if (pipe(pipefd) != 0) {
    free_survivors(blocks);
    return fail("pipe");
  }

  pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    free_survivors(blocks);
#if defined(CRT_TARGET_OS_WINDOWS)
    if (errno == ENOTSUP) {
      return 0;
    }
#endif
    return fail("fork");
  }

  if (pid == 0) {
    close(pipefd[0]);
    _exit(child_main(blocks, pipefd[1]));
  }

  close(pipefd[1]);
  if (read(pipefd[0], &child_reported_ok, 1) != 1) {
    child_reported_ok = 0;
  }
  close(pipefd[0]);

  if (waitpid(pid, &child_status, 0) != pid) {
    free_survivors(blocks);
    return fail("waitpid");
  }
  if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0 || child_reported_ok != 1) {
    free_survivors(blocks);
    return fail("child reported failure or exited abnormally");
  }

  /* The critical check this whole test exists for: the parent's own
   * memory must still show the ORIGINAL fill pattern, not whatever the
   * child mutated its own copies to. */
  for (i = 0; i < NUM_BLOCKS; ++i) {
    if (blocks[i].freed_before_fork) {
      continue;
    }
    if (!verify_fill(blocks[i].ptr, blocks[i].size, blocks[i].fill)) {
      free_survivors(blocks);
      return fail("parent memory was mutated by the child (fork copy/aliasing bug)");
    }
  }

  free_survivors(blocks);
  printf("malloc_fork_regions_test: ok\n");
  return 0;
}
