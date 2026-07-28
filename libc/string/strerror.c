#include <errno.h>
#include <stdio.h>
#include <string.h>

struct errno_name {
  int value;
  const char* message;
};

static const struct errno_name errno_names[] = {
    {EPERM, "Operation not permitted"},
    {ENOENT, "No such file or directory"},
    {EINTR, "Interrupted system call"},
    {EIO, "Input/output error"},
    {EBADF, "Bad file descriptor"},
    {EAGAIN, "Resource temporarily unavailable"},
    {ENOMEM, "Cannot allocate memory"},
    {EACCES, "Permission denied"},
    {EBUSY, "Device or resource busy"},
    {EEXIST, "File exists"},
    {EXDEV, "Invalid cross-device link"},
    {ENODEV, "No such device"},
    {ENOTDIR, "Not a directory"},
    {EISDIR, "Is a directory"},
    {EINVAL, "Invalid argument"},
    {EMFILE, "Too many open files"},
    {ENOSPC, "No space left on device"},
    {EPIPE, "Broken pipe"},
    {ERANGE, "Numerical result out of range"},
    {ENOSYS, "Function not implemented"},
    {ENOTSUP, "Operation not supported"},
    {ETIMEDOUT, "Connection timed out"},
};

static const char* errno_message(int errnum) {
  size_t i;

  for (i = 0; i < sizeof(errno_names) / sizeof(errno_names[0]); ++i) {
    if (errno_names[i].value == errnum) {
      return errno_names[i].message;
    }
  }
  return 0;
}

int strerror_r(int errnum, char* buf, size_t buflen) {
  const char* message;

  if (buf == 0 || buflen == 0) {
    return EINVAL;
  }
  message = errno_message(errnum);
  if (message != 0) {
    size_t len = strlen(message);
    if (len >= buflen) {
      len = buflen - 1;
    }
    memcpy(buf, message, len);
    buf[len] = 0;
    return 0;
  }
  snprintf(buf, buflen, "Unknown error %d", errnum);
  return EINVAL;
}

char* strerror(int errnum) {
  static char buffer[64];

  (void)strerror_r(errnum, buffer, sizeof(buffer));
  return buffer;
}
