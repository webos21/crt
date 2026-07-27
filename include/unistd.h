#ifndef CRT_UNISTD_H
#define CRT_UNISTD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long ssize_t;

ssize_t write(int fd, const void* buf, size_t count);
void _exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
