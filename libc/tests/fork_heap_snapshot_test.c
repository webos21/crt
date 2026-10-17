/* Own the real allocator in this fixture so a worker can hold its private
 * lock deterministically, without adding test hooks to production malloc.
 * Resetting an inherited lock alone must not permit a snapshot mid-mutation. */
#include "../src/malloc.c"
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(CRT_TARGET_OS_WINDOWS)
int main(void) {
  puts("fork_heap_snapshot_test: skipped (Windows memory-copy fork)");
  return 0;
}
#else
static crt_atomic_int holder_ready, release_holder, caller_ready, begin_call, attempting;
static int report_fds[2], records[2], callback_report = -1;
static int prepares, parents, children, allocate_in_prepare;
static int use_spawn, call_error, child_status;
static char executable[4096];
static posix_spawn_file_actions_t actions;

static void callback_allocate(void) {
  void* p = malloc(123);
  if (p == 0) _exit(90);
  memset(p, 0x5a, 123);
  free(p);
}
static void prepare(void) {
  ++prepares;
  if (allocate_in_prepare) callback_allocate();
}
static void parent(void) {
  ++parents;
  callback_allocate();
}
static void child(void) {
  ++children;
  if (callback_report >= 0 && write(callback_report, "f", 1) != 1) _exit(91);
  callback_allocate();
}
static void* hold_heap(void* ignored) {
  (void)ignored;
  crt_spin_lock(&heap_lock);
  crt_atomic_store_release(&holder_ready, 1);
  while (!crt_atomic_load_acquire(&release_holder)) sched_yield();
  crt_spin_unlock(&heap_lock);
  return 0;
}
static void* create_child(void* ignored) {
  pid_t pid = -1;
  (void)ignored;
  crt_atomic_store_release(&caller_ready, 1);
  while (!crt_atomic_load_acquire(&begin_call)) sched_yield();
  crt_atomic_store_release(&attempting, 1);
  if (use_spawn) {
    char* args[] = {executable, "child", 0};
    call_error = posix_spawn(&pid, executable, &actions, 0, args, environ);
  } else {
    pid = fork();
    if (pid == 0) {
      callback_allocate();
      _exit(children == 1 ? 7 : 92);
    }
    call_error = pid < 0 ? errno : 0;
  }
  if (call_error == 0 && waitpid(pid, &child_status, 0) != pid) call_error = ECHILD;
  return 0;
}
static int frozen_heap_case(int spawn) {
  pthread_t holder, caller;
  struct pollfd report;
  int early, before_prepares = prepares, before_parents = parents;
  char byte;

  use_spawn = spawn;
  call_error = child_status = 0;
  crt_atomic_store_release(&holder_ready, 0);
  crt_atomic_store_release(&release_holder, 0);
  crt_atomic_store_release(&caller_ready, 0);
  crt_atomic_store_release(&begin_call, 0);
  crt_atomic_store_release(&attempting, 0);
  if (pipe(report_fds) != 0) return 0;
  callback_report = report_fds[1];
  if (spawn) {
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, records) != 0
        || posix_spawn_file_actions_init(&actions) != 0
        || posix_spawn_file_actions_adddup2(&actions, report_fds[1], 77) != 0
        || posix_spawn_file_actions_adddup2(&actions, records[1], 78) != 0
        || posix_spawn_file_actions_addclose(&actions, records[0]) != 0
        || posix_spawn_file_actions_addclose(&actions, records[1]) != 0
        || posix_spawn_file_actions_addclose(&actions, report_fds[0]) != 0
        || posix_spawn_file_actions_addclose(&actions, report_fds[1]) != 0
        || write(records[0], "heap snapshot", 13) != 13) return 0;
  }
  /* Start the caller first: its native thread/TLS initialization may allocate. */
  if (pthread_create(&caller, 0, create_child, 0) != 0) return 0;
  while (!crt_atomic_load_acquire(&caller_ready)) sched_yield();
  if (pthread_create(&holder, 0, hold_heap, 0) != 0) return 0;
  while (!crt_atomic_load_acquire(&holder_ready)) sched_yield();
  crt_atomic_store_release(&begin_call, 1);
  while (!crt_atomic_load_acquire(&attempting)) sched_yield();
  report.fd = report_fds[0]; report.events = POLLIN; report.revents = 0;
  early = poll(&report, 1, 1000);
  crt_atomic_store_release(&release_holder, 1);
  if (pthread_join(holder, 0) != 0 || pthread_join(caller, 0) != 0) return 0;
  report.revents = 0;
  if (early == 0) early = poll(&report, 1, 1000) == 1 ? 0 : -1;
  if (call_error != 0 || early < 0 || read(report_fds[0], &byte, 1) != 1) {
    fprintf(stderr, "fork_heap_snapshot_test: %s child report (error=%d, status=%d)\n",
            spawn ? "spawn" : "fork", call_error, child_status);
    return 0;
  }
  close(report_fds[0]); close(report_fds[1]); callback_report = -1;
  if (spawn) {
    posix_spawn_file_actions_destroy(&actions);
    close(records[0]); close(records[1]);
  }
  if (early != 0) {
    fprintf(stderr, "fork_heap_snapshot_test: %s copied a heap critical section\n", spawn ? "spawn" : "fork");
    return 0;
  }
  return call_error == 0 && WIFEXITED(child_status) && WEXITSTATUS(child_status) == 7
      && byte == (spawn ? 's' : 'f')
      && prepares == before_prepares + !spawn && parents == before_parents + !spawn;
}
int main(int argc, char** argv) {
  ssize_t length;
  pid_t pid;
  int status;
  if (argc == 2 && strcmp(argv[1], "child") == 0) {
    char payload[32];
    callback_allocate();
    if (read(78, payload, sizeof payload) != 13 || memcmp(payload, "heap snapshot", 13)
        || write(77, "s", 1) != 1) return 93;
    return 7;
  }
  length = readlink("/proc/self/exe", executable, sizeof executable - 1);
  if (length <= 0) return 1;
  executable[length] = 0;
  if (pthread_atfork(prepare, parent, child) != 0 || !frozen_heap_case(0) || !frozen_heap_case(1)) return 1;
  /* User prepare/parent/child callbacks must remain free to use malloc. */
  allocate_in_prepare = 1;
  pid = fork();
  if (pid == 0) _exit(children == 1 ? 0 : 94);
  if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
  puts("fork_heap_snapshot_test: ok");
  return 0;
}
#endif
