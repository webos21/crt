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
  return geteuid();
}

gid_t getgid(void) {
  return getegid();
}

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_getegid(void);
#endif

/* A real gap found and fixed 2026-09-02 by libcrtmedia's own PulseAudio
 * native-protocol audio sink backend (src/arch/linux/audio_sink_linux.c):
 * this returned a hardcoded 0 unconditionally, so the SCM_CREDENTIALS
 * ancillary message that sink sends on its very first message to a real
 * PulseAudio server (gid=getgid()) carried a false gid whenever the real
 * process gid was not actually 0 -- confirmed for real via strace against
 * the live WSLg PulseAudio server: `sendmsg(...) = -1 EPERM (Operation
 * not permitted)`, the kernel correctly rejecting a spoofed SCM_CREDENTIALS
 * gid the sending process does not actually hold. Fixed on Linux via the
 * real getegid() syscall (matching geteuid()'s own already-real
 * implementation, fd.c) -- macOS/Windows keep the previous hardcoded 0
 * (this project's own synthetic single-user identity model there, per
 * this file's own top comment; unlike geteuid()/getuid(), no real
 * consumer has yet needed a genuine macOS/Windows gid, so extending this
 * fix there is left a real, separate, flagged follow-up rather than
 * guessed at here). */
gid_t getegid(void) {
#if defined(CRT_TARGET_OS_LINUX)
  long result = __crt_sys_getegid();

  if (result < 0 && result >= -4095) {
    return (gid_t)__set_errno((int)-result);
  }
  return (gid_t)result;
#else
  return 0;
#endif
}

char* getlogin(void) {
  char* name = getenv("LOGNAME");

  if (name == 0 || name[0] == 0) {
    name = getenv("USER");
  }
  if (name != 0 && name[0] != 0) {
    return name;
  }
  return synthetic_passwd.pw_name;
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

static int copy_string_field(char** out, char** cursor, size_t* remaining, const char* value) {
  size_t len = strlen(value) + 1;

  if (len > *remaining) {
    return ERANGE;
  }
  memcpy(*cursor, value, len);
  *out = *cursor;
  *cursor += len;
  *remaining -= len;
  return 0;
}

static int copy_passwd(const struct passwd* source,
                       struct passwd* pwd,
                       char* buf,
                       size_t buflen,
                       struct passwd** result) {
  char* cursor = buf;
  size_t remaining = buflen;
  int error;

  if (pwd == 0 || result == 0 || (buf == 0 && buflen != 0)) {
    return EINVAL;
  }
  *result = 0;
  if (source == 0) {
    return 0;
  }
  *pwd = *source;
  if ((error = copy_string_field(&pwd->pw_name, &cursor, &remaining, source->pw_name)) != 0 ||
      (error = copy_string_field(&pwd->pw_passwd, &cursor, &remaining, source->pw_passwd)) != 0 ||
      (error = copy_string_field(&pwd->pw_gecos, &cursor, &remaining, source->pw_gecos)) != 0 ||
      (error = copy_string_field(&pwd->pw_dir, &cursor, &remaining, source->pw_dir)) != 0 ||
      (error = copy_string_field(&pwd->pw_shell, &cursor, &remaining, source->pw_shell)) != 0) {
    return error;
  }
  *result = pwd;
  return 0;
}

int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result) {
  return copy_passwd(uid == 0 ? &synthetic_passwd : 0, pwd, buf, buflen, result);
}

int getpwnam_r(const char* name, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result) {
  if (name == 0) {
    if (result != 0) {
      *result = 0;
    }
    return 0;
  }
  return copy_passwd(strcmp(name, synthetic_passwd.pw_name) == 0 ? &synthetic_passwd : 0,
                     pwd,
                     buf,
                     buflen,
                     result);
}

struct group* getgrgid(gid_t gid) {
  if (gid != 0) {
    errno = ENOENT;
    return 0;
  }
  return &synthetic_group;
}

static int copy_group(const struct group* source,
                      struct group* grp,
                      char* buf,
                      size_t buflen,
                      struct group** result) {
  char* cursor = buf;
  size_t remaining = buflen;
  int error;

  if (grp == 0 || result == 0 || (buf == 0 && buflen != 0)) {
    return EINVAL;
  }
  *result = 0;
  if (source == 0) {
    return 0;
  }
  *grp = *source;
  if ((error = copy_string_field(&grp->gr_name, &cursor, &remaining, source->gr_name)) != 0 ||
      (error = copy_string_field(&grp->gr_passwd, &cursor, &remaining, source->gr_passwd)) != 0) {
    return error;
  }
  if (remaining < sizeof(char*)) {
    return ERANGE;
  }
  grp->gr_mem = (char**)cursor;
  grp->gr_mem[0] = 0;
  *result = grp;
  return 0;
}

int getgrgid_r(gid_t gid, struct group* grp, char* buf, size_t buflen, struct group** result) {
  return copy_group(gid == 0 ? &synthetic_group : 0, grp, buf, buflen, result);
}

int getgrnam_r(const char* name, struct group* grp, char* buf, size_t buflen, struct group** result) {
  if (name == 0) {
    if (result != 0) {
      *result = 0;
    }
    return 0;
  }
  return copy_group(strcmp(name, synthetic_group.gr_name) == 0 ? &synthetic_group : 0,
                    grp,
                    buf,
                    buflen,
                    result);
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

int getgrouplist(const char* user, gid_t group, gid_t* groups, int* ngroups) {
  if (user == 0 || ngroups == 0) {
    errno = EINVAL;
    return -1;
  }
  if (*ngroups < 1 || groups == 0) {
    *ngroups = 1;
    return -1;
  }
  groups[0] = group;
  *ngroups = 1;
  return 0;
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
  uid_t current = geteuid();

  /* Setting an id to its own current value is always a no-op success per
   * POSIX, regardless of privilege; this CRT model otherwise only accepts
   * -1 (unchanged) or 0 (root) since it does not implement real privilege
   * switching. */
  if ((ruid == (uid_t)-1 || ruid == 0 || ruid == current) &&
      (euid == (uid_t)-1 || euid == 0 || euid == current) &&
      (suid == (uid_t)-1 || suid == 0 || suid == current)) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}

int setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
  gid_t current = getegid();

  if ((rgid == (gid_t)-1 || rgid == 0 || rgid == current) &&
      (egid == (gid_t)-1 || egid == 0 || egid == current) &&
      (sgid == (gid_t)-1 || sgid == 0 || sgid == current)) {
    return 0;
  }
  errno = ENOTSUP;
  return -1;
}

int setuid(uid_t uid) {
  return setresuid(uid, uid, uid);
}

int setgid(gid_t gid) {
  return setresgid(gid, gid, gid);
}
