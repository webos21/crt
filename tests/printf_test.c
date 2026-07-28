#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "printf_test: %s\n", message);
  return 1;
}

int main(void) {
  char buffer[128];
  int result;

  result = snprintf(buffer, sizeof(buffer), "%s %c %d %u %x %X %%", "fmt", 'A', -42, 42u, 0x2a, 0x2a);
  if (result != 20 || strcmp(buffer, "fmt A -42 42 2a 2A %") != 0) {
    return fail("snprintf basic");
  }

  result = snprintf(buffer, 8, "abcdefghi");
  if (result != 9 || strcmp(buffer, "abcdefg") != 0) {
    return fail("snprintf truncation");
  }

  result = snprintf(buffer, sizeof(buffer), "%lld %llu", -9223372036854775807LL - 1LL,
                    18446744073709551615ULL);
  if (result != 41 ||
      strcmp(buffer, "-9223372036854775808 18446744073709551615") != 0) {
    return fail("long long");
  }

  result = snprintf(buffer, sizeof(buffer), "|%08d|%-6s|%+d|% d|%#x|%#o|%.3s|%5zu|%zd|",
                    42, "hi", 7, 7, 0x2a, 10u, "abcdef", (size_t)9, (ssize_t)-3);
  if (strcmp(buffer, "|00000042|hi    |+7| 7|0x2a|012|abc|    9|-3|") != 0) {
    return fail("flags width precision");
  }

  printf("printf_test: %s %d %x\n", "ok", 7, 255);
  return 0;
}
