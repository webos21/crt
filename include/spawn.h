#ifndef CRT_SPAWN_H
#define CRT_SPAWN_H

#include <sched.h>
#include <signal.h>
#include <sys/types.h>

#define POSIX_SPAWN_RESETIDS 1
#define POSIX_SPAWN_SETPGROUP 2
#define POSIX_SPAWN_SETSIGDEF 4
#define POSIX_SPAWN_SETSIGMASK 8
#define POSIX_SPAWN_SETSCHEDPARAM 16
#define POSIX_SPAWN_SETSCHEDULER 32
#define POSIX_SPAWN_USEVFORK 64
#define POSIX_SPAWN_SETSID 128
#define POSIX_SPAWN_CLOEXEC_DEFAULT 256

typedef struct __posix_spawnattr* posix_spawnattr_t;
typedef struct __posix_spawn_file_actions* posix_spawn_file_actions_t;

#ifdef __cplusplus
extern "C" {
#endif

int posix_spawn(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attrp,
    char* const argv[],
    char* const envp[]);

int posix_spawnp(
    pid_t* pid,
    const char* file,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attrp,
    char* const argv[],
    char* const envp[]);

int posix_spawnattr_init(posix_spawnattr_t* attr);
int posix_spawnattr_destroy(posix_spawnattr_t* attr);
int posix_spawnattr_setflags(posix_spawnattr_t* attr, short flags);
int posix_spawnattr_getflags(const posix_spawnattr_t* attr, short* flags);
int posix_spawnattr_setpgroup(posix_spawnattr_t* attr, pid_t pgroup);
int posix_spawnattr_getpgroup(const posix_spawnattr_t* attr, pid_t* pgroup);
int posix_spawnattr_setschedparam(posix_spawnattr_t* attr, const struct sched_param* param);
int posix_spawnattr_getschedparam(const posix_spawnattr_t* attr, struct sched_param* param);
int posix_spawnattr_setschedpolicy(posix_spawnattr_t* attr, int policy);
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t* attr, int* policy);
int posix_spawnattr_setsigmask(posix_spawnattr_t* attr, const sigset_t* mask);
int posix_spawnattr_getsigmask(const posix_spawnattr_t* attr, sigset_t* mask);
int posix_spawnattr_setsigmask64(posix_spawnattr_t* attr, const sigset64_t* mask);
int posix_spawnattr_getsigmask64(const posix_spawnattr_t* attr, sigset64_t* mask);
int posix_spawnattr_setsigdefault(posix_spawnattr_t* attr, const sigset_t* mask);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t* attr, sigset_t* mask);
int posix_spawnattr_setsigdefault64(posix_spawnattr_t* attr, const sigset64_t* mask);
int posix_spawnattr_getsigdefault64(const posix_spawnattr_t* attr, sigset64_t* mask);

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* actions);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* actions);
int posix_spawn_file_actions_addopen(
    posix_spawn_file_actions_t* actions,
    int fd,
    const char* path,
    int flags,
    mode_t mode);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* actions, int fd);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* actions, int fd, int new_fd);
int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t* actions, const char* path);
int posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t* actions, int fd);

#ifdef __cplusplus
}
#endif

#endif
