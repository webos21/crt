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
void __crt_seqpacket_dup(int oldfd, int newfd);

#endif
