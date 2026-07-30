#ifndef CRT_SYS_VFS_H
#define CRT_SYS_VFS_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int __val[2];
} fsid_t;

struct statfs {
  uint64_t f_type;
  uint64_t f_bsize;
  uint64_t f_blocks;
  uint64_t f_bfree;
  uint64_t f_bavail;
  uint64_t f_files;
  uint64_t f_ffree;
  fsid_t f_fsid;
  uint64_t f_namelen;
  uint64_t f_frsize;
  uint64_t f_flags;
  uint64_t f_spare[4];
};

struct statfs64 {
  uint64_t f_type;
  uint64_t f_bsize;
  uint64_t f_blocks;
  uint64_t f_bfree;
  uint64_t f_bavail;
  uint64_t f_files;
  uint64_t f_ffree;
  fsid_t f_fsid;
  uint64_t f_namelen;
  uint64_t f_frsize;
  uint64_t f_flags;
  uint64_t f_spare[4];
};

#define _STATFS_F_NAMELEN
#define _STATFS_F_FRSIZE
#define _STATFS_F_FLAGS

int statfs(const char* path, struct statfs* buf);
int statfs64(const char* path, struct statfs64* buf);
int fstatfs(int fd, struct statfs* buf);
int fstatfs64(int fd, struct statfs64* buf);

#ifdef __cplusplus
}
#endif

#endif
