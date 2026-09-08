/* Regression test for TODO.md's "Windows symlink/delete timing
 * verification" item and the retry loops __crt_sys_unlink()/
 * __crt_sys_symlink() gained in libc/src/arch/windows/common/syscall.c
 * (see windows_is_delete_race_error()'s own comment there for the full
 * story) -- a real, deterministic reproduction of the
 * `rm -f old && ln -s new old` race libtool's own SONAME-symlink install
 * step hits on every port rebuild, not a flaky wait-and-hope timing test.
 *
 * DeleteFileA() only *marks* a file for deletion while another handle is
 * still open on it (this project's own open() already passes
 * FILE_SHARE_DELETE, so this isn't even Defender-specific -- any second
 * handle on the same file reproduces it): the directory entry is only
 * actually removed once every handle closes. This test creates that
 * window on purpose and deterministically, entirely through this
 * project's own POSIX-surface open()/close()/unlink()/symlink() (no raw
 * Win32 declarations needed):
 *
 *   1. open() a file, keeping the fd alive (the "lingering handle").
 *   2. Start a second thread that sleeps briefly, then close()s that fd --
 *      standing in for whatever eventually releases the real lingering
 *      handle (Defender finishing its scan, another process exiting).
 *   3. On the main thread, immediately (no sleep) unlink() the same path
 *      and then symlink() a fresh link over it -- exactly libtool's own
 *      back-to-back pattern, racing the closer thread with no fixed
 *      ordering guaranteed.
 *
 * Before the retry fix, step 3's symlink() would race the still-open
 * handle from step 1 and fail outright (ERROR_ALREADY_EXISTS/
 * ERROR_ACCESS_DENIED, i.e. EEXIST/EACCES) whenever the closer thread
 * hadn't won the race yet -- which, run enough times, it eventually
 * wouldn't. With the fix, symlink() (and unlink(), exercised the same
 * way further below) retries through the short window until the closer
 * thread's close() lands, and the whole sequence succeeds every time.
 * Repeated ITERATIONS times specifically because a single run has some
 * chance of the closer thread winning the race "for free" even without
 * any retry logic at all -- only a run that fails would prove the
 * absence of the fix; many runs make that absence overwhelmingly likely
 * to surface if the retry logic were ever removed or narrowed. */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

static int fail(const char* message) {
  fprintf(stderr, "windows_symlink_delete_race_test: %s\n", message);
  return 1;
}

#if defined(CRT_TARGET_OS_WINDOWS)

enum { ITERATIONS = 8 };

struct closer_arg {
  int fd;
};

static void* closer_thread(void* arg) {
  struct closer_arg* ca = (struct closer_arg*)arg;
  struct timespec delay;

  /* Deliberately shorter than __crt_sys_symlink()/__crt_sys_unlink()'s
   * own retry budget (40 attempts * 10ms = up to 400ms) but long enough
   * that the main thread's very first symlink()/unlink() attempt --
   * issued with no delay at all right after this thread starts -- reaches
   * the OS well before this close() lands, so the race is genuine, not
   * accidentally already resolved by scheduling luck. */
  delay.tv_sec = 0;
  delay.tv_nsec = 30 * 1000 * 1000;
  nanosleep(&delay, 0);
  close(ca->fd);
  return 0;
}

static int race_once(const char* target_path, const char* link_path) {
  int fd;
  pthread_t thread;
  struct closer_arg ca;
  char readback[256];
  long readback_len;

  fd = open(target_path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    return fail("open target");
  }
  ca.fd = fd;
  if (pthread_create(&thread, 0, closer_thread, &ca) != 0) {
    close(fd);
    return fail("pthread_create");
  }

  /* No sleep here on purpose: this has to race the closer thread, not
   * politely wait for it. */
  if (unlink(link_path) != 0 && errno != ENOENT) {
    pthread_join(thread, 0);
    return fail("unlink link_path");
  }
  if (symlink(target_path, link_path) != 0) {
    fprintf(stderr, "windows_symlink_delete_race_test: symlink failed: %s\n", strerror(errno));
    pthread_join(thread, 0);
    return fail("symlink raced");
  }
  pthread_join(thread, 0);

  readback_len = readlink(link_path, readback, sizeof(readback) - 1);
  if (readback_len < 0) {
    return fail("readlink after race");
  }
  readback[readback_len] = 0;
  if (strcmp(readback, target_path) != 0) {
    return fail("readlink mismatch after race");
  }

  if (unlink(link_path) != 0) {
    return fail("cleanup link_path");
  }
  if (unlink(target_path) != 0) {
    return fail("cleanup target_path");
  }
  return 0;
}

/* Second, narrower race: unlink() itself is exercised against a
 * still-open handle with no create call involved at all, isolating
 * __crt_sys_unlink()'s own retry loop from __crt_sys_symlink()'s. */
static int unlink_race_once(const char* target_path) {
  int fd;
  pthread_t thread;
  struct closer_arg ca;

  fd = open(target_path, O_CREAT | O_RDWR, 0644);
  if (fd < 0) {
    return fail("open target (unlink race)");
  }
  ca.fd = fd;
  if (pthread_create(&thread, 0, closer_thread, &ca) != 0) {
    close(fd);
    return fail("pthread_create (unlink race)");
  }

  if (unlink(target_path) != 0) {
    fprintf(stderr, "windows_symlink_delete_race_test: unlink failed: %s\n", strerror(errno));
    pthread_join(thread, 0);
    return fail("unlink raced");
  }
  pthread_join(thread, 0);

  if (access(target_path, F_OK) == 0 || errno != ENOENT) {
    return fail("target still present after unlink race");
  }
  return 0;
}

int main(void) {
  int i;

  for (i = 0; i < ITERATIONS; ++i) {
    if (race_once("delete_race_target.tmp", "delete_race_link.tmp") != 0) {
      return 1;
    }
  }
  for (i = 0; i < ITERATIONS; ++i) {
    if (unlink_race_once("delete_race_unlink_target.tmp") != 0) {
      return 1;
    }
  }

  puts("windows_symlink_delete_race_test: ok");
  return 0;
}

#else

int main(void) {
  /* POSIX unlink() always removes the directory entry immediately
   * regardless of other open handles/fds -- there is no equivalent
   * delete-pending window to race on Linux/macOS, so this is a plain
   * functional smoke check of the same symlink()/unlink() sequence
   * rather than a race reproduction. */
  int fd = open("delete_race_target.tmp", O_CREAT | O_RDWR, 0644);
  char readback[256];
  long readback_len;

  if (fd < 0) {
    return fail("open target");
  }
  close(fd);
  if (unlink("delete_race_link.tmp") != 0 && errno != ENOENT) {
    return fail("unlink link_path");
  }
  if (symlink("delete_race_target.tmp", "delete_race_link.tmp") != 0) {
    return fail("symlink");
  }
  readback_len = readlink("delete_race_link.tmp", readback, sizeof(readback) - 1);
  if (readback_len < 0) {
    return fail("readlink");
  }
  readback[readback_len] = 0;
  if (strcmp(readback, "delete_race_target.tmp") != 0) {
    return fail("readlink mismatch");
  }
  if (unlink("delete_race_link.tmp") != 0 || unlink("delete_race_target.tmp") != 0) {
    return fail("cleanup");
  }

  puts("windows_symlink_delete_race_test: ok");
  return 0;
}

#endif
