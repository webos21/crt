#ifndef CRT_SYS_TTYDEFAULTS_H
#define CRT_SYS_TTYDEFAULTS_H

#include <termios.h>

#ifndef CTRL
#define CTRL(c) ((c) & 0x1f)
#endif

#define TTYDEF_IFLAG (ICRNL | IXON)
#define TTYDEF_OFLAG (OPOST | ONLCR)
#define TTYDEF_LFLAG (ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN)
#define TTYDEF_CFLAG (CREAD | CS8)

#endif
