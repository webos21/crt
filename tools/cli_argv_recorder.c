/* Producer-side helper of tools/export_cli_toolchain.py on Windows: stands in for the host C/C++ compiler,
 * writes its argv as a JSON array to %CAPTURE_ARGV% and exits 0. `-print-resource-dir` is forwarded to the
 * real compiler named by %REAL_CLANG% so the wrapper sees the genuine value. Never part of an SDK or the CLI. */
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put(FILE* out, const char* text) {
  fputc('"', out);
  for (; *text; ++text) {
    unsigned char c = (unsigned char)*text;
    if (c == '"' || c == '\\') { fputc('\\', out); fputc(c, out); }
    else if (c < 0x20) fprintf(out, "\\u%04x", c);
    else fputc(c, out);
  }
  fputc('"', out);
}

int main(int argc, char** argv) {
  const char* real = getenv("REAL_CLANG");
  const char* capture = getenv("CAPTURE_ARGV");
  if (argc == 2 && !strcmp(argv[1], "-print-resource-dir") && real) {
    const char* args[] = {real, "-print-resource-dir", NULL};
    return (int)_spawnv(_P_WAIT, real, args);
  }
  if (!capture) return 2;
  FILE* out = fopen(capture, "wb");
  if (!out) return 3;
  fputc('[', out);
  for (int i = 1; i < argc; ++i) {
    if (i > 1) fputc(',', out);
    put(out, argv[i]);
  }
  fputc(']', out);
  return fclose(out) ? 4 : 0;
}
