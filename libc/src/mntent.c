#include <errno.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>

static int is_space_char(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char* skip_spaces(char* s) {
  while (*s != 0 && is_space_char((unsigned char)*s)) {
    ++s;
  }
  return s;
}

static char* next_field(char** cursor) {
  char* start;
  char* end;

  if (cursor == 0 || *cursor == 0) {
    return 0;
  }
  start = skip_spaces(*cursor);
  if (*start == 0 || *start == '#') {
    *cursor = start;
    return 0;
  }
  end = start;
  while (*end != 0 && !is_space_char((unsigned char)*end)) {
    ++end;
  }
  if (*end != 0) {
    *end++ = 0;
  }
  *cursor = end;
  return start;
}

static int parse_int_field(char** cursor) {
  char* field = next_field(cursor);
  char* end = 0;
  long value;

  if (field == 0) {
    return 0;
  }
  value = strtol(field, &end, 10);
  if (end == field) {
    return 0;
  }
  return (int)value;
}

FILE* setmntent(const char* filename, const char* type) {
  return fopen(filename, type);
}

int endmntent(FILE* fp) {
  if (fp != 0) {
    fclose(fp);
  }
  return 1;
}

struct mntent* getmntent_r(FILE* fp, struct mntent* entry, char* buf, int size) {
  if (fp == 0 || entry == 0 || buf == 0 || size <= 0) {
    errno = EINVAL;
    return 0;
  }

  while (fgets(buf, size, fp) != 0) {
    char* cursor = buf;
    char* first = next_field(&cursor);

    if (first == 0) {
      continue;
    }
    entry->mnt_fsname = first;
    entry->mnt_dir = next_field(&cursor);
    entry->mnt_type = next_field(&cursor);
    entry->mnt_opts = next_field(&cursor);
    if (entry->mnt_dir == 0 || entry->mnt_type == 0 || entry->mnt_opts == 0) {
      continue;
    }
    entry->mnt_freq = parse_int_field(&cursor);
    entry->mnt_passno = parse_int_field(&cursor);
    return entry;
  }
  return 0;
}

struct mntent* getmntent(FILE* fp) {
  static struct mntent entry;
  static char buffer[4096];

  return getmntent_r(fp, &entry, buffer, sizeof(buffer));
}

char* hasmntopt(const struct mntent* entry, const char* option) {
  char* opts;
  size_t option_len;

  if (entry == 0 || entry->mnt_opts == 0 || option == 0) {
    return 0;
  }
  opts = entry->mnt_opts;
  option_len = strlen(option);
  while (*opts != 0) {
    char* end = opts;
    size_t len;

    while (*end != 0 && *end != ',') {
      ++end;
    }
    len = (size_t)(end - opts);
    if (len == option_len && strncmp(opts, option, option_len) == 0) {
      return opts;
    }
    opts = *end == ',' ? end + 1 : end;
  }
  return 0;
}
