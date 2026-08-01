#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int flock(int fd, int operation) {
  struct flock lock;
  int cmd = (operation & LOCK_NB) != 0 ? F_SETLK : F_SETLKW;

  if ((operation & ~(LOCK_SH | LOCK_EX | LOCK_NB | LOCK_UN)) != 0) {
    errno = EINVAL;
    return -1;
  }
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_pid = 0;
  if ((operation & LOCK_UN) != 0) {
    lock.l_type = F_UNLCK;
  } else if ((operation & LOCK_EX) != 0) {
    lock.l_type = F_WRLCK;
  } else if ((operation & LOCK_SH) != 0) {
    lock.l_type = F_RDLCK;
  } else {
    errno = EINVAL;
    return -1;
  }
  return fcntl(fd, cmd, &lock);
}
