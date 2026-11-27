/* What a signal does to a process with no handler for it, seen from the parent with waitpid(), for
 * raise(), kill(getpid()), pthread_kill(pthread_self()) and abort(). As in Bionic, raise() is a real
 * signal to the calling thread: the default action ends the process by that signal. Every case runs
 * in a forked child. */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SURVIVED 42

enum how { BY_RAISE, BY_SELF_KILL, BY_PTHREAD_KILL, BY_RAISE_AFTER_RESTORE, HOW_COUNT };
static const char* const how_name[HOW_COUNT] = {"raise", "self-kill", "pthread_kill", "raise after restore"};

static volatile sig_atomic_t handled;
static volatile sig_atomic_t handled_signal;
static int failures;
static const char* self_path;

static void count_handler(int sig) {
  handled_signal = sig;
  ++handled;
}

static void exit_seven_handler(int sig) {
  (void)sig;
  _exit(7);
}

static void atexit_marker(void) {
  _exit(99);
}

static void fail(const char* what, int sig, int status) {
  fprintf(stderr, "signal_default_action_test: %s (signal %d, wait status 0x%x)\n", what, sig, status);
  ++failures;
}

/* The test's own parent may ignore or block signals, and both are inherited. */
static void make_default(int sig) {
  sigset_t set;

  if (sig != SIGKILL && sig != SIGSTOP && signal(sig, SIG_DFL) == SIG_ERR) _exit(120);
  sigemptyset(&set);
  sigaddset(&set, sig);
  if (sigprocmask(SIG_UNBLOCK, &set, 0) != 0) _exit(121);
}

static int send(enum how how, int sig) {
  switch (how) {
    case BY_SELF_KILL:
      return kill(getpid(), sig);
    case BY_PTHREAD_KILL:
      return pthread_kill(pthread_self(), sig);
    default:
      return raise(sig);
  }
}

static int run_child(void (*body)(int, int), int a, int b) {
  int status = -1;
  pid_t pid = fork();

  if (pid == 0) {
    body(a, b);
    _exit(SURVIVED);
  }
  if (pid < 0 || waitpid(pid, &status, 0) != pid) {
    fail("fork/waitpid", b, errno);
  }
  return status;
}

static void default_body(int how, int sig) {
  make_default(sig);
  if (atexit(atexit_marker) != 0) _exit(122);
  if (how == BY_RAISE_AFTER_RESTORE) {
    struct sigaction action;
    struct sigaction old;

    memset(&action, 0, sizeof(action));
    action.sa_handler = count_handler;
    if (sigaction(sig, &action, &old) != 0 || raise(sig) != 0 || handled != 1 ||
        sigaction(sig, &old, 0) != 0) {
      _exit(123);
    }
  }
  (void)send((enum how)how, sig);
}

static void expect_killed(const char* what, int status, int sig) {
  if (!WIFSIGNALED(status) || WTERMSIG(status) != sig || WIFEXITED(status) || WIFSTOPPED(status)) {
    fail(what, sig, status);
  }
}

static void expect_exit(const char* what, int status, int code, int sig) {
  if (!WIFEXITED(status) || WEXITSTATUS(status) != code) {
    fail(what, sig, status);
  }
}

static void test_terminating(void) {
  /* One by raise() each. SIGSTKFLT and SIGPWR do not exist on macOS and are not listed. */
  static const int all[] = {SIGHUP,  SIGINT,  SIGQUIT, SIGILL,    SIGTRAP, SIGABRT, SIGBUS,
                            SIGFPE,  SIGKILL, SIGUSR1, SIGSEGV,   SIGUSR2, SIGPIPE, SIGALRM,
                            SIGTERM, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGSYS,
#if !defined(CRT_TARGET_OS_MACOS)
                            SIGIO,
#endif
  };
  /* Every way of sending, for the common ones. */
  static const int common[] = {SIGHUP, SIGINT, SIGQUIT, SIGUSR1, SIGTERM};
  size_t i;
  int how;

  for (i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    expect_killed("raise: default action", run_child(default_body, BY_RAISE, all[i]), all[i]);
  }
  for (how = BY_SELF_KILL; how < HOW_COUNT; ++how) {
    for (i = 0; i < sizeof(common) / sizeof(common[0]); ++i) {
      expect_killed(how_name[how], run_child(default_body, how, common[i]), common[i]);
    }
  }
  expect_killed("self-kill SIGKILL", run_child(default_body, BY_SELF_KILL, SIGKILL), SIGKILL);
}

static void ignored_by_default_body(int how, int sig) {
  make_default(sig);
  if (send((enum how)how, sig) != 0) _exit(124);
}

static void ignored_by_request_body(int how, int sig) {
  if (signal(sig, SIG_IGN) == SIG_ERR || send((enum how)how, sig) != 0) _exit(124);
}

static void test_ignored(void) {
  static const int by_default[] = {SIGCHLD, SIGCONT, SIGURG, SIGWINCH,
#if defined(CRT_TARGET_OS_MACOS)
                                   SIGIO, /* Darwin discards it by default; Linux terminates */
#endif
  };
  static const int by_request[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM};
  size_t i;
  int how;

  for (how = BY_RAISE; how <= BY_PTHREAD_KILL; ++how) {
    for (i = 0; i < sizeof(by_default) / sizeof(by_default[0]); ++i) {
      expect_exit("default action is ignore", run_child(ignored_by_default_body, how, by_default[i]),
                  SURVIVED, by_default[i]);
    }
    for (i = 0; i < sizeof(by_request) / sizeof(by_request[0]); ++i) {
      expect_exit("SIG_IGN", run_child(ignored_by_request_body, how, by_request[i]), SURVIVED,
                  by_request[i]);
    }
  }
}

static void blocked_body(int how, int sig) {
  sigset_t set;

  make_default(sig);
  sigemptyset(&set);
  sigaddset(&set, sig);
  if (signal(sig, count_handler) == SIG_ERR || sigprocmask(SIG_BLOCK, &set, 0) != 0) _exit(125);
  if (send((enum how)how, sig) != 0 || handled != 0) _exit(126);
  /* Pending, not lost: unblocking delivers it. */
  if (sigprocmask(SIG_UNBLOCK, &set, 0) != 0 || handled != 1 || handled_signal != sig) _exit(127);
  /* The same with the default action: the process survives only while the signal is blocked. */
  if (sigprocmask(SIG_BLOCK, &set, 0) != 0 || signal(sig, SIG_DFL) == SIG_ERR) _exit(118);
  if (send((enum how)how, sig) != 0) _exit(119);
  (void)sigprocmask(SIG_UNBLOCK, &set, 0);
}

static void test_blocked(void) {
  int how;

  for (how = BY_RAISE; how <= BY_PTHREAD_KILL; ++how) {
    expect_killed("blocked, then unblocked", run_child(blocked_body, how, SIGTERM), SIGTERM);
  }
}

static pthread_t raising_thread;
static volatile sig_atomic_t handled_on_raising_thread;

static void thread_check_handler(int sig) {
  (void)sig;
  handled_on_raising_thread = pthread_equal(pthread_self(), raising_thread);
}

static void* raise_on_thread(void* argument) {
  int sig = *(int*)argument;

  raising_thread = pthread_self();
  /* raise() is for the calling thread, and its handler has run when it returns. */
  if (signal(sig, thread_check_handler) == SIG_ERR || raise(sig) != 0 || !handled_on_raising_thread) _exit(116);
  if (signal(sig, SIG_DFL) == SIG_ERR) _exit(117);
  (void)raise(sig);
  return 0;
}

/* From a thread other than the first, the default action still ends the whole process. It runs in a
 * fresh exec of this test: pthread_create() in a merely forked child crashes on macOS (a fork()
 * limitation there, independent of signals). */
#define THREAD_CASE "--raise-on-second-thread"

static int thread_case_main(void) {
  pthread_t thread;
  int sig = SIGTERM;

  make_default(sig);
  if (pthread_create(&thread, 0, raise_on_thread, &sig) != 0 || pthread_join(thread, 0) != 0) return 115;
  return SURVIVED;
}

static void exec_thread_case_body(int unused, int sig) {
  char flag[] = THREAD_CASE;
  char* arguments[3];

  (void)unused;
  (void)sig;
  arguments[0] = (char*)self_path;
  arguments[1] = flag;
  arguments[2] = 0;
  execv(self_path, arguments);
  _exit(114);
}

enum abort_case { ABORT_DEFAULT, ABORT_IGNORED, ABORT_BLOCKED, ABORT_HANDLER_RETURNS, ABORT_HANDLER_EXITS };

static void abort_body(int which, int sig) {
  sigset_t set;

  make_default(sig);
  if (atexit(atexit_marker) != 0) _exit(122);
  if (which == ABORT_IGNORED && signal(sig, SIG_IGN) == SIG_ERR) _exit(123);
  if (which == ABORT_BLOCKED) {
    sigemptyset(&set);
    sigaddset(&set, sig);
    if (signal(sig, count_handler) == SIG_ERR || sigprocmask(SIG_BLOCK, &set, 0) != 0) _exit(123);
  }
  if (which == ABORT_HANDLER_RETURNS && signal(sig, count_handler) == SIG_ERR) _exit(123);
  if (which == ABORT_HANDLER_EXITS && signal(sig, exit_seven_handler) == SIG_ERR) _exit(123);
  abort();
}

static int report_fd = -1;

static void report_handler(int sig) {
  char byte = (char)sig;

  handled = 1;
  (void)write(report_fd, &byte, 1);
}

static void test_abort(void) {
  int fds[2];
  char byte = 0;
  int status;
  pid_t pid;

  expect_killed("abort", run_child(abort_body, ABORT_DEFAULT, SIGABRT), SIGABRT);
  expect_killed("abort, SIGABRT ignored", run_child(abort_body, ABORT_IGNORED, SIGABRT), SIGABRT);
  expect_killed("abort, SIGABRT blocked", run_child(abort_body, ABORT_BLOCKED, SIGABRT), SIGABRT);
  expect_killed("abort, handler returns", run_child(abort_body, ABORT_HANDLER_RETURNS, SIGABRT), SIGABRT);
  expect_exit("abort, handler exits", run_child(abort_body, ABORT_HANDLER_EXITS, SIGABRT), 7, SIGABRT);

  /* abort() runs the handler before it ends the process, even when SIGABRT was blocked. */
  if (pipe(fds) != 0) {
    fail("pipe", SIGABRT, errno);
    return;
  }
  pid = fork();
  if (pid == 0) {
    sigset_t set;

    close(fds[0]);
    report_fd = fds[1];
    sigemptyset(&set);
    sigaddset(&set, SIGABRT);
    if (signal(SIGABRT, report_handler) == SIG_ERR || sigprocmask(SIG_BLOCK, &set, 0) != 0) _exit(123);
    abort();
  }
  close(fds[1]);
  if (read(fds[0], &byte, 1) != 1 || byte != SIGABRT) fail("abort did not run the handler", SIGABRT, byte);
  close(fds[0]);
  status = -1;
  if (waitpid(pid, &status, 0) != pid) fail("waitpid", SIGABRT, errno);
  expect_killed("abort after handler", status, SIGABRT);
}

#if !defined(CRT_TARGET_OS_WINDOWS)
/* Signals from another process, with numbers that differ between Bionic and Darwin: the child's
 * handler and the parent's wait status must both speak Bionic. */
static void test_parent_kill(void) {
  static const int sigs[] = {SIGBUS, SIGUSR1, SIGUSR2, SIGSYS, SIGURG};
  size_t i;

  for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) {
    int caught;

    for (caught = 0; caught <= 1; ++caught) {
      int ready[2];
      int report[2];
      char byte = 0;
      int status = -1;
      pid_t pid;

      if (!caught && sigs[i] == SIGURG) continue; /* ignored by default */
      if (pipe(ready) != 0 || pipe(report) != 0) {
        fail("pipe", sigs[i], errno);
        return;
      }
      pid = fork();
      if (pid == 0) {
        close(ready[0]);
        close(report[0]);
        report_fd = report[1];
        make_default(sigs[i]);
        if (caught && signal(sigs[i], report_handler) == SIG_ERR) _exit(123);
        (void)write(ready[1], "r", 1);
        while (!handled) {
          usleep(1000);
        }
        _exit(SURVIVED);
      }
      close(ready[1]);
      close(report[1]);
      if (read(ready[0], &byte, 1) != 1) fail("child not ready", sigs[i], errno);
      if (kill(pid, sigs[i]) != 0) fail("kill(child)", sigs[i], errno);
      if (caught && (read(report[0], &byte, 1) != 1 || byte != sigs[i])) {
        fail("handler saw another signal", sigs[i], byte);
      }
      if (waitpid(pid, &status, 0) != pid) fail("waitpid", sigs[i], errno);
      if (caught) {
        expect_exit("caught parent kill", status, SURVIVED, sigs[i]);
      } else {
        expect_killed("parent kill", status, sigs[i]);
      }
      close(ready[0]);
      close(report[0]);
    }
  }
}

/* SIGSTOP stops; the stop is reported to a WUNTRACED wait and SIGCONT resumes. (SIGTSTP, SIGTTIN and
 * SIGTTOU are not tried: the kernel discards them in an orphaned process group, which a test runner
 * may be.) */
static void test_stop(void) {
  int how;

  for (how = BY_RAISE; how <= BY_PTHREAD_KILL; ++how) {
    int status = -1;
    pid_t pid = fork();

    if (pid == 0) {
      (void)send((enum how)how, SIGSTOP);
      _exit(SURVIVED);
    }
    if (waitpid(pid, &status, WUNTRACED) != pid || !WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP ||
        WIFSIGNALED(status) || WIFEXITED(status)) {
      fail("stop not reported", SIGSTOP, status);
      (void)kill(pid, SIGKILL);
      (void)waitpid(pid, &status, 0);
      continue;
    }
    if (kill(pid, SIGCONT) != 0 || waitpid(pid, &status, 0) != pid) fail("continue", SIGCONT, errno);
    expect_exit("resumed after SIGCONT", status, SURVIVED, SIGSTOP);
  }
}
#else
static void windows_stop_body(int how, int sig) {
  (void)how;
  make_default(sig);
  errno = 0;
  if (raise(sig) != -1 || errno != ENOTSUP) _exit(124);
}

/* No job control: the stop signals' default action is refused rather than faked. */
static void test_stop(void) {
  static const int sigs[] = {SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU};
  size_t i;

  for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); ++i) {
    expect_exit("stop is ENOTSUP", run_child(windows_stop_body, 0, sigs[i]), SURVIVED, sigs[i]);
  }
}
#endif

static void test_arguments(void) {
  struct sigaction action;

  if (raise(0) != 0) fail("raise(0)", 0, errno);
  errno = 0;
  if (raise(-1) != -1 || errno != EINVAL) fail("raise(-1)", -1, errno);
  errno = 0;
  if (raise(1000) != -1 || errno != EINVAL) fail("raise(1000)", 1000, errno);
  memset(&action, 0, sizeof(action));
  action.sa_handler = count_handler;
  errno = 0;
  if (sigaction(SIGKILL, &action, 0) != -1 || errno != EINVAL) fail("catch SIGKILL", SIGKILL, errno);
  errno = 0;
  if (signal(SIGSTOP, SIG_IGN) != SIG_ERR || errno != EINVAL) fail("ignore SIGSTOP", SIGSTOP, errno);
}

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], THREAD_CASE) == 0) {
    return thread_case_main();
  }
  self_path = argv[0];
  test_arguments();
  test_terminating();
  test_ignored();
  test_blocked();
  expect_killed("raise on a second thread", run_child(exec_thread_case_body, 0, SIGTERM), SIGTERM);
  test_abort();
#if !defined(CRT_TARGET_OS_WINDOWS)
  test_parent_kill();
#endif
  test_stop();
  if (failures != 0) {
    fprintf(stderr, "signal_default_action_test: %d failure(s)\n", failures);
    return 1;
  }
  puts("signal_default_action_test: ok");
  return 0;
}
