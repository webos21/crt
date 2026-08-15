/* Focused stress regression for TODO.md's "Windows shell/process stress
 * hardening" item: many concurrently-live children sharing a single
 * inherited pipe the way GNU make's own jobserver protocol does (a fixed
 * pool of single-byte "tokens" written into a pipe up front; each worker
 * blocks reading one token, does its work, then writes the token back),
 * combined with close-on-exec fd filtering and a full waitpid(-1) drain --
 * all under real concurrent load, not the one-child-at-a-time coverage
 * windows_fd_snapshot_test.c already has.
 *
 * Portable on purpose (built and run on every host, not just Windows, via
 * the plain add_crt_test wiring in tests/CMakeLists.txt -- like
 * windows_fd_snapshot_test.c, whose own non-Windows branch already relies
 * on the same "/proc/self/exe" self-relaunch this file uses): the
 * jobserver-sharing bug class this guards against (fd-inheritance/
 * close-on-exec handling breaking under concurrent multi-child load) is
 * Windows-specific in its PAL implementation (CreateProcess-based
 * posix_spawn/__crt_shell_fork_exec, not a real fork()), but a portable
 * test gives Linux/macOS free regression coverage of the same jobserver
 * *protocol* on their own, independently-correct posix_spawn path, and
 * costs nothing extra to keep running there.
 *
 * Design: TOKEN_COUNT (6) tokens, CHILD_COUNT (40) workers -- deliberately
 * many more workers than tokens, so most children genuinely block on
 * read() waiting for another child to release a token, forcing real
 * concurrent contention on the shared inherited pipe fd rather than each
 * child getting an uncontended token immediately. Every worker also gets
 * a distinct, deliberately-not-inherited (FD_CLOEXEC) pair of "secret"
 * pipe fd numbers passed via argv and must observe EBADF using them --
 * checked by every single worker (not just one), since a close-on-exec
 * filtering bug could plausibly only affect fds above/below some count or
 * only the Nth child spawned, not every child uniformly. */
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum { TOKEN_COUNT = 6, CHILD_COUNT = 40 };

static int fail(const char* message) {
  fprintf(stderr, "process_stress_test: %s\n", message);
  return 1;
}

static int parse_fd_arg(const char* text) {
  int value = 0;

  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text - '0');
    ++text;
  }
  return *text == 0 ? value : -1;
}

static void format_fd_arg(int fd, char* buffer, size_t size) {
  char digits[16];
  size_t count = 0;
  size_t pos = 0;

  if (size == 0) {
    return;
  }
  do {
    digits[count++] = (char)('0' + (fd % 10));
    fd /= 10;
  } while (fd != 0 && count < sizeof(digits));
  while (count > 0 && pos + 1 < size) {
    buffer[pos++] = digits[--count];
  }
  buffer[pos] = 0;
}

/* Worker exit codes, distinct per failure so the parent's tally can
 * report which specific stage broke without extra IPC. */
enum {
  WORKER_OK = 0,
  WORKER_BAD_ARGS = 90,
  WORKER_TOKEN_READ_FAILED = 91,
  WORKER_TOKEN_WRITE_BACK_FAILED = 92,
  WORKER_CLOEXEC_READ_LEAKED = 93,
  WORKER_CLOEXEC_WRITE_LEAKED = 94
};

static int run_worker(int argc, char** argv) {
  int job_r;
  int job_w;
  int secret_r;
  int secret_w;
  char token = 0;
  char probe = 0;

  if (argc != 6) {
    return WORKER_BAD_ARGS;
  }
  job_r = parse_fd_arg(argv[2]);
  job_w = parse_fd_arg(argv[3]);
  secret_r = parse_fd_arg(argv[4]);
  secret_w = parse_fd_arg(argv[5]);
  if (job_r < 0 || job_w < 0 || secret_r < 0 || secret_w < 0) {
    return WORKER_BAD_ARGS;
  }

  /* Acquire: block until a token is available. With far more workers
   * than tokens, most workers genuinely wait here for a sibling to
   * release one -- real contention on the shared inherited pipe, not a
   * rubber-stamp read. */
  if (read(job_r, &token, 1) != 1) {
    return WORKER_TOKEN_READ_FAILED;
  }

  /* Close-on-exec check: the secret pipe's fds were marked FD_CLOEXEC
   * before this worker was spawned, so neither should have survived
   * into this process at all. Checked by every worker, not just one. */
  errno = 0;
  if (read(secret_r, &probe, 1) != -1 || errno != EBADF) {
    return WORKER_CLOEXEC_READ_LEAKED;
  }
  errno = 0;
  if (write(secret_w, "x", 1) != -1 || errno != EBADF) {
    return WORKER_CLOEXEC_WRITE_LEAKED;
  }

  /* Release: hand the token back immediately so other blocked workers
   * (and, at the end, the parent's own drain count) can make progress. */
  if (write(job_w, &token, 1) != 1) {
    return WORKER_TOKEN_WRITE_BACK_FAILED;
  }

  return WORKER_OK;
}

static int run_parent(void) {
  int job_pipe[2];
  int secret_pipe[2];
  pid_t pids[CHILD_COUNT];
  int exit_codes[CHILD_COUNT];
  int seen[CHILD_COUNT];
  int i;
  int reaped;
  int drained_tokens;
  char job_r_arg[16];
  char job_w_arg[16];
  char secret_r_arg[16];
  char secret_w_arg[16];

  if (pipe(job_pipe) != 0) {
    return fail("job pipe");
  }
  if (pipe(secret_pipe) != 0) {
    close(job_pipe[0]);
    close(job_pipe[1]);
    return fail("secret pipe");
  }
  if (fcntl(secret_pipe[0], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(secret_pipe[1], F_SETFD, FD_CLOEXEC) != 0) {
    close(job_pipe[0]);
    close(job_pipe[1]);
    close(secret_pipe[0]);
    close(secret_pipe[1]);
    return fail("secret pipe cloexec");
  }

  /* Seed the jobserver pipe with its token pool up front, exactly like
   * GNU make writing N '+' bytes into its own jobserver pipe before
   * spawning any recipe commands. */
  for (i = 0; i < TOKEN_COUNT; ++i) {
    if (write(job_pipe[1], "+", 1) != 1) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      close(secret_pipe[0]);
      close(secret_pipe[1]);
      return fail("token seed");
    }
  }

  format_fd_arg(job_pipe[0], job_r_arg, sizeof(job_r_arg));
  format_fd_arg(job_pipe[1], job_w_arg, sizeof(job_w_arg));
  format_fd_arg(secret_pipe[0], secret_r_arg, sizeof(secret_r_arg));
  format_fd_arg(secret_pipe[1], secret_w_arg, sizeof(secret_w_arg));

  /* Spawn every worker before reaping any of them, so CHILD_COUNT
   * processes are genuinely alive at once, all racing to read the same
   * TOKEN_COUNT-token pipe -- the "many live children" half of this
   * regression, not a spawn-one-wait-one loop. */
  for (i = 0; i < CHILD_COUNT; ++i) {
    char* child_argv[] = {
      "/proc/self/exe", "worker", job_r_arg, job_w_arg, secret_r_arg, secret_w_arg, 0
    };

    if (posix_spawn(&pids[i], "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      close(secret_pipe[0]);
      close(secret_pipe[1]);
      return fail("worker spawn");
    }
  }
  close(secret_pipe[0]);
  close(secret_pipe[1]);

  memset(seen, 0, sizeof(seen));
  reaped = 0;
  while (reaped < CHILD_COUNT) {
    int status = 0;
    pid_t got = waitpid(-1, &status, 0);
    int slot;

    if (got < 0) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      return fail("worker waitpid");
    }
    if (!WIFEXITED(status)) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      return fail("worker did not exit normally");
    }
    for (slot = 0; slot < CHILD_COUNT; ++slot) {
      if (pids[slot] == got) {
        if (seen[slot]) {
          close(job_pipe[0]);
          close(job_pipe[1]);
          return fail("worker pid reaped twice");
        }
        seen[slot] = 1;
        exit_codes[slot] = WEXITSTATUS(status);
        break;
      }
    }
    if (slot == CHILD_COUNT) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      return fail("worker unknown pid");
    }
    ++reaped;
  }

  /* Full drain: every pid accounted for, and one more waitpid(-1) must
   * now report ECHILD, exactly like windows_fd_snapshot_test.c's own
   * (much smaller-scale) multi-child drain check. */
  errno = 0;
  if (waitpid(-1, 0, WNOHANG) != -1 || errno != ECHILD) {
    close(job_pipe[0]);
    close(job_pipe[1]);
    return fail("drain not exhausted");
  }

  for (i = 0; i < CHILD_COUNT; ++i) {
    if (!seen[i]) {
      close(job_pipe[0]);
      close(job_pipe[1]);
      return fail("worker missing from reap set");
    }
    if (exit_codes[i] != WORKER_OK) {
      fprintf(stderr, "process_stress_test: worker %d exited %d\n", i, exit_codes[i]);
      close(job_pipe[0]);
      close(job_pipe[1]);
      return fail("worker reported failure");
    }
  }

  /* Every token handed out must have been handed back exactly once --
   * confirms CHILD_COUNT processes concurrently reading/writing one
   * inherited pipe fd never lost or duplicated a byte. */
  if (fcntl(job_pipe[0], F_SETFL, O_NONBLOCK) != 0) {
    close(job_pipe[0]);
    close(job_pipe[1]);
    return fail("job pipe nonblock");
  }
  drained_tokens = 0;
  for (;;) {
    char token = 0;
    ssize_t got = read(job_pipe[0], &token, 1);

    if (got == 1) {
      ++drained_tokens;
      continue;
    }
    if (got < 0 && errno == EAGAIN) {
      break;
    }
    close(job_pipe[0]);
    close(job_pipe[1]);
    return fail("job pipe drain read");
  }
  close(job_pipe[0]);
  close(job_pipe[1]);
  if (drained_tokens != TOKEN_COUNT) {
    fprintf(stderr, "process_stress_test: expected %d tokens back, got %d\n",
            TOKEN_COUNT, drained_tokens);
    return fail("token count mismatch");
  }

  puts("process_stress_test: ok");
  return 0;
}

int main(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "worker") == 0) {
    return run_worker(argc, argv);
  }
  return run_parent();
}
