#ifndef CRT_SYS_EVENTFD_H
#define CRT_SYS_EVENTFD_H

/* Linux-only in real Bionic too (Android only ever runs on the Linux
 * kernel, so Bionic never needed to stub this out for another host) --
 * see docs/bionic_libc_gaps.md and HISTORY.md's 2026-08-17 entry. Declared
 * on every host so portable code that merely #includes this and compiles
 * against the surface keeps working everywhere. Windows gets a real,
 * from-scratch emulation (a Win32 Event HANDLE + a 64-bit counter --
 * see libc/src/arch/windows/common/syscall.c's own CRT_FD_KIND_EVENTFD
 * comment), added 2026-09-01 after a portable consumer's own configure-
 * time feature probe (curl's) misdetected the original ENOSYS-stub
 * version as usable and broke -- see HISTORY.md's dated entry for that
 * regression and its fix. macOS also gets a real, from-scratch emulation
 * (a real pipe(2) pair as the underlying kernel object -- see
 * libc/src/fd.c's own "Real eventfd() emulation for macOS" comment),
 * added 2026-09-02 for the same reason. */

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
