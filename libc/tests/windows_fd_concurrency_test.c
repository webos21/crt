#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

struct reader {
  char path[128];
  char expected;
  unsigned failures;
};

static pthread_barrier_t start;

static void* read_file(void* arg) {
  struct reader* reader = arg;
  unsigned i;

  pthread_barrier_wait(&start);
  for (i = 0; i < 20000; ++i) {
    char byte = 0;
    int fd = open(reader->path, O_RDONLY);
    if (fd < 0) {
      ++reader->failures;
      continue;
    }
    sched_yield();
    if (read(fd, &byte, 1) != 1 || byte != reader->expected) {
      ++reader->failures;
    }
    if (close(fd) != 0) {
      ++reader->failures;
    }
  }
  return 0;
}

int main(void) {
  char directory[] = "fd-concurrency-XXXXXX";
  struct reader readers[4] = {0};
  pthread_t threads[4];
  unsigned i;
  unsigned failures = 0;

  if (!mkdtemp(directory) || pthread_barrier_init(&start, 0, 4) != 0) return 1;
  for (i = 0; i < 4; ++i) {
    int fd;
    snprintf(readers[i].path, sizeof(readers[i].path), "%s/input-%u", directory, i);
    readers[i].expected = (char)('A' + i);
    fd = open(readers[i].path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 || write(fd, &readers[i].expected, 1) != 1 || close(fd) != 0) return 2;
  }
  for (i = 0; i < 4; ++i) {
    if (pthread_create(&threads[i], 0, read_file, &readers[i]) != 0) return 3;
  }
  for (i = 0; i < 4; ++i) {
    if (pthread_join(threads[i], 0) != 0) return 4;
    failures += readers[i].failures;
    if (unlink(readers[i].path) != 0) ++failures;
  }
  if (rmdir(directory) != 0 || pthread_barrier_destroy(&start) != 0) ++failures;
  if (failures) {
    fprintf(stderr, "windows_fd_concurrency_test: %u I/O or cleanup failures\n", failures);
    return 5;
  }
  puts("windows_fd_concurrency_test: ok");
  return 0;
}
