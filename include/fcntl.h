#ifndef CRT_FCNTL_H
#define CRT_FCNTL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002

#if defined(CRT_TARGET_OS_MACOS)
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#define O_APPEND 0x0008
#else
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#endif

int open(const char* path, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif
