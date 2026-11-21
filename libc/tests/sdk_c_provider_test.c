/* Installed-SDK consumer: distinguish actual mapped providers from link inputs. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int initialized;
__attribute__((constructor)) static void initialize(void) { initialized = 1; }

#ifdef CRT_TEST_PROJECT_DSO
int *crt_provider_errno(void);
void *crt_provider_allocate(void);
int crt_provider_initialized(void);
#endif

int main(void) {
  if (!initialized) return 2;
  unsigned char *allocation = calloc(32, 1);
  if (!allocation || allocation[0] || allocation[31]) return 3;
  free(allocation);
#ifdef CRT_TEST_PROJECT_DSO
  if (!crt_provider_initialized() || crt_provider_errno() != &errno) return 4;
  allocation = crt_provider_allocate();
  if (!allocation || allocation[0] != 73) return 5;
  free(allocation);
#endif
  const unsigned char text[] = "CRT C provider";
  if (crc32(0, text, sizeof(text)) == 0) return 6;
  printf("ZLIB:%s\n", zlibVersion());
  FILE *maps = fopen("/proc/self/maps", "r");
  if (!maps) return 7;
  char line[4096];
  while (fgets(line, sizeof(line), maps)) fputs(line, stdout);
  fclose(maps);
  puts("sdk_c_provider: ok");
  return 0;
}
