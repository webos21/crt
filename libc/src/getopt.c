#include <getopt.h>
#include <stdio.h>
#include <string.h>

char* optarg;
int optind = 1;
int opterr = 1;
int optopt;

static char* getopt_next;

static const char* short_options_start(const char* optstring) {
  while (*optstring == '+' || *optstring == '-' || *optstring == ':') {
    ++optstring;
  }
  return optstring;
}

static const char* find_short_option(int option, const char* optstring) {
  const char* p = short_options_start(optstring);

  for (; *p != 0; ++p) {
    if (*p == ':') {
      continue;
    }
    if ((unsigned char)*p == (unsigned char)option) {
      return p;
    }
  }
  return 0;
}

static int option_error(const char* argv0, const char* message, int option) {
  if (opterr != 0 && argv0 != 0) {
    if (option != 0) {
      fprintf(stderr, "%s: %s -- %c\n", argv0, message, option);
    } else {
      fprintf(stderr, "%s: %s\n", argv0, message);
    }
  }
  return '?';
}

static int long_option_error(const char* argv0, const char* message, const char* name) {
  if (opterr != 0 && argv0 != 0) {
    fprintf(stderr, "%s: %s -- %s\n", argv0, message, name);
  }
  return '?';
}

static int match_long_option(
    const struct option* longopts,
    const char* name,
    size_t name_len,
    int* ambiguous) {
  int found = -1;
  int i;

  *ambiguous = 0;
  if (longopts == 0) {
    return -1;
  }
  for (i = 0; longopts[i].name != 0; ++i) {
    if (strncmp(longopts[i].name, name, name_len) == 0) {
      if (longopts[i].name[name_len] == 0) {
        *ambiguous = 0;
        return i;
      }
      if (found >= 0) {
        *ambiguous = 1;
      }
      found = i;
    }
  }
  return *ambiguous ? -1 : found;
}

static int parse_long_option(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int* longindex,
    const char* name) {
  const char* value = strchr(name, '=');
  size_t name_len = value != 0 ? (size_t)(value - name) : strlen(name);
  int ambiguous = 0;
  int index = match_long_option(longopts, name, name_len, &ambiguous);
  const struct option* option;

  (void)optstring;
  if (ambiguous) {
    ++optind;
    return long_option_error(argv[0], "option is ambiguous", name);
  }
  if (index < 0) {
    ++optind;
    return long_option_error(argv[0], "unrecognized option", name);
  }

  option = &longopts[index];
  if (longindex != 0) {
    *longindex = index;
  }
  optarg = 0;
  if (value != 0) {
    ++value;
    if (option->has_arg == no_argument) {
      ++optind;
      return long_option_error(argv[0], "option doesn't allow an argument", name);
    }
    optarg = (char*)value;
  } else if (option->has_arg == required_argument) {
    if (optind + 1 >= argc) {
      ++optind;
      optopt = option->val;
      return optstring != 0 && optstring[0] == ':' ? ':' :
          long_option_error(argv[0], "option requires an argument", name);
    }
    optarg = argv[++optind];
  }

  ++optind;
  getopt_next = 0;
  if (option->flag != 0) {
    *option->flag = option->val;
    return 0;
  }
  return option->val;
}

static int should_try_long_only(
    const char* arg,
    const char* optstring,
    const struct option* longopts) {
  int ambiguous = 0;
  size_t len;
  const char* value;

  if (longopts == 0 || arg[0] != '-' || arg[1] == '-' || arg[1] == 0) {
    return 0;
  }
  if (find_short_option((unsigned char)arg[1], optstring) != 0 && arg[2] == 0) {
    return 0;
  }
  value = strchr(arg + 1, '=');
  len = value != 0 ? (size_t)(value - (arg + 1)) : strlen(arg + 1);
  return match_long_option(longopts, arg + 1, len, &ambiguous) >= 0 && !ambiguous;
}

static int long_option_requires_separate_arg(
    const char* name,
    const struct option* longopts) {
  const char* value = strchr(name, '=');
  size_t name_len = value != 0 ? (size_t)(value - name) : strlen(name);
  int ambiguous = 0;
  int index;

  if (value != 0) {
    return 0;
  }
  index = match_long_option(longopts, name, name_len, &ambiguous);
  return index >= 0 && !ambiguous && longopts[index].has_arg == required_argument;
}

static int short_option_requires_separate_arg(const char* arg, const char* optstring) {
  const char* short_opt;

  if (arg[0] != '-' || arg[1] == '-' || arg[1] == 0) {
    return 0;
  }
  short_opt = find_short_option((unsigned char)arg[1], optstring);
  if (short_opt == 0 || short_opt[1] != ':' || short_opt[2] == ':') {
    return 0;
  }
  return arg[2] == 0;
}

static int option_group_length(
    int argc,
    char* const argv[],
    int index,
    const char* optstring,
    const struct option* longopts,
    int long_only) {
  char* arg = argv[index];

  if (strcmp(arg, "--") == 0) {
    return 1;
  }
  if (arg[0] == '-' && arg[1] == '-' && longopts != 0) {
    return long_option_requires_separate_arg(arg + 2, longopts) && index + 1 < argc ? 2 : 1;
  }
  if (long_only && should_try_long_only(arg, optstring, longopts)) {
    return long_option_requires_separate_arg(arg + 1, longopts) && index + 1 < argc ? 2 : 1;
  }
  return short_option_requires_separate_arg(arg, optstring) && index + 1 < argc ? 2 : 1;
}

static int permute_next_option(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int long_only) {
  int scan;
  int group_len;
  int i;
  char* saved[2];
  char** mutable_argv = (char**)argv;

  if (optstring[0] == '+') {
    return 0;
  }
  for (scan = optind + 1; scan < argc; ++scan) {
    char* arg = argv[scan];

    if (arg == 0 || arg[0] != '-' || arg[1] == 0) {
      continue;
    }
    if (strcmp(arg, "--") == 0 ||
        arg[1] == '-' ||
        long_only ||
        find_short_option((unsigned char)arg[1], optstring) != 0) {
      group_len = option_group_length(argc, argv, scan, optstring, longopts, long_only);
      for (i = 0; i < group_len; ++i) {
        saved[i] = argv[scan + i];
      }
      memmove(
          &mutable_argv[optind + group_len],
          &mutable_argv[optind],
          (size_t)(scan - optind) * sizeof(mutable_argv[0]));
      for (i = 0; i < group_len; ++i) {
        mutable_argv[optind + i] = saved[i];
      }
      return 1;
    }
  }
  return 0;
}

static int getopt_internal(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int* longindex,
    int long_only) {
  const char* short_opt;
  int option;

  if (optind <= 0) {
    optind = 1;
    getopt_next = 0;
  }
  optarg = 0;
  if (optstring == 0) {
    optstring = "";
  }

  if (getopt_next == 0 || *getopt_next == 0) {
    char* arg;

    if (optind >= argc) {
      return -1;
    }
    arg = argv[optind];
    if (arg == 0 || arg[0] != '-' || arg[1] == 0) {
      if (!permute_next_option(argc, argv, optstring, longopts, long_only)) {
        return -1;
      }
      arg = argv[optind];
    }
    if (strcmp(arg, "--") == 0) {
      ++optind;
      return -1;
    }
    if (arg[1] == '-' && longopts != 0) {
      return parse_long_option(argc, argv, optstring, longopts, longindex, arg + 2);
    }
    if (long_only && should_try_long_only(arg, optstring, longopts)) {
      return parse_long_option(argc, argv, optstring, longopts, longindex, arg + 1);
    }
    getopt_next = arg + 1;
  }

  option = (unsigned char)*getopt_next++;
  short_opt = find_short_option(option, optstring);
  if (short_opt == 0) {
    optopt = option;
    if (*getopt_next == 0) {
      ++optind;
      getopt_next = 0;
    }
    return option_error(argv[0], "invalid option", option);
  }

  if (short_opt[1] == ':') {
    if (*getopt_next != 0) {
      optarg = getopt_next;
      ++optind;
      getopt_next = 0;
    } else if (short_opt[2] == ':') {
      optarg = 0;
      ++optind;
      getopt_next = 0;
    } else if (optind + 1 < argc) {
      optarg = argv[++optind];
      ++optind;
      getopt_next = 0;
    } else {
      optopt = option;
      ++optind;
      getopt_next = 0;
      return optstring[0] == ':' ? ':' :
          option_error(argv[0], "option requires an argument", option);
    }
  } else if (*getopt_next == 0) {
    ++optind;
    getopt_next = 0;
  }
  return option;
}

int getopt(int argc, char* const argv[], const char* optstring) {
  return getopt_internal(argc, argv, optstring, 0, 0, 0);
}

int getopt_long(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int* longindex) {
  return getopt_internal(argc, argv, optstring, longopts, longindex, 0);
}

int getopt_long_only(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int* longindex) {
  return getopt_internal(argc, argv, optstring, longopts, longindex, 1);
}
