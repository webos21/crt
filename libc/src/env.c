#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CRT_ENV_MAX 64

static char* env_entries[CRT_ENV_MAX];

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

char* getenv(const char* name) {
  size_t name_len;
  int index;

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
    for (i = 0; i < CRT_ENV_MAX; ++i) {
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

  if (!env_name_valid(name)) {
    errno = EINVAL;
    return -1;
  }
  name_len = env_name_len(name);
  index = env_find(name, name_len);
  if (index >= 0) {
    free(env_entries[index]);
    env_entries[index] = 0;
  }
  return 0;
}
