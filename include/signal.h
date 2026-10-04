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
typedef sighandler_t sig_t;

union sigval {
  int sival_int;
  void* sival_ptr;
};

typedef union sigval sigval_t;

struct sigevent {
  int sigev_notify;
  int sigev_signo;
  union sigval sigev_value;
  void (*sigev_notify_function)(union sigval);
  void* sigev_notify_attributes;
};

/* Bionic/Linux siginfo_t (bionic/libc/kernel/uapi/asm-generic/siginfo.h): the
 * same 128-byte layout the Linux kernel hands to an SA_SIGINFO handler, with the
 * usual si_* accessor macros over the union. On Linux the backend can therefore
 * forward the kernel's own record; macOS and Windows fill the fields they know. */
typedef struct siginfo {
  int si_signo;
  int si_errno;
  int si_code;
#if defined(__LP64__) || defined(_WIN64)
  int __pad0;
#endif
  union {
    int _pad[28];
    struct {
      pid_t _pid;
      uid_t _uid;
    } _kill;
    struct {
      int _tid;
      int _overrun;
      union sigval _sigval;
      int _sys_private;
    } _timer;
    struct {
      pid_t _pid;
      uid_t _uid;
      union sigval _sigval;
    } _rt;
    struct {
      pid_t _pid;
      uid_t _uid;
      int _status;
      long _utime;
      long _stime;
    } _sigchld;
    struct {
      void* _addr;
      short _addr_lsb;
    } _sigfault;
    struct {
      long _band;
      int _fd;
    } _sigpoll;
  } _sifields;
} siginfo_t;

#define si_pid _sifields._kill._pid
#define si_uid _sifields._kill._uid
#define si_tid _sifields._timer._tid
#define si_overrun _sifields._timer._overrun
#define si_status _sifields._sigchld._status
#define si_utime _sifields._sigchld._utime
#define si_stime _sifields._sigchld._stime
#define si_value _sifields._rt._sigval
#define si_int _sifields._rt._sigval.sival_int
#define si_ptr _sifields._rt._sigval.sival_ptr
#define si_addr _sifields._sigfault._addr
#define si_addr_lsb _sifields._sigfault._addr_lsb
#define si_band _sifields._sigpoll._band
#define si_fd _sifields._sigpoll._fd

struct sigaction {
  union {
    sighandler_t sa_handler;
    void (*sa_sigaction)(int, siginfo_t*, void*);
  } __sigaction_handler;
  sigset_t sa_mask;
  int sa_flags;
};

#define sa_handler __sigaction_handler.sa_handler
#define sa_sigaction __sigaction_handler.sa_sigaction

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
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPOLL SIGIO
#define SIGPWR 30
#define SIGSYS 31
#define NSIG 32

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGEV_SIGNAL 0
#define SIGEV_NONE 1
#define SIGEV_THREAD 2

#define SA_NOCLDSTOP 1
#define SA_NOCLDWAIT 2
#define SA_SIGINFO 4
#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000
#define SA_NODEFER 0x40000000
#define SA_RESETHAND 0x80000000

#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6

// SIGFPE si_code values (matches Linux/Bionic <bits/siginfo-consts.h>
// numbering). Added for onetrueawk's SIGFPE handler (main.c), which
// switches on these to report which kind of floating-point exception
// occurred; not yet guaranteed to be populated accurately by every
// platform's SIGFPE delivery path in this project, in which case a
// handler relying on them just falls through to a generic message
// rather than misbehaving.
#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8

int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig);
int sigdelset(sigset_t* set, int sig);
int sigismember(const sigset_t* set, int sig);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset);
/* Declared here as in Bionic's <signal.h> (pthread_t is intptr_t, see <pthread.h>). */
int pthread_kill(intptr_t thread, int sig);
int sigsuspend(const sigset_t* mask);
sighandler_t signal(int sig, sighandler_t handler);
sighandler_t bsd_signal(int sig, sighandler_t handler);
int raise(int sig);
int kill(pid_t pid, int sig);
int killpg(pid_t pgrp, int sig);

extern const char* const sys_signame[NSIG];
extern const char* const sys_siglist[NSIG];

#ifdef __cplusplus
}
#endif

#endif
