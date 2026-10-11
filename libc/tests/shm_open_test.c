/* shm_open()/shm_unlink(): create, size, map, reopen by name, unlink. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void) {
  const char* name = "/crt_shm_open_test";
  int fd;
  int second;
  char* map;

  shm_unlink(name);
  if (shm_open("no-slash", O_RDWR | O_CREAT, 0600) != -1 || errno != EINVAL) {
    fprintf(stderr, "shm_open_test: bad name accepted\n");
    return 1;
  }
  if (shm_open("/a/b", O_RDWR | O_CREAT, 0600) != -1 || errno != EINVAL) {
    fprintf(stderr, "shm_open_test: nested name accepted\n");
    return 1;
  }
  fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0 || ftruncate(fd, 4096) != 0) {
    fprintf(stderr, "shm_open_test: create failed (%d)\n", errno);
    return 1;
  }
  if (shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600) != -1 || errno != EEXIST) {
    fprintf(stderr, "shm_open_test: O_EXCL did not fail\n");
    return 1;
  }
  map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    fprintf(stderr, "shm_open_test: mmap failed\n");
    return 1;
  }
  strcpy(map, "shared");
  second = shm_open(name, O_RDONLY, 0);
  if (second < 0) {
    fprintf(stderr, "shm_open_test: reopen failed\n");
    return 1;
  }
  {
    char buffer[8] = {0};
    if (pread(second, buffer, 6, 0) != 6 || strcmp(buffer, "shared") != 0) {
      fprintf(stderr, "shm_open_test: content not shared\n");
      return 1;
    }
  }
  if (shm_unlink(name) != 0 || shm_unlink(name) != -1 || errno != ENOENT) {
    fprintf(stderr, "shm_open_test: unlink semantics\n");
    return 1;
  }
  printf("shm_open_test: ok\n");
  return 0;
}
