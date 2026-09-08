#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* TODO.md's "Expand Windows shell smoke tests" item: real mksh
 * *interpreter*-level coverage (fd 3+ redirection, `{ }` grouped
 * commands, `&`/`wait` backgrounding, and autoconf-shaped
 * subshell/redirection idioms), as opposed to tests/shell_smoke_test.c,
 * which only exercises this project's own __crt_shell_spawn()/
 * posix_spawn() PAL primitives directly from C -- never actually
 * routing anything through mksh's own script parser. Same technique as
 * tests/mksh_subshell_status_test.c: spawn the real `/bin/sh` (aliased
 * to mksh in this project's own rootfs) via posix_spawn(), capture real
 * stdout, and check it against what a real script run would produce --
 * the thing under test is mksh's own interpreter behavior, not this
 * project's libc/PAL. Needs CRT_ROOTFS set to resolve /bin/sh through
 * the rootfs (tests/CMakeLists.txt's own ENVIRONMENT property on this
 * test does this, same as mksh_subshell_status_test.c). */

static int fail(const char* message) {
  fprintf(stderr, "mksh_shell_smoke_test: %s\n", message);
  return 1;
}

/* Runs `/bin/sh -c script`, capturing its stdout into out_buf and its
 * own real process exit code into *out_exit_status. Returns 0 on
 * success (spawn/wait succeeded, regardless of the script's own exit
 * code or output) or -1 on a harness-level failure. */
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

struct case_spec {
  const char* label;
  const char* script;
  const char* expect_stdout;
  int expect_exit;
};

static int run_case(const struct case_spec* c) {
  char buf[512];
  int exit_status = -1;

  if (run_shell(c->script, &exit_status, buf, sizeof(buf)) != 0) {
    fprintf(stderr, "mksh_shell_smoke_test: %s: failed to run (%s)\n", c->label, c->script);
    return -1;
  }
  if (c->expect_exit >= 0 && exit_status != c->expect_exit) {
    fprintf(stderr, "mksh_shell_smoke_test: %s: exit=%d, expected %d (script: %s)\n",
            c->label, exit_status, c->expect_exit, c->script);
    return -1;
  }
  if (c->expect_stdout != 0 && strcmp(buf, c->expect_stdout) != 0) {
    fprintf(stderr, "mksh_shell_smoke_test: %s: stdout=[%s], expected [%s] (script: %s)\n",
            c->label, buf, c->expect_stdout, c->script);
    return -1;
  }
  return 0;
}

int main(void) {
  static const struct case_spec cases[] = {
    /* --- fd 3+ redirection inside mksh --- */
    /* Write through fd 3 (opened via `exec`) to a real file, then read
     * the file back out through fd 1 (`cat`) so the C harness only ever
     * has to look at captured stdout, not open a temp file itself. */
    {
      "fd3 write via exec",
      "rm -f mksh_smoke_fd3.tmp; "
      "exec 3>mksh_smoke_fd3.tmp; echo hi3 >&3; exec 3>&-; "
      "cat mksh_smoke_fd3.tmp; rm -f mksh_smoke_fd3.tmp",
      "hi3\n", 0
    },
    /* Read through fd 3 (opened via `exec`) from a real file. */
    {
      "fd3 read via exec",
      "echo source-line >mksh_smoke_fd3in.tmp; "
      "exec 3<mksh_smoke_fd3in.tmp; read line <&3; exec 3<&-; "
      "rm -f mksh_smoke_fd3in.tmp; echo \"got:$line\"",
      "got:source-line\n", 0
    },
    /* The classic three-step fd swap idiom (autoconf/libtool use this
     * to temporarily swap stdout and stderr): 3>&1 1>&2 2>&3 3>&- .
     * With stderr itself redirected into the pipe this test already
     * captures via `2>&1` on the *outer* command, the swap means the
     * inner "tostdout" ends up on the process's real stderr (which
     * isn't captured) while "tostderr" ends up on stdout (which is) --
     * so only "tostderr" should appear in the captured output. */
    {
      "fd3 stdout/stderr swap",
      "{ echo tostdout; echo tostderr >&2; } 3>&1 1>&2 2>&3 3>&-",
      "tostderr\n", 0
    },
    /* fd 3 explicitly passed through a subshell's own redirection,
     * matching the earlier "Windows mksh subshell status quirk" fix's
     * own repro shape ((exit N) 2>/dev/null) but for fd 3 specifically. */
    {
      "fd3 through subshell",
      "(echo sub3 >&3) 3>&1",
      "sub3\n", 0
    },

    /* --- grouped commands { } --- */
    /* { } runs in the CURRENT shell -- a variable set inside must stay
     * visible after the group closes, unlike a (...) subshell. */
    {
      "brace group shares shell state",
      "x=1; { x=2; }; echo \"x=$x\"",
      "x=2\n", 0
    },
    {
      "subshell does not share state (contrast case)",
      "x=1; (x=2); echo \"x=$x\"",
      "x=1\n", 0
    },
    /* A brace group's own exit status must propagate through a `;`-list
     * the same way a plain command's does -- this is the same class of
     * bug the subshell-status fix addressed for TPAREN; confirm TLIST
     * doesn't lose it for a brace group either. */
    {
      "brace group status in ;-list",
      "{ false; }; echo \"status=$?\"",
      "status=1\n", 0
    },
    /* cd inside a brace group affects the current shell's cwd (POSIX);
     * inside a subshell it must not leak out. */
    {
      "brace group cd leaks, subshell cd does not",
      "start=$(pwd); { cd /; }; after_brace=$(pwd); cd \"$start\"; "
      "(cd /); after_sub=$(pwd); "
      "if [ \"$after_brace\" = \"/\" ] && [ \"$after_sub\" = \"$start\" ]; "
      "then echo ok; else echo \"fail brace=$after_brace sub=$after_sub start=$start\"; fi",
      "ok\n", 0
    },

    /* --- background commands (non-interactive semantics) --- */
    /* `cmd &` must not block the script; the next statement should run
     * immediately, and `wait` (with no args) should then block until
     * the background job finishes. */
    {
      "background then wait",
      "{ sleep 1 2>/dev/null || :; echo bg-done; } & "
      "echo started; wait; echo after-wait",
      "started\nbg-done\nafter-wait\n", 0
    },
    /* $! captures the backgrounded job's pid, and `wait $!` retrieves
     * its real exit status -- not always 0, confirming actual job
     * tracking rather than just "wait returned". */
    {
      "wait $! retrieves real status",
      "(exit 4) & wait $!; echo \"status=$?\"",
      "status=4\n", 0
    },
    /* Multiple background jobs, waited on in launch order via separate
     * $! captures -- a shape closer to a real parallel build's own
     * job-tracking than a single lone background job. */
    {
      "multiple background jobs",
      "(exit 1) & p1=$!; (exit 2) & p2=$!; "
      "wait $p1; s1=$?; wait $p2; s2=$?; "
      "echo \"s1=$s1 s2=$s2\"",
      "s1=1 s2=2\n", 0
    },

    /* --- configure-script-shaped subshell/redirection idioms --- */
    /* The exact scenario exec.c's own comment documents as historically
     * broken: `(exit $ac_status)` as the tail statement of a `{ ...; }`
     * group, autoconf's own idiom for probing optional tools. */
    {
      "(exit $ac_status) as brace-group tail",
      "ac_status=3; { true; (exit $ac_status); }; echo \"status=$?\"",
      "status=3\n", 0
    },
    /* A subshell used directly as an `if` condition -- common in
     * configure scripts probing for optional compiler/tool behavior. */
    {
      "subshell as if-condition",
      "if (exit 0); then echo yes; else echo no; fi",
      "yes\n", 0
    },
    {
      "failing subshell as if-condition",
      "if (exit 1); then echo yes; else echo no; fi",
      "no\n", 0
    },
    /* configure's own `ac_fn_c_try_compile`-shaped pattern: a subshell
     * probe whose result feeds a following `;`-separated check, with
     * output suppressed via redirection -- the exact real-world shape
     * (RANLIB || true, redirected) the subshell-status bug was found
     * from, generalized beyond ranlib specifically. */
    {
      "configure-shaped probe-then-check",
      "(exit 1) >/dev/null 2>&1; "
      "if test $? -eq 0; then echo probe-ok; else echo probe-failed; fi",
      "probe-failed\n", 0
    },
  };
  size_t i;
  int failures = 0;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (run_case(&cases[i]) != 0) {
      ++failures;
    }
  }

  if (failures != 0) {
    fprintf(stderr, "mksh_shell_smoke_test: %d case(s) failed\n", failures);
    return fail("cases failed");
  }

  puts("mksh_shell_smoke_test: ok");
  return 0;
}
