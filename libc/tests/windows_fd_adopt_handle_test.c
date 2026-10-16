/* __crt_windows_fd_adopt_handle(): an open kernel HANDLE becomes a CRT descriptor and then rides what a
 * descriptor rides -- here SCM_RIGHTS between two processes' worth of table (a socketpair in one process
 * and a spawned child), fcntl(FD_CLOEXEC) and close(). The first user is Web Tranche 9B, whose D3D12 shared
 * texture and fences are NT handles; an event stands in for them (any handle DuplicateHandle accepts).
 *
 * Windows-only PAL hook: on other hosts the test prints "skipped" and succeeds. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) || defined(__linux__)
int main(void) {
  printf("windows_fd_adopt_handle_test: skipped (Windows PAL hook)\n");
  return 0;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
uintptr_t __crt_windows_fd_get_handle(int fd);
int __crt_windows_fd_adopt_handle(uintptr_t handle);
__declspec(dllimport) void* __stdcall CreateEventA(void*, int, int, const char*);
__declspec(dllimport) int __stdcall SetEvent(void*);
__declspec(dllimport) int __stdcall ResetEvent(void*);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void*, unsigned long);
__declspec(dllimport) int __stdcall CloseHandle(void*);
__declspec(dllimport) int __stdcall GetHandleInformation(void*, unsigned long*);

static int failures;
#define CHECK(condition, message)                                                                \
  do {                                                                                           \
    if (!(condition)) {                                                                          \
      fprintf(stderr, "windows_fd_adopt_handle_test: %s (line %d, errno %d)\n", message, __LINE__, errno); \
      ++failures;                                                                                \
    }                                                                                            \
  } while (0)

static int send_fd(int channel, int fd) {
  char byte = 'x';
  char control[CMSG_SPACE(sizeof(int))];
  struct iovec iov = {&byte, 1};
  struct msghdr msg;
  struct cmsghdr* cm;

  memset(&msg, 0, sizeof(msg));
  memset(control, 0, sizeof(control));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);
  cm = CMSG_FIRSTHDR(&msg);
  cm->cmsg_level = SOL_SOCKET;
  cm->cmsg_type = SCM_RIGHTS;
  cm->cmsg_len = CMSG_LEN(sizeof(int));
  memcpy(CMSG_DATA(cm), &fd, sizeof(int));
  return sendmsg(channel, &msg, 0) == 1 ? 0 : -1;
}

static int receive_fd(int channel) {
  char byte;
  char control[CMSG_SPACE(sizeof(int))];
  struct iovec iov = {&byte, 1};
  struct msghdr msg;
  struct cmsghdr* cm;
  int fd = -1;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);
  if (recvmsg(channel, &msg, 0) != 1 || (cm = CMSG_FIRSTHDR(&msg)) == 0 || cm->cmsg_type != SCM_RIGHTS) return -1;
  memcpy(&fd, CMSG_DATA(cm), sizeof(int));
  return fd;
}

/* The child: takes the descriptor, signals the event behind it, and answers. */
static int run_child(int channel) {
  int fd = receive_fd(channel);
  void* handle;

  if (fd < 0) return 50;
  handle = (void*)__crt_windows_fd_get_handle(fd);
  if (!handle || !SetEvent(handle)) return 51;
  return close(fd) == 0 ? 0 : 52;
}

int main(int argc, char** argv) {
  void* event;
  int fd, channel[2], received, status = 0;
  pid_t pid;
  char number[16];
  char* child_argv[4];
  unsigned long flags = 0;

  if (argc > 2 && strcmp(argv[1], "child") == 0) return run_child(atoi(argv[2]));

  event = CreateEventA(0, 1 /* manual reset */, 0, 0);
  CHECK(event != 0, "create an event");
  CHECK(__crt_windows_fd_adopt_handle(0) == -1 && errno == EINVAL, "a null handle is refused");
  fd = __crt_windows_fd_adopt_handle((uintptr_t)event);
  CHECK(fd >= 3, "adopt a kernel handle as a descriptor");
  CHECK(__crt_windows_fd_get_handle(fd) == (uintptr_t)event, "the descriptor maps back to the same handle");
  CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 && (fcntl(fd, F_GETFD) & FD_CLOEXEC), "close-on-exec is settable");

  /* In-process: SCM_RIGHTS gives a second descriptor on the same kernel object. */
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, channel) == 0, "socketpair");
  CHECK(send_fd(channel[0], fd) == 0, "send the adopted descriptor");
  received = receive_fd(channel[1]);
  CHECK(received >= 0 && received != fd, "receive a duplicate descriptor");
  CHECK(WaitForSingleObject(event, 0) != 0, "the event is not signaled yet");
  CHECK(SetEvent((void*)__crt_windows_fd_get_handle(received)), "signal through the received descriptor");
  CHECK(WaitForSingleObject(event, 0) == 0, "the original handle sees the signal: one object");
  ResetEvent(event);
  CHECK(close(received) == 0 && GetHandleInformation(event, &flags), "closing a duplicate keeps the original open");

  /* Another process: the spawned child gets the descriptor over a second pair. */
  snprintf(number, sizeof(number), "%d", channel[1]);
  child_argv[0] = argv[0];
  child_argv[1] = (char*)"child";
  child_argv[2] = number;
  child_argv[3] = 0;
  CHECK(posix_spawn(&pid, argv[0], 0, 0, child_argv, environ) == 0, "start the child");
  CHECK(send_fd(channel[0], fd) == 0, "send the adopted descriptor to the child");
  CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0, "the child used the descriptor");
  CHECK(WaitForSingleObject(event, 5000) == 0, "the child's SetEvent reached this process's handle");

  CHECK(close(fd) == 0, "close the adopted descriptor");
  CHECK(!GetHandleInformation(event, &flags), "close() closed the kernel handle");
  close(channel[0]);
  close(channel[1]);
  if (failures) {
    fprintf(stderr, "windows_fd_adopt_handle_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("windows_fd_adopt_handle_test: ok\n");
  return 0;
}
#endif
