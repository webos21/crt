#ifndef CRT_SYS_MOUNT_H
#define CRT_SYS_MOUNT_H

#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MNT_FORCE 1
#define MNT_DETACH 2
#define MNT_EXPIRE 4
#define UMOUNT_NOFOLLOW 8

int mount(const char* source, const char* target, const char* fs_type, unsigned long flags,
          const void* data);
int umount(const char* target);
int umount2(const char* target, int flags);

#ifdef __cplusplus
}
#endif

#endif
