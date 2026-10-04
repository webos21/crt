#ifndef CRT_SYS_SYSINFO_H
#define CRT_SYS_SYSINFO_H

#include <stddef.h>

struct sysinfo {
  long uptime;
  unsigned long loads[3];
  unsigned long totalram;
  unsigned long freeram;
  unsigned long sharedram;
  unsigned long bufferram;
  unsigned long totalswap;
  unsigned long freeswap;
  unsigned short procs;
  unsigned short pad;
  unsigned long totalhigh;
  unsigned long freehigh;
  unsigned int mem_unit;
  char _f[20 - 2 * sizeof(long) - sizeof(int)];
};

#ifdef __cplusplus
extern "C" {
#endif

int sysinfo(struct sysinfo* info);

#ifdef __cplusplus
}
#endif

#endif
