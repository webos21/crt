#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <private/crt_fd_table.h>
#include <private/crt_shell_process.h>

static int fail(const char* message) {
  fprintf(stderr, "windows_fd_snapshot_test: %s\n", message);
  return 1;
}

#if defined(CRT_TARGET_OS_WINDOWS)
static int parse_fd_arg(const char* text) {
  int value = 0;

  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text - '0');
    ++text;
  }
  return *text == 0 ? value : -1;
}

static void format_fd_arg(int fd, char* buffer, size_t size) {
  char digits[16];
  size_t count = 0;
  size_t pos = 0;

  if (size == 0) {
    return;
  }
  do {
    digits[count++] = (char)('0' + (fd % 10));
    fd /= 10;
  } while (fd != 0 && count < sizeof(digits));
  while (count > 0 && pos + 1 < size) {
    buffer[pos++] = digits[--count];
  }
  buffer[pos] = 0;
}
#endif

int main(int argc, char** argv) {
  struct crt_fd_snapshot snapshot;

#if defined(CRT_TARGET_OS_WINDOWS)
  int pipefd[2];
  char out = 'x';
  char in = 0;
  char encoded[8192];
  struct crt_fd_snapshot decoded;

  if (argc == 3 && strcmp(argv[1], "child") == 0) {
    int child_fd = parse_fd_arg(argv[2]);

    if (child_fd < 0 || write(child_fd, "b", 1) != 1) {
      return 77;
    }
    return 9;
  }
  if (argc == 2 && strcmp(argv[1], "action-child") == 0) {
    if (write(7, "c", 1) != 1) {
      return 78;
    }
    return 10;
  }
  if (argc == 3 && strcmp(argv[1], "cloexec-child") == 0) {
    int child_fd = parse_fd_arg(argv[2]);

    errno = 0;
    if (child_fd < 0 || write(child_fd, "d", 1) != -1 || errno != EBADF) {
      return 79;
    }
    return 11;
  }
  if (argc == 3 && strcmp(argv[1], "socket-child") == 0) {
    int child_fd = parse_fd_arg(argv[2]);
    char byte = 0;

    if (child_fd < 0 ||
        send(child_fd, "s", 1, 0) != 1 ||
        recv(child_fd, &byte, 1, 0) != 1 ||
        byte != 'p') {
      return 80;
    }
    return 12;
  }
  if (argc == 3 && strcmp(argv[1], "exit-child") == 0) {
    int code = parse_fd_arg(argv[2]);

    return code >= 0 ? code : 81;
  }
  if (argc == 2 && strcmp(argv[1], "argv0-child") == 0) {
    return strcmp(argv[0], "toybox-applet") == 0 ? 13 : 82;
  }
  if (argc == 2 && strcmp(argv[1], "copy-stdio-child") == 0) {
    char byte = 0;

    if (read(0, &byte, 1) != 1) {
      return 83;
    }
    if (write(1, &byte, 1) != 1) {
      return 84;
    }
    return 14;
  }

  memset(&snapshot, 0, sizeof(snapshot));
  if (pipe(pipefd) != 0) {
    return fail("pipe");
  }
  if (__crt_fd_snapshot_export(&snapshot) != 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("export");
  }
  if (snapshot.magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot.version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot.count == 0 ||
      snapshot.capacity != CRT_FD_SNAPSHOT_MAX) {
    __crt_fd_snapshot_dispose(&snapshot);
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("snapshot header");
  }
  if (__crt_fd_snapshot_encode(&snapshot, encoded, sizeof(encoded)) != 0 ||
      __crt_fd_snapshot_decode(encoded, &decoded) != 0 ||
      decoded.magic != CRT_FD_SNAPSHOT_MAGIC ||
      decoded.version != CRT_FD_SNAPSHOT_VERSION ||
      decoded.count != snapshot.count) {
    __crt_fd_snapshot_dispose(&snapshot);
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("snapshot codec");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  if (__crt_fd_snapshot_import(&snapshot) != 0) {
    __crt_fd_snapshot_dispose(&snapshot);
    return fail("import");
  }
  __crt_fd_snapshot_dispose(&snapshot);
  if (write(pipefd[1], &out, 1) != 1 ||
      read(pipefd[0], &in, 1) != 1 ||
      in != out) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("roundtrip fd");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  {
    int input_pipe[2];
    int output_pipe[2];
#if !defined(CRT_TARGET_OS_WINDOWS)
    pid_t fork_pid;
    int status = 0;
    char byte = 0;
#endif

    if (pipe(input_pipe) != 0 || pipe(output_pipe) != 0) {
      return fail("fork exec pipe setup");
    }
#if defined(CRT_TARGET_OS_WINDOWS)
    close(input_pipe[0]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
#else
    fork_pid = fork();
    if (fork_pid < 0 && errno == ENOTSUP) {
      close(input_pipe[0]);
      close(input_pipe[1]);
      close(output_pipe[0]);
      close(output_pipe[1]);
    } else if (fork_pid < 0) {
      close(input_pipe[0]);
      close(input_pipe[1]);
      close(output_pipe[0]);
      close(output_pipe[1]);
      return fail("fork exec fork");
    } else if (fork_pid == 0) {
      char* child_argv[] = {"/proc/self/exe", "copy-stdio-child", 0};

      close(input_pipe[1]);
      close(output_pipe[0]);
      if (dup2(input_pipe[0], 0) != 0 ||
          dup2(output_pipe[1], 1) != 1) {
        _exit(85);
      }
      close(input_pipe[0]);
      close(output_pipe[1]);
      execve("/proc/self/exe", child_argv, environ);
      _exit(errno);
    } else {
      int io_failed = 0;

      close(input_pipe[0]);
      close(output_pipe[1]);
      if (write(input_pipe[1], "z", 1) != 1 ||
          close(input_pipe[1]) != 0 ||
          read(output_pipe[0], &byte, 1) != 1 ||
          byte != 'z') {
        io_failed = 1;
      }
      close(output_pipe[0]);
      if (waitpid(fork_pid, &status, 0) != fork_pid ||
          !WIFEXITED(status) ||
          WEXITSTATUS(status) != 14) {
        fprintf(stderr, "windows_fd_snapshot_test: fork exec status=%d exited=%d code=%d\n",
                status, WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return fail("fork exec pipe wait");
      }
      if (io_failed) {
        return fail("fork exec pipe io");
      }
    }
#endif
  }
  {
    pid_t pid;
    int status = 0;
    char* child_argv[] = {"toybox-applet", "argv0-child", 0};

    if (posix_spawn(&pid, "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
      return fail("argv0 spawn");
    }
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 13) {
      return fail("argv0 wait");
    }
  }
  {
    pid_t pid;
    int status = 0;
    posix_spawnattr_t attr;
    sigset_t mask;
    char* child_argv[] = {"toybox-applet", "argv0-child", 0};

    if (posix_spawnattr_init(&attr) != 0 ||
        sigemptyset(&mask) != 0 ||
        posix_spawnattr_setsigmask(&attr, &mask) != 0 ||
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_RESETIDS) != 0) {
      return fail("spawn attr setup");
    }
    if (posix_spawn(&pid, "/proc/self/exe", 0, &attr, child_argv, environ) != 0) {
      posix_spawnattr_destroy(&attr);
      return fail("spawn attr");
    }
    posix_spawnattr_destroy(&attr);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 13) {
      return fail("spawn attr wait");
    }
  }
  if (pipe(pipefd) != 0) {
    return fail("transport pipe");
  }
  {
    pid_t pid;
    int status = 0;
    char fd_arg[16];
    char* child_argv[] = {"/proc/self/exe", "child", fd_arg, 0};

    format_fd_arg(pipefd[1], fd_arg, sizeof(fd_arg));
    if (posix_spawn(&pid, "/proc/self/exe", 0, 0, child_argv, environ) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("transport spawn");
    }
    close(pipefd[1]);
    in = 0;
    if (read(pipefd[0], &in, 1) != 1 || in != 'b') {
      close(pipefd[0]);
      return fail("transport read");
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 9) {
      return fail("transport wait");
    }
  }
  if (pipe(pipefd) != 0) {
    return fail("action pipe");
  }
  {
    pid_t pid;
    int status = 0;
    posix_spawn_file_actions_t actions;
    char* child_argv[] = {"/proc/self/exe", "action-child", 0};

    if (posix_spawn_file_actions_init(&actions) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, pipefd[1], 7) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("action setup");
    }
    if (posix_spawn(&pid, "/proc/self/exe", &actions, 0, child_argv, environ) != 0) {
      posix_spawn_file_actions_destroy(&actions);
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("action spawn");
    }
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    in = 0;
    if (read(pipefd[0], &in, 1) != 1 || in != 'c') {
      close(pipefd[0]);
      return fail("action read");
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 10) {
      return fail("action wait");
    }
  }
  if (pipe(pipefd) != 0) {
    return fail("cloexec pipe");
  }
  {
    pid_t pid;
    int status = 0;
    char fd_arg[16];
    char* child_argv[] = {"/proc/self/exe", "cloexec-child", fd_arg, 0};

    format_fd_arg(pipefd[1], fd_arg, sizeof(fd_arg));
    if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) != 0 ||
        fcntl(pipefd[1], F_GETFD) != FD_CLOEXEC) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("cloexec fcntl");
    }
    if (__crt_shell_fork_exec(&pid, "/proc/self/exe", 0, child_argv, environ) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return fail("cloexec shell fork exec");
    }
    close(pipefd[0]);
    close(pipefd[1]);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 11) {
      return fail("cloexec wait");
    }
  }
  {
    int server = -1;
    int client = -1;
    int accepted = -1;
    int yes = 1;
    struct sockaddr_in addr;
    struct sockaddr_in bound;
    socklen_t bound_len = sizeof(bound);
    pid_t pid;
    int status = 0;
    char fd_arg[16];
    char byte = 0;
    char* child_argv[] = {"/proc/self/exe", "socket-child", fd_arg, 0};

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
      return fail("socket server");
    }
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
      close(server);
      return fail("socket setsockopt");
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(server, 1) != 0) {
      close(server);
      return fail("socket listen");
    }
    memset(&bound, 0, sizeof(bound));
    if (getsockname(server, (struct sockaddr*)&bound, &bound_len) != 0 ||
        bound.sin_port == 0) {
      close(server);
      return fail("socket getsockname");
    }
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0 ||
        connect(client, (struct sockaddr*)&bound, sizeof(bound)) != 0) {
      if (client >= 0) {
        close(client);
      }
      close(server);
      return fail("socket connect");
    }
    accepted = accept(server, (struct sockaddr*)&addr, &bound_len);
    close(server);
    if (accepted < 0) {
      close(client);
      return fail("socket accept");
    }
    format_fd_arg(client, fd_arg, sizeof(fd_arg));
    if (__crt_shell_fork_exec(&pid, "/proc/self/exe", 0, child_argv, environ) != 0) {
      close(accepted);
      close(client);
      return fail("socket shell fork exec");
    }
    close(client);
    if (recv(accepted, &byte, 1, 0) != 1 ||
        byte != 's' ||
        send(accepted, "p", 1, 0) != 1) {
      close(accepted);
      return fail("socket inherited io");
    }
    close(accepted);
    if (waitpid(pid, &status, 0) != pid ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 12) {
      return fail("socket wait");
    }
  }
  {
    enum { CHILD_COUNT = 5 };
    pid_t pids[CHILD_COUNT];
    int seen[CHILD_COUNT];
    int i;

    memset(seen, 0, sizeof(seen));
    for (i = 0; i < CHILD_COUNT; ++i) {
      char code_arg[16];
      char* child_argv[] = {"/proc/self/exe", "exit-child", code_arg, 0};

      format_fd_arg(20 + i, code_arg, sizeof(code_arg));
      if (__crt_shell_fork_exec(&pids[i], "/proc/self/exe", 0, child_argv, environ) != 0) {
        return fail("multi child spawn");
      }
    }
    for (i = 0; i < CHILD_COUNT; ++i) {
      int status = 0;
      pid_t got = waitpid(-1, &status, 0);
      int slot;

      if (got < 0 || !WIFEXITED(status)) {
        return fail("multi child waitpid");
      }
      for (slot = 0; slot < CHILD_COUNT; ++slot) {
        if (pids[slot] == got) {
          if (seen[slot] || WEXITSTATUS(status) != 20 + slot) {
            return fail("multi child status");
          }
          seen[slot] = 1;
          break;
        }
      }
      if (slot == CHILD_COUNT) {
        return fail("multi child unknown pid");
      }
    }
    for (i = 0; i < CHILD_COUNT; ++i) {
      if (!seen[i]) {
        return fail("multi child missing pid");
      }
    }
    errno = 0;
    if (waitpid(-1, 0, WNOHANG) != -1 || errno != ECHILD) {
      return fail("multi child drained");
    }
  }
#else
  (void)argc;
  (void)argv;
  memset(&snapshot, 0, sizeof(snapshot));
  if (__crt_fd_snapshot_export(&snapshot) != ENOTSUP ||
      snapshot.magic != CRT_FD_SNAPSHOT_MAGIC ||
      snapshot.version != CRT_FD_SNAPSHOT_VERSION ||
      snapshot.capacity != CRT_FD_SNAPSHOT_MAX) {
    return fail("non-windows export policy");
  }
  if (__crt_fd_snapshot_import(&snapshot) != ENOTSUP) {
    return fail("non-windows import policy");
  }
  __crt_fd_snapshot_dispose(&snapshot);
#endif

  puts("windows_fd_snapshot_test: ok");
  return 0;
}
