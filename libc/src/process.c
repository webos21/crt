#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <private/crt_fd_table.h>
#include <private/crt_shell_process.h>
#include <private/crt_signal.h>
#include <private/crt_spawn.h>
#include <private/crt_tls.h>

long __crt_sys_getpid(void);
long __crt_sys_getppid(void);
long __crt_sys_kill(long pid, int sig);
long __crt_sys_execve(const char* path, char* const argv[], char* const envp[]);
long __crt_sys_waitpid(long pid, int* status, int options);
#if defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_posix_spawn(
    const char* path,
    char* const argv[],
    char* const envp[],
    long* pid,
    int search_path,
    const posix_spawn_file_actions_t actions,
    const posix_spawnattr_t attr);
#endif
long __crt_sysconf_page_size(void);
long __crt_sysconf_nprocessors_conf(void);
long __crt_sysconf_nprocessors_onln(void);
long __crt_sysconf_phys_pages(void);
long __crt_sysconf_avphys_pages(void);
void __crt_malloc_after_fork_child(void);
void __crt_pthread_after_fork_child(void);
void __crt_stdio_after_fork_child(void);

#if defined(CRT_TARGET_OS_LINUX)
long __crt_sys_fork(void);
long __crt_sys_wait4(long pid, int* status, int options, void* rusage);

long __crt_sys_waitpid(long pid, int* status, int options) {
  return __crt_sys_wait4(pid, status, options, 0);
}

#elif defined(CRT_TARGET_OS_MACOS)
long __crt_sys_fork(void);
long __crt_sys_wait4(long pid, int* status, int options, void* rusage);

long __crt_sys_waitpid(long pid, int* status, int options) {
  return __crt_sys_wait4(pid, status, options, 0);
}
#elif defined(CRT_TARGET_OS_WINDOWS)
long __crt_sys_fork(void) {
  return -ENOTSUP;
}

long __crt_sys_execve(const char* path, char* const argv[], char* const envp[]) {
  long pid = 0;
  int status = 0;
  long result;

  result = __crt_sys_posix_spawn(path, argv, envp != 0 ? envp : environ, &pid, 0, 0, 0);
  if (result < 0) {
    return result;
  }
  result = __crt_sys_waitpid(pid, &status, 0);
  if (result < 0) {
    return result;
  }
  if (WIFEXITED(status)) {
    _exit(WEXITSTATUS(status));
  }
  _exit(127);
  return -ENOTSUP;
}
#endif

#define CRT_SYSTEM_CLK_TCK 100
#define CRT_SYSTEM_IOV_MAX 1024
#define CRT_SYSTEM_DELAYTIMER_MAX 2147483647L
#define CRT_SYSTEM_MQ_OPEN_MAX 8
#define CRT_SYSTEM_MQ_PRIO_MAX 32768
#define CRT_SYSTEM_SEM_NSEMS_MAX 256
#define CRT_SYSTEM_SEM_VALUE_MAX 0x3fffffffL
#define CRT_SYSTEM_SIGQUEUE_MAX 32
#define CRT_SYSTEM_TIMER_MAX 32
#define CRT_SYSTEM_LOGIN_NAME_MAX 256
#define CRT_SYSTEM_TTY_NAME_MAX 32
#define CRT_SYSTEM_ATEXIT_MAX 65536
#define CRT_SYSTEM_THREAD_KEYS_MAX 128
#define CRT_SYSTEM_THREAD_STACK_MIN 16384
#define CRT_SYSTEM_THREAD_THREADS_MAX 2048
#define CRT_SYSTEM_OPEN_MAX 1024
#define CRT_SYSTEM_NGROUPS_MAX 32
#define CRT_SYSTEM_ARG_MAX 131072
#define CRT_SYSTEM_PASS_MAX 128
#define CRT_SYSTEM_RE_DUP_MAX 255
#define CRT_SYSTEM_TZNAME_MAX 6
#define CRT_SYSTEM_GET_R_SIZE_MAX 1024
#if defined(CRT_TARGET_OS_LINUX)
static int matches_cpu_number(const char* s) {
  size_t i;

  if (s == 0 || s[0] != 'c' || s[1] != 'p' || s[2] != 'u' || s[3] < '0' || s[3] > '9') {
    return 0;
  }
  for (i = 4; s[i] != 0; ++i) {
    if (s[i] < '0' || s[i] > '9') {
      return 0;
    }
  }
  return 1;
}

long __crt_sysconf_page_size(void) {
  return 4096;
}

long __crt_sysconf_nprocessors_conf(void) {
  DIR* dir = opendir("/sys/devices/system/cpu");
  struct dirent* entry;
  long count = 0;

  if (dir == 0) {
    return 1;
  }
  while ((entry = readdir(dir)) != 0) {
    if ((entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN) &&
        matches_cpu_number(entry->d_name)) {
      ++count;
    }
  }
  closedir(dir);
  return count > 0 ? count : 1;
}

long __crt_sysconf_nprocessors_onln(void) {
  int fd = open("/proc/stat", O_RDONLY);
  char buffer[4096];
  ssize_t bytes;
  size_t i = 0;
  long count = 0;

  if (fd < 0) {
    return 1;
  }
  bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return 1;
  }
  buffer[bytes] = 0;
  while (i < (size_t)bytes) {
    size_t line_start = i;

    while (i < (size_t)bytes && buffer[i] != '\n') {
      ++i;
    }
    if (i > line_start) {
      char saved = buffer[i];
      buffer[i] = 0;
      if (matches_cpu_number(buffer + line_start)) {
        ++count;
      }
      buffer[i] = saved;
    }
    if (i < (size_t)bytes && buffer[i] == '\n') {
      ++i;
    }
  }
  return count > 0 ? count : 1;
}

static long linux_meminfo_pages(const char* key) {
  int fd = open("/proc/meminfo", O_RDONLY);
  char buffer[4096];
  ssize_t bytes;
  size_t key_len = strlen(key);
  size_t i = 0;
  long page_size = __crt_sysconf_page_size();

  if (fd < 0) {
    return -1;
  }
  bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0 || page_size <= 0) {
    return -1;
  }
  buffer[bytes] = 0;
  while (i < (size_t)bytes) {
    size_t line_start = i;

    while (i < (size_t)bytes && buffer[i] != '\n') {
      ++i;
    }
    if (i > line_start && strncmp(buffer + line_start, key, key_len) == 0) {
      char* end = 0;
      long kb = strtol(buffer + line_start + key_len, &end, 10);

      if (end != buffer + line_start + key_len && kb >= 0) {
        return (kb * 1024L) / page_size;
      }
    }
    if (i < (size_t)bytes && buffer[i] == '\n') {
      ++i;
    }
  }
  return -1;
}

long __crt_sysconf_phys_pages(void) {
  return linux_meminfo_pages("MemTotal:");
}

long __crt_sysconf_avphys_pages(void) {
  return linux_meminfo_pages("MemFree:");
}
#elif defined(CRT_TARGET_OS_MACOS)
#define CRT_DARWIN_CTL_HW 6
#define CRT_DARWIN_HW_NCPU 3
#define CRT_DARWIN_HW_PAGESIZE 7
#define CRT_DARWIN_HW_MEMSIZE 24

long __crt_sys_macos_sysctl(
    int* name,
    unsigned int namelen,
    void* oldp,
    unsigned long* oldlenp,
    void* newp,
    unsigned long newlen);

static long macos_sysctl_long(int mib0, int mib1) {
  int name[2];
  long value = 0;
  unsigned long length = sizeof(value);
  long result;

  name[0] = mib0;
  name[1] = mib1;
  result = __crt_sys_macos_sysctl(name, 2, &value, &length, 0, 0);
  return result < 0 ? -1 : value;
}

long __crt_sysconf_page_size(void) {
  long value = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_PAGESIZE);

  return value > 0 ? value : 4096;
}

long __crt_sysconf_nprocessors_conf(void) {
  long value = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_NCPU);

  return value > 0 ? value : 1;
}

long __crt_sysconf_nprocessors_onln(void) {
  return __crt_sysconf_nprocessors_conf();
}

long __crt_sysconf_phys_pages(void) {
  long memory_size = macos_sysctl_long(CRT_DARWIN_CTL_HW, CRT_DARWIN_HW_MEMSIZE);
  long page_size = __crt_sysconf_page_size();

  if (memory_size <= 0 || page_size <= 0) {
    return -1;
  }
  return memory_size / page_size;
}

long __crt_sysconf_avphys_pages(void) {
  return -1;
}
#endif

static long normalize_syscall_result(long result) {
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return result;
}

struct crt_atfork_handler {
  void (*prepare)(void);
  void (*parent)(void);
  void (*child)(void);
  struct crt_atfork_handler* next;
};

static struct crt_atfork_handler* atfork_handlers;
static struct crt_atfork_handler* atfork_handlers_tail;

static void crt_atfork_run_prepare_recursive(struct crt_atfork_handler* handler) {
  if (handler == 0) {
    return;
  }
  crt_atfork_run_prepare_recursive(handler->next);
  if (handler->prepare != 0) {
    handler->prepare();
  }
}

void __crt_atfork_prepare(void) {
  crt_atfork_run_prepare_recursive(atfork_handlers);
}

void __crt_atfork_parent(void) {
  struct crt_atfork_handler* handler;

  for (handler = atfork_handlers; handler != 0; handler = handler->next) {
    if (handler->parent != 0) {
      handler->parent();
    }
  }
}

static void crt_atfork_run_user_child(void) {
  struct crt_atfork_handler* handler;

  for (handler = atfork_handlers; handler != 0; handler = handler->next) {
    if (handler->child != 0) {
      handler->child();
    }
  }
}

void __crt_atfork_child(crt_thread_context* current_context) {
  __crt_thread_after_fork_child(current_context);
  __crt_pthread_after_fork_child();
  __crt_malloc_after_fork_child();
  __crt_fd_after_fork_child();
  __crt_stdio_after_fork_child();
  crt_atfork_run_user_child();
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
  struct crt_atfork_handler* handler =
      (struct crt_atfork_handler*)calloc(1, sizeof(*handler));

  if (handler == 0) {
    return errno;
  }
  handler->prepare = prepare;
  handler->parent = parent;
  handler->child = child;
  if (atfork_handlers == 0) {
    atfork_handlers = handler;
    atfork_handlers_tail = handler;
  } else {
    atfork_handlers_tail->next = handler;
    atfork_handlers_tail = handler;
  }
  return 0;
}

pid_t _Fork(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_fork());
}

pid_t fork(void) {
  crt_thread_context* current_context = __crt_thread_get_current();
  long child;

  __crt_atfork_prepare();
  child = __crt_sys_fork();
  if (child == 0) {
    __crt_atfork_child(current_context);
    return 0;
  }
  __crt_atfork_parent();
  return (pid_t)normalize_syscall_result(child);
}

pid_t vfork(void) {
  return fork();
}

static int posix_spawn_add_file_action(
    posix_spawn_file_actions_t* actions,
    enum crt_spawn_action_kind kind,
    int fd,
    int new_fd,
    const char* path,
    int flags,
    mode_t mode) {
  struct __posix_spawn_file_action* action;

  if (actions == 0 || *actions == 0) {
    return EINVAL;
  }
  action = (struct __posix_spawn_file_action*)calloc(1, sizeof(*action));
  if (action == 0) {
    return errno;
  }
  if (path != 0) {
    action->path = strdup(path);
    if (action->path == 0) {
      free(action);
      return errno;
    }
  }
  action->kind = kind;
  action->fd = fd;
  action->new_fd = new_fd;
  action->flags = flags;
  action->mode = mode;
  if ((*actions)->head == 0) {
    (*actions)->head = action;
    (*actions)->last = action;
  } else {
    (*actions)->last->next = action;
    (*actions)->last = action;
  }
  return 0;
}

#if !defined(CRT_TARGET_OS_WINDOWS)
static int spawn_attr_has_effect(const posix_spawnattr_t attr) {
  return attr != 0 &&
         (attr->flags & ~(POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK)) != 0;
}

static void apply_spawn_attr_or_exit(const posix_spawnattr_t attr) {
  if (attr == 0) {
    return;
  }
  if ((attr->flags & POSIX_SPAWN_SETSIGDEF) != 0) {
    __crt_signal_reset_defaults(attr->sigdefault64);
  }
  if ((attr->flags & POSIX_SPAWN_SETSIGMASK) != 0) {
    __crt_signal_set_mask(attr->sigmask64);
  }
}

static void apply_spawn_actions_or_exit(const posix_spawn_file_actions_t actions) {
  struct __posix_spawn_file_action* action;

  if (actions == 0) {
    return;
  }
  for (action = actions->head; action != 0; action = action->next) {
    if (action->kind == CRT_SPAWN_ACTION_OPEN) {
      int fd = open(action->path, action->flags, action->mode);

      if (fd < 0) {
        _exit(127);
      }
      if (fd != action->new_fd) {
        if (dup2(fd, action->new_fd) < 0) {
          _exit(127);
        }
        close(fd);
      }
    } else if (action->kind == CRT_SPAWN_ACTION_CLOSE) {
      close(action->fd);
    } else if (action->kind == CRT_SPAWN_ACTION_DUP2) {
      if (dup2(action->fd, action->new_fd) < 0) {
        _exit(127);
      }
    } else if (action->kind == CRT_SPAWN_ACTION_CHDIR) {
      if (chdir(action->path) < 0) {
        _exit(127);
      }
    } else {
      _exit(127);
    }
  }
}

long __crt_sys_posix_spawn(
    const char* path,
    char* const argv[],
    char* const envp[],
    long* pid,
    int search_path,
    const posix_spawn_file_actions_t actions,
    const posix_spawnattr_t attr) {
  long child;
  (void)search_path;

  if (spawn_attr_has_effect(attr)) {
    return -ENOTSUP;
  }
  child = __crt_sys_fork();
  if (child < 0) {
    return child;
  }
  if (child == 0) {
    apply_spawn_attr_or_exit(attr);
    apply_spawn_actions_or_exit(actions);
    __crt_sys_execve(path, argv, envp != 0 ? envp : environ);
    _exit(127);
  }
  if (pid != 0) {
    *pid = child;
  }
  return 0;
}
#endif

pid_t getpid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getpid());
}

pid_t getppid(void) {
  return (pid_t)normalize_syscall_result(__crt_sys_getppid());
}

int kill(pid_t pid, int sig) {
  long self;

  if (sig < 0) {
    errno = EINVAL;
    return -1;
  }
  self = __crt_sys_getpid();
  if (pid == (pid_t)self && sig != 0) {
    return raise(sig);
  }
  return (int)normalize_syscall_result(__crt_sys_kill((long)pid, sig));
}

int posix_spawnattr_init(posix_spawnattr_t* attr) {
  if (attr == 0) {
    return EINVAL;
  }
  *attr = (posix_spawnattr_t)calloc(1, sizeof(**attr));
  return *attr == 0 ? errno : 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t* attr) {
  if (attr == 0 || *attr == 0) {
    return EINVAL;
  }
  free(*attr);
  *attr = 0;
  return 0;
}

int posix_spawnattr_setflags(posix_spawnattr_t* attr, short flags) {
  short supported = POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF |
                    POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSCHEDPARAM |
                    POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_USEVFORK |
                    POSIX_SPAWN_SETSID | POSIX_SPAWN_CLOEXEC_DEFAULT;

  if (attr == 0 || *attr == 0 || (flags & ~supported) != 0) {
    return EINVAL;
  }
  (*attr)->flags = flags;
  return 0;
}

int posix_spawnattr_getflags(const posix_spawnattr_t* attr, short* flags) {
  if (attr == 0 || *attr == 0 || flags == 0) {
    return EINVAL;
  }
  *flags = (*attr)->flags;
  return 0;
}

int posix_spawnattr_setpgroup(posix_spawnattr_t* attr, pid_t pgroup) {
  if (attr == 0 || *attr == 0) {
    return EINVAL;
  }
  (*attr)->pgroup = pgroup;
  return 0;
}

int posix_spawnattr_getpgroup(const posix_spawnattr_t* attr, pid_t* pgroup) {
  if (attr == 0 || *attr == 0 || pgroup == 0) {
    return EINVAL;
  }
  *pgroup = (*attr)->pgroup;
  return 0;
}

int posix_spawnattr_setschedparam(posix_spawnattr_t* attr, const struct sched_param* param) {
  if (attr == 0 || *attr == 0 || param == 0) {
    return EINVAL;
  }
  (*attr)->schedparam = *param;
  return 0;
}

int posix_spawnattr_getschedparam(const posix_spawnattr_t* attr, struct sched_param* param) {
  if (attr == 0 || *attr == 0 || param == 0) {
    return EINVAL;
  }
  *param = (*attr)->schedparam;
  return 0;
}

int posix_spawnattr_setschedpolicy(posix_spawnattr_t* attr, int policy) {
  if (attr == 0 || *attr == 0) {
    return EINVAL;
  }
  (*attr)->schedpolicy = policy;
  return 0;
}

int posix_spawnattr_getschedpolicy(const posix_spawnattr_t* attr, int* policy) {
  if (attr == 0 || *attr == 0 || policy == 0) {
    return EINVAL;
  }
  *policy = (*attr)->schedpolicy;
  return 0;
}

int posix_spawnattr_setsigmask(posix_spawnattr_t* attr, const sigset_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  (*attr)->sigmask = *mask;
  (*attr)->sigmask64 = (sigset64_t)*mask;
  return 0;
}

int posix_spawnattr_getsigmask(const posix_spawnattr_t* attr, sigset_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  *mask = (*attr)->sigmask;
  return 0;
}

int posix_spawnattr_setsigmask64(posix_spawnattr_t* attr, const sigset64_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  (*attr)->sigmask64 = *mask;
  (*attr)->sigmask = (sigset_t)*mask;
  return 0;
}

int posix_spawnattr_getsigmask64(const posix_spawnattr_t* attr, sigset64_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  *mask = (*attr)->sigmask64;
  return 0;
}

int posix_spawnattr_setsigdefault(posix_spawnattr_t* attr, const sigset_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  (*attr)->sigdefault = *mask;
  (*attr)->sigdefault64 = (sigset64_t)*mask;
  return 0;
}

int posix_spawnattr_getsigdefault(const posix_spawnattr_t* attr, sigset_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  *mask = (*attr)->sigdefault;
  return 0;
}

int posix_spawnattr_setsigdefault64(posix_spawnattr_t* attr, const sigset64_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  (*attr)->sigdefault64 = *mask;
  (*attr)->sigdefault = (sigset_t)*mask;
  return 0;
}

int posix_spawnattr_getsigdefault64(const posix_spawnattr_t* attr, sigset64_t* mask) {
  if (attr == 0 || *attr == 0 || mask == 0) {
    return EINVAL;
  }
  *mask = (*attr)->sigdefault64;
  return 0;
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* actions) {
  if (actions == 0) {
    return EINVAL;
  }
  *actions = (posix_spawn_file_actions_t)calloc(1, sizeof(**actions));
  return *actions == 0 ? errno : 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* actions) {
  struct __posix_spawn_file_action* action;

  if (actions == 0 || *actions == 0) {
    return EINVAL;
  }
  action = (*actions)->head;
  while (action != 0) {
    struct __posix_spawn_file_action* next = action->next;

    free(action->path);
    free(action);
    action = next;
  }
  free(*actions);
  *actions = 0;
  return 0;
}

int posix_spawn_file_actions_addopen(
    posix_spawn_file_actions_t* actions,
    int fd,
    const char* path,
    int flags,
    mode_t mode) {
  if (fd < 0) {
    return EBADF;
  }
  if (path == 0) {
    return EINVAL;
  }
  return posix_spawn_add_file_action(actions, CRT_SPAWN_ACTION_OPEN, -1, fd, path, flags, mode);
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* actions, int fd) {
  if (fd < 0) {
    return EBADF;
  }
  return posix_spawn_add_file_action(actions, CRT_SPAWN_ACTION_CLOSE, fd, -1, 0, 0, 0);
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* actions, int fd, int new_fd) {
  if (fd < 0 || new_fd < 0) {
    return EBADF;
  }
  return posix_spawn_add_file_action(actions, CRT_SPAWN_ACTION_DUP2, fd, new_fd, 0, 0, 0);
}

int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t* actions, const char* path) {
  if (path == 0) {
    return EINVAL;
  }
  return posix_spawn_add_file_action(actions, CRT_SPAWN_ACTION_CHDIR, -1, -1, path, 0, 0);
}

int posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t* actions, int fd) {
  if (fd < 0) {
    return EBADF;
  }
  return posix_spawn_add_file_action(actions, CRT_SPAWN_ACTION_FCHDIR, fd, -1, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int* status, int options) {
  long result = __crt_sys_waitpid((long)pid, status, options);

  return (pid_t)normalize_syscall_result(result);
}

pid_t wait(int* status) {
  return waitpid((pid_t)-1, status, 0);
}

int posix_spawn(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attrp,
    char* const argv[],
    char* const envp[]) {
  long child_pid = 0;
  long result;

  if (path == 0) {
    return EINVAL;
  }
  result = __crt_sys_posix_spawn(
      path,
      argv,
      envp != 0 ? envp : environ,
      &child_pid,
      0,
      file_actions != 0 ? *file_actions : 0,
      attrp != 0 ? *attrp : 0);
  if (result < 0) {
    return (int)-result;
  }
  if (pid != 0) {
    *pid = (pid_t)child_pid;
  }
  return 0;
}

int posix_spawnp(
    pid_t* pid,
    const char* file,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attrp,
    char* const argv[],
    char* const envp[]) {
  long child_pid = 0;
  long result;

  if (file == 0) {
    return EINVAL;
  }
  result = __crt_sys_posix_spawn(
      file,
      argv,
      envp != 0 ? envp : environ,
      &child_pid,
      1,
      file_actions != 0 ? *file_actions : 0,
      attrp != 0 ? *attrp : 0);
  if (result < 0) {
    return (int)-result;
  }
  if (pid != 0) {
    *pid = (pid_t)child_pid;
  }
  return 0;
}

static int shell_copy_file_actions(
    posix_spawn_file_actions_t* out,
    const posix_spawn_file_actions_t* source,
    const char* cwd) {
  struct __posix_spawn_file_action* action;
  int result;

  result = posix_spawn_file_actions_init(out);
  if (result != 0) {
    return result;
  }
  if (source != 0 && *source != 0) {
    for (action = (*source)->head; action != 0; action = action->next) {
      if (action->kind == CRT_SPAWN_ACTION_OPEN) {
        result = posix_spawn_file_actions_addopen(
            out, action->new_fd, action->path, action->flags, action->mode);
      } else if (action->kind == CRT_SPAWN_ACTION_CLOSE) {
        result = posix_spawn_file_actions_addclose(out, action->fd);
      } else if (action->kind == CRT_SPAWN_ACTION_DUP2) {
        result = posix_spawn_file_actions_adddup2(out, action->fd, action->new_fd);
      } else if (action->kind == CRT_SPAWN_ACTION_CHDIR) {
        result = posix_spawn_file_actions_addchdir_np(out, action->path);
      } else if (action->kind == CRT_SPAWN_ACTION_FCHDIR) {
        result = posix_spawn_file_actions_addfchdir_np(out, action->fd);
      } else {
        result = ENOTSUP;
      }
      if (result != 0) {
        posix_spawn_file_actions_destroy(out);
        return result;
      }
    }
  }
  if (cwd != 0) {
    result = posix_spawn_file_actions_addchdir_np(out, cwd);
    if (result != 0) {
      posix_spawn_file_actions_destroy(out);
      return result;
    }
  }
  return 0;
}

static int shell_env_name_matches(const char* entry, const char* name) {
  size_t len = strlen(name);

  return strncmp(entry, name, len) == 0 && entry[len] == '=';
}

static char** shell_build_env_with_rootfs(char* const envp[], const char* rootfs) {
  char* const* source = envp != 0 ? envp : environ;
  size_t count = 0;
  size_t index = 0;
  size_t rootfs_len;
  char** result;
  char* root_entry;

  if (rootfs == 0) {
    return 0;
  }
  while (source != 0 && source[count] != 0) {
    ++count;
  }
  rootfs_len = strlen(rootfs);
  root_entry = (char*)malloc(sizeof("CRT_ROOTFS=") - 1 + rootfs_len + 1);
  if (root_entry == 0) {
    return 0;
  }
  memcpy(root_entry, "CRT_ROOTFS=", sizeof("CRT_ROOTFS=") - 1);
  memcpy(root_entry + sizeof("CRT_ROOTFS=") - 1, rootfs, rootfs_len + 1);
  result = (char**)malloc((count + 2) * sizeof(char*));
  if (result == 0) {
    free(root_entry);
    return 0;
  }
  for (count = 0; source != 0 && source[count] != 0; ++count) {
    if (shell_env_name_matches(source[count], "CRT_ROOTFS")) {
      continue;
    }
    result[index++] = source[count];
  }
  result[index++] = root_entry;
  result[index] = 0;
  return result;
}

static void shell_free_env_with_rootfs(char** envp) {
  size_t i;

  if (envp == 0) {
    return;
  }
  for (i = 0; envp[i] != 0; ++i) {
    if (shell_env_name_matches(envp[i], "CRT_ROOTFS")) {
      free(envp[i]);
      break;
    }
  }
  free(envp);
}

int __crt_shell_spawn(pid_t* pid, const struct crt_shell_child_spec* spec) {
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_t* actions_ptr = 0;
  posix_spawnattr_t attr;
  posix_spawnattr_t* attr_ptr = 0;
  char** env_override = 0;
  char* const* child_envp;
  short attr_flags = 0;
  int result;

  if (spec == 0 || spec->path == 0) {
    return EINVAL;
  }
  if ((spec->flags & CRT_SHELL_CHILD_SET_CWD) != 0 && spec->cwd == 0) {
    return EINVAL;
  }
  if ((spec->flags & CRT_SHELL_CHILD_SET_ROOTFS) != 0 && spec->rootfs == 0) {
    return EINVAL;
  }
  if ((spec->flags & CRT_SHELL_CHILD_FLUSH_STDIO) != 0 && fflush(0) != 0) {
    return errno;
  }
  if ((spec->flags & CRT_SHELL_CHILD_SET_CWD) != 0 || spec->file_actions != 0) {
    result = shell_copy_file_actions(
        &actions,
        spec->file_actions,
        (spec->flags & CRT_SHELL_CHILD_SET_CWD) != 0 ? spec->cwd : 0);
    if (result != 0) {
      return result;
    }
    actions_ptr = &actions;
  }
  if ((spec->flags & (CRT_SHELL_CHILD_SET_SIGMASK | CRT_SHELL_CHILD_SET_SIGDEFAULT)) != 0) {
    result = posix_spawnattr_init(&attr);
    if (result != 0) {
      if (actions_ptr != 0) {
        posix_spawn_file_actions_destroy(actions_ptr);
      }
      return result;
    }
    if ((spec->flags & CRT_SHELL_CHILD_SET_SIGMASK) != 0) {
      sigset64_t mask = spec->sigmask;

      attr_flags |= POSIX_SPAWN_SETSIGMASK;
      result = posix_spawnattr_setsigmask64(&attr, &mask);
      if (result != 0) {
        posix_spawnattr_destroy(&attr);
        if (actions_ptr != 0) {
          posix_spawn_file_actions_destroy(actions_ptr);
        }
        return result;
      }
    }
    if ((spec->flags & CRT_SHELL_CHILD_SET_SIGDEFAULT) != 0) {
      sigset64_t mask = spec->sigdefault;

      attr_flags |= POSIX_SPAWN_SETSIGDEF;
      result = posix_spawnattr_setsigdefault64(&attr, &mask);
      if (result != 0) {
        posix_spawnattr_destroy(&attr);
        if (actions_ptr != 0) {
          posix_spawn_file_actions_destroy(actions_ptr);
        }
        return result;
      }
    }
    result = posix_spawnattr_setflags(&attr, attr_flags);
    if (result != 0) {
      posix_spawnattr_destroy(&attr);
      if (actions_ptr != 0) {
        posix_spawn_file_actions_destroy(actions_ptr);
      }
      return result;
    }
    attr_ptr = &attr;
  }
  child_envp = spec->envp != 0 ? spec->envp : environ;
  if ((spec->flags & CRT_SHELL_CHILD_SET_ROOTFS) != 0) {
    env_override = shell_build_env_with_rootfs((char* const*)child_envp, spec->rootfs);
    if (env_override == 0) {
      if (attr_ptr != 0) {
        posix_spawnattr_destroy(attr_ptr);
      }
      if (actions_ptr != 0) {
        posix_spawn_file_actions_destroy(actions_ptr);
      }
      return ENOMEM;
    }
    child_envp = env_override;
  }
  result = posix_spawn(
      pid, spec->path, actions_ptr, attr_ptr, (char* const*)spec->argv, (char* const*)child_envp);
  shell_free_env_with_rootfs(env_override);
  if (attr_ptr != 0) {
    posix_spawnattr_destroy(attr_ptr);
  }
  if (actions_ptr != 0) {
    posix_spawn_file_actions_destroy(actions_ptr);
  }
  return result;
}

int __crt_shell_fork_exec(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    char* const argv[],
    char* const envp[]) {
  struct crt_shell_child_spec spec;

  memset(&spec, 0, sizeof(spec));
  spec.path = path;
  spec.argv = argv;
  spec.envp = envp;
  spec.file_actions = file_actions;
  spec.flags = CRT_SHELL_CHILD_FLUSH_STDIO;
  return __crt_shell_spawn(pid, &spec);
}

int execve(const char* path, char* const argv[], char* const envp[]) {
  long result;

  if (path == 0) {
    errno = EINVAL;
    return -1;
  }
  result = __crt_sys_execve(path, argv, envp);
  if (result < 0 && result >= -4095) {
    errno = (int)-result;
    return -1;
  }
  return (int)result;
}

long sysconf(int name) {
  switch (name) {
    case _SC_ARG_MAX:
      return CRT_SYSTEM_ARG_MAX;
    case _SC_CHILD_MAX:
      return _POSIX_CHILD_MAX;
    case _SC_CLK_TCK:
      return CRT_SYSTEM_CLK_TCK;
    case _SC_LINE_MAX:
      return 2048;
    case _SC_NGROUPS_MAX:
      return CRT_SYSTEM_NGROUPS_MAX;
    case _SC_OPEN_MAX:
      return CRT_SYSTEM_OPEN_MAX;
    case _SC_PASS_MAX:
      return CRT_SYSTEM_PASS_MAX;
    case _SC_2_C_BIND:
      return _POSIX_VERSION;
    case _SC_2_C_DEV:
      return -1;
    case _SC_2_C_VERSION:
      return _POSIX2_C_VERSION;
    case _SC_2_CHAR_TERM:
      return -1;
    case _SC_2_FORT_DEV:
    case _SC_2_FORT_RUN:
    case _SC_2_LOCALEDEF:
    case _SC_2_SW_DEV:
    case _SC_2_UPE:
      return -1;
    case _SC_2_VERSION:
      return _POSIX2_VERSION;
    case _SC_JOB_CONTROL:
      return _POSIX_JOB_CONTROL;
    case _SC_SAVED_IDS:
      return _POSIX_SAVED_IDS;
    case _SC_VERSION:
      return _POSIX_VERSION;
    case _SC_RE_DUP_MAX:
      return CRT_SYSTEM_RE_DUP_MAX;
    case _SC_STREAM_MAX:
      return FOPEN_MAX;
    case _SC_TZNAME_MAX:
      return CRT_SYSTEM_TZNAME_MAX;
    case _SC_XOPEN_CRYPT:
      return _XOPEN_CRYPT;
    case _SC_XOPEN_ENH_I18N:
      return _XOPEN_ENH_I18N;
    case _SC_XOPEN_SHM:
      return _XOPEN_SHM;
    case _SC_XOPEN_VERSION:
      return _XOPEN_VERSION;
    case _SC_XOPEN_XCU_VERSION:
      return _XOPEN_XCU_VERSION;
    case _SC_XOPEN_REALTIME:
      return _XOPEN_REALTIME;
    case _SC_XOPEN_REALTIME_THREADS:
      return _XOPEN_REALTIME_THREADS;
    case _SC_XOPEN_LEGACY:
      return _XOPEN_LEGACY;
    case _SC_ATEXIT_MAX:
      return CRT_SYSTEM_ATEXIT_MAX;
    case _SC_IOV_MAX:
      return CRT_SYSTEM_IOV_MAX;
    case _SC_PAGESIZE:
    case _SC_PAGE_SIZE:
      return __crt_sysconf_page_size();
    case _SC_XOPEN_UNIX:
      return _XOPEN_UNIX;
    case _SC_DELAYTIMER_MAX:
      return CRT_SYSTEM_DELAYTIMER_MAX;
    case _SC_MQ_OPEN_MAX:
      return CRT_SYSTEM_MQ_OPEN_MAX;
    case _SC_MQ_PRIO_MAX:
      return CRT_SYSTEM_MQ_PRIO_MAX;
    case _SC_RTSIG_MAX:
      return CRT_SYSTEM_SIGQUEUE_MAX;
    case _SC_SEM_NSEMS_MAX:
      return CRT_SYSTEM_SEM_NSEMS_MAX;
    case _SC_SEM_VALUE_MAX:
      return CRT_SYSTEM_SEM_VALUE_MAX;
    case _SC_SIGQUEUE_MAX:
      return CRT_SYSTEM_SIGQUEUE_MAX;
    case _SC_TIMER_MAX:
      return CRT_SYSTEM_TIMER_MAX;
    case _SC_ASYNCHRONOUS_IO:
      return -1;
    case _SC_FSYNC:
      return _POSIX_FSYNC;
    case _SC_MAPPED_FILES:
      return _POSIX_MAPPED_FILES;
    case _SC_MEMLOCK:
      return _POSIX_MEMLOCK;
    case _SC_MEMLOCK_RANGE:
      return _POSIX_MEMLOCK_RANGE;
    case _SC_MEMORY_PROTECTION:
      return _POSIX_MEMORY_PROTECTION;
    case _SC_MESSAGE_PASSING:
      return -1;
    case _SC_PRIORITIZED_IO:
      return -1;
    case _SC_PRIORITY_SCHEDULING:
      return _POSIX_PRIORITY_SCHEDULING;
    case _SC_REALTIME_SIGNALS:
      return _POSIX_REALTIME_SIGNALS;
    case _SC_SEMAPHORES:
      return _POSIX_SEMAPHORES;
    case _SC_SHARED_MEMORY_OBJECTS:
      return _POSIX_SHARED_MEMORY_OBJECTS;
    case _SC_SYNCHRONIZED_IO:
      return _POSIX_SYNCHRONIZED_IO;
    case _SC_TIMERS:
      return _POSIX_TIMERS;
    case _SC_GETGR_R_SIZE_MAX:
    case _SC_GETPW_R_SIZE_MAX:
      return CRT_SYSTEM_GET_R_SIZE_MAX;
    case _SC_LOGIN_NAME_MAX:
      return CRT_SYSTEM_LOGIN_NAME_MAX;
    case _SC_THREAD_DESTRUCTOR_ITERATIONS:
      return _POSIX_THREAD_DESTRUCTOR_ITERATIONS;
    case _SC_THREAD_KEYS_MAX:
      return CRT_SYSTEM_THREAD_KEYS_MAX;
    case _SC_THREAD_STACK_MIN:
      return CRT_SYSTEM_THREAD_STACK_MIN;
    case _SC_THREAD_THREADS_MAX:
      return CRT_SYSTEM_THREAD_THREADS_MAX;
    case _SC_TTY_NAME_MAX:
      return CRT_SYSTEM_TTY_NAME_MAX;
    case _SC_THREADS:
      return _POSIX_THREADS;
    case _SC_THREAD_ATTR_STACKADDR:
    case _SC_THREAD_ATTR_STACKSIZE:
      return -1;
    case _SC_THREAD_PRIORITY_SCHEDULING:
      return _POSIX_THREAD_PRIORITY_SCHEDULING;
    case _SC_THREAD_PRIO_INHERIT:
      return _POSIX_THREAD_PRIO_INHERIT;
    case _SC_THREAD_PRIO_PROTECT:
      return _POSIX_THREAD_PRIO_PROTECT;
    case _SC_THREAD_SAFE_FUNCTIONS:
      return _POSIX_THREAD_SAFE_FUNCTIONS;
    case _SC_NPROCESSORS_CONF:
      return __crt_sysconf_nprocessors_conf();
    case _SC_NPROCESSORS_ONLN:
      return __crt_sysconf_nprocessors_onln();
    case _SC_PHYS_PAGES:
      return __crt_sysconf_phys_pages();
    case _SC_AVPHYS_PAGES:
      return __crt_sysconf_avphys_pages();
    case _SC_MONOTONIC_CLOCK:
      return _POSIX_MONOTONIC_CLOCK;
    case _SC_ADVISORY_INFO:
      return -1;
    case _SC_BARRIERS:
      return _POSIX_BARRIERS;
    case _SC_CLOCK_SELECTION:
      return -1;
    case _SC_CPUTIME:
      return -1;
    case _SC_HOST_NAME_MAX:
      return _POSIX_HOST_NAME_MAX;
    case _SC_IPV6:
      return _POSIX_VERSION;
    case _SC_RAW_SOCKETS:
      return -1;
    case _SC_READER_WRITER_LOCKS:
      return _POSIX_READER_WRITER_LOCKS;
    case _SC_REGEXP:
      return _POSIX_VERSION;
    default:
      errno = ENOSYS;
      return -1;
  }
}
