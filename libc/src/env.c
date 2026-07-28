#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CRT_ENV_MAX 64

static char* env_entries[CRT_ENV_MAX];
char** environ = env_entries;
static int env_initialized;

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
  int i;

  for (i = 0; i < CRT_ENV_MAX; ++i) {
    if (env_entries[i] != 0 &&
        strncmp(env_entries[i], name, name_len) == 0 &&
        env_entries[i][name_len] == '=') {
      return i;
    }
  }
  return -1;
}

static int env_find_entry(const char* entry, size_t name_len) {
  int i;

  for (i = 0; i < CRT_ENV_MAX; ++i) {
    if (env_entries[i] != 0 &&
        strncmp(env_entries[i], entry, name_len) == 0 &&
        env_entries[i][name_len] == '=') {
      return i;
    }
  }
  return -1;
}

static int env_store_entry(const char* entry, int overwrite) {
  size_t length;
  size_t name_len;
  char* copy;
  int index;
  int i;

  if (entry == 0 || entry[0] == '=' || strchr(entry, '=') == 0) {
    return 0;
  }
  name_len = env_name_len(entry);
  index = env_find_entry(entry, name_len);
  if (index >= 0 && !overwrite) {
    return 0;
  }
  if (index < 0) {
    for (i = 0; i < CRT_ENV_MAX - 1; ++i) {
      if (env_entries[i] == 0) {
        index = i;
        break;
      }
    }
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

void __crt_env_init(char** envp) {
  char** cursor;

  if (env_initialized) {
    return;
  }
  env_initialized = 1;
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

int setenv(const char* name, const char* value, int overwrite) {
  size_t name_len;
  size_t value_len;
  char* entry;
  int index;
  int i;

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
    for (i = 0; i < CRT_ENV_MAX - 1; ++i) {
      if (env_entries[i] == 0) {
        index = i;
        break;
      }
    }
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
  int i;

  __crt_env_init(0);
  if (!env_name_valid(name)) {
    errno = EINVAL;
    return -1;
  }
  name_len = env_name_len(name);
  index = env_find(name, name_len);
  if (index >= 0) {
    free(env_entries[index]);
    for (i = index; i < CRT_ENV_MAX - 1; ++i) {
      env_entries[i] = env_entries[i + 1];
      if (env_entries[i] == 0) {
        break;
      }
    }
    env_entries[CRT_ENV_MAX - 1] = 0;
  }
  return 0;
}
