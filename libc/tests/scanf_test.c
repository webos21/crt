#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "scanf_test: %s\n", message);
  return 1;
}

int main(void) {
  FILE* stream;
  unsigned long start = 0;
  unsigned long end = 0;
  unsigned long offset = 0;
  long inode = 0;
  char perms[10];
  char dev[10];
  char path[32];
  int count = 0;
  int value = 0;
  float f = 0.0f;
  char word[16];
  char set[16];
  char chars[4] = {0, 0, 0, 0};
  char* allocated_word = 0;
  char* allocated_set = 0;
  char* allocated_chars = 0;
  wchar_t wide_word[16];
  wchar_t* allocated_wide = 0;
  double d = 0.0;
  long double ld = 0.0L;
  long compat_long = 0;
  unsigned long compat_octal = 0;
  unsigned long compat_unsigned = 0;
  long long compat_ll = 0;
  int binary = 0;
  int fixed32 = 0;
  void* pointer = 0;

  if (sscanf("1000-1fff rwxp 40 08:01 123 /tmp/a",
             "%lx-%lx %9s %lx %9s %ld %s%n",
             &start, &end, perms, &offset, dev, &inode, path, &count) != 7) {
    return fail("sscanf maps");
  }
  if (start != 0x1000UL || end != 0x1fffUL || offset != 0x40UL || inode != 123 ||
      strcmp(perms, "rwxp") != 0 || strcmp(dev, "08:01") != 0 ||
      strcmp(path, "/tmp/a") != 0 || count != 34) {
    return fail("sscanf maps values");
  }

  if (sscanf("  -42 3.5 hello abcXYZ", "%d %f %15s %3[abc]%3c",
             &value, &f, word, set, chars) != 5) {
    return fail("sscanf mixed");
  }
  if (value != -42 || f < 3.49f || f > 3.51f || strcmp(word, "hello") != 0 ||
      strcmp(set, "abc") != 0 || memcmp(chars, "XYZ", 3) != 0) {
    return fail("sscanf mixed values");
  }

  if (sscanf("0x1.8p2 0x1.2p3", "%la %La", &d, &ld) != 2 ||
      d < 5.99 || d > 6.01 || ld < 8.99L || ld > 9.01L) {
    return fail("hex float");
  }

  if (sscanf("alpha betaXYZ", "%ms %m[be] %3mc",
             &allocated_word, &allocated_set, &allocated_chars) != 3 ||
      strcmp(allocated_word, "alpha") != 0 || strcmp(allocated_set, "be") != 0 ||
      strcmp(allocated_chars, "taX") != 0) {
    free(allocated_word);
    free(allocated_set);
    free(allocated_chars);
    return fail("allocation modifier");
  }
  free(allocated_word);
  free(allocated_set);
  free(allocated_chars);
  allocated_word = 0;
  if (sscanf("gamma", "%as", &allocated_word) != 1 || strcmp(allocated_word, "gamma") != 0) {
    free(allocated_word);
    return fail("gnu allocation modifier");
  }
  free(allocated_word);

  memset(wide_word, 0, sizeof(wide_word));
  if (sscanf("wide 0x1p1", "%ls %La", wide_word, &ld) != 2 ||
      wcscmp(wide_word, L"wide") != 0 || ld < 1.99L || ld > 2.01L) {
    return fail("wide and long double");
  }
  if (sscanf("delta", "%mls", &allocated_wide) != 1 ||
      wcscmp(allocated_wide, L"delta") != 0) {
    free(allocated_wide);
    return fail("wide allocation modifier");
  }
  free(allocated_wide);

  memset(set, 0, sizeof(set));
  if (sscanf("bdf", "%[a-c-e]", set) != 1 || strcmp(set, "bd") != 0) {
    return fail("bionic scanset range");
  }
  if (sscanf("0b101 0B11", "%i %b", &binary, &fixed32) != 2 ||
      binary != 5 || fixed32 != 3) {
    return fail("bionic binary integer");
  }
  if (sscanf("-12 17 20 123456789", "%D %O %U %qd", &compat_long,
             &compat_octal, &compat_unsigned, &compat_ll) != 4 ||
      compat_long != -12 || compat_octal != 15 || compat_unsigned != 20 ||
      compat_ll != 123456789LL) {
    return fail("bsd compat integer");
  }
  if (sscanf("42 0x1234", "%w32d %p", &fixed32, &pointer) != 2 ||
      fixed32 != 42 || pointer != (void*)(uintptr_t)0x1234) {
    return fail("bionic integer modifiers");
  }

  stream = fopen("scanf_test.tmp", "w+");
  if (stream == 0) {
    return fail("fopen");
  }
  if (fputs("77 file", stream) == EOF || fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    remove("scanf_test.tmp");
    return fail("setup");
  }
  value = 0;
  word[0] = 0;
  if (fscanf(stream, "%d %15s", &value, word) != 2 || value != 77 ||
      strcmp(word, "file") != 0) {
    fclose(stream);
    remove("scanf_test.tmp");
    return fail("fscanf");
  }
  if (fclose(stream) != 0 || remove("scanf_test.tmp") != 0) {
    return fail("cleanup");
  }

  printf("scanf_test: ok\n");
  return 0;
}
