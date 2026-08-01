#ifndef CRT_PRIVATE_CRT_FD_TABLE_H
#define CRT_PRIVATE_CRT_FD_TABLE_H

#include <stdint.h>

#define CRT_FD_SNAPSHOT_MAGIC 0x43525446U
#define CRT_FD_SNAPSHOT_VERSION 1U
#define CRT_FD_SNAPSHOT_MAX 64U

#define CRT_FD_SNAPSHOT_KIND_NONE 0
#define CRT_FD_SNAPSHOT_KIND_FILE 1
#define CRT_FD_SNAPSHOT_KIND_SOCKET 2

#define CRT_FD_SNAPSHOT_FLAG_INHERITABLE 0x00000001U

struct crt_fd_snapshot_entry {
  int fd;
  int kind;
  unsigned int flags;
  uintptr_t handle;
};

struct crt_fd_snapshot {
  unsigned int magic;
  unsigned int version;
  unsigned int count;
  unsigned int capacity;
  struct crt_fd_snapshot_entry entries[CRT_FD_SNAPSHOT_MAX];
};

int __crt_fd_snapshot_export(struct crt_fd_snapshot* snapshot);
int __crt_fd_snapshot_import(const struct crt_fd_snapshot* snapshot);
void __crt_fd_snapshot_dispose(struct crt_fd_snapshot* snapshot);
void __crt_fd_after_fork_child(void);

#endif
