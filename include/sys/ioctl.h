#ifndef CRT_SYS_IOCTL_H
#define CRT_SYS_IOCTL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _IOC_NRBITS 8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS 2

#define _IOC_NRSHIFT 0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE 0U
#define _IOC_WRITE 1U
#define _IOC_READ 2U

#define _IOC(dir, type, nr, size) \
  (((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | ((nr) << _IOC_NRSHIFT) | \
   ((size) << _IOC_SIZESHIFT))
#define _IO(type, nr) _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, size_type) _IOC(_IOC_READ, (type), (nr), sizeof(size_type))
#define _IOW(type, nr, size_type) _IOC(_IOC_WRITE, (type), (nr), sizeof(size_type))
#define _IOWR(type, nr, size_type) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size_type))

#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404
#define TCSBRK 0x5409
#define TCXONC 0x540a
#define TCFLSH 0x540b
#define TIOCSCTTY 0x540e
#define TIOCGPGRP 0x540f
#define TIOCSPGRP 0x5410
#define TIOCNOTTY 0x5422
#define TIOCOUTQ 0x5411
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD 0x541b
#define TIOCINQ FIONREAD
#define FIONBIO 0x5421
#define FIONCLEX 0x5450
#define FIOCLEX 0x5451
#define FIOASYNC 0x5452

#define BLKGETSIZE64 _IOR(0x12, 114, unsigned long long)

struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};

int ioctl(int fd, int request, ...);

#ifdef __cplusplus
}
#endif

#endif
