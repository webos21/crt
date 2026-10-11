#ifndef CRT_PRIVATE_CRT_SEQPACKET_H
#define CRT_PRIVATE_CRT_SEQPACKET_H

#include <stddef.h>
#include <sys/types.h>

/* macOS only: AF_UNIX SOCK_SEQPACKET is a CRT-framed stream socket (libc/src/socket.c). The
 * descriptor-level calls that are not part of the socket API (read, write, close, dup) consult this
 * so a framed socket keeps its record semantics there too. Every function is a cheap no-op or a
 * "not framed" answer for any other descriptor. */
int __crt_seqpacket_fd(int fd);
ssize_t __crt_seqpacket_read(int fd, void* buf, size_t count);
ssize_t __crt_seqpacket_write(int fd, const void* buf, size_t count);
void __crt_seqpacket_close(int fd);
/* 0, or -1 when `newfd` is beyond the table and `oldfd` is framed (the caller closes newfd, EMFILE). */
int __crt_seqpacket_dup(int oldfd, int newfd);
/* In a forked child: forget locks another thread of the parent held. */
void __crt_seqpacket_after_fork_child(void);
/* exec: a malloc'd copy of envp carrying CRT_SEQPACKET_FDS (the framed descriptors that are still
 * sockets), or 0 on allocation failure. At startup of the new image, import it again. */
int __crt_seqpacket_adopt(int fd); /* test hook */
char** __crt_seqpacket_export_env(char* const* envp);
void __crt_seqpacket_import_env(char** envp);

#endif
