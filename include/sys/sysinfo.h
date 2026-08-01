#ifndef CRT_SYS_SYSINFO_H
#define CRT_SYS_SYSINFO_H

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
  unsigned long totalhigh;
  unsigned long freehigh;
  unsigned int mem_unit;
};

#ifdef __cplusplus
extern "C" {
#endif

int sysinfo(struct sysinfo* info);

#ifdef __cplusplus
}
#endif

#endif
