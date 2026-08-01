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
int getgroups(int size, gid_t list[]);
int setgroups(size_t size, const gid_t* list);
int initgroups(const char* user, gid_t group);

#ifdef __cplusplus
}
#endif

#endif
