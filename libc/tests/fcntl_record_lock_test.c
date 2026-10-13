/* POSIX record locks (fcntl F_SETLK/F_GETLK) as SQLite's unix VFS uses them.
 *
 * SQLite takes SHARED as: read lock PENDING byte, read lock the SHARED range, unlock the PENDING byte;
 * RESERVED as a write lock on one byte; EXCLUSIVE as a write lock on PENDING and then on the SHARED
 * range it already holds as a read lock (a conversion); and it gives them back by converting the range
 * to a read lock and unlocking PENDING..RESERVED, two bytes it locked separately, with ONE call, through
 * whichever descriptor it has at hand. A byte-range lock that is a thin wrapper of LockFileEx (an owner
 * handle, exact unlock regions, no conversion) fails the first of these with "database is locked".
 *
 * The same sequence runs against a second process for the conflicts only another process can see. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

#define PENDING_BYTE 0x40000000L
#define RESERVED_BYTE (PENDING_BYTE + 1)
#define SHARED_FIRST (PENDING_BYTE + 2)
#define SHARED_SIZE 510L

static int failures;

#define CHECK(condition, message)                                                                   \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      fprintf(stderr, "fcntl_record_lock_test: %s (line %d, errno %d)\n", message, __LINE__, errno); \
      ++failures;                                                                                   \
    }                                                                                               \
  } while (0)

static int lock_range(int fd, int type, long start, long length) {
  struct flock lock;

  memset(&lock, 0, sizeof(lock));
  lock.l_type = (short)type;
  lock.l_whence = SEEK_SET;
  lock.l_start = start;
  lock.l_len = length;
  return fcntl(fd, F_SETLK, &lock);
}

/* 0: nobody else holds a conflicting lock; 1: someone does. */
static int foreign_conflict(int fd, int type, long start, long length) {
  struct flock lock;

  memset(&lock, 0, sizeof(lock));
  lock.l_type = (short)type;
  lock.l_whence = SEEK_SET;
  lock.l_start = start;
  lock.l_len = length;
  if (fcntl(fd, F_GETLK, &lock) != 0) {
    return -1;
  }
  return lock.l_type != F_UNLCK;
}

/* The child: tries the locks another process of SQLite would try, and reports with its exit status. */
static int run_child(const char* path) {
  int fd = open(path, O_RDWR);
  int status = 0;

  if (fd < 0) {
    return 90;
  }
  /* The parent holds a shared lock on the SHARED range: readers are welcome, a writer is not. */
  if (lock_range(fd, F_RDLCK, SHARED_FIRST, SHARED_SIZE) != 0) {
    status |= 1;
  } else {
    lock_range(fd, F_UNLCK, SHARED_FIRST, SHARED_SIZE);
  }
  if (lock_range(fd, F_WRLCK, SHARED_FIRST, SHARED_SIZE) == 0) {
    status |= 2; /* must have been refused */
    lock_range(fd, F_UNLCK, SHARED_FIRST, SHARED_SIZE);
  } else if (errno != EAGAIN && errno != EACCES) {
    status |= 4;
  }
  /* The parent holds RESERVED: a second reserver is refused too. */
  if (lock_range(fd, F_WRLCK, RESERVED_BYTE, 1) == 0) {
    status |= 8;
    lock_range(fd, F_UNLCK, RESERVED_BYTE, 1);
  }
  close(fd);
  return status;
}

int main(int argc, char** argv) {
  char path[256];
  int a;
  int b;
  int status = 0;
  pid_t pid;
  char* child_argv[4];
  char zero = 0;

  if (argc > 2 && strcmp(argv[1], "child") == 0) {
    return run_child(argv[2]);
  }

  snprintf(path, sizeof(path), "fcntl_record_lock_test.%d.tmp", (int)getpid());
  a = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  b = open(path, O_RDWR);
  CHECK(a >= 0 && b >= 0, "open the file twice");
  if (a < 0 || b < 0) {
    return 1;
  }
  CHECK(write(a, &zero, 1) == 1, "file has a byte");

  /* SHARED */
  CHECK(lock_range(a, F_RDLCK, PENDING_BYTE, 1) == 0, "read-lock the pending byte");
  CHECK(lock_range(a, F_RDLCK, SHARED_FIRST, SHARED_SIZE) == 0, "read-lock the shared range");
  CHECK(lock_range(a, F_UNLCK, PENDING_BYTE, 1) == 0, "unlock the pending byte");
  /* RESERVED */
  CHECK(lock_range(a, F_WRLCK, RESERVED_BYTE, 1) == 0, "write-lock the reserved byte");

  /* Another process: shared is shared, exclusive and a second reserver are not. */
  child_argv[0] = argv[0];
  child_argv[1] = (char*)"child";
  child_argv[2] = path;
  child_argv[3] = 0;
  CHECK(posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) == 0, "start the other process");
  CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "another process: shared is granted, exclusive and a second RESERVED are refused");

  /* F_GETLK sees what another process would conflict with: nothing of ours */
  CHECK(foreign_conflict(b, F_WRLCK, SHARED_FIRST, SHARED_SIZE) == 0, "our own locks are not conflicts");

  /* EXCLUSIVE: PENDING, then the shared range as a write lock: a conversion of what a holds. */
  CHECK(lock_range(a, F_WRLCK, PENDING_BYTE, 1) == 0, "write-lock the pending byte");
  CHECK(lock_range(a, F_WRLCK, SHARED_FIRST, SHARED_SIZE) == 0, "convert the shared range to a write lock");

  /* Back down, through the OTHER descriptor: convert to shared, unlock PENDING..RESERVED in one call. */
  CHECK(lock_range(b, F_RDLCK, SHARED_FIRST, SHARED_SIZE) == 0, "convert the range back to a read lock (other fd)");
  CHECK(lock_range(b, F_UNLCK, PENDING_BYTE, 2) == 0, "unlock pending and reserved with one call");
  /* A part of a range: unlock the middle of the shared range, then the rest. */
  CHECK(lock_range(a, F_UNLCK, SHARED_FIRST + 100, 50) == 0, "unlock a part of a locked range");
  CHECK(lock_range(a, F_UNLCK, SHARED_FIRST, SHARED_SIZE) == 0, "unlock the remainder");
  CHECK(lock_range(a, F_UNLCK, SHARED_FIRST, SHARED_SIZE) == 0, "unlocking what is not locked succeeds");

  /* With everything released a write lock is granted again, the child sees nothing. */
  CHECK(lock_range(a, F_WRLCK, SHARED_FIRST, SHARED_SIZE) == 0, "write-lock after the unlocks");
  CHECK(lock_range(a, F_UNLCK, SHARED_FIRST, SHARED_SIZE) == 0, "and release it");

  close(a);
  close(b);
  unlink(path);
  if (failures != 0) {
    fprintf(stderr, "fcntl_record_lock_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("fcntl_record_lock_test: ok\n");
  return 0;
}
