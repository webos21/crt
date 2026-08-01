#ifndef CRT_DIRENT_H
#define CRT_DIRENT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __crt_DIR DIR;

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12

struct dirent {
  ino_t d_ino;
  unsigned char d_type;
  char d_name[256];
};

DIR* opendir(const char* path);
DIR* fdopendir(int fd);
struct dirent* readdir(DIR* dirp);
void rewinddir(DIR* dirp);
int dirfd(DIR* dirp);
int closedir(DIR* dirp);

#ifdef __cplusplus
}
#endif

#endif
