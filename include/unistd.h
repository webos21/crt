#ifndef CRT_UNISTD_H
#define CRT_UNISTD_H

#include <stddef.h>
#include <bits/sysconf.h>
#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L
#define _POSIX2_C_VERSION 200809L
#define _XOPEN_VERSION 700
#define _XOPEN_XCU_VERSION 4

#define _PC_FILESIZEBITS 0
#define _PC_LINK_MAX 1
#define _PC_MAX_CANON 2
#define _PC_MAX_INPUT 3
#define _PC_NAME_MAX 4
#define _PC_PATH_MAX 5
#define _PC_PIPE_BUF 6
#define _PC_2_SYMLINKS 7
#define _PC_ALLOC_SIZE_MIN 8
#define _PC_REC_INCR_XFER_SIZE 9
#define _PC_REC_MAX_XFER_SIZE 10
#define _PC_REC_MIN_XFER_SIZE 11
#define _PC_REC_XFER_ALIGN 12
#define _PC_SYMLINK_MAX 13
#define _PC_CHOWN_RESTRICTED 14
#define _PC_NO_TRUNC 15
#define _PC_VDISABLE 16
#define _PC_ASYNC_IO 17
#define _PC_PRIO_IO 18
#define _PC_SYNC_IO 19

#define _POSIX_AIO_LISTIO_MAX 2
#define _POSIX_AIO_MAX 1
#define _POSIX_ARG_MAX 4096
#define _POSIX_BARRIERS _POSIX_VERSION
#define _POSIX_CHILD_MAX 25
#define _POSIX_DELAYTIMER_MAX 32
#define _POSIX_HOST_NAME_MAX 255
#define _POSIX_LINK_MAX 8
#define _POSIX_LOGIN_NAME_MAX 9
#define _POSIX_MAX_CANON 255
#define _POSIX_MAX_INPUT 255
#define _POSIX_NAME_MAX 14
#define _POSIX_NGROUPS_MAX 8
#define _POSIX_OPEN_MAX 20
#define _POSIX_PATH_MAX 256
#define _POSIX_PIPE_BUF 512
#define _POSIX_RE_DUP_MAX 255
#define _POSIX_RTSIG_MAX 8
#define _POSIX_SEM_NSEMS_MAX 256
#define _POSIX_SEM_VALUE_MAX 32767
#define _POSIX_SIGQUEUE_MAX 32
#define _POSIX_SSIZE_MAX 32767
#define _POSIX_STREAM_MAX 8
#define _POSIX_SYMLINK_MAX 255
#define _POSIX_SYMLOOP_MAX 8
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4
#define _POSIX_THREAD_KEYS_MAX 128
#define _POSIX_THREAD_THREADS_MAX 64
#define _POSIX_TIMER_MAX 32
#define _POSIX_TTY_NAME_MAX 9
#define _POSIX_TZNAME_MAX 6

#define _POSIX2_BC_BASE_MAX 99
#define _POSIX2_BC_DIM_MAX 2048
#define _POSIX2_BC_SCALE_MAX 99
#define _POSIX2_BC_STRING_MAX 1000
#define _POSIX2_CHARCLASS_NAME_MAX 14
#define _POSIX2_COLL_WEIGHTS_MAX 2
#define _POSIX2_EXPR_NEST_MAX 32
#define _POSIX2_LINE_MAX 2048
#define _POSIX2_RE_DUP_MAX 255

#define _POSIX_FSYNC _POSIX_VERSION
#define _POSIX_JOB_CONTROL _POSIX_VERSION
#define _POSIX_MAPPED_FILES _POSIX_VERSION
#define _POSIX_MEMLOCK -1
#define _POSIX_MEMLOCK_RANGE -1
#define _POSIX_MEMORY_PROTECTION _POSIX_VERSION
#define _POSIX_MONOTONIC_CLOCK _POSIX_VERSION
#define _POSIX_PRIORITY_SCHEDULING -1
#define _POSIX_READER_WRITER_LOCKS _POSIX_VERSION
#define _POSIX_REALTIME_SIGNALS -1
#define _POSIX_SAVED_IDS _POSIX_VERSION
#define _POSIX_SEMAPHORES _POSIX_VERSION
#define _POSIX_SHARED_MEMORY_OBJECTS -1
#define _POSIX_SYNCHRONIZED_IO _POSIX_VERSION
#define _POSIX_THREADS _POSIX_VERSION
#define _POSIX_THREAD_ATTR_STACKADDR -1
#define _POSIX_THREAD_ATTR_STACKSIZE -1
#define _POSIX_THREAD_PRIO_INHERIT -1
#define _POSIX_THREAD_PRIO_PROTECT -1
#define _POSIX_THREAD_PRIORITY_SCHEDULING -1
#define _POSIX_THREAD_SAFE_FUNCTIONS _POSIX_VERSION
#define _POSIX_TIMERS _POSIX_VERSION

#define _XOPEN_CRYPT -1
#define _XOPEN_ENH_I18N -1
#define _XOPEN_LEGACY -1
#define _XOPEN_REALTIME -1
#define _XOPEN_REALTIME_THREADS -1
#define _XOPEN_SHM -1
#define _XOPEN_UNIX -1

#ifdef __cplusplus
extern "C" {
#endif

extern char** environ;
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int access(const char* path, int mode);
int faccessat(int dirfd, const char* path, int mode, int flags);
int chdir(const char* path);
int fchdir(int fd);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);
int unlinkat(int dirfd, const char* path, int flags);
int truncate(const char* path, off_t length);
int ftruncate(int fd, off_t length);
int fsync(int fd);
int fdatasync(int fd);
pid_t getpid(void);
pid_t getppid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
pid_t setsid(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
uid_t geteuid(void);
uid_t getuid(void);
gid_t getgid(void);
gid_t getegid(void);
pid_t getsid(pid_t pid);
int gethostname(char* name, size_t len);
int sethostname(const char* name, size_t len);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int setuid(uid_t uid);
int setgid(gid_t gid);
int nice(int inc);
int chroot(const char* path);
int fchown(int fd, uid_t owner, gid_t group);
int getopt(int argc, char* const argv[], const char* optstring);
int fchownat(int dirfd, const char* path, uid_t owner, gid_t group, int flags);
int lchown(const char* path, uid_t owner, gid_t group);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int isatty(int fd);
int pipe(int pipefd[2]);
unsigned int sleep(unsigned int seconds);
unsigned int alarm(unsigned int seconds);
long sysconf(int name);
long fpathconf(int fd, int name);
long pathconf(const char* path, int name);
ssize_t readlink(const char* path, char* buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t bufsiz);
int symlink(const char* target, const char* linkpath);
int symlinkat(const char* target, int newdirfd, const char* linkpath);
int link(const char* oldpath, const char* newpath);
int linkat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath, int flags);
pid_t _Fork(void);
pid_t fork(void);
pid_t vfork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
int execv(const char* path, char* const argv[]);
int execvp(const char* file, char* const argv[]);
long syscall(long number, ...);
void _exit(int status) __attribute__((noreturn));

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
}
#endif

#endif
