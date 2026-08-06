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
#define CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_HANDLE 0x00000004U
/* Mirrors the exporting fd's O_APPEND state (the only fd_flags bit that
 * matters across a spawn -- FD_CLOEXEC is meaningless on the far side of
 * an exec, and no other flag is tracked in fd_flags at all). Without
 * this, a child process that inherits an fd its parent opened with
 * O_APPEND has no way to know that, and __crt_sys_write() silently
 * writes from the file's current position (0, freshly opened) instead
 * of seeking to the end first -- observed as autoconf's own `printf ...
 * >>confdefs.h` idiom (an external command, unlike mksh's built-in
 * `echo`, so it always goes through this snapshot) losing every write
 * but the last. */
#define CRT_FD_SNAPSHOT_FLAG_APPEND 0x00000008U

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

/* Windows-only (declared unconditionally since this header carries no OS
 * guards elsewhere; only ever called from Windows aarch64 code). Lets
 * libc/src/arch/windows/common/fork_capable_relaunch.c hand this
 * process's current fd table across its own CreateProcessA() self-
 * relaunch hop, reusing the exact duplicate-into-child + pipe transport
 * __crt_sys_posix_spawn() uses for every ordinary spawn. See the
 * implementation in libc/src/arch/windows/common/syscall.c for the full
 * begin/finish/abort contract. */
int __crt_windows_fd_snapshot_relaunch_begin(unsigned long long* out_pipe_read_handle);
int __crt_windows_fd_snapshot_relaunch_finish(unsigned long long child_process_handle, unsigned long child_pid);
void __crt_windows_fd_snapshot_relaunch_abort(void);

#endif
