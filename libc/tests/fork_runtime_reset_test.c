#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/random.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "fork_runtime_reset_test: %s\n", message);
  return 1;
}

static FILE* locked_stream;
static volatile int worker_locked;
static volatile int worker_release;

static void* stream_lock_worker(void* arg) {
  (void)arg;

  flockfile(locked_stream);
  worker_locked = 1;
  while (!worker_release) {
    sched_yield();
  }
  funlockfile(locked_stream);
  return 0;
}

static int test_child_stdio_lock_reset(void) {
  pthread_t thread;
  pid_t pid;
  int status = 0;

  locked_stream = tmpfile();
  if (locked_stream == 0) {
    return fail("tmpfile");
  }
  worker_locked = 0;
  worker_release = 0;
  if (pthread_create(&thread, 0, stream_lock_worker, 0) != 0) {
    fclose(locked_stream);
    return fail("pthread_create");
  }
  while (!worker_locked) {
    sched_yield();
  }
  pid = fork();
  if (pid < 0) {
    worker_release = 1;
    pthread_join(thread, 0);
    fclose(locked_stream);
#if defined(CRT_TARGET_OS_WINDOWS)
    if (errno == ENOTSUP) {
      return 0;
    }
#endif
    return fail("fork");
  }
  if (pid == 0) {
    if (ftrylockfile(locked_stream) != 0) {
      _exit(77);
    }
    funlockfile(locked_stream);
    _exit(0);
  }
  worker_release = 1;
  if (pthread_join(thread, 0) != 0) {
    fclose(locked_stream);
    return fail("pthread_join");
  }
  fclose(locked_stream);
  if (waitpid(pid, &status, 0) != pid ||
      !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0) {
    return fail("child stdio lock reset");
  }
  return 0;
}

#if defined(CRT_TARGET_OS_WINDOWS)
__declspec(dllimport) unsigned long GetProcessId(void* process);
__declspec(dllimport) void* GetModuleHandleA(const char* name);
__declspec(dllimport) void* GetProcAddress(void* module, const char* name);
void __crt_fd_after_fork_child(void);

static void* watched_parent_handle;
static int borrowed_close_attempts;

/* Test-only IAT interposition: reject and record the forbidden close even
 * when the OS has not reused the number yet. All other closes delegate to
 * the real kernel32 entry. No production API/test hook is added to libc. */
static int watched_close_handle(void* handle) {
  int (*native_close_handle)(void*);
  if (watched_parent_handle != 0 && handle == watched_parent_handle) {
    ++borrowed_close_attempts;
    return 0;
  }
  native_close_handle = (int (*)(void*))GetProcAddress(GetModuleHandleA("kernel32.dll"), "CloseHandle");
  return native_close_handle != 0 ? native_close_handle(handle) : 0;
}
int (*__imp_CloseHandle)(void*) = watched_close_handle;

/* A memory-copy child does NOT inherit the parent's non-inheritable wait
 * handles. A copied numeric value can already name a new, child-owned OS
 * object. Exercise the private fd reset before fork() normally calls it,
 * watching for a forbidden close, not relying on OS handle reuse timing. */
static int test_child_wait_handle_ownership(void) {
  pid_t sibling = fork();
  pid_t child;
  uintptr_t value;
  void* borrowed = 0;
  int status = 0;

  if (sibling < 0) return fail("ownership sibling fork");
  if (sibling == 0) _exit(0);
  for (value = 4; value < 65536; value += 4) {
    if (GetProcessId((void*)value) == (unsigned long)sibling) {
      borrowed = (void*)value;
      break;
    }
  }
  if (borrowed == 0) {
    waitpid(sibling, &status, 0);
    return fail("ownership locate parent wait handle");
  }
  child = _Fork();
  if (child == 0) {
    watched_parent_handle = borrowed;
    borrowed_close_attempts = 0;
    __crt_fd_after_fork_child();
    watched_parent_handle = 0;
    _exit(borrowed_close_attempts != 0 ? 83 : 0);
  }
  if (child < 0) {
    waitpid(sibling, &status, 0);
    return fail("ownership child _Fork");
  }
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "fork_runtime_reset_test: ownership child status=%#x\n", (unsigned int)status);
    waitpid(sibling, &status, 0);
    return fail("child closed a handle it does not own");
  }
  if (waitpid(sibling, &status, 0) != sibling || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return fail("ownership sibling wait");
  return 0;
}
#endif

/* The Windows PAL resolves the native RNG lazily. A copied cache address
 * may point into a DLL absent from the child. Both inherited fd reads
 * and a new getrandom/open must work after fork and after raw _Fork. */
static int test_child_random_source(int raw_fork) {
  unsigned char bytes[16];
  int fd = open("/dev/urandom", O_RDONLY);
  pid_t child;
  int status = 0;
  if (fd < 0 || read(fd, bytes, sizeof(bytes)) != (ssize_t)sizeof(bytes)) {
    if (fd >= 0) close(fd);
    return fail("parent random source");
  }
  child = raw_fork ? _Fork() : fork();
  if (child == 0) {
    /* Exercise both lazy-rebinding entry points as the child's first use. */
    if (raw_fork && getrandom(bytes, sizeof(bytes), 0) != (ssize_t)sizeof(bytes)) _exit(85);
    if (read(fd, bytes, sizeof(bytes)) != (ssize_t)sizeof(bytes)) _exit(84);
    if (!raw_fork && getrandom(bytes, sizeof(bytes), 0) != (ssize_t)sizeof(bytes)) _exit(85);
    close(fd);
    _exit(0);
  }
  close(fd);
  if (child < 0) return fail("random source fork");
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "fork_runtime_reset_test: random child status=%#x\n", (unsigned int)status);
    return fail("child random source");
  }
  return 0;
}

int main(void) {
  if (test_child_stdio_lock_reset() != 0) {
    return 1;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (test_child_wait_handle_ownership() != 0) return 1;
#endif
  if (test_child_random_source(0) != 0 || test_child_random_source(1) != 0) return 1;
  puts("fork_runtime_reset_test: ok");
  return 0;
}
