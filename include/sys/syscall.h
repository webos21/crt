#ifndef CRT_SYS_SYSCALL_H
#define CRT_SYS_SYSCALL_H

#if defined(__aarch64__)
#define SYS_getpid 172
#define SYS_renameat2 276
#define SYS_rt_sigprocmask 135
#define SYS_futex 98
#define SYS_gettid 178
#define SYS_sysinfo 179
#define SYS_sendfile 71
#define SYS_fallocate 47
#define SYS_tgkill 131
#define SYS_rt_sigsuspend 133
#define SYS_sched_setparam 118
#define SYS_sched_setscheduler 119
#define SYS_sched_getscheduler 120
#define SYS_sched_getparam 121
#define SYS_sched_get_priority_max 125
#define SYS_sched_get_priority_min 126
#elif defined(__x86_64__)
#define SYS_getpid 39
#define SYS_renameat2 316
#define SYS_rt_sigprocmask 14
#define SYS_futex 202
#define SYS_gettid 186
#define SYS_sysinfo 99
#define SYS_sendfile 40
#define SYS_fallocate 285
#define SYS_tgkill 234
#define SYS_rt_sigsuspend 130
#define SYS_sched_setparam 142
#define SYS_sched_getparam 143
#define SYS_sched_setscheduler 144
#define SYS_sched_getscheduler 145
#define SYS_sched_get_priority_max 146
#define SYS_sched_get_priority_min 147
#else
#define SYS_getpid 39
#define SYS_renameat2 316
#define SYS_rt_sigprocmask 14
#define SYS_futex 202
#define SYS_gettid 186
#define SYS_sysinfo 99
#define SYS_sendfile 40
#define SYS_fallocate 285
#define SYS_tgkill 234
#define SYS_sched_setparam 142
#define SYS_sched_getparam 143
#define SYS_sched_setscheduler 144
#define SYS_sched_getscheduler 145
#define SYS_sched_get_priority_max 146
#define SYS_sched_get_priority_min 147
#endif

/* Bionic's <sys/syscall.h> spells each number both __NR_x (<asm/unistd.h>) and
 * SYS_x. */
#define __NR_fallocate SYS_fallocate
#define __NR_futex SYS_futex
#define __NR_getpid SYS_getpid
#define __NR_gettid SYS_gettid
#define __NR_renameat2 SYS_renameat2
#define __NR_rt_sigprocmask SYS_rt_sigprocmask
#define __NR_sched_get_priority_max SYS_sched_get_priority_max
#define __NR_sched_get_priority_min SYS_sched_get_priority_min
#define __NR_sched_getparam SYS_sched_getparam
#define __NR_sched_getscheduler SYS_sched_getscheduler
#define __NR_sched_setparam SYS_sched_setparam
#define __NR_sched_setscheduler SYS_sched_setscheduler
#define __NR_tgkill SYS_tgkill
#define __NR_rt_sigsuspend SYS_rt_sigsuspend
#define __NR_sysinfo SYS_sysinfo
#define __NR_sendfile SYS_sendfile

#endif
