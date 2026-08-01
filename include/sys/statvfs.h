#ifndef CRT_SYS_STATVFS_H
#define CRT_SYS_STATVFS_H

#include <stdint.h>
#include <sys/types.h>

typedef uint64_t fsblkcnt_t;
typedef uint64_t fsfilcnt_t;

struct statvfs {
  unsigned long f_bsize;
  unsigned long f_frsize;
  fsblkcnt_t f_blocks;
  fsblkcnt_t f_bfree;
  fsblkcnt_t f_bavail;
  fsfilcnt_t f_files;
  fsfilcnt_t f_ffree;
  fsfilcnt_t f_favail;
  unsigned long f_fsid;
  unsigned long f_flag;
  unsigned long f_namemax;
};

#ifdef __cplusplus
extern "C" {
#endif

int statvfs(const char* path, struct statvfs* buf);
int fstatvfs(int fd, struct statvfs* buf);

#ifdef __cplusplus
}
#endif

#endif
