#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "glob_test: %s\n", message);
  return 1;
}

static int write_file(const char* path) {
  FILE* f = fopen(path, "w");

  if (f == 0) {
    return -1;
  }
  fputs("x", f);
  return fclose(f);
}

static void cleanup(void) {
  remove("glob_test_dir/sub/inner.txt");
  remove("glob_test_dir/sub");
  remove("glob_test_dir/alpha.txt");
  remove("glob_test_dir/beta.txt");
  remove("glob_test_dir/gamma.log");
  remove("glob_test_dir/.hidden.txt");
  remove("glob_test_dir");
}

static int errfunc_calls;

static int record_errfunc(const char* epath, int eerrno) {
  (void)epath;
  (void)eerrno;
  ++errfunc_calls;
  return 0;
}

int main(void) {
  glob_t g;
  int result;

  cleanup(); /* best-effort, in case a previous failed run left state behind */

  if (mkdir("glob_test_dir", 0755) != 0 || mkdir("glob_test_dir/sub", 0755) != 0) {
    return fail("mkdir setup");
  }
  if (write_file("glob_test_dir/alpha.txt") != 0 || write_file("glob_test_dir/beta.txt") != 0 ||
      write_file("glob_test_dir/gamma.log") != 0 ||
      write_file("glob_test_dir/.hidden.txt") != 0 ||
      write_file("glob_test_dir/sub/inner.txt") != 0) {
    cleanup();
    return fail("file setup");
  }

  /* Wildcard match, sorted by default, hidden files excluded unless the
   * pattern itself starts with '.'. */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/*.txt", 0, 0, &g);
  if (result != 0 || g.gl_pathc != 2 || strcmp(g.gl_pathv[0], "glob_test_dir/alpha.txt") != 0 ||
      strcmp(g.gl_pathv[1], "glob_test_dir/beta.txt") != 0 || g.gl_pathv[2] != 0) {
    globfree(&g);
    cleanup();
    return fail("wildcard *.txt");
  }
  globfree(&g);
  if (g.gl_pathv != 0 || g.gl_pathc != 0) {
    cleanup();
    return fail("globfree reset");
  }

  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/*", 0, 0, &g);
  if (result != 0 || g.gl_pathc != 4 || strcmp(g.gl_pathv[0], "glob_test_dir/alpha.txt") != 0 ||
      strcmp(g.gl_pathv[1], "glob_test_dir/beta.txt") != 0 ||
      strcmp(g.gl_pathv[2], "glob_test_dir/gamma.log") != 0 ||
      strcmp(g.gl_pathv[3], "glob_test_dir/sub") != 0) {
    globfree(&g);
    cleanup();
    return fail("wildcard * (hidden file excluded, dot files excluded)");
  }
  globfree(&g);

  /* Leading-dot pattern only matches dotfiles, never "." or "..". */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/.*", 0, 0, &g);
  if (result != 0 || g.gl_pathc != 1 || strcmp(g.gl_pathv[0], "glob_test_dir/.hidden.txt") != 0) {
    globfree(&g);
    cleanup();
    return fail("dotfile pattern");
  }
  globfree(&g);

  /* Multi-component pattern (wildcard in a non-final segment too). */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/s*/*.txt", 0, 0, &g);
  if (result != 0 || g.gl_pathc != 1 ||
      strcmp(g.gl_pathv[0], "glob_test_dir/sub/inner.txt") != 0) {
    globfree(&g);
    cleanup();
    return fail("multi-component wildcard");
  }
  globfree(&g);

  /* GLOB_MARK appends '/' to directory matches only. */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/*", GLOB_MARK, 0, &g);
  if (result != 0 || g.gl_pathc != 4 ||
      strcmp(g.gl_pathv[0], "glob_test_dir/alpha.txt") != 0 ||
      strcmp(g.gl_pathv[3], "glob_test_dir/sub/") != 0) {
    globfree(&g);
    cleanup();
    return fail("GLOB_MARK");
  }
  globfree(&g);

  /* No match, no GLOB_NOCHECK: GLOB_NOMATCH, empty result set. */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/nomatch*.xyz", 0, 0, &g);
  if (result != GLOB_NOMATCH || g.gl_pathc != 0) {
    globfree(&g);
    cleanup();
    return fail("GLOB_NOMATCH");
  }
  globfree(&g);

  /* No match, with GLOB_NOCHECK: the pattern itself comes back verbatim. */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/nomatch*.xyz", GLOB_NOCHECK, 0, &g);
  if (result != 0 || g.gl_pathc != 1 ||
      strcmp(g.gl_pathv[0], "glob_test_dir/nomatch*.xyz") != 0) {
    globfree(&g);
    cleanup();
    return fail("GLOB_NOCHECK");
  }
  globfree(&g);

  /* GLOB_APPEND accumulates across calls into the same glob_t. */
  memset(&g, 0, sizeof(g));
  if (glob("glob_test_dir/*.txt", 0, 0, &g) != 0 || g.gl_pathc != 2) {
    globfree(&g);
    cleanup();
    return fail("GLOB_APPEND first call");
  }
  if (glob("glob_test_dir/*.log", GLOB_APPEND, 0, &g) != 0 || g.gl_pathc != 3 ||
      strcmp(g.gl_pathv[0], "glob_test_dir/alpha.txt") != 0 ||
      strcmp(g.gl_pathv[1], "glob_test_dir/beta.txt") != 0 ||
      strcmp(g.gl_pathv[2], "glob_test_dir/gamma.log") != 0 || g.gl_pathv[3] != 0) {
    globfree(&g);
    cleanup();
    return fail("GLOB_APPEND second call");
  }
  globfree(&g);

  /* GLOB_DOOFFS reserves leading NULL slots in gl_pathv. */
  memset(&g, 0, sizeof(g));
  g.gl_offs = 2;
  result = glob("glob_test_dir/*.txt", GLOB_DOOFFS, 0, &g);
  if (result != 0 || g.gl_pathc != 2 || g.gl_pathv[0] != 0 || g.gl_pathv[1] != 0 ||
      strcmp(g.gl_pathv[2], "glob_test_dir/alpha.txt") != 0) {
    globfree(&g);
    cleanup();
    return fail("GLOB_DOOFFS");
  }
  globfree(&g);

  /* A missing directory is silently ignored (0 matches) unless GLOB_ERR
   * is set or errfunc reports it should abort. */
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/doesnotexist/*.txt", 0, 0, &g);
  if (result != GLOB_NOMATCH || g.gl_pathc != 0) {
    globfree(&g);
    cleanup();
    return fail("missing directory silently ignored");
  }
  globfree(&g);

  errfunc_calls = 0;
  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/doesnotexist/*.txt", 0, record_errfunc, &g);
  if (result != GLOB_NOMATCH || errfunc_calls != 1) {
    globfree(&g);
    cleanup();
    return fail("errfunc invoked, no abort without GLOB_ERR");
  }
  globfree(&g);

  memset(&g, 0, sizeof(g));
  result = glob("glob_test_dir/doesnotexist/*.txt", GLOB_ERR, 0, &g);
  if (result != GLOB_ABORTED) {
    globfree(&g);
    cleanup();
    return fail("GLOB_ERR aborts on missing directory");
  }
  globfree(&g);

  /* NULL argument safety. */
  if (glob(0, 0, 0, &g) != GLOB_NOSYS || glob("x", 0, 0, 0) != GLOB_NOSYS) {
    cleanup();
    return fail("NULL arguments");
  }

  cleanup();
  printf("glob_test: ok\n");
  return 0;
}
