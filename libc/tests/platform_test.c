#include <crt/platform.h>
#include <crt/dl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count_image(const struct crt_dl_image* image, size_t size, void* data) {
  int* count = data;
  if (size != sizeof(*image) || image->path == 0 || image->path[0] == 0 ||
      strcmp(image->format, crt_get_platform_info()->image_format) != 0) return 99;
  ++*count;
  return 0;
}
static int stop_image(const struct crt_dl_image* image, size_t size, void* data) {
  (void)image; (void)size; (void)data;
  return 17;
}

int main(int argc, char** argv) {
  const struct crt_platform_info* info = crt_get_platform_info();
  const char* variable = strcmp(info->os, "windows") == 0 ? "USERPROFILE" : "HOME";
  const char* value = getenv(variable);
  char* saved = value ? strdup(value) : 0;
  char home[4096];
  FILE* file;
  int count = 0;
  if (argc < 1 || info->arch[0] == 0 || crt_is_native_executable(argv[0]) != 1) return 1;
  if (crt_dl_iterate_images(count_image, &count) != 0 || count == 0 ||
      crt_dl_iterate_images(stop_image, 0) != 17) return 2;
  errno = 0;
  if (crt_dl_iterate_images(0, 0) != -1 || errno != EINVAL) return 3;
  if (setenv(variable, "/tmp/CRT home space", 1) != 0 ||
      crt_get_user_home(home, sizeof(home)) != 0 || strcmp(home, "/tmp/CRT home space") != 0) return 4;
  errno = 0;
  if (crt_get_user_home(home, 2) != -1 || errno != ERANGE) return 5;
  if (setenv(variable, "relative-home", 1) != 0) return 6;
  errno = 0;
  if (crt_get_user_home(home, sizeof(home)) != -1 || errno != EINVAL) return 7;
  unsetenv(variable);
  errno = 0;
  if (crt_get_user_home(home, sizeof(home)) != -1 || errno != ENOENT) return 8;
  if (saved) { setenv(variable, saved, 1); free(saved); }
  file = fopen("platform-script-test.tmp", "wb");
  if (!file || fputs("#!/bin/sh\nexit 0\n", file) < 0 || fclose(file) != 0) return 9;
  count = crt_is_native_executable("platform-script-test.tmp");
  remove("platform-script-test.tmp");
  if (count != 0) return 10;
  errno = 0;
  if (crt_is_native_executable(0) != -1 || errno != EINVAL) return 11;
  puts("platform_test: ok");
  return 0;
}
