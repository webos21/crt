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
  size_t i;

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

  if (munmap(memory, 4096) != 0) {
    return fail("munmap");
  }

  puts("mman_test: ok");
  return 0;
}
