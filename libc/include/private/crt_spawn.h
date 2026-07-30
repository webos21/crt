#ifndef CRT_PRIVATE_CRT_SPAWN_H
#define CRT_PRIVATE_CRT_SPAWN_H

#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>

enum crt_spawn_action_kind {
  CRT_SPAWN_ACTION_OPEN,
  CRT_SPAWN_ACTION_CLOSE,
  CRT_SPAWN_ACTION_DUP2,
  CRT_SPAWN_ACTION_CHDIR,
  CRT_SPAWN_ACTION_FCHDIR
};

struct __posix_spawn_file_action {
  struct __posix_spawn_file_action* next;
  enum crt_spawn_action_kind kind;
  int fd;
  int new_fd;
  char* path;
  int flags;
  mode_t mode;
};

struct __posix_spawn_file_actions {
  struct __posix_spawn_file_action* head;
  struct __posix_spawn_file_action* last;
};

struct __posix_spawnattr {
  short flags;
  pid_t pgroup;
  struct sched_param schedparam;
  int schedpolicy;
  sigset_t sigmask;
  sigset_t sigdefault;
  sigset64_t sigmask64;
  sigset64_t sigdefault64;
};

#endif
