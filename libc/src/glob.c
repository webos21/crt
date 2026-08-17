#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CRT_GLOB_MAX_COMPONENTS 64

struct crt_glob_state {
  int flags;
  int (*errfunc)(const char*, int);
  int aborted;
  char** results;
  size_t results_count;
  size_t results_capacity;
};

static int crt_glob_has_magic(const char* s) {
  for (; *s != 0; ++s) {
    if (*s == '*' || *s == '?' || *s == '[') {
      return 1;
    }
  }
  return 0;
}

static int crt_glob_is_dir(const char* path) {
  struct stat st;

  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void crt_glob_join(char* out, size_t out_size, const char* base, const char* name) {
  if (base[0] == 0) {
    snprintf(out, out_size, "%s", name);
  } else if (strcmp(base, "/") == 0) {
    snprintf(out, out_size, "/%s", name);
  } else {
    snprintf(out, out_size, "%s/%s", base, name);
  }
}

static int crt_glob_add(struct crt_glob_state* state, const char* path) {
  if (state->results_count == state->results_capacity) {
    size_t new_capacity = state->results_capacity == 0 ? 16 : state->results_capacity * 2;
    char** grown = (char**)realloc(state->results, new_capacity * sizeof(char*));

    if (grown == 0) {
      return -1;
    }
    state->results = grown;
    state->results_capacity = new_capacity;
  }
  state->results[state->results_count] = strdup(path);
  if (state->results[state->results_count] == 0) {
    return -1;
  }
  ++state->results_count;
  return 0;
}

static void crt_glob_add_match(struct crt_glob_state* state, const char* path) {
  char marked[PATH_MAX];
  const char* final_path = path;

  if ((state->flags & GLOB_MARK) != 0 && crt_glob_is_dir(path)) {
    snprintf(marked, sizeof(marked), "%s/", path);
    final_path = marked;
  }
  if (crt_glob_add(state, final_path) != 0) {
    state->aborted = 1;
  }
}

static void crt_glob_walk(
    struct crt_glob_state* state, const char* base, char** components, int count,
    int must_be_dir_if_last);

static void crt_glob_match_dir(
    struct crt_glob_state* state, const char* dir_path, const char* component,
    char** remaining_components, int remaining_count, int must_be_dir_if_last) {
  DIR* dh;
  struct dirent* entry;
  int fnmatch_flags = (state->flags & GLOB_NOESCAPE) != 0 ? FNM_NOESCAPE : 0;

  dh = opendir(dir_path[0] != 0 ? dir_path : ".");
  if (dh == 0) {
    if (state->errfunc != 0) {
      if (state->errfunc(dir_path, errno) != 0) {
        state->aborted = 1;
      }
    }
    if ((state->flags & GLOB_ERR) != 0) {
      state->aborted = 1;
    }
    return;
  }

  while (!state->aborted && (entry = readdir(dh)) != 0) {
    const char* name = entry->d_name;
    char joined[PATH_MAX];

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    if (name[0] == '.' && component[0] != '.') {
      continue;
    }
    if (fnmatch(component, name, fnmatch_flags) != 0) {
      continue;
    }
    crt_glob_join(joined, sizeof(joined), dir_path, name);
    if (remaining_count == 0) {
      if (must_be_dir_if_last == 0 || crt_glob_is_dir(joined)) {
        crt_glob_add_match(state, joined);
      }
    } else if (crt_glob_is_dir(joined)) {
      crt_glob_walk(state, joined, remaining_components, remaining_count, must_be_dir_if_last);
    }
  }
  closedir(dh);
}

static void crt_glob_walk(
    struct crt_glob_state* state, const char* base, char** components, int count,
    int must_be_dir_if_last) {
  const char* component;
  char joined[PATH_MAX];

  if (state->aborted || count == 0) {
    return;
  }
  component = components[0];
  if (!crt_glob_has_magic(component)) {
    struct stat st;

    crt_glob_join(joined, sizeof(joined), base, component);
    if (count == 1) {
      if (must_be_dir_if_last != 0) {
        if (crt_glob_is_dir(joined)) {
          crt_glob_add_match(state, joined);
        }
      } else if (lstat(joined, &st) == 0) {
        crt_glob_add_match(state, joined);
      }
    } else if (stat(joined, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        crt_glob_walk(state, joined, components + 1, count - 1, must_be_dir_if_last);
      }
    } else {
      /* A literal (non-wildcard) path segment that can't be stat()'d --
       * report it the same way a failed opendir() on a wildcard segment
       * is reported below, for consistency. */
      if (state->errfunc != 0 && state->errfunc(joined, errno) != 0) {
        state->aborted = 1;
      }
      if ((state->flags & GLOB_ERR) != 0) {
        state->aborted = 1;
      }
    }
    return;
  }
  crt_glob_match_dir(state, base, component, components + 1, count - 1, must_be_dir_if_last);
}

static int crt_glob_compare(const void* a, const void* b) {
  const char* const* pa = (const char* const*)a;
  const char* const* pb = (const char* const*)b;

  return strcmp(*pa, *pb);
}

int glob(
    const char* pattern, int flags, int (*errfunc)(const char* epath, int eerrno),
    glob_t* pglob) {
  struct crt_glob_state state;
  char* components[CRT_GLOB_MAX_COMPONENTS];
  int component_count = 0;
  char* pattern_copy;
  char* token;
  int is_absolute;
  int trailing_slash;
  size_t existing_count;
  char** existing;
  size_t offs;
  size_t i;

  if (pattern == 0 || pglob == 0) {
    return GLOB_NOSYS;
  }

  is_absolute = pattern[0] == '/';
  trailing_slash = pattern[0] != 0 && pattern[strlen(pattern) - 1] == '/';

  pattern_copy = strdup(pattern);
  if (pattern_copy == 0) {
    return GLOB_NOSPACE;
  }
  {
    char* saveptr = 0;

    token = strtok_r(pattern_copy, "/", &saveptr);
    while (token != 0 && component_count < CRT_GLOB_MAX_COMPONENTS) {
      components[component_count++] = token;
      token = strtok_r(0, "/", &saveptr);
    }
  }

  state.flags = flags;
  state.errfunc = errfunc;
  state.aborted = 0;
  state.results = 0;
  state.results_count = 0;
  state.results_capacity = 0;

  if (component_count == 0) {
    if (is_absolute && crt_glob_is_dir("/")) {
      crt_glob_add(&state, "/");
    }
  } else {
    crt_glob_walk(&state, is_absolute ? "/" : "", components, component_count, trailing_slash);
  }
  free(pattern_copy);

  if (state.aborted) {
    for (i = 0; i < state.results_count; ++i) {
      free(state.results[i]);
    }
    free(state.results);
    return GLOB_ABORTED;
  }

  if (state.results_count == 0 && (flags & GLOB_NOCHECK) != 0) {
    if (crt_glob_add(&state, pattern) != 0) {
      free(state.results);
      return GLOB_NOSPACE;
    }
  }

  if ((flags & GLOB_NOSORT) == 0 && state.results_count > 1) {
    qsort(state.results, state.results_count, sizeof(char*), crt_glob_compare);
  }

  if ((flags & GLOB_APPEND) == 0 && (flags & GLOB_DOOFFS) == 0) {
    pglob->gl_offs = 0;
  }
  existing_count = 0;
  existing = 0;
  if ((flags & GLOB_APPEND) != 0 && pglob->gl_pathv != 0) {
    existing_count = pglob->gl_pathc;
    existing = pglob->gl_pathv;
  }
  offs = (flags & GLOB_DOOFFS) != 0 ? pglob->gl_offs : 0;

  {
    size_t new_total = offs + existing_count + state.results_count;
    char** new_pathv = (char**)malloc((new_total + 1) * sizeof(char*));

    if (new_pathv == 0) {
      for (i = 0; i < state.results_count; ++i) {
        free(state.results[i]);
      }
      free(state.results);
      return GLOB_NOSPACE;
    }
    for (i = 0; i < offs; ++i) {
      new_pathv[i] = 0;
    }
    if (existing != 0) {
      for (i = 0; i < existing_count; ++i) {
        new_pathv[offs + i] = existing[offs + i];
      }
      free(existing);
    }
    for (i = 0; i < state.results_count; ++i) {
      new_pathv[offs + existing_count + i] = state.results[i];
    }
    new_pathv[new_total] = 0;
    free(state.results);

    pglob->gl_pathv = new_pathv;
    pglob->gl_pathc = existing_count + state.results_count;
  }

  if (state.results_count == 0 && (flags & GLOB_NOCHECK) == 0) {
    return GLOB_NOMATCH;
  }
  return 0;
}

void globfree(glob_t* pglob) {
  size_t i;

  if (pglob == 0 || pglob->gl_pathv == 0) {
    return;
  }
  for (i = 0; i < pglob->gl_pathc; ++i) {
    free(pglob->gl_pathv[pglob->gl_offs + i]);
  }
  free(pglob->gl_pathv);
  pglob->gl_pathv = 0;
  pglob->gl_pathc = 0;
}
