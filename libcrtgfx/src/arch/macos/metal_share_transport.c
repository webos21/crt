/* CRT descriptors/pthreads stay in a C99 TU with CRT headers, separate from
 * the Objective-C++ SDK boundary. Darwin's fcntl/stat/socket ABI is NOT ours. */
#include "metal_shared_native.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

struct lease_offer { void* native; int fd; };
static uint32_t live_offers;
uint32_t crtgfx_metal_share_live_offers(void) { return __atomic_load_n(&live_offers, __ATOMIC_ACQUIRE); }

static void* serve_offer(void* opaque) {
  struct lease_offer* offer = (struct lease_offer*)opaque;
  for (;;) {
    struct pollfd fd = {offer->fd, POLLIN, 0};
    int result = poll(&fd, 1, 0);
    if ((result < 0 && errno != EINTR) || fd.revents) break;
    if (!crtgfx_metal_share_serve(offer->native, 10)) break;
  }
  crtgfx_metal_share_offer_release(offer->native);
  close(offer->fd); free(offer);
  __atomic_sub_fetch(&live_offers, 1, __ATOMIC_RELEASE);
  return 0;
}

int crtgfx_metal_share_export(void* record, int out[2]) {
  struct crtgfx_metal_share_ticket ticket;
  struct lease_offer* offer;
  pthread_t thread;
  int lease[2] = {-1, -1};
  int metadata = -1;
  void* native = crtgfx_metal_share_offer(record, &ticket);
  if (!native) return 0;
  offer = (struct lease_offer*)malloc(sizeof *offer);
  if (!offer) goto fail;
  metadata = memfd_create("crt-metal-resource", MFD_CLOEXEC);
  if (metadata < 0 || write(metadata, &ticket, sizeof ticket) != (ssize_t)sizeof ticket
      || socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, lease) != 0) goto fail;
  offer->native = native; offer->fd = lease[0];
  __atomic_add_fetch(&live_offers, 1, __ATOMIC_RELAXED);
  if (pthread_create(&thread, 0, serve_offer, offer) != 0) {
    __atomic_sub_fetch(&live_offers, 1, __ATOMIC_RELEASE); goto fail;
  }
  pthread_detach(thread);
  out[0] = metadata; out[1] = lease[1];
  return 1;
fail:
  if (metadata >= 0) close(metadata);
  if (lease[0] >= 0) close(lease[0]);
  if (lease[1] >= 0) close(lease[1]);
  free(offer); crtgfx_metal_share_offer_release(native);
  return 0;
}

void* crtgfx_metal_share_import(void* device, const int fds[2]) {
  struct crtgfx_metal_share_ticket ticket;
  struct stat info;
  int lease;
  void* record;
  if (fds[0] < 0 || fds[1] < 0 || pread(fds[0], &ticket, sizeof ticket, 0) != (ssize_t)sizeof ticket
      || fstat(fds[1], &info) != 0 || !S_ISSOCK(info.st_mode)) return 0;
  lease = fcntl(fds[1], F_DUPFD_CLOEXEC, 0);
  if (lease < 0) return 0;
  record = crtgfx_metal_share_open(device, &ticket);
  close(lease);
  return record;
}
