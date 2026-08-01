#ifndef CRT_TERMIOS_H
#define CRT_TERMIOS_H

#include <sys/types.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_line;
  cc_t c_cc[NCCS];
  speed_t c_ispeed;
  speed_t c_ospeed;
};

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9
#define VSUSP 10

#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IXON 0002000
#define IXANY 0004000
#define IXOFF 0010000
#define IUTF8 0040000

#define OPOST 0000001
#define ONLCR 0000004

#define CSIZE 0000060
#define CS8 0000060
#define CREAD 0000200

#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define ECHOCTL 0001000
#define ECHOKE 0004000
#define IEXTEN 0100000

#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2

#define TCOOFF 0
#define TCOON 1
#define TCIOFF 2
#define TCION 3

#define B0 0
#define B9600 13
#define B38400 15
#define B115200 4098

#ifdef __cplusplus
extern "C" {
#endif

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
int tcdrain(int fd);
int tcflow(int fd, int action);
int tcflush(int fd, int queue_selector);
int tcsendbreak(int fd, int duration);
speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);
int cfsetispeed(struct termios* termios_p, speed_t speed);
int cfsetospeed(struct termios* termios_p, speed_t speed);
int cfsetspeed(struct termios* termios_p, speed_t speed);
void cfmakeraw(struct termios* termios_p);

#ifdef __cplusplus
}
#endif

#endif
