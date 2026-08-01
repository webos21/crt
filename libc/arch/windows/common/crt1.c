#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <private/crt_fd_table.h>

#if defined(_M_IX86) || defined(__i386__)
#define CRT_WINAPI __stdcall
#else
#define CRT_WINAPI
#endif

#define CRT_ARG_MAX 256
#define CRT_COMMAND_LINE_MAX 8192

__declspec(dllimport) char* CRT_WINAPI GetCommandLineA(void);

int main(int argc, char** argv, char** envp);
void __crt_env_set_initial(char** envp);
void __crt_rootfs_bootstrap(int argc, char** argv);
void exit(int status);

static char command_line_storage[CRT_COMMAND_LINE_MAX];
static char* argv_storage[CRT_ARG_MAX];

static int command_line_space(int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int parse_windows_command_line(char* command_line, char** argv, int max_args) {
  int argc = 0;
  char* read = command_line;
  char* write = command_line;

  while (*read != 0 && argc + 1 < max_args) {
    int in_quotes = 0;

    while (command_line_space((unsigned char)*read)) {
      ++read;
    }
    if (*read == 0) {
      break;
    }
    argv[argc++] = write;
    while (*read != 0) {
      if (*read == '"') {
        in_quotes = !in_quotes;
        ++read;
        continue;
      }
      if (!in_quotes && command_line_space((unsigned char)*read)) {
        ++read;
        break;
      }
      if (*read == '\\' && read[1] == '"') {
        *write++ = '"';
        read += 2;
        continue;
      }
      *write++ = *read++;
    }
    *write++ = 0;
  }
  argv[argc] = 0;
  return argc;
}

void mainCRTStartup(void) {
  char* command_line = GetCommandLineA();
  int argc = 0;

  __crt_env_set_initial(0);
  __crt_child_bootstrap();
  if (command_line != 0) {
    size_t length = strlen(command_line);

    if (length >= sizeof(command_line_storage)) {
      length = sizeof(command_line_storage) - 1;
    }
    memcpy(command_line_storage, command_line, length);
    command_line_storage[length] = 0;
    argc = parse_windows_command_line(command_line_storage, argv_storage, CRT_ARG_MAX);
  }
  if (argc == 0) {
    argv_storage[0] = "";
    argv_storage[1] = 0;
    argc = 1;
  }
  __crt_rootfs_bootstrap(argc, argv_storage);
  exit(main(argc, argv_storage, environ));
}
