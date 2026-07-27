#ifndef CRT_SYS_MMAN_H
#define CRT_SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_SHARED 0x0001
#define MAP_PRIVATE 0x0002

#if defined(CRT_TARGET_OS_MACOS)
#define MAP_ANONYMOUS 0x1000
#else
#define MAP_ANONYMOUS 0x0020
#endif
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void*)-1)

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t length);

#ifdef __cplusplus
}
#endif

#endif
