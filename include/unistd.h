#ifndef CRT_UNISTD_H
#define CRT_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

extern char** environ;

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int access(const char* path, int mode);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
pid_t getpid(void);
pid_t getppid(void);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int isatty(int fd);
int pipe(int pipefd[2]);
ssize_t readlink(const char* path, char* buf, size_t bufsiz);
int symlink(const char* target, const char* linkpath);
void _exit(int status) __attribute__((noreturn));

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
}
#endif

#endif
