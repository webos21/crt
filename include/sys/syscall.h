#ifndef CRT_SYS_SYSCALL_H
#define CRT_SYS_SYSCALL_H

#if defined(__aarch64__)
#define SYS_getpid 172
#define SYS_renameat2 276
#define SYS_rt_sigprocmask 135
#define SYS_futex 98
#elif defined(__x86_64__)
#define SYS_getpid 39
#define SYS_renameat2 316
#define SYS_rt_sigprocmask 14
#define SYS_futex 202
#else
#define SYS_getpid 39
#define SYS_renameat2 316
#define SYS_rt_sigprocmask 14
#define SYS_futex 202
#endif

#endif
