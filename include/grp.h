#ifndef CRT_GRP_H
#define CRT_GRP_H

#include <stddef.h>
#include <sys/types.h>

struct group {
  char* gr_name;
  char* gr_passwd;
  gid_t gr_gid;
  char** gr_mem;
};

#ifdef __cplusplus
extern "C" {
#endif

struct group* getgrgid(gid_t gid);
struct group* getgrnam(const char* name);
int getgrgid_r(gid_t gid, struct group* grp, char* buf, size_t buflen, struct group** result);
int getgrnam_r(const char* name, struct group* grp, char* buf, size_t buflen, struct group** result);
int getgroups(int size, gid_t list[]);
int getgrouplist(const char* user, gid_t group, gid_t* groups, int* ngroups);
int setgroups(size_t size, const gid_t* list);
int initgroups(const char* user, gid_t group);

#ifdef __cplusplus
}
#endif

#endif
