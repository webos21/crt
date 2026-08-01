#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <unistd.h>

static int fail(const char* message) {
  fprintf(stderr, "file_path_test: %s\n", message);
  return 1;
}

static void cleanup_file_path_test_dir(void) {
  DIR* dir;
  struct dirent* entry;

  if (chdir("file_path_test.dir") != 0) {
    return;
  }
  (void)chmod("sample.tmp", 0600);
  (void)remove("sample.link");
  (void)remove("sample.tmp");
  (void)remove("created.tmp");
  (void)remove("mkstemp_test.XXXXXX");
  (void)remove("realpath_dir/nested.link");
  (void)rmdir("realpath_dir");
  dir = opendir(".");
  if (dir != 0) {
    while ((entry = readdir(dir)) != 0) {
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
        (void)chmod(entry->d_name, 0600);
        (void)remove(entry->d_name);
      }
    }
    (void)closedir(dir);
  }
  (void)chdir("..");
  (void)rmdir("file_path_test.dir");
}

int main(void) {
  char cwd[4096];
  char byte = 'Z';
  char readback = 0;
  char linkbuf[64];
  char mktemplate[] = "mkstemp_test.XXXXXX";
  char resolved[PATH_MAX];
  char* allocated_path;
  int fd;
  int copy;
  int high_copy;
  int found_sample = 0;
  int pipefd[2];
  int created_fd;
  int dir_open_fd;
  long page_size;
  mode_t old_mask;
  DIR* dir;
  struct dirent* entry;
  struct stat st;
  struct statfs sfs;
  struct flock lock;
  struct timeval tv[2];

  if (getcwd(cwd, sizeof(cwd)) == 0 || cwd[0] == 0) {
    return fail("getcwd");
  }
  cleanup_file_path_test_dir();
  if (mkdir("file_path_test.dir", 0777) != 0) {
    return fail("mkdir");
  }
  if (stat("file_path_test.dir", &st) != 0 || !S_ISDIR(st.st_mode)) {
    return fail("stat dir");
  }
  if (S_ISREG(st.st_mode) || S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
      S_ISFIFO(st.st_mode) || S_ISLNK(st.st_mode) || S_ISSOCK(st.st_mode)) {
    return fail("stat type macros");
  }
  if (chdir("file_path_test.dir") != 0) {
    return fail("chdir into");
  }
  fd = open("sample.tmp", O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0) {
    return fail("open sample");
  }
  if (write(fd, &byte, 1) != 1) {
    close(fd);
    return fail("write sample");
  }
  if (fsync(fd) != 0 || fdatasync(fd) != 0) {
    close(fd);
    return fail("fsync sample");
  }
  if (access("sample.tmp", F_OK | R_OK | W_OK) != 0) {
    close(fd);
    return fail("access sample");
  }
  if (fchmod(fd, 0600) != 0) {
    close(fd);
    return fail("fchmod sample");
  }
  if (chmod("sample.tmp", 0400) != 0) {
    close(fd);
    return fail("chmod readonly");
  }
  if (stat("sample.tmp", &st) != 0 ||
      (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0) {
    close(fd);
    return fail("chmod readonly mode");
  }
  if (chmod("sample.tmp", 0600) != 0 ||
      stat("sample.tmp", &st) != 0 ||
      (st.st_mode & S_IWUSR) == 0) {
    close(fd);
    return fail("chmod writable mode");
  }
  if (access("missing.tmp", F_OK) == 0) {
    close(fd);
    return fail("access missing");
  }
  old_mask = umask(0022);
  if (umask(old_mask) != 0022) {
    close(fd);
    return fail("umask");
  }
  high_copy = mkstemp(mktemplate);
  if (high_copy < 0 || strstr(mktemplate, "XXXXXX") != 0) {
    close(fd);
    return fail("mkstemp");
  }
  if (write(high_copy, &byte, 1) != 1) {
    close(high_copy);
    close(fd);
    return fail("write mkstemp");
  }
  close(high_copy);
  if (stat(mktemplate, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    return fail("stat mkstemp");
  }
  if (remove(mktemplate) != 0) {
    close(fd);
    return fail("remove mkstemp");
  }
  created_fd = creat("created.tmp", 0600);
  if (created_fd < 0) {
    close(fd);
    return fail("creat");
  }
  if (write(created_fd, "abcd", 4) != 4 ||
      ftruncate(created_fd, 2) != 0 ||
      fstat(created_fd, &st) != 0 ||
      st.st_size != 2) {
    close(created_fd);
    close(fd);
    return fail("ftruncate");
  }
  close(created_fd);
  if (truncate("created.tmp", 1) != 0 ||
      stat("created.tmp", &st) != 0 ||
      st.st_size != 1) {
    close(fd);
    return fail("truncate");
  }
  if (unlink("created.tmp") != 0 ||
      access("created.tmp", F_OK) == 0) {
    close(fd);
    return fail("unlink");
  }
  if (stat("sample.tmp", &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1) {
    close(fd);
    return fail("stat sample");
  }
  memset(&st, 0, sizeof(st));
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1 || st.st_nlink < 1) {
    close(fd);
    return fail("fstat sample");
  }
  byte = 'Y';
  readback = 0;
  if (pwrite(fd, &byte, 1, 0) != 1 ||
      pread(fd, &readback, 1, 0) != 1 ||
      readback != 'Y') {
    close(fd);
    return fail("pread pwrite");
  }
  memset(&sfs, 0, sizeof(sfs));
  if (statfs("sample.tmp", &sfs) != 0 ||
      sfs.f_bsize < 4096 ||
      sfs.f_namelen == 0) {
    close(fd);
    return fail("statfs sample");
  }
  memset(&sfs, 0, sizeof(sfs));
  if (fstatfs(fd, &sfs) != 0 ||
      sfs.f_bsize < 4096 ||
      sfs.f_namelen == 0) {
    close(fd);
    return fail("fstatfs sample");
  }
  page_size = sysconf(_SC_PAGESIZE);
  if (page_size < 4096 || (page_size & (page_size - 1)) != 0) {
    close(fd);
    return fail("sysconf pagesize");
  }
  tv[0].tv_sec = 1000;
  tv[0].tv_usec = 125000;
  tv[1].tv_sec = 1000;
  tv[1].tv_usec = 250000;
  if (utimes("sample.tmp", tv) != 0 ||
      stat("sample.tmp", &st) != 0 ||
      st.st_mtime != 1000) {
    close(fd);
    return fail("utimes timestamp");
  }
  tv[0].tv_sec = 1001;
  tv[0].tv_usec = 125000;
  tv[1].tv_sec = 1001;
  tv[1].tv_usec = 250000;
  if (futimes(fd, tv) != 0 ||
      fstat(fd, &st) != 0 ||
      st.st_mtime != 1001) {
    close(fd);
    return fail("futimes timestamp");
  }
  if (geteuid() == (uid_t)-1) {
    close(fd);
    return fail("geteuid");
  }
  if (fchown(fd, geteuid(), (gid_t)-1) != 0) {
    close(fd);
    return fail("fchown");
  }
#if defined(CRT_TARGET_OS_WINDOWS)
  if (fstat(fd, &st) != 0 || st.st_uid != geteuid()) {
    close(fd);
    return fail("windows synthetic uid");
  }
#endif
  if (isatty(fd) != 0) {
    close(fd);
    return fail("isatty regular");
  }
  if (fcntl(fd, F_GETFD) != 0 ||
      fcntl(fd, F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(fd, F_SETFL, O_NONBLOCK) != 0 ||
      fcntl(fd, F_GETFL) < 0) {
    close(fd);
    return fail("fcntl flags");
  }
  memset(&lock, 0, sizeof(lock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 1;
  if (fcntl(fd, F_SETLK, &lock) != 0) {
    close(fd);
    return fail("fcntl setlk");
  }
  lock.l_type = F_UNLCK;
  if (fcntl(fd, F_SETLK, &lock) != 0) {
    close(fd);
    return fail("fcntl unlock");
  }
  lock.l_type = F_RDLCK;
  if (fcntl(fd, F_GETLK, &lock) != 0 ||
      (lock.l_type != F_UNLCK && lock.l_type != F_RDLCK && lock.l_type != F_WRLCK)) {
    close(fd);
    return fail("fcntl getlk");
  }
  memset(&st, 0, sizeof(st));
  if (lstat("sample.tmp", &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 1) {
    close(fd);
    return fail("lstat sample");
  }
  if (realpath("./sample.tmp", resolved) == 0 || strstr(resolved, "sample.tmp") == 0) {
    close(fd);
    return fail("realpath buffer");
  }
  dir = opendir(".");
  if (dir == 0) {
    close(fd);
    return fail("opendir");
  }
  dir_open_fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dir_open_fd < 0) {
    closedir(dir);
    close(fd);
    return fail("open directory");
  }
  if (fstat(dir_open_fd, &st) != 0) {
    close(dir_open_fd);
    closedir(dir);
    close(fd);
    return fail("fstat directory");
  }
  if (!S_ISDIR(st.st_mode)) {
    closedir(dir);
    close(dir_open_fd);
    close(fd);
    return fail("directory mode");
  }
  close(dir_open_fd);
  if (dirfd(dir) < 0) {
    closedir(dir);
    close(fd);
    return fail("dirfd");
  }
  if (fstat(dirfd(dir), &st) != 0 || !S_ISDIR(st.st_mode)) {
    closedir(dir);
    close(fd);
    return fail("dirfd fstat");
  }
#if !defined(CRT_TARGET_OS_WINDOWS)
  memset(&st, 0, sizeof(st));
  if (fstatat(dirfd(dir), "sample.tmp", &st, 0) != 0 ||
      !S_ISREG(st.st_mode) ||
      st.st_size != 1 ||
      st.st_blksize == 0 ||
      st.st_dev == 0 ||
      st.st_ino == 0) {
    closedir(dir);
    close(fd);
    return fail("dirfd fstatat metadata");
  }
#endif
  while ((entry = readdir(dir)) != 0) {
    if (strcmp(entry->d_name, "sample.tmp") == 0) {
      found_sample = 1;
      if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_REG) {
        closedir(dir);
        close(fd);
        return fail("readdir type");
      }
    }
  }
  rewinddir(dir);
  found_sample = 0;
  while ((entry = readdir(dir)) != 0) {
    if (strcmp(entry->d_name, "sample.tmp") == 0) {
      found_sample = 1;
      break;
    }
  }
  if (closedir(dir) != 0 || !found_sample) {
    close(fd);
    return fail("rewinddir sample");
  }
  allocated_path = realpath("sample.tmp", 0);
  if (allocated_path == 0 || strstr(allocated_path, "sample.tmp") == 0) {
    free(allocated_path);
    close(fd);
    return fail("realpath alloc");
  }
  free(allocated_path);
#if defined(CRT_TARGET_OS_WINDOWS)
  errno = 0;
  if (symlink("sample.tmp", "sample.link") == 0 || errno != ENOSYS) {
    close(fd);
    return fail("windows symlink policy");
  }
  errno = 0;
  if (readlink("sample.link", linkbuf, sizeof(linkbuf)) >= 0 || errno != ENOSYS) {
    close(fd);
    return fail("windows readlink policy");
  }
#else
  if (symlink("sample.tmp", "sample.link") != 0) {
    close(fd);
    return fail("symlink");
  }
  memset(linkbuf, 0, sizeof(linkbuf));
  if (readlink("sample.link", linkbuf, sizeof(linkbuf) - 1) != 10 ||
      strcmp(linkbuf, "sample.tmp") != 0) {
    close(fd);
    return fail("readlink");
  }
  memset(&st, 0, sizeof(st));
  if (lstat("sample.link", &st) != 0 || !S_ISLNK(st.st_mode)) {
    close(fd);
    return fail("lstat symlink");
  }
  memset(resolved, 0, sizeof(resolved));
  if (realpath("sample.link", resolved) == 0 ||
      strstr(resolved, "sample.tmp") == 0 ||
      strstr(resolved, "sample.link") != 0) {
    close(fd);
    return fail("realpath symlink");
  }
  if (mkdir("realpath_dir", 0777) != 0 ||
      symlink("../sample.tmp", "realpath_dir/nested.link") != 0) {
    close(fd);
    return fail("realpath nested setup");
  }
  memset(resolved, 0, sizeof(resolved));
  if (realpath("realpath_dir/nested.link", resolved) == 0 ||
      strstr(resolved, "sample.tmp") == 0 ||
      strstr(resolved, "nested.link") != 0) {
    close(fd);
    return fail("realpath relative symlink");
  }
  if (remove("realpath_dir/nested.link") != 0 ||
      rmdir("realpath_dir") != 0) {
    close(fd);
    return fail("remove realpath nested");
  }
  if (remove("sample.link") != 0) {
    close(fd);
    return fail("remove symlink");
  }
#endif
  if (lseek(fd, 0, SEEK_SET) != 0) {
    close(fd);
    return fail("lseek sample");
  }
  copy = dup(fd);
  if (copy < 0) {
    close(fd);
    return fail("dup");
  }
  if (read(copy, &readback, 1) != 1 || readback != 'Y') {
    close(copy);
    close(fd);
    return fail("read dup");
  }
  close(copy);
  if (dup2(fd, 10) != 10) {
    close(fd);
    return fail("dup2");
  }
  close(10);
  high_copy = fcntl(fd, F_DUPFD_CLOEXEC, 8);
  if (high_copy < 8) {
    close(fd);
    return fail("fcntl dupfd cloexec");
  }
  close(high_copy);
  high_copy = fcntl(fd, F_DUPFD, 8);
  if (high_copy < 8) {
    close(fd);
    return fail("fcntl dupfd");
  }
  close(high_copy);
  if (pipe(pipefd) != 0) {
    close(fd);
    return fail("pipe");
  }
  byte = 'P';
  readback = 0;
  if (write(pipefd[1], &byte, 1) != 1 ||
      read(pipefd[0], &readback, 1) != 1 ||
      readback != 'P') {
    close(pipefd[0]);
    close(pipefd[1]);
    close(fd);
    return fail("pipe read/write");
  }
  close(pipefd[0]);
  close(pipefd[1]);
  close(fd);
  if (remove("sample.tmp") != 0) {
    return fail("remove sample");
  }
  if (chdir(cwd) != 0) {
    return fail("chdir back");
  }
  if (rmdir("file_path_test.dir") != 0) {
    return fail("rmdir");
  }

  printf("file_path_test: ok\n");
  return 0;
}
