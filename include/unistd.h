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

#define _POSIX_ARG_MAX 4096
#define _POSIX_BARRIERS _POSIX_VERSION
#define _POSIX_CHILD_MAX 25
#define _POSIX_DELAYTIMER_MAX 32
#define _POSIX_HOST_NAME_MAX 255
#define _POSIX_LINK_MAX 8
#define _POSIX_LOGIN_NAME_MAX 9
#define _POSIX_NGROUPS_MAX 8
#define _POSIX_OPEN_MAX 20
#define _POSIX_PATH_MAX 256
#define _POSIX_PIPE_BUF 512
#define _POSIX_RE_DUP_MAX 255
#define _POSIX_SEM_NSEMS_MAX 256
#define _POSIX_SEM_VALUE_MAX 32767
#define _POSIX_SIGQUEUE_MAX 32
#define _POSIX_SSIZE_MAX 32767
#define _POSIX_STREAM_MAX 8
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4
#define _POSIX_THREAD_KEYS_MAX 128
#define _POSIX_THREAD_THREADS_MAX 64
#define _POSIX_TIMER_MAX 32
#define _POSIX_TTY_NAME_MAX 9
#define _POSIX_TZNAME_MAX 6

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

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int access(const char* path, int mode);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);
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
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int nice(int inc);
int fchown(int fd, uid_t owner, gid_t group);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int isatty(int fd);
int pipe(int pipefd[2]);
unsigned int sleep(unsigned int seconds);
unsigned int alarm(unsigned int seconds);
long sysconf(int name);
ssize_t readlink(const char* path, char* buf, size_t bufsiz);
int symlink(const char* target, const char* linkpath);
pid_t _Fork(void);
pid_t fork(void);
pid_t vfork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
void _exit(int status) __attribute__((noreturn));

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
}
#endif

#endif
