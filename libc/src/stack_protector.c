#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

uintptr_t __stack_chk_guard = (uintptr_t)0x3141592653589793ULL;

void __stack_chk_fail(void) {
  static const char message[] = "stack check failed\n";
  (void)write(2, message, sizeof(message) - 1);
  abort();
}
