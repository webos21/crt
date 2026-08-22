#ifndef CRT_LIBCXX_WIN32_SHIM_WINIOCTL_H
#define CRT_LIBCXX_WIN32_SHIM_WINIOCTL_H

/* The libc++ <filesystem> Windows backend uses these documented Kernel32
 * reparse-point constants while implementing read_symlink().  Keep this
 * deliberately narrow: the source supplies its own reparse-data layout, so
 * importing the full driver-oriented SDK header would add no value. */
#define FSCTL_GET_REPARSE_POINT 0x000900A8UL
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE 16384
#define IO_REPARSE_TAG_SYMLINK 0xA000000CUL
#define ERROR_REPARSE_TAG_INVALID 4392L

#endif
