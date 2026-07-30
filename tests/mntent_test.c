#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

static int fail(const char* message) {
  printf("mntent_test: %s\n", message);
  return 1;
}

int main(void) {
  int fd;
  FILE* fp;
  struct mntent entry;
  char buffer[MAXPATHLEN * 3];
  const char data[] =
      "# comment\n"
      "rootfs / rootfs rw,seclabel 0 0\n"
      "tmpfs /tmp tmpfs rw,nosuid,nodev 0 0\n";

  if (PATH_MAX < 4096 || MAXPATHLEN < PATH_MAX || NAME_MAX != 255) {
    return fail("limits");
  }
  fd = open("mntent_test.mounts", O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (fd < 0) {
    return fail("open");
  }
  if (write(fd, data, sizeof(data) - 1) != (ssize_t)(sizeof(data) - 1) ||
      close(fd) != 0) {
    return fail("write");
  }
  fp = setmntent("mntent_test.mounts", "r");
  if (fp == 0) {
    return fail("setmntent");
  }
  if (getmntent_r(fp, &entry, buffer, sizeof(buffer)) != &entry ||
      strcmp(entry.mnt_fsname, "rootfs") != 0 ||
      strcmp(entry.mnt_dir, "/") != 0 ||
      strcmp(entry.mnt_type, "rootfs") != 0 ||
      hasmntopt(&entry, "rw") == 0 ||
      hasmntopt(&entry, "ro") != 0) {
    return fail("first entry");
  }
  if (getmntent_r(fp, &entry, buffer, sizeof(buffer)) != &entry ||
      strcmp(entry.mnt_dir, "/tmp") != 0 ||
      hasmntopt(&entry, "nosuid") == 0 ||
      entry.mnt_freq != 0 ||
      entry.mnt_passno != 0) {
    return fail("second entry");
  }
  if (getmntent_r(fp, &entry, buffer, sizeof(buffer)) != 0) {
    return fail("eof");
  }
  if (endmntent(fp) != 1) {
    return fail("endmntent");
  }
  if (unlink("mntent_test.mounts") != 0) {
    return fail("cleanup");
  }
  printf("mntent_test: ok\n");
  return 0;
}
