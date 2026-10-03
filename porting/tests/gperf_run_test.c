/* Runs the freshly built gperf on a small keyword set and checks that it
 * emits a perfect-hash lookup function containing every keyword. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GPERF_PATH
#error "GPERF_PATH must name the built gperf executable"
#endif

int main(void) {
  const char *input =
      "struct keyword { const char *name; int id; };\n"
      "%%\n"
      "alpha, 1\n"
      "beta, 2\n"
      "gamma, 3\n"
      "delta, 4\n"
      "%%\n";
  const char *keyword_file = "gperf_run_test.gperf";
  FILE *file = fopen(keyword_file, "w");
  if (file == NULL) return 1;
  fputs(input, file);
  fclose(file);

  char command[1024];
  snprintf(command, sizeof(command), "%s -t %s", GPERF_PATH, keyword_file);
  FILE *pipe = popen(command, "r");
  if (pipe == NULL) {
    printf("gperf_run_test: popen failed\n");
    return 2;
  }
  char output[65536];
  size_t used = fread(output, 1, sizeof(output) - 1, pipe);
  output[used] = '\0';
  int status = pclose(pipe);
  remove(keyword_file);
  if (status != 0) {
    printf("gperf_run_test: gperf exited with status %d\n", status);
    return 3;
  }
  static const char *const keywords[] = {"alpha", "beta", "gamma", "delta"};
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
    if (strstr(output, keywords[i]) == NULL) {
      printf("gperf_run_test: keyword %s missing from the generated table\n", keywords[i]);
      return 4;
    }
  }
  if (strstr(output, "in_word_set") == NULL) {
    printf("gperf_run_test: no lookup function emitted\n");
    return 5;
  }
  printf("gperf_run_test: ok keywords=4 bytes=%zu\n", used);
  return 0;
}
