#ifndef CRT_SYS_PRCTL_H
#define CRT_SYS_PRCTL_H

/*
 * Linux-only: prctl() is a Linux kernel concept with no equivalent on
 * macOS/Windows (real Bionic's own <sys/prctl.h> is Linux-specific too).
 * Declared on every host per this project's PAL-surface philosophy, but
 * only ever real on Linux -- see prctl.c. This project already has its
 * own thread-naming path (pthread_setname_np/pthread_getname_np), so
 * PR_SET_NAME/PR_GET_NAME here exist for source compatibility with code
 * written against real prctl(), not because this project depends on them
 * internally.
 *
 * The PR_* option values below are a fixed Linux UAPI (uapi/linux/
 * prctl.h) -- like elf.h's ELF64 constants, they carry no per-host
 * "unverified" caveat, since they are not host-kernel-numbering the way
 * raw syscall numbers are. The raw prctl() syscall trampoline itself
 * (prctl.c) *does* carry that caveat, matching every other Linux raw
 * syscall this project has added in a Windows-only dev session.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PR_SET_PDEATHSIG 1
#define PR_GET_PDEATHSIG 2
#define PR_GET_DUMPABLE 3
#define PR_SET_DUMPABLE 4
#define PR_GET_KEEPCAPS 7
#define PR_SET_KEEPCAPS 8
#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_GET_SECCOMP 21
#define PR_SET_SECCOMP 22
#define PR_CAPBSET_READ 23
#define PR_CAPBSET_DROP 24
#define PR_SET_TIMERSLACK 29
#define PR_GET_TIMERSLACK 30
#define PR_SET_CHILD_SUBREAPER 36
#define PR_GET_CHILD_SUBREAPER 37
#define PR_SET_NO_NEW_PRIVS 38
#define PR_GET_NO_NEW_PRIVS 39

/* PR_SET_NAME/PR_GET_NAME buffers hold up to 16 bytes including the NUL,
 * matching TASK_COMM_LEN in the Linux kernel. */
#define CRT_PR_NAME_MAX 16

int prctl(int option, ...);

#ifdef __cplusplus
}
#endif

#endif
