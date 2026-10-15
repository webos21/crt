/* Compile the actual Linux backend with fake OS boundaries. No device opens. */
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
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

/* An isolated fake Pulse peer exercises the actual framing/poll/ACK path.
 * No sound server or audio/capture device is contacted. */
static int pulse_peer_reply(int fd, uint32_t expected, int latency, int playing, uint64_t read_bytes, int error) {
  crtmedia_pulse_packet request;
  if (pulse_read_packet(fd, &request) != 0) return 1;
  uint32_t command = request.command, tag = request.tag;
  free(request.owned);
  if (command != expected) return 1;
  uint8_t payload[80], descriptor[20];
  crtmedia_pulse_writer writer = {payload, 0, sizeof payload};
  pw_tag_u32(&writer, error ? CRTMEDIA_PULSE_COMMAND_ERROR : CRTMEDIA_PULSE_COMMAND_REPLY);
  pw_tag_u32(&writer, tag);
  if (latency && !error) {
    uint8_t fields[55] = {0};
    fields[18] = playing < 0 ? 'x' : (playing ? '1' : '0');
    fields[46] = 'r';
    for (unsigned i = 0; i < 8; ++i) fields[47 + i] = (uint8_t)(read_bytes >> (56 - i * 8));
    pw_bytes(&writer, fields, sizeof fields);
  }
  pw_descriptor(descriptor, writer.len, CRTMEDIA_PULSE_INVALID_INDEX);
  return pulse_send_all(fd, descriptor, sizeof descriptor) || pulse_send_all(fd, payload, writer.len);
}

static void test_pulse_tail_position(int scenario) {
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) { CHECK(0); return; }
  pid_t peer = fork();
  if (peer == 0) {
    close(sockets[0]);
    alarm(5);
    int playing = scenario == 0 ? 1 : (scenario == 5 ? -1 : 0);
    uint64_t read_bytes = scenario == 1 ? 80 : (scenario == 6 ? 100 : 40);
    int failed = pulse_peer_reply(sockets[1], CRTMEDIA_PULSE_COMMAND_GET_PLAYBACK_LATENCY,
                                 1, playing, read_bytes, 0);
    if (!failed && scenario >= 2 && scenario <= 4) {
      failed = pulse_peer_reply(sockets[1], CRTMEDIA_PULSE_COMMAND_DRAIN_PLAYBACK_STREAM,
                                0, 0, 0, scenario == 3);
      if (!failed && scenario != 3)
        failed = pulse_peer_reply(sockets[1], CRTMEDIA_PULSE_COMMAND_GET_PLAYBACK_LATENCY,
                                  1, 0, 100, scenario == 4);
    }
    close(sockets[1]);
    _exit(failed ? 1 : 0);
  }
  close(sockets[1]);
  if (peer < 0) { close(sockets[0]); CHECK(0); return; }
  crtmedia_audio_sink sink = {0};
  sink.fd = sockets[0];
  sink.block_align = 4;
  sink.pulse_bytes_written_total = 80;
  sink.pulse_next_tag = 1;
  uint64_t position = 777;
  crtmedia_result result = pulse_get_position_frames(&sink, &position);
  if (scenario >= 3 && scenario <= 5) {
    CHECK(result == CRTMEDIA_ERROR_UNSUPPORTED);
    CHECK(position == 777); /* No made-up progress after a failed ACK/query. */
  } else {
    CHECK(result == CRTMEDIA_OK);
    CHECK(position == (scenario == 0 ? 10 : (scenario == 1 ? 20 : 25)));
  }
  close(sockets[0]);
  int status = -1;
  CHECK(waitpid(peer, &status, 0) == peer);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

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
  for (int scenario = 0; scenario < 7; ++scenario) test_pulse_tail_position(scenario);
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
