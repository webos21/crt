#ifndef CRT_PTY_H
#define CRT_PTY_H

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int openpty(int* amaster, int* aslave, char* name, const struct termios* termp,
            const struct winsize* winp);
pid_t forkpty(int* amaster, char* name, const struct termios* termp,
              const struct winsize* winp);

#ifdef __cplusplus
}
#endif

#endif
