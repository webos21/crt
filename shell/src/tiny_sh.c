#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <private/crt_shell_process.h>

#define TINY_MAX_TOKENS 128
#define TINY_MAX_ARGS 32
#define TINY_MAX_COMMANDS 8
#define TINY_MAX_REDIRS 8
#define TINY_CONNECT_NONE 0
#define TINY_CONNECT_ALWAYS 1
#define TINY_CONNECT_AND 2
#define TINY_CONNECT_OR 3

struct tiny_redir {
  int fd;
  int flags;
  mode_t mode;
  const char* path;
};

struct tiny_command {
  char* argv[TINY_MAX_ARGS];
  int argc;
  struct tiny_redir redirs[TINY_MAX_REDIRS];
  int redir_count;
};

static const char* tiny_self_path;
static int tiny_last_status;

static int tiny_fail(const char* message) {
  fprintf(stderr, "sh: %s\n", message);
  return 2;
}

static int tiny_write_all(int fd, const char* text) {
  size_t offset = 0;
  size_t length = strlen(text);

  while (offset < length) {
    ssize_t written = write(fd, text + offset, length - offset);

    if (written <= 0) {
      return -1;
    }
    offset += (size_t)written;
  }
  return 0;
}

static int tiny_copy_fd(int input, int output) {
  char buffer[1024];

  for (;;) {
    ssize_t got = read(input, buffer, sizeof(buffer));

    if (got < 0) {
      return 1;
    }
    if (got == 0) {
      return 0;
    }
    if (write(output, buffer, (size_t)got) != got) {
      return 1;
    }
  }
}

static int tiny_builtin_echo(int argc, char** argv) {
  int i;

  for (i = 1; i < argc; ++i) {
    if (i != 1 && tiny_write_all(1, " ") != 0) {
      return 1;
    }
    if (tiny_write_all(1, argv[i]) != 0) {
      return 1;
    }
  }
  return tiny_write_all(1, "\n") == 0 ? 0 : 1;
}

static int tiny_builtin_cat(int argc, char** argv) {
  int i;

  if (argc == 1) {
    return tiny_copy_fd(0, 1);
  }
  for (i = 1; i < argc; ++i) {
    int fd = open(argv[i], O_RDONLY, 0);
    int result;

    if (fd < 0) {
      perror(argv[i]);
      return 1;
    }
    result = tiny_copy_fd(fd, 1);
    close(fd);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

static int tiny_builtin_upper(void) {
  char ch;

  while (read(0, &ch, 1) == 1) {
    ch = (char)toupper((unsigned char)ch);
    if (write(1, &ch, 1) != 1) {
      return 1;
    }
  }
  return 0;
}

static int tiny_run_builtin(int argc, char** argv) {
  char cwd[1024];

  if (argc == 0 || argv[0] == 0) {
    return 0;
  }
  if (strcmp(argv[0], "echo") == 0) {
    return tiny_builtin_echo(argc, argv);
  }
  if (strcmp(argv[0], "cat") == 0) {
    return tiny_builtin_cat(argc, argv);
  }
  if (strcmp(argv[0], "upper") == 0) {
    return tiny_builtin_upper();
  }
  if (strcmp(argv[0], "pwd") == 0) {
    if (getcwd(cwd, sizeof(cwd)) == 0) {
      perror("pwd");
      return 1;
    }
    return tiny_write_all(1, cwd) == 0 && tiny_write_all(1, "\n") == 0 ? 0 : 1;
  }
  if (strcmp(argv[0], "cd") == 0) {
    const char* path = argc >= 2 ? argv[1] : getenv("HOME");

    if (path == 0) {
      path = "/";
    }
    if (chdir(path) != 0) {
      perror("cd");
      return 1;
    }
    return 0;
  }
  if (strcmp(argv[0], "true") == 0) {
    return 0;
  }
  if (strcmp(argv[0], "false") == 0) {
    return 1;
  }
  if (strcmp(argv[0], "exit") == 0) {
    return argc >= 2 ? atoi(argv[1]) : 0;
  }
  return -1;
}

static int tiny_is_builtin(const char* name) {
  return name != 0 &&
         (strcmp(name, "echo") == 0 ||
          strcmp(name, "cat") == 0 ||
          strcmp(name, "upper") == 0 ||
          strcmp(name, "pwd") == 0 ||
          strcmp(name, "cd") == 0 ||
          strcmp(name, "true") == 0 ||
          strcmp(name, "false") == 0 ||
          strcmp(name, "exit") == 0);
}

static int tiny_is_name_start(int c) {
  return isalpha((unsigned char)c) || c == '_';
}

static int tiny_is_name_char(int c) {
  return isalnum((unsigned char)c) || c == '_';
}

static int tiny_is_assignment(const char* token) {
  const char* equal;
  const char* p;

  if (token == 0 || !tiny_is_name_start((unsigned char)token[0])) {
    return 0;
  }
  equal = strchr(token, '=');
  if (equal == 0 || equal == token) {
    return 0;
  }
  for (p = token; p < equal; ++p) {
    if (!tiny_is_name_char((unsigned char)*p)) {
      return 0;
    }
  }
  return 1;
}

static int tiny_set_assignment(const char* token) {
  const char* equal = strchr(token, '=');
  char* name;
  int result;

  if (equal == 0) {
    return -1;
  }
  name = (char*)malloc((size_t)(equal - token) + 1);
  if (name == 0) {
    return -1;
  }
  memcpy(name, token, (size_t)(equal - token));
  name[equal - token] = 0;
  result = setenv(name, equal + 1, 1);
  free(name);
  return result;
}

static char* tiny_expand_token(const char* input) {
  char* output;
  size_t size;
  size_t pos = 0;
  size_t i;

  if (input == 0) {
    return 0;
  }
  size = strlen(input) + 32;
  output = (char*)malloc(size);
  if (output == 0) {
    return 0;
  }
  for (i = 0; input[i] != 0; ++i) {
    const char* value = 0;
    char status_buffer[16];
    char name_buffer[128];
    size_t value_len;

    if (input[i] != '$') {
      if (pos + 2 >= size) {
        char* grown = (char*)realloc(output, size * 2);

        if (grown == 0) {
          free(output);
          return 0;
        }
        output = grown;
        size *= 2;
      }
      output[pos++] = input[i];
      continue;
    }
    if (input[i + 1] == '?') {
      snprintf(status_buffer, sizeof(status_buffer), "%d", tiny_last_status);
      value = status_buffer;
      ++i;
    } else if (tiny_is_name_start((unsigned char)input[i + 1])) {
      size_t name_len = 0;

      ++i;
      while (tiny_is_name_char((unsigned char)input[i]) && name_len + 1 < sizeof(name_buffer)) {
        name_buffer[name_len++] = input[i++];
      }
      --i;
      name_buffer[name_len] = 0;
      value = getenv(name_buffer);
      if (value == 0) {
        value = "";
      }
    } else {
      value = "$";
    }
    value_len = strlen(value);
    while (pos + value_len + 1 >= size) {
      char* grown = (char*)realloc(output, size * 2);

      if (grown == 0) {
        free(output);
        return 0;
      }
      output = grown;
      size *= 2;
    }
    memcpy(output + pos, value, value_len);
    pos += value_len;
  }
  output[pos] = 0;
  return output;
}

static void tiny_free_tokens(char** tokens, int token_count) {
  int i;

  for (i = 0; i < token_count; ++i) {
    free(tokens[i]);
  }
}

static int tiny_tokenize(char* script, char** tokens, int max_tokens) {
  int count = 0;
  char* read = script;

  while (*read != 0 && count + 1 < max_tokens) {
    while (isspace((unsigned char)*read)) {
      ++read;
    }
    if (*read == 0) {
      break;
    }
    if (read[0] == '&' && read[1] == '&') {
      char* token = (char*)malloc(3);

      if (token == 0) {
        return -1;
      }
      token[0] = *read++;
      token[1] = *read++;
      token[2] = 0;
      tokens[count++] = token;
      continue;
    }
    if (read[0] == '|' && read[1] == '|') {
      char* token = (char*)malloc(3);

      if (token == 0) {
        return -1;
      }
      token[0] = *read++;
      token[1] = *read++;
      token[2] = 0;
      tokens[count++] = token;
      continue;
    }
    if (read[0] >= '0' && read[0] <= '9' && read[1] == '>') {
      char* token = (char*)malloc(3);

      if (token == 0) {
        return -1;
      }
      token[0] = *read++;
      token[1] = *read++;
      token[2] = 0;
      tokens[count++] = token;
      continue;
    }
    if (*read == ';' || *read == '|' || *read == '<' || *read == '>') {
      char* token = (char*)malloc(2);

      if (token == 0) {
        return -1;
      }
      token[0] = *read++;
      token[1] = 0;
      tokens[count++] = token;
      continue;
    }
    {
      char* token = (char*)malloc(strlen(read) + 1);
      char* write = token;

      if (token == 0) {
        return -1;
      }
      while (*read != 0 && !isspace((unsigned char)*read) &&
             *read != ';' && *read != '|' && *read != '<' && *read != '>') {
        if (*read == '\'' || *read == '"') {
          int quote = *read++;

          while (*read != 0 && *read != quote) {
            *write++ = *read++;
          }
          if (*read == quote) {
            ++read;
          }
        } else if (*read == '\\' && read[1] != 0) {
          ++read;
          *write++ = *read++;
        } else {
          *write++ = *read++;
        }
      }
      *write = 0;
      tokens[count++] = token;
    }
  }
  tokens[count] = 0;
  return count;
}

static int tiny_add_redir(struct tiny_command* command, int fd, int flags, mode_t mode, const char* path) {
  struct tiny_redir* redir;

  if (command->redir_count == TINY_MAX_REDIRS) {
    return -1;
  }
  redir = &command->redirs[command->redir_count++];
  redir->fd = fd;
  redir->flags = flags;
  redir->mode = mode;
  redir->path = path;
  return 0;
}

static int tiny_parse(char** tokens, int token_count, struct tiny_command* commands, int* command_count) {
  int i;
  struct tiny_command* command = &commands[0];

  memset(commands, 0, sizeof(commands[0]) * TINY_MAX_COMMANDS);
  *command_count = 1;
  for (i = 0; i < token_count; ++i) {
    char* token = tokens[i];

    if (strcmp(token, "|") == 0) {
      if (command->argc == 0 || *command_count == TINY_MAX_COMMANDS) {
        return -1;
      }
      command = &commands[(*command_count)++];
      continue;
    }
    if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
      int output = token[0] == '>';

      if (i + 1 == token_count) {
        return -1;
      }
      char* path = tiny_expand_token(tokens[++i]);

      if (path == 0 ||
          tiny_add_redir(
              command,
              output ? 1 : 0,
              output ? O_CREAT | O_WRONLY | O_TRUNC : O_RDONLY,
              0666,
              path) != 0) {
        return -1;
      }
      continue;
    }
    if (strlen(token) == 2 && token[0] >= '0' && token[0] <= '9' && token[1] == '>') {
      char* path;

      if (i + 1 == token_count ||
          (path = tiny_expand_token(tokens[++i])) == 0 ||
          tiny_add_redir(command, token[0] - '0', O_CREAT | O_WRONLY | O_TRUNC, 0666, path) != 0) {
        return -1;
      }
      continue;
    }
    if (command->argc + 1 == TINY_MAX_ARGS) {
      return -1;
    }
    command->argv[command->argc] = tiny_expand_token(token);
    if (command->argv[command->argc] == 0) {
      return -1;
    }
    ++command->argc;
    command->argv[command->argc] = 0;
  }
  return commands[*command_count - 1].argc == 0 ? -1 : 0;
}

static int tiny_apply_assignments(struct tiny_command* command) {
  int count = 0;
  int i;

  while (count < command->argc && tiny_is_assignment(command->argv[count])) {
    if (tiny_set_assignment(command->argv[count]) != 0) {
      return -1;
    }
    ++count;
  }
  if (count == 0) {
    return 0;
  }
  for (i = count; i <= command->argc; ++i) {
    command->argv[i - count] = command->argv[i];
  }
  command->argc -= count;
  return 0;
}

static int tiny_apply_redirs_in_parent(const struct tiny_command* command, int saved[TINY_MAX_REDIRS]) {
  int i;

  for (i = 0; i < command->redir_count; ++i) {
    int fd = open(command->redirs[i].path, command->redirs[i].flags, command->redirs[i].mode);

    saved[i] = -1;
    if (fd < 0) {
      perror(command->redirs[i].path);
      return -1;
    }
    saved[i] = dup(command->redirs[i].fd);
    if (dup2(fd, command->redirs[i].fd) < 0) {
      close(fd);
      return -1;
    }
    close(fd);
  }
  return 0;
}

static void tiny_restore_redirs_in_parent(const struct tiny_command* command, int saved[TINY_MAX_REDIRS]) {
  int i;

  for (i = command->redir_count - 1; i >= 0; --i) {
    if (saved[i] >= 0) {
      dup2(saved[i], command->redirs[i].fd);
      close(saved[i]);
    }
  }
}

static int tiny_add_command_redirs(posix_spawn_file_actions_t* actions, const struct tiny_command* command) {
  int i;

  for (i = 0; i < command->redir_count; ++i) {
    int result = posix_spawn_file_actions_addopen(
        actions,
        command->redirs[i].fd,
        command->redirs[i].path,
        command->redirs[i].flags,
        command->redirs[i].mode);

    if (result != 0) {
      return result;
    }
  }
  return 0;
}

static int tiny_wait_for(pid_t pid) {
  int status = 0;

  if (waitpid(pid, &status, 0) != pid) {
    return 127;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return 127;
}

static int tiny_spawn_command(
    const struct tiny_command* command,
    int input_fd,
    int output_fd,
    pid_t* pid) {
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_t* actions_ptr = 0;
  int result;

  result = posix_spawn_file_actions_init(&actions);
  if (result != 0) {
    return result;
  }
  actions_ptr = &actions;
  if (input_fd >= 0 &&
      (posix_spawn_file_actions_adddup2(&actions, input_fd, 0) != 0 ||
       posix_spawn_file_actions_addclose(&actions, input_fd) != 0)) {
    posix_spawn_file_actions_destroy(&actions);
    return EIO;
  }
  if (output_fd >= 0 &&
      (posix_spawn_file_actions_adddup2(&actions, output_fd, 1) != 0 ||
       posix_spawn_file_actions_addclose(&actions, output_fd) != 0)) {
    posix_spawn_file_actions_destroy(&actions);
    return EIO;
  }
  result = tiny_add_command_redirs(&actions, command);
  if (result != 0) {
    posix_spawn_file_actions_destroy(&actions);
    return result;
  }
  if (tiny_is_builtin(command->argv[0])) {
    char* child_argv[TINY_MAX_ARGS + 2];
    struct crt_shell_child_spec spec;
    int i;

    child_argv[0] = (char*)tiny_self_path;
    child_argv[1] = "--builtin";
    for (i = 0; i < command->argc; ++i) {
      child_argv[i + 2] = command->argv[i];
    }
    child_argv[command->argc + 2] = 0;
    memset(&spec, 0, sizeof(spec));
    spec.path = tiny_self_path;
    spec.argv = child_argv;
    spec.envp = environ;
    spec.file_actions = actions_ptr;
    spec.flags = CRT_SHELL_CHILD_FLUSH_STDIO;
    result = __crt_shell_spawn(pid, &spec);
  } else {
    result = posix_spawnp(pid, command->argv[0], actions_ptr, 0, command->argv, environ);
  }
  posix_spawn_file_actions_destroy(&actions);
  return result;
}

static int tiny_run_single(struct tiny_command* command) {
  int result;
  int saved[TINY_MAX_REDIRS];

  if (tiny_apply_assignments(command) != 0) {
    return 1;
  }
  if (command->argc == 0) {
    return 0;
  }
  if (strcmp(command->argv[0], "exec") == 0) {
    if (command->argc == 1) {
      return 0;
    }
    execve(command->argv[1], &command->argv[1], environ);
    return 127;
  }
  if (tiny_is_builtin(command->argv[0])) {
    if (tiny_apply_redirs_in_parent(command, saved) != 0) {
      return 1;
    }
    result = tiny_run_builtin(command->argc, command->argv);
    tiny_restore_redirs_in_parent(command, saved);
    return result;
  }
  {
    pid_t pid;

    result = tiny_spawn_command(command, -1, -1, &pid);
    if (result != 0) {
      errno = result;
      perror(command->argv[0]);
      return 127;
    }
    return tiny_wait_for(pid);
  }
}

static int tiny_run_commands(struct tiny_command* commands, int command_count) {
  pid_t pids[TINY_MAX_COMMANDS];
  int pid_count = 0;
  int previous_read = -1;
  int i;
  int last_status = 0;

  if (command_count == 1) {
    return tiny_run_single(&commands[0]);
  }
  for (i = 0; i < command_count; ++i) {
    int pipefd[2] = {-1, -1};
    int output_fd = -1;
    int result;

    if (tiny_apply_assignments(&commands[i]) != 0 || commands[i].argc == 0) {
      return 1;
    }
    if (i + 1 < command_count) {
      if (pipe(pipefd) != 0) {
        return tiny_fail("pipe");
      }
      output_fd = pipefd[1];
    }
    result = tiny_spawn_command(&commands[i], previous_read, output_fd, &pids[pid_count]);
    if (result != 0) {
      errno = result;
      perror(commands[i].argv[0]);
      if (previous_read >= 0) {
        close(previous_read);
      }
      if (pipefd[0] >= 0) {
        close(pipefd[0]);
      }
      if (pipefd[1] >= 0) {
        close(pipefd[1]);
      }
      return 127;
    }
    ++pid_count;
    if (previous_read >= 0) {
      close(previous_read);
    }
    if (output_fd >= 0) {
      close(output_fd);
    }
    previous_read = pipefd[0];
  }
  if (previous_read >= 0) {
    close(previous_read);
  }
  for (i = 0; i < pid_count; ++i) {
    int status = tiny_wait_for(pids[i]);

    if (i + 1 == pid_count) {
      last_status = status;
    }
  }
  return last_status;
}

static int tiny_run_script(char* script) {
  char* tokens[TINY_MAX_TOKENS];
  struct tiny_command commands[TINY_MAX_COMMANDS];
  int token_count = tiny_tokenize(script, tokens, TINY_MAX_TOKENS);
  int start = 0;
  int connector = TINY_CONNECT_ALWAYS;
  int status = 0;

  if (token_count < 0) {
    return tiny_fail("tokenize");
  }
  if (token_count == 0) {
    return 0;
  }
  while (start < token_count) {
    int end = start;
    int next_connector = TINY_CONNECT_NONE;
    int command_count = 0;

    while (end < token_count) {
      if (strcmp(tokens[end], ";") == 0) {
        next_connector = TINY_CONNECT_ALWAYS;
        break;
      }
      if (strcmp(tokens[end], "&&") == 0) {
        next_connector = TINY_CONNECT_AND;
        break;
      }
      if (strcmp(tokens[end], "||") == 0) {
        next_connector = TINY_CONNECT_OR;
        break;
      }
      ++end;
    }
    if (end == start) {
      tiny_free_tokens(tokens, token_count);
      return tiny_fail("syntax error");
    }
    if (connector == TINY_CONNECT_ALWAYS ||
        (connector == TINY_CONNECT_AND && status == 0) ||
        (connector == TINY_CONNECT_OR && status != 0)) {
      if (tiny_parse(&tokens[start], end - start, commands, &command_count) != 0) {
        tiny_free_tokens(tokens, token_count);
        return tiny_fail("syntax error");
      }
      status = tiny_run_commands(commands, command_count);
      tiny_last_status = status;
    }
    connector = next_connector;
    start = next_connector == TINY_CONNECT_NONE ? end : end + 1;
  }
  tiny_free_tokens(tokens, token_count);
  return status;
}

int main(int argc, char** argv) {
  tiny_self_path = argv != 0 && argv[0] != 0 ? argv[0] : "/system/bin/sh";
  if (argc >= 2 && strcmp(argv[1], "--builtin") == 0) {
    return tiny_run_builtin(argc - 2, &argv[2]);
  }
  if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
    char* script = strdup(argv[2]);
    int result;

    if (script == 0) {
      return 2;
    }
    result = tiny_run_script(script);
    free(script);
    return result;
  }
  if (argc >= 2) {
    return tiny_fail("only -c scripts are supported by crt tiny sh");
  }
  return 0;
}
