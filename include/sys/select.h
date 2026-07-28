#ifndef CRT_SYS_SELECT_H
#define CRT_SYS_SELECT_H

#include <sys/time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD_SETSIZE 1024

typedef unsigned long __fd_mask;

typedef struct fd_set {
  __fd_mask fds_bits[FD_SETSIZE / (8 * sizeof(__fd_mask))];
} fd_set;

#define __FD_MASK_BITS (8 * (int)sizeof(__fd_mask))
#define __FD_ELT(fd) ((fd) / __FD_MASK_BITS)
#define __FD_MASK(fd) ((__fd_mask)1UL << ((fd) % __FD_MASK_BITS))

#define FD_ZERO(set)                                                           \
  do {                                                                        \
    int __i;                                                                  \
    for (__i = 0; __i < (int)(sizeof((set)->fds_bits) / sizeof(__fd_mask));    \
         ++__i) {                                                             \
      (set)->fds_bits[__i] = 0;                                               \
    }                                                                         \
  } while (0)
#define FD_SET(fd, set) ((set)->fds_bits[__FD_ELT(fd)] |= __FD_MASK(fd))
#define FD_CLR(fd, set) ((set)->fds_bits[__FD_ELT(fd)] &= ~__FD_MASK(fd))
#define FD_ISSET(fd, set) (((set)->fds_bits[__FD_ELT(fd)] & __FD_MASK(fd)) != 0)

int select(
    int nfds,
    fd_set* readfds,
    fd_set* writefds,
    fd_set* exceptfds,
    struct timeval* timeout);

#ifdef __cplusplus
}
#endif

#endif
