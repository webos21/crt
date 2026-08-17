#ifndef CRT_SYS_EVENTFD_H
#define CRT_SYS_EVENTFD_H

/* Linux-only in real Bionic too (Android only ever runs on the Linux
 * kernel, so Bionic never needed to stub this out for another host) --
 * see docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-17 entry. Declared
 * on every host so portable code that merely #includes this and compiles
 * against the surface keeps working everywhere; eventfd() itself returns
 * ENOSYS on macOS/Windows, matching this project's existing
 * libc/src/inotify.c precedent for a Linux-only kernel feature with no
 * real host equivalent to fall back to. */

#include <fcntl.h> /* O_CLOEXEC/O_NONBLOCK, reused below -- same values
                     * real Linux/Bionic's own EFD_CLOEXEC/EFD_NONBLOCK
                     * are defined as. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t eventfd_t;

#define EFD_SEMAPHORE 0x00000001
#define EFD_CLOEXEC O_CLOEXEC
#define EFD_NONBLOCK O_NONBLOCK

int eventfd(unsigned int initval, int flags);
int eventfd_read(int fd, eventfd_t* value);
int eventfd_write(int fd, eventfd_t value);

#ifdef __cplusplus
}
#endif

#endif
