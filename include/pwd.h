#ifndef CRT_PWD_H
#define CRT_PWD_H

#include <stddef.h>
#include <sys/types.h>

struct passwd {
  char* pw_name;
  char* pw_passwd;
  uid_t pw_uid;
  gid_t pw_gid;
  char* pw_gecos;
  char* pw_dir;
  char* pw_shell;
};

#ifdef __cplusplus
extern "C" {
#endif

struct passwd* getpwuid(uid_t uid);
struct passwd* getpwnam(const char* name);
int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result);
int getpwnam_r(const char* name, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result);

#ifdef __cplusplus
}
#endif

#endif
