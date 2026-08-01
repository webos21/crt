#ifndef CRT_PRIVATE_CRT_FD_TABLE_H
#define CRT_PRIVATE_CRT_FD_TABLE_H

#include <stdint.h>

#define CRT_FD_SNAPSHOT_MAGIC 0x43525446U
#define CRT_FD_SNAPSHOT_VERSION 1U
#define CRT_FD_SNAPSHOT_MAX 64U
#define CRT_FD_SOCKET_PROTOCOL_INFO_SIZE 512U

#define CRT_FD_SNAPSHOT_KIND_NONE 0
#define CRT_FD_SNAPSHOT_KIND_FILE 1
#define CRT_FD_SNAPSHOT_KIND_SOCKET 2

#define CRT_FD_SNAPSHOT_FLAG_INHERITABLE 0x00000001U
#define CRT_FD_SNAPSHOT_FLAG_SOCKET_DUPLICATED 0x00000002U

#define CRT_FD_SNAPSHOT_ENV "CRT_FD_SNAPSHOT"
#define CRT_FD_SNAPSHOT_PIPE_ENV "CRT_FD_SNAPSHOT_PIPE"
#define CRT_CHILD_BOOTSTRAP_ENV "CRT_CHILD_BOOTSTRAP"
#define CRT_CHILD_BOOTSTRAP_VERSION "1"
#define CRT_BOOTSTRAP_CWD_ENV "CRT_BOOTSTRAP_CWD"
#define CRT_BOOTSTRAP_ROOTFS_ENV "CRT_BOOTSTRAP_ROOTFS"
#define CRT_BOOTSTRAP_SIGMASK_ENV "CRT_BOOTSTRAP_SIGMASK"
#define CRT_BOOTSTRAP_SIGDEFAULT_ENV "CRT_BOOTSTRAP_SIGDEFAULT"

struct crt_fd_snapshot_entry {
  int fd;
  int kind;
  unsigned int flags;
  uintptr_t handle;
  unsigned int socket_protocol_info_size;
  unsigned char socket_protocol_info[CRT_FD_SOCKET_PROTOCOL_INFO_SIZE];
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
int __crt_fd_snapshot_encode(const struct crt_fd_snapshot* snapshot, char* buffer, unsigned long size);
int __crt_fd_snapshot_decode(const char* text, struct crt_fd_snapshot* snapshot);
int __crt_fd_get_cloexec(int fd);
int __crt_fd_set_cloexec(int fd, int cloexec);
void __crt_fd_after_fork_child(void);
void __crt_child_bootstrap(void);

#endif
