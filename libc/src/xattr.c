#include <errno.h>
#include <sys/xattr.h>

ssize_t getxattr(const char* path, const char* name, void* value, size_t size) {
  (void)path;
  (void)name;
  (void)value;
  (void)size;
  return __set_errno(ENODATA);
}

ssize_t lgetxattr(const char* path, const char* name, void* value, size_t size) {
  (void)path;
  (void)name;
  (void)value;
  (void)size;
  return __set_errno(ENODATA);
}

ssize_t fgetxattr(int fd, const char* name, void* value, size_t size) {
  (void)fd;
  (void)name;
  (void)value;
  (void)size;
  return __set_errno(ENODATA);
}

ssize_t listxattr(const char* path, char* list, size_t size) {
  (void)path;
  (void)list;
  (void)size;
  return 0;
}

ssize_t llistxattr(const char* path, char* list, size_t size) {
  (void)path;
  (void)list;
  (void)size;
  return 0;
}

ssize_t flistxattr(int fd, char* list, size_t size) {
  (void)fd;
  (void)list;
  (void)size;
  return 0;
}

int setxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
  (void)path;
  (void)name;
  (void)value;
  (void)size;
  (void)flags;
  return __set_errno(ENOTSUP);
}

int lsetxattr(const char* path, const char* name, const void* value, size_t size, int flags) {
  (void)path;
  (void)name;
  (void)value;
  (void)size;
  (void)flags;
  return __set_errno(ENOTSUP);
}

int fsetxattr(int fd, const char* name, const void* value, size_t size, int flags) {
  (void)fd;
  (void)name;
  (void)value;
  (void)size;
  (void)flags;
  return __set_errno(ENOTSUP);
}

int removexattr(const char* path, const char* name) {
  (void)path;
  (void)name;
  return __set_errno(ENODATA);
}

int lremovexattr(const char* path, const char* name) {
  (void)path;
  (void)name;
  return __set_errno(ENODATA);
}

int fremovexattr(int fd, const char* name) {
  (void)fd;
  (void)name;
  return __set_errno(ENODATA);
}
