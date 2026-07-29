#ifndef CRT_SETJMP_H
#define CRT_SETJMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t jmp_buf[32];
typedef intptr_t sigjmp_buf[34];

int setjmp(jmp_buf env);
int _setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value) __attribute__((noreturn));
void _longjmp(jmp_buf env, int value) __attribute__((noreturn));
void siglongjmp(sigjmp_buf env, int value) __attribute__((noreturn));

#define sigsetjmp(env, savesigs) \
  (((env)[32] = ((savesigs) != 0)), ((env)[33] = 0), setjmp((intptr_t*)(env)))

#ifdef __cplusplus
}
#endif

#endif
