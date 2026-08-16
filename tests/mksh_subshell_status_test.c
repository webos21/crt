#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Regression test for the "Windows mksh subshell status quirk" TODO.md
 * and docs/sysroot_ports.md documented (found via zlib's own
 * `-@ ($(RANLIB) $@ || true) >/dev/null 2>&1` line, worked around at the
 * recipe level with `RANLIB=true` but never root-caused). Root-caused
 * this session: a `(subshell); next_command` sequence -- a TLIST node
 * whose non-final item is a TPAREN -- lost the subshell's real exit
 * status on Windows specifically. `shell/mksh/src/exec.c`'s execute()
 * has an early-return path (only ever reached by a TPAREN node on
 * Windows -- MKSH_CRT_SHELL_CHILD_SPEC, which adds TPAREN to this path's
 * trigger conditions so a subshell always gets real process isolation,
 * is Windows-only, see that file's own comment) that called exchild()
 * and returned its result directly, without setting the shared `exstat`
 * global the way every other path through execute() does at its
 * "Break:" tail. That's invisible whenever the caller captures and uses
 * execute()'s own return value (a standalone `(cmd)` on its own line,
 * where main.c's shell() loop assigns `exstat = execute(...)` itself),
 * but TLIST's own loop (`case TLIST:` in exec.c) discards the return
 * value of every list item except the last one -- so `(false); echo $?`
 * printed 0 instead of 1, while the exact same command without the
 * subshell (`false; echo $?`) printed the correct 1, because TCOM's own
 * dispatch already reaches the "Break:" tail regardless of what any
 * caller does with the return value. Fixed by making that early-return
 * path set exstat too, mirroring the "Break:" tail exactly. This test
 * spawns the real `/bin/sh` (aliased to mksh in this project's own
 * rootfs) via posix_spawn(), capturing real stdout/exit-code, rather
 * than testing anything at the C level directly -- the bug is entirely
 * about mksh's own interpreter behavior, not this project's libc/PAL.
 * Needs CRT_ROOTFS set (tests/CMakeLists.txt's own ENVIRONMENT property
 * on this test does this) so /bin/sh resolves through the rootfs. */

static int fail(const char* message) {
  fprintf(stderr, "mksh_subshell_status_test: %s\n", message);
  return 1;
}

/* Runs `/bin/sh -c script`, capturing its real stdout. Returns 0 and
 * sets *out_exit_status to the shell's own real process exit code on
 * success (spawn/wait succeeded); returns -1 only on a harness-level
 * failure (couldn't even spawn/pipe/wait), never because the script's
 * own exit code was nonzero -- callers decide what a "successful" run
 * looks like for their own script. */
static int run_shell(const char* script, int* out_exit_status, char* out_buf, size_t out_buf_size) {
  int pipefd[2];
  posix_spawn_file_actions_t actions;
  pid_t pid;
  ssize_t total = 0;
  int status = 0;
  char* argv[4];

  argv[0] = "/bin/sh";
  argv[1] = "-c";
  argv[2] = (char*)script;
  argv[3] = 0;

  if (pipe(pipefd) != 0) {
    return -1;
  }
  if (posix_spawn_file_actions_init(&actions) != 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }
  if (posix_spawn_file_actions_adddup2(&actions, pipefd[1], 1) != 0 ||
      posix_spawn_file_actions_addclose(&actions, pipefd[0]) != 0 ||
      posix_spawn_file_actions_addclose(&actions, pipefd[1]) != 0) {
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }
  if (posix_spawn(&pid, "/bin/sh", &actions, 0, argv, environ) != 0) {
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }
  posix_spawn_file_actions_destroy(&actions);
  close(pipefd[1]);

  memset(out_buf, 0, out_buf_size);
  while (total < (ssize_t)out_buf_size - 1) {
    ssize_t got = read(pipefd[0], out_buf + total, out_buf_size - 1 - (size_t)total);

    if (got <= 0) {
      break;
    }
    total += got;
  }
  close(pipefd[0]);

  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
    return -1;
  }
  *out_exit_status = WEXITSTATUS(status);
  return 0;
}

/* Runs a script that always ends with `echo "status=$?"`, parses that
 * marker out of stdout, and checks it against `expected` -- the script's
 * own exit code (from the trailing echo, always 0) is irrelevant here. */
static int check_echoed_status(const char* script, int expected) {
  char buf[256];
  int exit_status = -1;
  const char* marker;

  if (run_shell(script, &exit_status, buf, sizeof(buf)) != 0) {
    fprintf(stderr, "mksh_subshell_status_test: (%s) failed to run\n", script);
    return -1;
  }
  marker = strstr(buf, "status=");
  if (marker == 0) {
    fprintf(stderr, "mksh_subshell_status_test: (%s) no status= marker in output [%s]\n",
            script, buf);
    return -1;
  }
  if (atoi(marker + 7) != expected) {
    fprintf(stderr, "mksh_subshell_status_test: (%s) got status=%d, expected %d\n",
            script, atoi(marker + 7), expected);
    return -1;
  }
  return 0;
}

/* Runs a script whose own real process exit code (not anything echoed)
 * is the thing under test -- for a subshell as the final list item,
 * where main.c's own shell() loop captures execute()'s return value
 * directly (this path was never affected by the bug, but is worth
 * locking down permanently alongside the fix). */
static int check_own_exit_status(const char* script, int expected) {
  char buf[256];
  int exit_status = -1;

  if (run_shell(script, &exit_status, buf, sizeof(buf)) != 0) {
    fprintf(stderr, "mksh_subshell_status_test: (%s) failed to run\n", script);
    return -1;
  }
  if (exit_status != expected) {
    fprintf(stderr, "mksh_subshell_status_test: (%s) got exit=%d, expected %d\n",
            script, exit_status, expected);
    return -1;
  }
  return 0;
}

int main(void) {
  /* The exact original repro. */
  if (check_echoed_status("(false); echo \"status=$?\"", 1) != 0) {
    return fail("case: (false); echo $?");
  }
  /* A specific exit code, not just true/false's 0/1. */
  if (check_echoed_status("(exit 5); echo \"status=$?\"", 5) != 0) {
    return fail("case: (exit 5); echo $?");
  }
  /* A subshell with its own redirection, matching the real-world shape
   * (zlib's ranlib line) this was found from. */
  if (check_echoed_status("(exit 7) 2>/dev/null; echo \"status=$?\"", 7) != 0) {
    return fail("case: (exit 7) 2>/dev/null; echo $?");
  }
  /* The exact TODO.md-documented pattern. */
  if (check_echoed_status("(false || true) >/dev/null 2>&1; echo \"status=$?\"", 0) != 0) {
    return fail("case: (false || true) >/dev/null 2>&1; echo $?");
  }
  /* A successful subshell must still report 0 (not just "any wrong
   * value is caught" -- confirm the common case stays right too). */
  if (check_echoed_status("(true); echo \"status=$?\"", 0) != 0) {
    return fail("case: (true); echo $?");
  }
  /* Regression guards: these never had the bug (TCOM's own dispatch
   * already reaches execute()'s "Break:" tail), but are worth locking
   * down permanently alongside the fix, in the same test, so a future
   * change to this exact area can't quietly break them instead. */
  if (check_echoed_status("false; echo \"status=$?\"", 1) != 0) {
    return fail("case: false; echo $?");
  }
  if (check_echoed_status("x=$(false); echo \"status=$?\"", 1) != 0) {
    return fail("case: x=$(false); echo $?");
  }
  /* A subshell as the FINAL list item was never affected either. */
  if (check_own_exit_status("echo before >/dev/null; (exit 9)", 9) != 0) {
    return fail("case: echo before; (exit 9)");
  }

  puts("mksh_subshell_status_test: ok");
  return 0;
}
