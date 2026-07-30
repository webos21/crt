#ifndef CRT_SIGNAL_H
#define CRT_SIGNAL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int sig_atomic_t;
typedef unsigned long sigset_t;
typedef unsigned long long sigset64_t;
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22

sighandler_t signal(int sig, sighandler_t handler);
int raise(int sig);
int kill(pid_t pid, int sig);

#ifdef __cplusplus
}
#endif

#endif
