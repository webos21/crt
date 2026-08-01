#ifndef CRT_SYS_INOTIFY_H
#define CRT_SYS_INOTIFY_H

#include <stdint.h>

#define IN_MODIFY 0x00000002

struct inotify_event {
  int wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  char name[];
};

#ifdef __cplusplus
extern "C" {
#endif

int inotify_init(void);
int inotify_add_watch(int fd, const char* path, uint32_t mask);
int inotify_rm_watch(int fd, int wd);

#ifdef __cplusplus
}
#endif

#endif
