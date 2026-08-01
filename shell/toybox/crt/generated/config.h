#include "../../src/android/linux/generated/config.h"

#undef CFG_TOYBOX_LIBCRYPTO
#undef USE_TOYBOX_LIBCRYPTO
#define CFG_TOYBOX_LIBCRYPTO 0
#define USE_TOYBOX_LIBCRYPTO(...)

#undef CFG_TOYBOX_LIBZ
#undef USE_TOYBOX_LIBZ
#define CFG_TOYBOX_LIBZ 0
#define USE_TOYBOX_LIBZ(...)

#undef CFG_FALSE
#undef USE_FALSE
#define CFG_FALSE 1
#define USE_FALSE(...) __VA_ARGS__

#undef CFG_FLOCK
#undef USE_FLOCK
#define CFG_FLOCK 0
#define USE_FLOCK(...)

#undef CFG_GZIP
#undef USE_GZIP
#define CFG_GZIP 0
#define USE_GZIP(...)

#undef CFG_ZCAT
#undef USE_ZCAT
#define CFG_ZCAT 0
#define USE_ZCAT(...)

#undef CFG_MOUNT
#undef USE_MOUNT
#define CFG_MOUNT 0
#define USE_MOUNT(...)

#undef CFG_NPROC
#undef USE_NPROC
#define CFG_NPROC 0
#define USE_NPROC(...)

#undef CFG_PGREP
#undef USE_PGREP
#define CFG_PGREP 0
#define USE_PGREP(...)

#undef CFG_PKILL
#undef USE_PKILL
#define CFG_PKILL 0
#define USE_PKILL(...)

#undef CFG_PS
#undef USE_PS
#define CFG_PS 0
#define USE_PS(...)

#undef CFG_UMOUNT
#undef USE_UMOUNT
#define CFG_UMOUNT 0
#define USE_UMOUNT(...)

#undef CFG_UNSHARE
#undef USE_UNSHARE
#define CFG_UNSHARE 0
#define USE_UNSHARE(...)
