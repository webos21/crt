#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "mman_test: %s\n", message);
  return 1;
}

int main(void) {
  char* memory;
  char* remapped;
  size_t i;

  if (MAP_TYPE != 0x0f || MAP_FIXED != 0x10 || MS_ASYNC != 1 || MS_INVALIDATE != 2 ||
      MS_SYNC != 4 || MREMAP_MAYMOVE != 1 || MREMAP_FIXED != 2 ||
      POSIX_MADV_NORMAL != MADV_NORMAL || POSIX_MADV_DONTNEED != MADV_DONTNEED) {
    return fail("constants");
  }

  errno = 0;
  if (mmap(0, 0, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
      errno != EINVAL) {
    return fail("zero length");
  }

  memory = (char*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED) {
    return fail("mmap anonymous");
  }

  for (i = 0; i < 4096; ++i) {
    memory[i] = (char)(i & 0x7f);
  }
  for (i = 0; i < 4096; ++i) {
    if (memory[i] != (char)(i & 0x7f)) {
      munmap(memory, 4096);
      return fail("memory contents");
    }
  }

  memcpy(memory, "mapped text", sizeof("mapped text"));
  if (strcmp(memory, "mapped text") != 0) {
    munmap(memory, 4096);
    return fail("string contents");
  }

  errno = 0;
  if (mprotect(memory, 0, PROT_READ) != -1 || errno != EINVAL) {
    munmap(memory, 4096);
    return fail("mprotect zero length");
  }
  if (mprotect(memory, 4096, PROT_READ) != 0 ||
      mprotect(memory, 4096, PROT_READ | PROT_WRITE) != 0) {
    munmap(memory, 4096);
    return fail("mprotect");
  }
  memory[0] = 'M';
  if (memory[0] != 'M') {
    munmap(memory, 4096);
    return fail("mprotect write");
  }

  if (madvise(memory, 4096, MADV_NORMAL) != 0) {
    munmap(memory, 4096);
    return fail("madvise normal");
  }
  if (posix_madvise(memory, 4096, POSIX_MADV_NORMAL) != 0) {
    munmap(memory, 4096);
    return fail("posix_madvise normal");
  }

  if (munmap(memory, 4096) != 0) {
    return fail("munmap");
  }

  memory = (char*)mmap64(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory == MAP_FAILED) {
    return fail("mmap64 anonymous");
  }
  remapped = (char*)mremap(memory, 4096, 4096, 0);
  if (remapped == MAP_FAILED) {
    if (errno != ENOSYS) {
      munmap(memory, 4096);
      return fail("mremap errno");
    }
    remapped = memory;
  }
  if (munmap(remapped, 4096) != 0) {
    return fail("munmap remapped");
  }

  puts("mman_test: ok");
  return 0;
}
