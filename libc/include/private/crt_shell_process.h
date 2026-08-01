#ifndef CRT_PRIVATE_CRT_SHELL_PROCESS_H
#define CRT_PRIVATE_CRT_SHELL_PROCESS_H

#include <spawn.h>
#include <sys/types.h>

int __crt_shell_fork_exec(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    char* const argv[],
    char* const envp[]);

#endif
