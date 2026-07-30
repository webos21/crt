#include <stdio.h>
#include <string.h>
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
