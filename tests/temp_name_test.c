#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "temp_name_test: %s\n", message);
  return 1;
}

static int has_no_template_x(const char* path) {
  return strstr(path, "XXXXXX") == 0 && strstr(path, "XXXXXXXXXX") == 0;
}

int main(void) {
  char tmpnam_buffer[L_tmpnam];
  char ctermid_buffer[L_ctermid];
  char mktemplate[] = "temp_name_mktemp.XXXXXX";
  char* path;
  char* static_path;
  char* dynamic_path;

  if (L_tmpnam < 4096 || TMP_MAX < 1000000 || L_ctermid < 32) {
    return fail("stdio constants");
  }

  if (setenv("TMPDIR", ".", 1) != 0) {
    return fail("setenv TMPDIR");
  }

  errno = 0;
  path = tmpnam(tmpnam_buffer);
  if (path != tmpnam_buffer ||
      strncmp(path, "./tmpnam.", 9) != 0 ||
      !has_no_template_x(path) ||
      access(path, F_OK) == 0 ||
      errno != ENOENT) {
    return fail("tmpnam buffer");
  }

  static_path = tmpnam(0);
  if (static_path == 0 ||
      strncmp(static_path, "./tmpnam.", 9) != 0 ||
      !has_no_template_x(static_path)) {
    return fail("tmpnam static");
  }

  dynamic_path = tempnam("ignored", "pref.");
  if (dynamic_path == 0 ||
      strncmp(dynamic_path, "./pref.", 7) != 0 ||
      !has_no_template_x(dynamic_path) ||
      access(dynamic_path, F_OK) == 0 ||
      errno != ENOENT) {
    free(dynamic_path);
    return fail("tempnam TMPDIR");
  }
  free(dynamic_path);

  if (unsetenv("TMPDIR") != 0) {
    return fail("unsetenv TMPDIR");
  }
  dynamic_path = tempnam("custom-dir", 0);
  if (dynamic_path == 0 ||
      strncmp(dynamic_path, "custom-dir/tempnam.", 19) != 0 ||
      !has_no_template_x(dynamic_path)) {
    free(dynamic_path);
    return fail("tempnam dir default prefix");
  }
  free(dynamic_path);

  path = mktemp(mktemplate);
  if (path != mktemplate ||
      strncmp(path, "temp_name_mktemp.", 17) != 0 ||
      !has_no_template_x(path) ||
      access(path, F_OK) == 0 ||
      errno != ENOENT) {
    return fail("mktemp");
  }

  if (strcmp(ctermid(0), "/dev/tty") != 0 ||
      ctermid(ctermid_buffer) != ctermid_buffer ||
      strcmp(ctermid_buffer, "/dev/tty") != 0) {
    return fail("ctermid");
  }

  printf("temp_name_test: ok\n");
  return 0;
}
