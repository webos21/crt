#include <setjmp.h>

void siglongjmp(sigjmp_buf env, int value) {
  longjmp((intptr_t*)env, value);
}
