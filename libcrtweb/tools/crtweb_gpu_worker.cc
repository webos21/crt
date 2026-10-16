/* CRT-owned GPU resource worker. Statically linked CRT/SDK graphics, no WebKit
 * library in this process. Its WebView owns a separate ordinary crtweb_host.
 * Browser CPU BGRA -> worker GPU upload -> exported GPU image -> application.
 * Only the final worker/application leg is zero-copy/shared GPU memory.
 */
#include "crtweb/webview.h"
#include "crtweb_gpu_wire.h"
#include "crtgfx/skia_shared.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/ganesh/GrDirectContext.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
// A failing step says where: the worker's only report is its exit status and one stderr line.
bool fail_at(int line) {
  fprintf(stderr, "crtweb_gpu_worker: step failed at line %d\n", line);
  return false;
}
#define FAIL() fail_at(__LINE__)
struct Worker {
  int socket = -1;
  crtgfx_gpu_device* device = nullptr;
  sk_sp<GrDirectContext> context;
  crtweb::WebView browser;
  struct Buffer {
    std::unique_ptr<crtgfx_skia_shared_image> image;
    uint32_t generation = 0;
    uint64_t serial = 0;
    bool busy = false, pending_release = false;
    std::chrono::steady_clock::time_point release_deadline;
  };
  std::array<Buffer, CRTWEB_GPU_MAX_BUFFERS> buffers;
  uint32_t generation = 0, width = 0, height = 0, last_buffer = 0;
  uint64_t serial = 0, acked = 0, source_serial = 0;
  uint32_t uploads = 0, reused = 0, retired = 0, peak = 0;
  bool bye = false;
  ~Worker() {
    if (context) context->flushAndSubmit(GrSyncCpu::kYes);
    for (auto& b : buffers) b.image.reset();
    browser.shutdown(); context.reset();
    if (device) crtgfx_gpu_device_release(device);
    if (socket >= 0) close(socket);
  }
  bool send(const crtweb_gpu_packet& p, const int* fds = nullptr, size_t count = 0) {
    return crtweb_gpu_send(socket, &p, fds, count) == 0;
  }
  bool remove(unsigned i) {
    auto p = crtweb_gpu_message(CRTWEB_GPU_BUFFER_REMOVE);
    p.buffer_id = i + 1; p.generation = buffers[i].generation;
    if (!send(p)) return FAIL();
    buffers[i] = Buffer(); ++retired;
    return true;
  }
  bool collect() {
    for (unsigned i = 0; i < buffers.size(); ++i) {
      auto& b = buffers[i];
      if (!b.image) continue;
      if (b.pending_release) {
        crtgfx_result result = b.image->wait_released(0);
        if (result == CRTGFX_ERROR_TIMEOUT) {
          if (std::chrono::steady_clock::now() >= b.release_deadline) return FAIL();
          continue;
        }
        if (result != CRTGFX_OK) return FAIL();
        b.pending_release = false; b.busy = false; ++reused;
      }
      if (b.generation != generation && !b.busy && !b.pending_release && !remove(i)) return FAIL();
    }
    return true;
  }
  bool pool(uint32_t w, uint32_t h) {
    if (w == width && h == height) return true;
    if (!w || !h || w > CRTGFX_SHARED_IMAGE_MAX_EXTENT || h > CRTGFX_SHARED_IMAGE_MAX_EXTENT || generation == UINT32_MAX) return FAIL();
    ++generation; width = w; height = h;
    if (!collect()) return FAIL();
    unsigned created = 0;
    for (unsigned i = 0; i < buffers.size() && created < CRTWEB_GPU_POOL_SIZE; ++i) {
      auto& b = buffers[i];
      if (b.image) continue;
      if (crtgfx_skia_shared_create(device, w, h, &b.image) != CRTGFX_OK) return FAIL();
      b.generation = generation;
      auto p = crtweb_gpu_message(CRTWEB_GPU_BUFFER_ADD);
      p.buffer_id = i + 1; p.generation = generation; p.payload.image = b.image->description();
      int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
      if (b.image->export_handles(fds) != CRTGFX_OK) return FAIL();
      bool sent = send(p, fds, crtgfx_shared_image_handle_count(&p.payload.image));
      crtweb_gpu_close_handles(fds);
      if (!sent) return FAIL();
      ++created;
    }
    unsigned live = 0; for (const auto& b : buffers) if (b.image) ++live;
    if (live > peak) peak = live;
    return created == CRTWEB_GPU_POOL_SIZE;
  }
  bool message(const crtweb_gpu_packet& p) {
    if (p.type == CRTWEB_GPU_BYE) { bye = true; return true; }
    if (p.type == CRTWEB_GPU_ACK) {
      if (p.serial != serial || p.serial <= acked || p.buffer_id != last_buffer
          || p.generation != buffers[p.buffer_id - 1].generation) return FAIL();
      acked = p.serial;
      return true;
    }
    if (p.type == CRTWEB_GPU_RELEASE) {
      auto& b = buffers[p.buffer_id - 1];
      if (!b.image || !b.busy || b.pending_release || b.generation != p.generation || b.serial != p.serial) return FAIL();
      b.pending_release = true;
      b.release_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
      return true;
    }
    if (p.type == CRTWEB_GPU_COMMAND) {
      if (p.payload.command.code == CRTWEB_GPU_EVALUATE)
        return browser.evaluate(std::string(p.payload.command.text, p.payload.command.length), nullptr);
      if (p.payload.command.code == CRTWEB_GPU_RESIZE) {
        unsigned w = 0, h = 0; char extra;
        if (sscanf(p.payload.command.text, "%u %u %c", &w, &h, &extra) != 2
            || !w || !h || w > CRTGFX_SHARED_IMAGE_MAX_EXTENT || h > CRTGFX_SHARED_IMAGE_MAX_EXTENT) return FAIL();
        browser.resize(w, h);
        return true;
      }
    }
    return FAIL();
  }
  bool receive(int timeout_ms) {
    bool any = false;
    while (!bye) {
      struct pollfd pfd {socket, POLLIN, 0};
      int ready = poll(&pfd, 1, any ? 0 : timeout_ms);
      if (ready <= 0) return ready == 0 || errno == EINTR;
      crtweb_gpu_packet p; int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
      int got = crtweb_gpu_recv(socket, &p, fds);
      crtweb_gpu_close_handles(fds);
      if (got <= 0 || !message(p)) return FAIL();
      any = true;
    }
    return true;
  }
  bool publish() {
    if (!browser.has_frame() || browser.frame_serial() == source_serial || serial != acked) return true;
    if (!pool(browser.surface()->frame_width(), browser.surface()->frame_height())) return FAIL();
    for (unsigned i = 0; i < buffers.size(); ++i) {
      auto& b = buffers[i];
      if (!b.image || b.generation != generation || b.busy || b.pending_release) continue;
      auto provider = browser.surface_provider();
      SkImage* source = provider.acquire(provider.user, nullptr);
      if (!source) return true;
      sk_sp<SkSurface> target;
      bool ok = !source->isTextureBacked() && b.image->begin_write(context.get(), 2000, &target) == CRTGFX_OK;
      if (ok) {
        target->getCanvas()->clear(SK_ColorTRANSPARENT);
        target->getCanvas()->drawImage(source, 0, 0);
        ok = b.image->end_write(context.get(), target.get()) == CRTGFX_OK
            && b.image->wait_ready(2000) == CRTGFX_OK;
      }
      // Retain the CPU source until its upload really completed. No readback.
      target.reset(); provider.release(provider.user, nullptr, source);
      if (!ok) return FAIL();
      source_serial = browser.frame_serial(); browser.frame_presented();
      auto p = crtweb_gpu_message(CRTWEB_GPU_FRAME);
      p.serial = ++serial; p.buffer_id = i + 1; p.generation = generation;
      b.busy = true; b.serial = serial; last_buffer = i + 1; ++uploads;
      return send(p);
    }
    return true;
  }
  bool run(const char* browser_host, const char* url) {
    struct pollfd pfd {socket, POLLIN, 0};
    if (poll(&pfd, 1, 10000) <= 0) return FAIL();
    crtweb_gpu_packet hello; int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
    int got = crtweb_gpu_recv(socket, &hello, fds); crtweb_gpu_close_handles(fds);
    if (got != 1 || hello.type != CRTWEB_GPU_HELLO) return FAIL();
    crtgfx_shared_image_desc identity {};
    bool supported = crtgfx_gpu_device_create(0, &device) == CRTGFX_OK
        && crtgfx_skia_shared_query(device, &identity) == CRTGFX_OK
        && hello.payload.image.version == identity.version && hello.payload.image.backend == identity.backend
        && hello.payload.image.handle_kind == identity.handle_kind && hello.payload.image.format == identity.format
        && !memcmp(hello.payload.image.device_uuid, identity.device_uuid, 16)
        && !memcmp(hello.payload.image.driver_uuid, identity.driver_uuid, 16);
    auto config = crtweb_gpu_message(CRTWEB_GPU_CONFIG);
    config.flags = supported ? CRTWEB_GPU_MODE_SHARED : CRTWEB_GPU_MODE_CPU;
    config.payload.image = identity;
    if (!send(config)) return FAIL();
    if (!supported) { puts("crtweb_gpu_worker: mode=cpu-fallback resources=0"); return true; }
    context = crtgfx_skia_make_gpu_context(device);
    browser.on_load = [](crtweb::WebView::LoadEvent event, const std::string& url, const std::string& message) {
      if (event == crtweb::WebView::LoadEvent::Failed)
        fprintf(stderr, "crtweb_gpu_worker: load failed url=%s message=%s\n", url.c_str(), message.c_str());
    };
    if (!context) { fprintf(stderr, "crtweb_gpu_worker: no GPU context for the worker device\n"); return FAIL(); }
    if (!browser.launch(browser_host, hello.payload.image.width, hello.payload.image.height, url)) {
      fprintf(stderr, "crtweb_gpu_worker: could not launch the browser host %s\n", browser_host);
      return FAIL();
    }
    while (!bye && browser.running()) {
      if (!receive(2)) return FAIL();
      if (bye) break;
      browser.poll(0);
      if (!collect() || !publish()) return FAIL();
    }
    printf("crtweb_gpu_worker: uploads=%u reused=%u retired=%u peak_buffers=%u gpu_readbacks=0\n", uploads, reused, retired, peak);
    return bye;
  }
};
}

extern "C" int main(int argc, char** argv) {
  if (argc != 4) { fprintf(stderr, "usage: crtweb_gpu_worker SOCKET BROWSER_HOST URL\n"); return 2; }
  Worker worker;
  worker.socket = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  sockaddr_un address {}; address.sun_family = AF_UNIX;
  if (worker.socket < 0 || strlen(argv[1]) >= sizeof address.sun_path) return 2;
  memcpy(address.sun_path, argv[1], strlen(argv[1]) + 1);
  if (connect(worker.socket, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) return 1;
  bool ok = worker.run(argv[2], argv[3]);
  if (!ok) { worker.send(crtweb_gpu_message(CRTWEB_GPU_ERROR)); fprintf(stderr, "crtweb_gpu_worker: FAILED\n"); }
  return ok ? 0 : 1;
}
