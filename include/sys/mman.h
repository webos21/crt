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

#define MAP_FILE 0x0000
#define MAP_SHARED 0x0001
#define MAP_PRIVATE 0x0002
#define MAP_TYPE 0x000f
#define MAP_FIXED 0x0010

#define MAP_ANONYMOUS 0x0020
#define MAP_GROWSDOWN 0x0100
#define MAP_DENYWRITE 0x0800
#define MAP_EXECUTABLE 0x1000
#define MAP_LOCKED 0x2000
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000
#define MAP_NONBLOCK 0x10000
#define MAP_STACK 0x20000
#define MAP_HUGETLB 0x40000
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void*)-1)

#define MS_ASYNC 1
#define MS_INVALIDATE 2
#define MS_SYNC 4

#define MCL_CURRENT 1
#define MCL_FUTURE 2
#define MCL_ONFAULT 4

#define MREMAP_MAYMOVE 1
#define MREMAP_FIXED 2

#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_FREE 8
#define MADV_REMOVE 9
#define MADV_DONTFORK 10
#define MADV_DOFORK 11
#define MADV_MERGEABLE 12
#define MADV_UNMERGEABLE 13
#define MADV_HUGEPAGE 14
#define MADV_NOHUGEPAGE 15
#define MADV_DONTDUMP 16
#define MADV_DODUMP 17
#define MADV_WIPEONFORK 18
#define MADV_KEEPONFORK 19

#define POSIX_MADV_NORMAL MADV_NORMAL
#define POSIX_MADV_RANDOM MADV_RANDOM
#define POSIX_MADV_SEQUENTIAL MADV_SEQUENTIAL
#define POSIX_MADV_WILLNEED MADV_WILLNEED
#define POSIX_MADV_DONTNEED MADV_DONTNEED

#define MFD_CLOEXEC 0x0001U
#define MFD_ALLOW_SEALING 0x0002U

/* memfd_create() -- Linux has a real syscall for this; macOS and Windows
 * don't have any comparable kernel primitive. Implemented portably on every
 * host as create-a-uniquely-named-file-then-unlink-it-immediately (the
 * exact same proven technique this project's own tmpfile() already uses,
 * see libc/src/stdio.c), rather than as a per-host raw syscall/PAL
 * feature. This gives a real fd nameless-on-the-filesystem, suitable for
 * mmap(MAP_SHARED), matching what memfd_create()'s actual near-term
 * consumers need (e.g. wl_shm-style shared buffers) -- but not real Linux
 * memfd's sealing support (F_ADD_SEALS/F_GET_SEALS aren't implemented;
 * MFD_ALLOW_SEALING is accepted but has no effect). See
 * docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-16 entry. */
int memfd_create(const char* name, unsigned int flags);

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
void* mmap64(void* addr, size_t length, int prot, int flags, int fd, off64_t offset);
int mprotect(void* addr, size_t length, int prot);
int munmap(void* addr, size_t length);
int msync(void* addr, size_t length, int flags);
void* mremap(void* old_addr, size_t old_size, size_t new_size, int flags, ...);
int mlockall(int flags);
int munlockall(void);
int mlock(const void* addr, size_t length);
int mlock2(const void* addr, size_t length, int flags);
int munlock(const void* addr, size_t length);
int mincore(void* addr, size_t length, unsigned char* vector);
int madvise(void* addr, size_t length, int advice);
int posix_madvise(void* addr, size_t length, int advice);

#ifdef __cplusplus
}
#endif

#endif
