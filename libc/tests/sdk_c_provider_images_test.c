/* Installed-SDK consumer: use the public PAL to report actual runtime providers. */
#include <crt/dl.h>
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

static int report(const struct crt_dl_image *image, size_t size, void *data) {
  (void)size;
  (void)data;
  if (image->path) printf("IMAGE:%s\n", image->path);
  return 0;
}

int main(void) {
  unsigned char *allocation;
  unsigned char encoded[128], decoded[128];
  const unsigned char text[] = "CRT C shared provider";
  uLongf encoded_size = sizeof(encoded), decoded_size = sizeof(decoded);
  if (!initialized) return 2;
  allocation = calloc(32, 1);
  if (!allocation || allocation[0] || allocation[31]) return 3;
  free(allocation);
#ifdef CRT_TEST_PROJECT_DSO
  if (!crt_provider_initialized() || crt_provider_errno() != &errno) return 4;
  allocation = crt_provider_allocate();
  if (!allocation || allocation[0] != 73) return 5;
  free(allocation);
#endif
  if (compress2(encoded, &encoded_size, text, sizeof(text), 6) != Z_OK ||
      uncompress(decoded, &decoded_size, encoded, encoded_size) != Z_OK ||
      decoded_size != sizeof(text) || memcmp(decoded, text, sizeof(text))) return 6;
  if (crt_dl_iterate_images(report, 0)) return 7;
  printf("ZLIB:%s\n", zlibVersion());
  puts("sdk_c_provider: ok");
  return 0;
}
