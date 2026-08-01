#ifndef CRT_SYS_SYSMACROS_H
#define CRT_SYS_SYSMACROS_H

#include <sys/types.h>

#define major(dev) ((unsigned int)(((dev) >> 8) & 0xfff))
#define minor(dev) ((unsigned int)(((dev) & 0xff) | (((dev) >> 12) & 0xfff00)))
#define makedev(maj, min) \
  ((dev_t)((((dev_t)(maj) & 0xfff) << 8) | ((dev_t)(min) & 0xff) | (((dev_t)(min) & 0xfff00) << 12)))

#endif
