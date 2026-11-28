/* fork() while another thread holds the Linux thread-TLS setup lock (thread_tls.c).
 *
 * The child has only the forking thread, so a lock that its copy of memory shows as held is never
 * released and the child's next pthread_create() would hang. The first setup also records the TLS
 * modules while it holds that lock, so the child may inherit half a table.
 *
 * This fixture owns thread_tls.c. It makes one thread redo the setup and holds it inside, then
 * forks. */
#if defined(CRT_TARGET_OS_LINUX)
#include <string.h>

static int test_memcmp(const void* left, const void* right, size_t size);

#define memcmp test_memcmp
#include "../src/arch/linux/common/thread_tls.c"
#undef memcmp
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEADLINE_MILLISECONDS 10000

static pid_t holder_tid;
static int holder_parked;
static int release_holder;
static size_t modules_at_hold;
static size_t holder_size;
/* The alignment is for aarch64, where the setup declines unless the initial thread pointer is as
 * aligned as it asks of its own threads; the loader aligns it as the executable's TLS requires. */
static __thread __attribute__((aligned(64))) int thread_value = 41;

static void sleep_one_millisecond(void) {
  struct timespec delay = {0, 1000000L};

  (void)nanosleep(&delay, 0);
}

static int wait_for_flag(const int* flag) {
  int i;

  for (i = 0; i < DEADLINE_MILLISECONDS && !__atomic_load_n(flag, __ATOMIC_ACQUIRE); ++i) {
    sleep_one_millisecond();
  }
  return __atomic_load_n(flag, __ATOMIC_ACQUIRE);
}

/* The setup checks the ELF header of each shared object, which comes after it has recorded the
 * executable's TLS segment. */
static int test_memcmp(const void* left, const void* right, size_t size) {
  if (holder_tid != 0 && holder_tid == gettid()) {
    holder_tid = 0;
    modules_at_hold = tls_module_count;
    __atomic_store_n(&holder_parked, 1, __ATOMIC_RELEASE);
    (void)wait_for_flag(&release_holder);
  }
  return memcmp(left, right, size);
}

static void* holder(void* arg) {
  (void)arg;
  /* As if no setup had been tried yet, which is the state of the first pthread_create(). */
  tls_state = 0;
  tls_module_count = 0;
  tls_static_size = 0;
  holder_tid = gettid();
  holder_size = __crt_linux_thread_tls_size();
  return 0;
}

static void* child_worker(void* arg) {
  int* creator_value = (int*)arg;

  /* A thread with its own TLS block: a separate variable that starts from the initial image. */
  return (void*)(uintptr_t)(&thread_value != creator_value && thread_value == 41);
}

static int run_child(void) {
  pthread_t thread;
  void* own_tls = 0;

  thread_value = 7;
  if (pthread_create(&thread, 0, child_worker, &thread_value) != 0) {
    return 2;
  }
  if (pthread_join(thread, &own_tls) != 0) {
    return 3;
  }
  return own_tls != 0 ? 0 : 4;
}

int main(void) {
  pthread_t thread;
  pid_t child;
  pid_t waited = 0;
  int status = 0;
  int i;

  if (__crt_linux_thread_tls_size() == 0) {
    fprintf(stderr, "fork_tls_lock_test: native thread TLS is not available\n");
    return 1;
  }
  if (pthread_create(&thread, 0, holder, 0) != 0) {
    fprintf(stderr, "fork_tls_lock_test: pthread_create\n");
    return 1;
  }
  if (!wait_for_flag(&holder_parked) || modules_at_hold == 0) {
    fprintf(stderr, "fork_tls_lock_test: the holder did not stop inside the setup\n");
    return 1;
  }
  child = fork();
  if (child == 0) {
    _exit(run_child());
  }
  for (i = 0; child > 0 && i < DEADLINE_MILLISECONDS / 2; ++i) {
    waited = waitpid(child, &status, WNOHANG);
    if (waited != 0) {
      break;
    }
    sleep_one_millisecond();
  }
  __atomic_store_n(&release_holder, 1, __ATOMIC_RELEASE);
  if (child < 0) {
    fprintf(stderr, "fork_tls_lock_test: fork\n");
    return 1;
  }
  if (waited == 0) {
    (void)kill(child, SIGKILL);
    (void)waitpid(child, &status, 0);
    fprintf(stderr, "fork_tls_lock_test: the child hung in pthread_create()\n");
    return 1;
  }
  if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "fork_tls_lock_test: child status %#x\n", status);
    return 1;
  }
  if (pthread_join(thread, 0) != 0 || holder_size == 0) {
    fprintf(stderr, "fork_tls_lock_test: the parent's setup did not finish\n");
    return 1;
  }
  puts("fork_tls_lock_test: ok");
  return 0;
}
#else
#include <stdio.h>

int main(void) {
  puts("fork_tls_lock_test: skipped (Linux thread TLS setup)");
  return 0;
}
#endif
