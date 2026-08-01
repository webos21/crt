#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char* empty_group_members[] = {0};

static struct passwd synthetic_passwd = {
  "shell",
  "x",
  0,
  0,
  "CRT shell user",
  "/",
  "/system/bin/sh",
};

static struct group synthetic_group = {
  "shell",
  "x",
  0,
  empty_group_members,
};

uid_t getuid(void) {
  return 0;
}

gid_t getgid(void) {
  return 0;
}

gid_t getegid(void) {
  return 0;
}

struct passwd* getpwuid(uid_t uid) {
  if (uid != 0) {
    errno = ENOENT;
    return 0;
  }
  return &synthetic_passwd;
}

struct passwd* getpwnam(const char* name) {
  if (name == 0 || strcmp(name, synthetic_passwd.pw_name) != 0) {
    errno = ENOENT;
    return 0;
  }
  return &synthetic_passwd;
}

struct group* getgrgid(gid_t gid) {
  if (gid != 0) {
    errno = ENOENT;
    return 0;
  }
  return &synthetic_group;
}

struct group* getgrnam(const char* name) {
  if (name == 0 || strcmp(name, synthetic_group.gr_name) != 0) {
    errno = ENOENT;
    return 0;
  }
  return &synthetic_group;
}

int getgroups(int size, gid_t list[]) {
  if (size < 0) {
    errno = EINVAL;
    return -1;
  }
  if (size > 0 && list != 0) {
    list[0] = 0;
  }
  return 1;
}

int setgroups(size_t size, const gid_t* list) {
  (void)list;
  if (size == 0) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}

int initgroups(const char* user, gid_t group) {
  (void)user;
  return group == 0 ? 0 : -1;
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid) {
  if ((ruid == (uid_t)-1 || ruid == 0) &&
      (euid == (uid_t)-1 || euid == 0) &&
      (suid == (uid_t)-1 || suid == 0)) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
  if ((rgid == (gid_t)-1 || rgid == 0) &&
      (egid == (gid_t)-1 || egid == 0) &&
      (sgid == (gid_t)-1 || sgid == 0)) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}
