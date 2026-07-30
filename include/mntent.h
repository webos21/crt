#ifndef CRT_MNTENT_H
#define CRT_MNTENT_H

#include <paths.h>
#include <stdio.h>

#define MOUNTED _PATH_MOUNTED

#define MNTTYPE_IGNORE "ignore"
#define MNTTYPE_NFS "nfs"
#define MNTTYPE_SWAP "swap"

#define MNTOPT_DEFAULTS "defaults"
#define MNTOPT_NOAUTO "noauto"
#define MNTOPT_NOSUID "nosuid"
#define MNTOPT_RO "ro"
#define MNTOPT_RW "rw"
#define MNTOPT_SUID "suid"

struct mntent {
  char* mnt_fsname;
  char* mnt_dir;
  char* mnt_type;
  char* mnt_opts;
  int mnt_freq;
  int mnt_passno;
};

#ifdef __cplusplus
extern "C" {
#endif

int endmntent(FILE* fp);
struct mntent* getmntent(FILE* fp);
struct mntent* getmntent_r(FILE* fp, struct mntent* entry, char* buf, int size);
char* hasmntopt(const struct mntent* entry, const char* option);
FILE* setmntent(const char* filename, const char* type);

#ifdef __cplusplus
}
#endif

#endif
