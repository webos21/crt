#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "poll_select_test: %s\n", message);
  return 1;
}

int main(void) {
  int pipefd[2];
  struct pollfd pfd;
  fd_set readfds;
  struct timeval tv;
  struct timespec ts;
  char byte = 'Q';
  char readback = 0;
  int result;

  if (pipe(pipefd) != 0) {
    return fail("pipe");
  }

  pfd.fd = pipefd[0];
  pfd.events = POLLIN;
  pfd.revents = 0;
  result = poll(&pfd, 1, 0);
  if (result != 0 || pfd.revents != 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("poll empty pipe");
  }

  pfd.fd = pipefd[1];
  pfd.events = POLLOUT;
  pfd.revents = 0;
  result = poll(&pfd, 1, 0);
  if (result != 1 || (pfd.revents & POLLOUT) == 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("poll write pipe");
  }

  if (write(pipefd[1], &byte, 1) != 1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("write pipe");
  }

  pfd.fd = pipefd[0];
  pfd.events = POLLIN;
  pfd.revents = 0;
  result = poll(&pfd, 1, 100);
  if (result != 1 || (pfd.revents & POLLIN) == 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("poll read pipe");
  }

  FD_ZERO(&readfds);
  FD_SET(pipefd[0], &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  result = select(pipefd[0] + 1, &readfds, 0, 0, &tv);
  if (result != 1 || !FD_ISSET(pipefd[0], &readfds)) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("select read pipe");
  }

  if (read(pipefd[0], &readback, 1) != 1 || readback != 'Q') {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("read pipe");
  }

  FD_ZERO(&readfds);
  FD_SET(pipefd[0], &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  result = select(pipefd[0] + 1, &readfds, 0, 0, &tv);
  if (result != 0 || FD_ISSET(pipefd[0], &readfds)) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("select timeout");
  }

  FD_ZERO(&readfds);
  FD_SET(pipefd[0], &readfds);
  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  result = pselect(pipefd[0] + 1, &readfds, 0, 0, &ts, 0);
  if (result != 0 || FD_ISSET(pipefd[0], &readfds)) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("pselect timeout");
  }

  ts.tv_sec = 0;
  ts.tv_nsec = 1000000000L;
  errno = 0;
  result = pselect(0, 0, 0, 0, &ts, 0);
  if (result != -1 || errno != EINVAL) {
    close(pipefd[0]);
    close(pipefd[1]);
    return fail("pselect invalid timeout");
  }

  close(pipefd[0]);
  close(pipefd[1]);
  printf("poll_select_test: ok\n");
  return 0;
}
