/* Compile the actual Linux backend with fake OS boundaries. No device opens. */
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static DIR* fake_opendir(const char* path);
static struct dirent* fake_readdir(DIR* dir);
static int fake_closedir(DIR* dir);
static int fake_open(const char* path, int flags, ...);
static int fake_close(int fd);
static int fake_ioctl(int fd, unsigned long command, ...);

#define opendir fake_opendir
#define readdir fake_readdir
#define closedir fake_closedir
#define open fake_open
#define close fake_close
#define ioctl fake_ioctl
#include "../src/arch/linux/audio_sink_linux.c"
#undef opendir
#undef readdir
#undef closedir
#undef open
#undef close
#undef ioctl

static const char* names[8];
static unsigned cursor, opens;
static char opened[80];
static long transferred;
static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); ++failures; } } while (0)

static DIR* fake_opendir(const char* path) { (void)path; cursor = 0; return (DIR*)1; }
static struct dirent* fake_readdir(DIR* dir) {
  (void)dir;
  static struct dirent entry;
  if (!names[cursor]) return NULL;
  snprintf(entry.d_name, sizeof(entry.d_name), "%s", names[cursor++]);
  return &entry;
}
static int fake_closedir(DIR* dir) { (void)dir; return 0; }
static int fake_open(const char* path, int flags, ...) {
  (void)flags; ++opens; snprintf(opened, sizeof(opened), "%s", path); return 42;
}
static int fake_close(int fd) { (void)fd; return 0; }
static int fake_ioctl(int fd, unsigned long command, ...) {
  (void)fd;
  if (command == CRTMEDIA_SNDRV_PCM_IOCTL_WRITEI_FRAMES) {
    va_list args;
    va_start(args, command);
    struct crtmedia_snd_xferi* xfer = va_arg(args, struct crtmedia_snd_xferi*);
    va_end(args);
    xfer->result = transferred; /* frames deliberately remains the request. */
  }
  return 0;
}

int main(void) {
  crtmedia_audio_sink sink = {0};
  crtmedia_audio_sink_desc desc = {CRTMEDIA_SAMPLE_FORMAT_S16, 48000, 2};
  names[0] = "pcmC0D0c"; names[1] = "pcmC1D0c";
  names[2] = "pcmC0D0p-extra"; names[3] = "pcmC0D0";
  CHECK(alsa_try_open(&desc, &sink) == CRTMEDIA_ERROR_UNSUPPORTED);
  CHECK(opens == 0);
  names[4] = "pcmC1D0p"; names[5] = "pcmC0D3p";
  CHECK(alsa_try_open(&desc, &sink) == CRTMEDIA_OK);
  CHECK(opens == 1 && strcmp(opened, "/dev/snd/pcmC0D3p") == 0);
  short samples[20] = {0};
  transferred = 3; CHECK(alsa_write(&sink, samples, 10) == 3);
  transferred = 0; CHECK(alsa_write(&sink, samples, 10) == 0);
  transferred = 10; CHECK(alsa_write(&sink, samples, 10) == 10);
  transferred = -1; CHECK(alsa_write(&sink, samples, 10) < 0);
  transferred = 11; CHECK(alsa_write(&sink, samples, 10) < 0);
  if (failures) return 1;
  puts("crtmedia_audio_sink_linux_test: ok");
  return 0;
}
