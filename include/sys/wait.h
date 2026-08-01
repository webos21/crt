#ifndef CRT_SYS_WAIT_H
#define CRT_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG 1
#define WUNTRACED 2
#define WCONTINUED 8

#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WTERMSIG(status) ((status) & 0x7f)
#define WSTOPSIG(status) WEXITSTATUS(status)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WIFSIGNALED(status) (WTERMSIG(status) != 0 && WTERMSIG(status) != 0x7f)
#define WIFSTOPPED(status) (WTERMSIG(status) == 0x7f)
#define WIFCONTINUED(status) (0)

#ifdef __cplusplus
extern "C" {
#endif

pid_t wait(int* status);
pid_t waitpid(pid_t pid, int* status, int options);

#ifdef __cplusplus
}
#endif

#endif
