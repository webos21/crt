#ifndef CRT_PRIVATE_CRT_SHELL_PROCESS_H
#define CRT_PRIVATE_CRT_SHELL_PROCESS_H

#include <spawn.h>
#include <signal.h>
#include <sys/types.h>

#define CRT_SHELL_CHILD_FLUSH_STDIO 0x00000001U
#define CRT_SHELL_CHILD_SET_CWD 0x00000002U
#define CRT_SHELL_CHILD_SET_ROOTFS 0x00000004U
#define CRT_SHELL_CHILD_SET_SIGMASK 0x00000008U
#define CRT_SHELL_CHILD_SET_SIGDEFAULT 0x00000010U

struct crt_shell_child_spec {
  const char* path;
  char* const* argv;
  char* const* envp;
  const posix_spawn_file_actions_t* file_actions;
  const char* cwd;
  const char* rootfs;
  sigset64_t sigmask;
  sigset64_t sigdefault;
  unsigned int flags;
};

int __crt_shell_spawn(pid_t* pid, const struct crt_shell_child_spec* spec);
int __crt_shell_fork_exec(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    char* const argv[],
    char* const envp[]);

#endif
