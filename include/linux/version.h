#ifndef CRT_LINUX_VERSION_H
#define CRT_LINUX_VERSION_H

/*
 * Minimal Linux UAPI compatibility header used by upstream libc++ sources
 * such as filesystem/operations.cpp when they probe kernel feature support.
 * Keep it aligned with the host toolchain's Linux version macros so the CRT
 * sysroot behaves like a modern Linux development environment without pulling
 * in arbitrary host kernel headers.
 */

#define LINUX_VERSION_CODE 395276
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + ((c) > 255 ? 255 : (c)))
#define LINUX_VERSION_MAJOR 6
#define LINUX_VERSION_PATCHLEVEL 8
#define LINUX_VERSION_SUBLEVEL 12

#endif
