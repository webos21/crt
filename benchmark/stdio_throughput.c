/* Build with crt-cc -O3 and run on the same host and filesystem for comparisons. */
#include <stdio.h>
#include <string.h>
#include <time.h>

static unsigned char block[64 * 1024];

static double seconds(void) {
  struct timespec time;
  if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return -1.0;
  return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static int measure(int unbuffered) {
  const unsigned count = unbuffered ? 16 : 512;
  const double mib = (double)(sizeof(block) * count) / (1024 * 1024);
  FILE* file = tmpfile();
  double start, elapsed;
  unsigned i;

  if (!file) return 1;
  if (unbuffered && setvbuf(file, 0, _IONBF, 0) != 0) {
    fclose(file);
    return 2;
  }
  memset(block, 0xa5, sizeof(block));
  start = seconds();
  for (i = 0; i < count; ++i) {
    if (fwrite(block, sizeof(block), 1, file) != 1) {
      fclose(file);
      return 3;
    }
  }
  if (fflush(file) != 0) {
    fclose(file);
    return 4;
  }
  elapsed = seconds() - start;
  if (start < 0 || elapsed <= 0) {
    fclose(file);
    return 5;
  }
  printf("%s fwrite: %.1f MiB/s\n", unbuffered ? "unbuffered" : "buffered", mib / elapsed);
  if (unbuffered) return fclose(file) != 0;
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 6;
  }
  start = seconds();
  for (i = 0; i < count; ++i) {
    if (fread(block, sizeof(block), 1, file) != 1 || block[0] != 0xa5 ||
        block[sizeof(block) - 1] != 0xa5) {
      fclose(file);
      return 7;
    }
  }
  elapsed = seconds() - start;
  if (start < 0 || elapsed <= 0) {
    fclose(file);
    return 8;
  }
  printf("buffered fread: %.1f MiB/s\n", mib / elapsed);
  return fclose(file) != 0;
}

int main(void) {
  return measure(0) || measure(1);
}
