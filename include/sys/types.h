#ifndef CRT_SYS_TYPES_H
#define CRT_SYS_TYPES_H

#include <bits/crt_types.h>
#include <stddef.h> /* size_t -- real-world POSIX systems (and Android Bionic
                      * itself) expose it from <sys/types.h> too, not just
                      * <stddef.h>; plenty of portable software (including
                      * curl's own CURL_SIZEOF autoconf macro) assumes this. */

/* time_t/clock_t -- same reasoning as size_t above (real-world <sys/types.h>
 * exposes these too, not just <time.h>; curl's own CURL_SIZEOF probe for
 * time_t only #includes <sys/types.h>). Guarded so including both this
 * header and <time.h> in the same translation unit -- extremely common --
 * doesn't produce a duplicate-typedef error; <time.h> guards its own
 * definitions the same way. */
#ifndef __CRT_TIME_T_DEFINED
#define __CRT_TIME_T_DEFINED
typedef __crt_time_t time_t;
#endif
#ifndef __CRT_CLOCK_T_DEFINED
#define __CRT_CLOCK_T_DEFINED
typedef __crt_clock_t clock_t;
#endif

typedef __crt_ssize_t ssize_t;
typedef __crt_off_t off_t;
typedef __crt_off_t off64_t;
typedef __crt_mode_t mode_t;
typedef __crt_dev_t dev_t;
typedef __crt_ino_t ino_t;
typedef __crt_nlink_t nlink_t;
typedef __crt_blksize_t blksize_t;
typedef __crt_blkcnt_t blkcnt_t;
typedef __crt_pid_t pid_t;
typedef __crt_uid_t uid_t;
typedef __crt_gid_t gid_t;
typedef __crt_socklen_t socklen_t;

#endif
