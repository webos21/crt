#include <errno.h>
#include <stdlib.h>
static int initialized;
__attribute__((constructor)) static void initialize(void) { initialized = 1; }
int crt_provider_initialized(void) { return initialized; }
int *crt_provider_errno(void) { return &errno; }
void *crt_provider_allocate(void) {
  unsigned char *result = malloc(32);
  if (result) result[0] = 73;
  return result;
}
