#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <private/crt_auxv.h>

#define CRT_ENV_INITIAL_CAPACITY 64
#define CRT_ROOTFS_DEFAULT_PATH "/system/bin:/bin:/usr/bin"

static char* env_static_entries[CRT_ENV_INITIAL_CAPACITY];
static char** env_entries = env_static_entries;
char** environ = env_static_entries;
static size_t env_capacity = CRT_ENV_INITIAL_CAPACITY;
static int env_initialized;
/* Not static: Linux's getauxval() (libc/src/arch/linux/common/auxv.c) reads
 * this same untouched, kernel-provided pointer to locate the ELF auxiliary
 * vector. See private/crt_auxv.h. */
char** __crt_initial_envp;

void __crt_env_init(char** envp);

#if defined(CRT_TARGET_OS_WINDOWS)
typedef int BOOL;

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

__declspec(dllimport) char* CRT_WINAPI GetEnvironmentStringsA(void);
__declspec(dllimport) BOOL CRT_WINAPI FreeEnvironmentStringsA(char* lpszEnvironmentBlock);
#endif

static size_t env_name_len(const char* name) {
  size_t len = 0;

  while (name[len] != 0 && name[len] != '=') {
    ++len;
  }
  return len;
}

static int env_name_valid(const char* name) {
  return name != 0 && name[0] != 0 && strchr(name, '=') == 0;
}

static int env_find(const char* name, size_t name_len) {
  size_t i;

  for (i = 0; i < env_capacity; ++i) {
    if (env_entries[i] != 0 &&
        strncmp(env_entries[i], name, name_len) == 0 &&
        env_entries[i][name_len] == '=') {
      return (int)i;
    }
  }
  return -1;
}

static int env_find_entry(const char* entry, size_t name_len) {
  size_t i;

  for (i = 0; i < env_capacity; ++i) {
    if (env_entries[i] != 0 &&
        strncmp(env_entries[i], entry, name_len) == 0 &&
        env_entries[i][name_len] == '=') {
      return (int)i;
    }
  }
  return -1;
}

static int env_expand(void) {
  size_t old_capacity = env_capacity;
  size_t new_capacity = old_capacity * 2;
  char** new_entries = (char**)malloc(new_capacity * sizeof(char*));
  size_t i;

  if (new_entries == 0) {
    errno = ENOMEM;
    return -1;
  }
  for (i = 0; i < new_capacity; ++i) {
    new_entries[i] = i < old_capacity ? env_entries[i] : 0;
  }
  if (env_entries != env_static_entries) {
    free(env_entries);
  }
  env_entries = new_entries;
  environ = env_entries;
  env_capacity = new_capacity;
  return 0;
}

static int env_find_free_slot(void) {
  size_t i;

  for (;;) {
    for (i = 0; i + 1 < env_capacity; ++i) {
      if (env_entries[i] == 0) {
        return (int)i;
      }
    }
    if (env_expand() != 0) {
      return -1;
    }
  }
}

static int env_store_entry(const char* entry, int overwrite) {
  size_t length;
  size_t name_len;
  char* copy;
  int index;

  if (entry == 0 || entry[0] == '=' || strchr(entry, '=') == 0) {
    return 0;
  }
  name_len = env_name_len(entry);
  index = env_find_entry(entry, name_len);
  if (index >= 0 && !overwrite) {
    return 0;
  }
  if (index < 0) {
    index = env_find_free_slot();
  }
  if (index < 0) {
    errno = ENOMEM;
    return -1;
  }

  length = strlen(entry);
  copy = (char*)malloc(length + 1);
  if (copy == 0) {
    errno = ENOMEM;
    return -1;
  }
  memcpy(copy, entry, length + 1);
  free(env_entries[index]);
  env_entries[index] = copy;
  return 0;
}

long __crt_sys_thread_id(void);

/* The initial thread's id, recorded here because this runs on it at startup. Linux's gettid()
 * returns the pid for the initial thread and a distinct id for every other one (WTF decides
 * "main thread" with getpid() == syscall(SYS_gettid)); macOS thread ids are not the pid, so
 * gettid() uses this to give the initial thread the pid there too (libc/src/process.c). */
long __crt_initial_thread_id;

/* macOS: the initial thread's stack top, so pthread_getattr_np() can describe the initial thread's
 * stack (macOS has no /proc/self/maps). The kernel places argv, envp and the `apple` vector with
 * their strings at the very top of the initial stack, so the end of the highest of those strings,
 * rounded up to a page, bounds it from above; stack-bounds consumers such as WTF then use
 * `environ` as the origin anyway. */
unsigned long __crt_initial_stack_top;

static void record_initial_stack_top(char** envp) {
#if defined(CRT_TARGET_OS_MACOS)
  unsigned long highest = 0;
  char** cursor;

  if (envp == 0) {
    return;
  }
  for (cursor = envp; *cursor != 0; ++cursor) {
    unsigned long end = (unsigned long)*cursor + strlen(*cursor) + 1;
    if (end > highest) {
      highest = end;
    }
  }
  for (++cursor; *cursor != 0; ++cursor) {
    unsigned long end = (unsigned long)*cursor + strlen(*cursor) + 1;
    if (end > highest) {
      highest = end;
    }
  }
  if (highest != 0) {
    __crt_initial_stack_top = (highest + 4095UL) & ~4095UL;
  }
#else
  (void)envp;
#endif
}

void __crt_env_set_initial(char** envp) {
  __crt_initial_envp = envp;
  __crt_initial_thread_id = __crt_sys_thread_id();
  record_initial_stack_top(envp);
}

#if defined(CRT_TARGET_OS_WINDOWS)
static int rootfs_path_separator(int c) {
  return c == '/' || c == '\\';
}
#endif

static int rootfs_path_is_absolute(const char* path) {
  if (path == 0 || path[0] == 0) {
    return 0;
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':') {
    return 1;
  }
  return rootfs_path_separator(path[0]) && rootfs_path_separator(path[1]);
#else
  return path[0] == '/';
#endif
}

static int rootfs_join_argv0(char* output, size_t output_size, const char* argv0) {
  char cwd[PATH_MAX];
  size_t cwd_len;
  size_t argv_len;

  if (argv0 == 0 || argv0[0] == 0 || output == 0 || output_size == 0) {
    return -1;
  }
  argv_len = strlen(argv0);
  if (rootfs_path_is_absolute(argv0)) {
    if (argv_len + 1 > output_size) {
      return -1;
    }
    memcpy(output, argv0, argv_len + 1);
    return 0;
  }
  if (strchr(argv0, '/') == 0 && strchr(argv0, '\\') == 0) {
    return -1;
  }
  if (getcwd(cwd, sizeof(cwd)) == 0) {
    return -1;
  }
  cwd_len = strlen(cwd);
  if (cwd_len + 1 + argv_len + 1 > output_size) {
    return -1;
  }
  memcpy(output, cwd, cwd_len);
  output[cwd_len] = '/';
  memcpy(output + cwd_len + 1, argv0, argv_len + 1);
  return 0;
}

static void rootfs_normalize_slashes(char* path) {
  size_t i;

  for (i = 0; path[i] != 0; ++i) {
    if (path[i] == '\\') {
      path[i] = '/';
    }
  }
}

static const char* rootfs_find_marker(const char* path) {
  const char* marker = 0;
  const char* cursor = path;

  while ((cursor = strstr(cursor, "/rootfs/")) != 0) {
    marker = cursor;
    cursor += 8;
  }
  return marker;
}

static int rootfs_has_runtime_suffix(const char* suffix) {
  return strncmp(suffix, "system/bin/", 11) == 0 ||
         strncmp(suffix, "bin/", 4) == 0 ||
         strncmp(suffix, "usr/bin/", 8) == 0;
}

/* getprogname()/setprogname()/__progname: Android Bionic's own surface
 * (bionic/libc/bionic/getprogname.cpp and __libc_init_common(): __progname is
 * the basename of argv[0], "<unknown>" before startup has run, and
 * setprogname() stores the basename of its argument without copying it).
 * Needed by gnulib (gperf, and every GNU tool that uses its getprogname
 * module) and BSD-derived code. argv[0] outlives main(), so keeping the
 * pointer is as valid here as it is in Bionic. Windows paths use '\\' too. */
const char* __progname = "<unknown>";

void setprogname(const char* name) {
  const char* cursor;

  if (name == 0) {
    return;
  }
  for (cursor = name; *cursor != 0; ++cursor) {
    if (*cursor == '/'
#ifdef _WIN32
        || *cursor == '\\'
#endif
    ) {
      name = cursor + 1;
    }
  }
  __progname = name;
}

const char* getprogname(void) {
  return __progname;
}

void __crt_rootfs_bootstrap(int argc, char** argv) {
  char absolute_path[PATH_MAX];
  const char* marker;
  const char* suffix;
  size_t root_len;

  if (argc > 0 && argv != 0 && argv[0] != 0) {
    setprogname(argv[0]);
  }
  __crt_env_init(0);
  if (getenv("CRT_ROOTFS") != 0) {
    return;
  }
  if (argc <= 0 || argv == 0 || argv[0] == 0) {
    return;
  }
  if (rootfs_join_argv0(absolute_path, sizeof(absolute_path), argv[0]) != 0) {
    return;
  }
  rootfs_normalize_slashes(absolute_path);
  marker = rootfs_find_marker(absolute_path);
  if (marker == 0) {
    return;
  }
  suffix = marker + 8;
  if (!rootfs_has_runtime_suffix(suffix)) {
    return;
  }
  root_len = (size_t)(marker - absolute_path) + 7;
  absolute_path[root_len] = 0;
  if (setenv("CRT_ROOTFS", absolute_path, 1) != 0) {
    return;
  }
  (void)setenv("PATH", CRT_ROOTFS_DEFAULT_PATH, 1);
  (void)chdir("/");
}

void __crt_env_init(char** envp) {
  char** cursor;

  if (env_initialized) {
    return;
  }
  env_initialized = 1;
  if (envp == 0) {
    envp = __crt_initial_envp;
  }
  if (envp != 0) {
    for (cursor = envp; *cursor != 0; ++cursor) {
      if (env_store_entry(*cursor, 0) != 0) {
        break;
      }
    }
    return;
  }

#if defined(CRT_TARGET_OS_WINDOWS)
  {
    char* block = GetEnvironmentStringsA();
    char* entry = block;

    if (block == 0) {
      return;
    }
    while (*entry != 0) {
      (void)env_store_entry(entry, 0);
      entry += strlen(entry) + 1;
    }
    (void)FreeEnvironmentStringsA(block);
  }
#endif
}

char* getenv(const char* name) {
  size_t name_len;
  int index;

  __crt_env_init(0);
  if (!env_name_valid(name)) {
    return 0;
  }
  name_len = env_name_len(name);
  index = env_find(name, name_len);
  if (index < 0) {
    return 0;
  }
  return env_entries[index] + name_len + 1;
}

int putenv(char* entry) {
  __crt_env_init(0);
  if (entry == 0 || entry[0] == '=' || strchr(entry, '=') == 0) {
    errno = EINVAL;
    return -1;
  }
  return env_store_entry(entry, 1);
}

int setenv(const char* name, const char* value, int overwrite) {
  size_t name_len;
  size_t value_len;
  char* entry;
  int index;

  __crt_env_init(0);
  if (!env_name_valid(name) || value == 0) {
    errno = EINVAL;
    return -1;
  }
  name_len = env_name_len(name);
  index = env_find(name, name_len);
  if (index >= 0 && !overwrite) {
    return 0;
  }
  if (index < 0) {
    index = env_find_free_slot();
  }
  if (index < 0) {
    errno = ENOMEM;
    return -1;
  }

  value_len = strlen(value);
  entry = (char*)malloc(name_len + 1 + value_len + 1);
  if (entry == 0) {
    errno = ENOMEM;
    return -1;
  }
  memcpy(entry, name, name_len);
  entry[name_len] = '=';
  memcpy(entry + name_len + 1, value, value_len + 1);
  free(env_entries[index]);
  env_entries[index] = entry;
  return 0;
}

int unsetenv(const char* name) {
  size_t name_len;
  int index;
  size_t i;

  __crt_env_init(0);
  if (!env_name_valid(name)) {
    errno = EINVAL;
    return -1;
  }
  name_len = env_name_len(name);
  index = env_find(name, name_len);
  if (index >= 0) {
    free(env_entries[index]);
    for (i = (size_t)index; i + 1 < env_capacity; ++i) {
      env_entries[i] = env_entries[i + 1];
      if (env_entries[i] == 0) {
        break;
      }
    }
    env_entries[env_capacity - 1] = 0;
  }
  return 0;
}
