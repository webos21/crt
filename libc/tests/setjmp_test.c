#include <setjmp.h>
#include <stdio.h>

static jmp_buf env;
static sigjmp_buf sig_env;

static void jump_with_zero(void) {
  longjmp(env, 0);
}

static void jump_with_value(void) {
  longjmp(env, 7);
}

static void sig_jump(void) {
  siglongjmp(sig_env, 3);
}

int main(void) {
  volatile int stage = 0;
  int value = setjmp(env);
  if (value == 0 && stage == 0) {
    stage = 1;
    jump_with_zero();
  }
  if (value != 1 || stage != 1) {
    fprintf(stderr, "setjmp_test: longjmp zero\n");
    return 1;
  }

  value = _setjmp(env);
  if (value == 0 && stage == 1) {
    stage = 2;
    jump_with_value();
  }
  if (value != 7 || stage != 2) {
    fprintf(stderr, "setjmp_test: longjmp value\n");
    return 1;
  }

  value = sigsetjmp(sig_env, 1);
  if (value == 0 && stage == 2) {
    stage = 3;
    sig_jump();
  }
  if (value != 3 || stage != 3) {
    fprintf(stderr, "setjmp_test: siglongjmp\n");
    return 1;
  }

  printf("setjmp_test: ok\n");
  return 0;
}
