#include <errno.h>
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>

ssize_t getrandom(void* buffer, size_t buffer_size, unsigned int flags) {
  unsigned char* out = (unsigned char*)buffer;
  size_t done = 0;
  int fd;

  if (buffer == 0 && buffer_size != 0) {
    return __set_errno(EFAULT);
  }
  if ((flags & ~(GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE)) != 0) {
    return __set_errno(EINVAL);
  }

  fd = open((flags & GRND_RANDOM) ? "/dev/random" : "/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  while (done < buffer_size) {
    ssize_t n = read(fd, out + done, buffer_size - done);
    if (n < 0) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return done == 0 ? -1 : (ssize_t)done;
    }
    if (n == 0) break;
    done += (size_t)n;
  }
  close(fd);
  return (ssize_t)done;
}

int getentropy(void* buffer, size_t buffer_size) {
  size_t done = 0;

  if (buffer_size > 256) {
    return __set_errno(EIO);
  }
  while (done < buffer_size) {
    ssize_t n = getrandom((unsigned char*)buffer + done, buffer_size - done, 0);
    if (n < 0) return -1;
    if (n == 0) return __set_errno(EIO);
    done += (size_t)n;
  }
  return 0;
}
