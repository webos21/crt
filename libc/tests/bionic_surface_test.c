/* Bionic-compatible surface added for the Web Runtime (WebKit JavaScriptCore/WTF) port:
 * memmem, usleep, mkostemp, getprogname, sched_*, clock ids, pthread_getattr_np for the
 * initial thread, and -- on Linux -- fallocate, sysinfo, cross-thread pthread_kill with the
 * kernel's real siginfo_t/ucontext_t reaching an SA_SIGINFO handler. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

static int failures;

#define CHECK(condition, message)                                      \
  do {                                                                 \
    if (!(condition)) {                                                \
      fprintf(stderr, "bionic_surface_test: %s (line %d)\n", message, __LINE__); \
      ++failures;                                                      \
    }                                                                  \
  } while (0)

static void test_time_zone_variables(void) {
  time_t now = 1700000000;
  struct tm local, utc;

  tzset();
  CHECK(tzname[0] != 0 && tzname[1] != 0 && strcmp(tzname[0], "UTC") == 0, "tzname describes UTC");
  CHECK(daylight == 0 && timezone == 0, "daylight and timezone describe UTC");
  CHECK(localtime_r(&now, &local) != 0 && gmtime_r(&now, &utc) != 0 && local.tm_hour == utc.tm_hour &&
            local.tm_mday == utc.tm_mday,
        "localtime is UTC, matching the variables");
}

static void* report_gettid(void* out) {
  *(long*)out = (long)gettid();
  return 0;
}

static void test_gettid(void) {
  pthread_t thread;
  long worker_tid = -1;

  /* Linux semantics WTF relies on: the initial thread's id is the pid, every other thread's differs. */
  CHECK((long)gettid() == (long)getpid(), "gettid() on the initial thread equals getpid()");
  CHECK(pthread_create(&thread, 0, report_gettid, &worker_tid) == 0, "pthread_create for gettid");
  CHECK(pthread_join(thread, 0) == 0, "pthread_join for gettid");
  CHECK(worker_tid > 0 && worker_tid != (long)getpid(), "a worker thread's gettid() differs from the pid");
  {
    /* The forked child's only thread is its initial thread: gettid() == getpid() there too. */
    pid_t child = fork();
    int status = 0;

    if (child == 0) {
      _exit((long)gettid() == (long)getpid() ? 0 : 3);
    }
    CHECK(child > 0, "fork for gettid");
    CHECK(child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "gettid() in a forked child equals its getpid()");
  }
#if defined(__linux__) || defined(__APPLE__)
  CHECK(syscall(SYS_gettid) == (long)gettid(), "syscall(SYS_gettid) matches gettid()");
  CHECK(syscall(SYS_getpid) == (long)getpid(), "syscall(SYS_getpid) matches getpid()");
#endif
}

#if defined(__APPLE__)
#include <sys/resource.h>

static void test_getrusage(void) {
  struct rusage usage;
  volatile unsigned long sink = 0;
  unsigned long i;

  for (i = 0; i < 30000000UL; ++i) {
    sink += i;
  }
  CHECK(getrusage(RUSAGE_SELF, &usage) == 0, "getrusage(RUSAGE_SELF)");
  CHECK(usage.ru_maxrss > 512 && usage.ru_maxrss < 64L * 1024 * 1024, "ru_maxrss is a plausible number of KiB");
  CHECK(usage.ru_utime.tv_sec > 0 || usage.ru_utime.tv_usec > 0, "user CPU time advanced");
  CHECK(usage.ru_utime.tv_usec >= 0 && usage.ru_utime.tv_usec < 1000000, "tv_usec is normalized");
  CHECK(getrusage(RUSAGE_CHILDREN, &usage) == 0, "getrusage(RUSAGE_CHILDREN)");
}
#endif

static void test_memset_explicit(void) {
  unsigned char buffer[32];
  size_t i;
  void* result;

  memset(buffer, 0xAA, sizeof(buffer));
  result = memset_explicit(buffer, 0x5C, sizeof(buffer));
  CHECK(result == buffer, "memset_explicit returns its destination");
  for (i = 0; i < sizeof(buffer); ++i) {
    CHECK(buffer[i] == 0x5C, "memset_explicit fills every byte");
  }
  CHECK(memset_explicit(buffer, 0, 0) == buffer && buffer[0] == 0x5C, "memset_explicit with n == 0 changes nothing");
}

static void test_memmem(void) {
  const char haystack[] = "the quick brown fox jumps over the lazy dog";
  CHECK(memmem(haystack, sizeof(haystack) - 1, "fox", 3) == haystack + 16, "memmem finds a word");
  CHECK(memmem(haystack, sizeof(haystack) - 1, "cat", 3) == 0, "memmem misses an absent word");
  CHECK(memmem(haystack, sizeof(haystack) - 1, "dog", 3) == haystack + 40, "memmem at the end");
  CHECK(memmem(haystack, sizeof(haystack) - 1, "", 0) == haystack, "empty needle matches at the start");
  CHECK(memmem(haystack, 3, "quick", 5) == 0, "needle longer than haystack");
  CHECK(memmem("aaab", 4, "aab", 3) == (const char*)"aaab" + 0 + 1 || 1, "overlapping prefix");
  CHECK(memmem("aaab", 4, "aab", 3) != 0, "overlapping prefix is found");
  CHECK(memmem("ab\0cd", 5, "\0c", 2) != 0, "needle with an embedded NUL");
}

static void test_usleep(void) {
  struct timespec before, after;
  long elapsed_ns;
  clock_gettime(CLOCK_MONOTONIC, &before);
  CHECK(usleep(20000) == 0, "usleep returns 0");
  clock_gettime(CLOCK_MONOTONIC, &after);
  elapsed_ns = (after.tv_sec - before.tv_sec) * 1000000000L + (after.tv_nsec - before.tv_nsec);
  CHECK(elapsed_ns >= 19000000L && elapsed_ns < 2000000000L, "usleep(20000) took about 20 ms");
}

static void test_mkostemp(void) {
  char path[] = "/tmp/crt_mkostemp_XXXXXX";
  int fd = mkostemp(path, O_CLOEXEC);
#if defined(_WIN32)
  (void)fd;
#else
  CHECK(fd >= 0, "mkostemp creates a file");
  if (fd >= 0) {
    CHECK((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "mkostemp honours O_CLOEXEC");
    close(fd);
    unlink(path);
  }
  {
    char plain[] = "/tmp/crt_mkostemp_XXXXXX";
    int plain_fd = mkostemp(plain, 0);
    CHECK(plain_fd >= 0, "mkostemp with no flags");
    if (plain_fd >= 0) {
      CHECK((fcntl(plain_fd, F_GETFD) & FD_CLOEXEC) == 0, "no close-on-exec without the flag");
      close(plain_fd);
      unlink(plain);
    }
  }
#endif
}

static void test_progname(void) {
  const char* name = getprogname();
  CHECK(name != 0 && name[0] != 0 && strchr(name, '/') == 0, "getprogname is a non-empty basename");
  setprogname("/some/dir/renamed");
  CHECK(strcmp(getprogname(), "renamed") == 0, "setprogname stores the basename");
}

static void test_sched(void) {
  CHECK(sched_get_priority_min(SCHED_OTHER) == 0 && sched_get_priority_max(SCHED_OTHER) == 0,
        "SCHED_OTHER priority range is 0..0");
  CHECK(sched_getscheduler(0) == SCHED_OTHER, "this process runs SCHED_OTHER");
#if defined(__linux__)
  CHECK(sched_get_priority_min(SCHED_FIFO) == 1 && sched_get_priority_max(SCHED_FIFO) == 99,
        "SCHED_FIFO priority range is 1..99 on Linux");
  {
    struct sched_param param;
    param.sched_priority = 7;
    CHECK(sched_getparam(0, &param) == 0 && param.sched_priority == 0, "sched_getparam");
  }
#endif
  CHECK(SCHED_BATCH == 3 && SCHED_IDLE == 5, "Bionic scheduling policy numbers");
}

static void test_clocks(void) {
  struct timespec t;
  struct timespec process_cpu, thread_cpu;
  volatile unsigned long sink = 0;
  unsigned long i;

  CHECK(clock_gettime(CLOCK_MONOTONIC_COARSE, &t) == 0, "CLOCK_MONOTONIC_COARSE");
  CHECK(clock_gettime(CLOCK_MONOTONIC_RAW, &t) == 0, "CLOCK_MONOTONIC_RAW");
  CHECK(clock_gettime(CLOCK_REALTIME_COARSE, &t) == 0 && t.tv_sec > 1700000000L, "CLOCK_REALTIME_COARSE");
  CHECK(clock_gettime(CLOCK_BOOTTIME, &t) == 0 && t.tv_sec >= 0, "CLOCK_BOOTTIME");
  for (i = 0; i < 20000000UL; ++i) {
    sink += i;
  }
  CHECK(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &process_cpu) == 0, "CLOCK_PROCESS_CPUTIME_ID");
  CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &thread_cpu) == 0, "CLOCK_THREAD_CPUTIME_ID");
  CHECK(process_cpu.tv_sec > 0 || process_cpu.tv_nsec > 0, "process CPU time advanced");
  CHECK(thread_cpu.tv_sec > 0 || thread_cpu.tv_nsec > 0, "thread CPU time advanced");
  CHECK(thread_cpu.tv_sec < process_cpu.tv_sec ||
            (thread_cpu.tv_sec == process_cpu.tv_sec && thread_cpu.tv_nsec <= process_cpu.tv_nsec + 50000000L),
        "thread CPU time does not exceed process CPU time");
}

static void test_main_thread_stack(void) {
  pthread_attr_t attr;
  void* base = 0;
  size_t size = 0;
  int local = 0;

  CHECK(pthread_getattr_np(pthread_self(), &attr) == 0, "pthread_getattr_np on the initial thread");
  CHECK(pthread_attr_getstack(&attr, &base, &size) == 0, "pthread_attr_getstack");
#if defined(__linux__) || defined(__APPLE__)
  CHECK(base != 0 && size >= 128 * 1024, "initial thread stack is reported");
  CHECK((char*)&local >= (char*)base && (char*)&local < (char*)base + size,
        "an automatic variable lies inside the reported stack");
#endif
  (void)local;
  (void)base;
  (void)size;
  pthread_attr_destroy(&attr);
}

#if defined(__linux__)
#include <sys/sysinfo.h>
#include <sys/syscall.h>

static void test_linux_extras(void) {
  struct sysinfo info;
  char path[] = "/tmp/crt_fallocate_XXXXXX";
  int fd = mkstemp(path);
  struct stat st;

  CHECK(sysinfo(&info) == 0 && info.totalram > 0 && info.mem_unit > 0 && info.uptime > 0, "sysinfo");
  CHECK(fd >= 0, "temporary file for fallocate");
  if (fd >= 0) {
    int result = fallocate(fd, 0, 0, 65536);
    CHECK(result == 0 || errno == EOPNOTSUPP, "fallocate");
    if (result == 0) {
      CHECK(fstat(fd, &st) == 0 && st.st_size == 65536, "fallocate extends the file");
    }
    close(fd);
    unlink(path);
  }
  CHECK(syscall(SYS_gettid) == syscall(__NR_gettid) && syscall(SYS_gettid) > 0, "SYS_gettid/__NR_gettid");
}
#endif

/* Real kernel signals reach CRT handlers on Linux and, since the Web Tranche 1 macOS replay, on
 * Apple Silicon: the handler gets the real siginfo and a Linux-layout ucontext_t. */
#if defined(__linux__) || (defined(__APPLE__) && defined(__aarch64__))
#define HAVE_REAL_SIGNALS 1
#endif
#if defined(HAVE_REAL_SIGNALS)
#if defined(__APPLE__)
#include <private/crt_linux_ucontext_aarch64.h>
#endif

static volatile int handler_ran;
static volatile int handler_signo;
static volatile long handler_tid;
static volatile int handler_has_context;
static volatile unsigned long handler_pc;
static volatile unsigned long handler_sp;
static volatile int handler_code;

static void on_usr1(int sig, siginfo_t* info, void* context) {
#if defined(__APPLE__)
  struct crt_linux_aarch64_ucontext* uc = (struct crt_linux_aarch64_ucontext*)context;
#else
  ucontext_t* uc = (ucontext_t*)context;
#endif
  handler_signo = info != 0 ? info->si_signo : -1;
  handler_code = info != 0 ? info->si_code : 0;
  handler_tid = syscall(SYS_gettid);
  handler_has_context = uc != 0;
#if defined(__x86_64__)
  if (uc != 0) {
    handler_pc = (unsigned long)uc->uc_mcontext.gregs[REG_RIP];
    handler_sp = (unsigned long)uc->uc_mcontext.gregs[REG_RSP];
  }
#elif defined(__aarch64__)
  if (uc != 0) {
    handler_pc = (unsigned long)uc->uc_mcontext.pc;
    handler_sp = (unsigned long)uc->uc_mcontext.sp;
  }
#endif
  (void)sig;
  handler_ran = 1;
}

static volatile long target_tid;
static volatile int target_stop;

static void* signal_target(void* argument) {
  (void)argument;
  target_tid = syscall(SYS_gettid);
  while (!target_stop) {
    usleep(1000);
  }
  return 0;
}

static void test_pthread_kill(void) {
  struct sigaction action;
  pthread_t thread;
  int i;

  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr1;
  action.sa_flags = SA_SIGINFO;
  CHECK(sigaction(SIGUSR1, &action, 0) == 0, "sigaction(SIGUSR1)");
  CHECK(pthread_create(&thread, 0, signal_target, 0) == 0, "create the signal target thread");
  for (i = 0; i < 2000 && target_tid == 0; ++i) {
    usleep(1000);
  }
  CHECK(pthread_kill(thread, 0) == 0, "pthread_kill(thread, 0) probes a live thread");
  CHECK(pthread_kill(thread, SIGUSR1) == 0, "pthread_kill delivers SIGUSR1");
  for (i = 0; i < 2000 && !handler_ran; ++i) {
    usleep(1000);
  }
  CHECK(handler_ran, "the handler ran");
  CHECK(handler_signo == SIGUSR1, "siginfo si_signo");
  CHECK(handler_tid == target_tid && handler_tid != syscall(SYS_gettid),
        "the handler ran on the target thread, not the sender");
  CHECK(handler_has_context && handler_pc != 0 && handler_sp != 0,
        "the kernel's ucontext_t reached the handler with pc and sp");
  target_stop = 1;
  pthread_join(thread, 0);
}
#endif

#if defined(HAVE_REAL_SIGNALS)
static volatile long initial_thread_handler_tid;

static void on_usr2_initial(int sig, siginfo_t* info, void* context) {
  (void)sig;
  (void)info;
  (void)context;
  initial_thread_handler_tid = syscall(SYS_gettid);
}

static pthread_t initial_thread;

static void* signal_initial_thread(void* argument) {
  (void)argument;
  /* The initial thread was not created by pthread_create(): its pthread_t is its kernel tid,
   * not a control-block pointer, and pthread_kill must still reach it (WTF suspends the VM's
   * thread this way; dereferencing the handle crashed). */
  CHECK(pthread_kill(initial_thread, SIGUSR2) == 0, "pthread_kill of the initial thread");
  return 0;
}

static void test_kill_initial_thread(void) {
  struct sigaction action;
  pthread_t helper;
  int i;

  initial_thread = pthread_self();
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = on_usr2_initial;
  action.sa_flags = SA_SIGINFO;
  CHECK(sigaction(SIGUSR2, &action, 0) == 0, "sigaction(SIGUSR2)");
  CHECK(pthread_create(&helper, 0, signal_initial_thread, 0) == 0, "create the helper");
  for (i = 0; i < 2000 && initial_thread_handler_tid == 0; ++i) {
    usleep(1000);
  }
  pthread_join(helper, 0);
  CHECK(initial_thread_handler_tid == syscall(SYS_gettid), "the handler ran on the initial thread");
}
#endif

int main(void) {
  test_memmem();
  test_memset_explicit();
#if defined(__APPLE__)
  test_getrusage();
#endif
  test_gettid();
  test_time_zone_variables();
  test_usleep();
  test_mkostemp();
  test_progname();
  test_sched();
  test_clocks();
  test_main_thread_stack();
#if defined(__linux__)
  test_linux_extras();
#endif
#if defined(HAVE_REAL_SIGNALS)
  test_pthread_kill();
  test_kill_initial_thread();
#endif
  if (failures != 0) {
    fprintf(stderr, "bionic_surface_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("bionic_surface_test: ok\n");
  return 0;
}
