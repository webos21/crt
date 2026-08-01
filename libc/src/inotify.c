#include <errno.h>
#include <sys/inotify.h>

int inotify_init(void) {
  return __set_errno(ENOSYS);
}

int inotify_add_watch(int fd, const char* path, uint32_t mask) {
  (void)fd;
  (void)path;
  (void)mask;
  return __set_errno(ENOSYS);
}

int inotify_rm_watch(int fd, int wd) {
  (void)fd;
  (void)wd;
  return __set_errno(ENOSYS);
}
