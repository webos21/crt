#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <unistd.h>

/* sendfile must copy on every host: libc++'s filesystem copy_file chooses it whenever the header exists. */
#define SIZE 200003
static unsigned char data[SIZE];
static unsigned char copy[SIZE];

static int read_all(const char* path) {
  int fd = open(path, O_RDONLY);
  ssize_t got = 0, step;
  if (fd < 0) return -1;
  while (got < SIZE && (step = read(fd, copy + got, SIZE - got)) > 0) got += step;
  close(fd);
  return (int)got;
}

int main(void) {
  const char* source = "sendfile_test_source.bin";
  const char* target = "sendfile_test_target.bin";
  int in, out;
  off_t offset = 1000;
  ssize_t copied;
  for (int i = 0; i < SIZE; ++i) data[i] = (unsigned char)(i * 31 + i / 251);
  in = open(source, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (in < 0 || write(in, data, SIZE) != SIZE) return 1;
  close(in);

  in = open(source, O_RDONLY);
  out = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (in < 0 || out < 0) return 2;
  copied = sendfile(out, in, 0, SIZE);
  close(in); close(out);
  if (copied != SIZE || read_all(target) != SIZE || memcmp(copy, data, SIZE) != 0) return 3;

  in = open(source, O_RDONLY);
  out = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  copied = sendfile(out, in, &offset, 70000);
  if (copied != 70000 || offset != 71000 || lseek(in, 0, SEEK_CUR) != 0) return 4;
  close(in); close(out);
  if (read_all(target) != 70000 || memcmp(copy, data + 1000, 70000) != 0) return 5;

  in = open(source, O_RDONLY);
  out = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  offset = SIZE - 10;
  copied = sendfile(out, in, &offset, 4096);
  close(in); close(out);
  if (copied != 10 || offset != SIZE) return 6;
  if (sendfile(-1, -1, 0, 1) != -1) return 7;
  unlink(source); unlink(target);
  puts("sendfile_test: ok");
  return 0;
}
