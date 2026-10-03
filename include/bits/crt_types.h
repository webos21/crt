#ifndef CRT_BITS_CRT_TYPES_H
#define CRT_BITS_CRT_TYPES_H

/* A leaf header on purpose: it names the fixed-width types through the
 * compiler's own predefined macros (the same ones <stdint.h> uses) instead of
 * including <stdint.h>. A consumer may interpose a <stdint.h> that pulls in
 * <wchar.h> -> <locale.h> -> <stdio.h>; if this header included it, <stdio.h>
 * would be parsed before __crt_off_t/__crt_ssize_t exist (found by gnulib in the
 * gperf port). */

typedef __INTPTR_TYPE__ __crt_ssize_t;
typedef __INT64_TYPE__ __crt_off_t;
typedef unsigned int __crt_mode_t;
typedef __UINT64_TYPE__ __crt_dev_t;
typedef __UINT64_TYPE__ __crt_ino_t;
typedef __UINT64_TYPE__ __crt_nlink_t;
typedef __INT64_TYPE__ __crt_blksize_t;
typedef __INT64_TYPE__ __crt_blkcnt_t;
typedef __INT32_TYPE__ __crt_pid_t;
typedef __UINT32_TYPE__ __crt_uid_t;
typedef __UINT32_TYPE__ __crt_gid_t;
typedef __UINT32_TYPE__ __crt_socklen_t;
typedef __INT64_TYPE__ __crt_time_t;
typedef long __crt_clock_t;
typedef int __crt_clockid_t;
typedef void* __crt_timer_t;
typedef unsigned long __crt_nfds_t;
typedef unsigned short __crt_sa_family_t;
typedef __UINT16_TYPE__ __crt_in_port_t;
typedef __UINT32_TYPE__ __crt_in_addr_t;
typedef unsigned long __crt_fd_mask;

#endif
