/* Actual browser -> independent CRT GPU worker -> shared Vulkan image -> UI.
 * Readback is acceptance-only. The worker never reads GPU pixels back and
 * the application's resource provider never uploads CPU frame pixels.
 */
#include "crtweb/gpu_surface.h"
#include "crtgfx/skia.h"
#include "../../../libcrtgfx/src/gpu_test_control.h"
#include "../common/crtweb_test_process.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <set>
#include <vector>
extern char** environ;

#if defined(_WIN32)
// Acceptance observer only (as in webview_gpu_test.cc): native declarations never enter the CRT surface.
extern "C" {
struct GpuModuleEntry {
  unsigned size, module, pid, globalUsage, processUsage;
  unsigned char* base;
  unsigned baseSize;
  void* handle;
  char name[256];
  char path[260];
};
int __stdcall Module32First(void*, GpuModuleEntry*);
int __stdcall Module32Next(void*, GpuModuleEntry*);
}
#endif

namespace {
int failures;
bool check(bool ok, const char* name) {
  if (!ok) { fprintf(stderr, "crtweb_gpu_resource_test: FAILED %s\n", name); ++failures; }
  return ok;
}
bool report_browser_maps(pid_t worker, std::vector<pid_t>& observed) {
#if defined(_WIN32)
  std::set<std::string> roles;
  auto report = [&](pid_t pid, const char* role) {
    observed.push_back(pid);
    void* snapshot = crtweb_test::CreateToolhelp32Snapshot(0x8 | 0x10, static_cast<unsigned>(pid));
    if (snapshot == reinterpret_cast<void*>(-1)) return;
    std::set<std::string> files;
    GpuModuleEntry entry = {};
    entry.size = sizeof entry;
    for (int more = Module32First(snapshot, &entry); more; more = Module32Next(snapshot, &entry)) {
      std::string path = entry.path;
      for (char& c : path) if (c == '\\') c = '/';
      files.insert(path);
    }
    crtweb_test::CloseHandle(snapshot);
    if (!files.empty()) roles.insert(role);
    for (const auto& file : files) printf("MAPPED %s %s\n", role, file.c_str());
  };
  for (const auto& host : crtweb_test::children(worker)) {
    if (host.name != "crtweb_host") continue;
    report(host.pid, "HostProcess");
    for (const auto& child : crtweb_test::children(host.pid))
      if (child.name == "WebProcess" || child.name == "NetworkProcess") report(child.pid, child.name.c_str());
  }
  return check(roles.size() == 3, "all browser roles observed behind GPU worker");
#elif defined(__linux__)
  std::set<std::string> roles;
  auto report = [&](pid_t pid, const char* role) {
    observed.push_back(pid);
    FILE* maps = fopen(("/proc/" + std::to_string(pid) + "/maps").c_str(), "r");
    if (!maps) return;
    std::set<std::string> files;
    char line[8192];
    while (fgets(line, sizeof line, maps)) {
      char* path = strchr(line, '/');
      if (!path || !strncmp(path, "/memfd:", 7)) continue;
      path[strcspn(path, "\r\n")] = 0;
      char permissions[5] {};
      std::string own = "/crt_memfd_" + std::to_string(pid) + "_";
      if (sscanf(line, "%*lx-%*lx %4s", permissions) == 1 && !strchr(permissions, 'x')
          && strstr(path, own.c_str()) && strstr(path, ".tmp (deleted)")) continue;
      files.insert(path);
    }
    fclose(maps);
    if (!files.empty()) roles.insert(role);
    for (const auto& path : files) printf("MAPPED %s %s\n", role, path.c_str());
  };
  for (const auto& host : crtweb_test::children(worker)) {
    if (host.name != "crtweb_host") continue;
    report(host.pid, "HostProcess");
    for (const auto& child : crtweb_test::children(host.pid))
      if (child.name == "WebProcess" || child.name == "NetworkProcess") report(child.pid, child.name.c_str());
  }
  return check(roles.size() == 3, "all browser roles observed behind GPU worker");
#elif defined(__APPLE__) || defined(WTF_CRT_MACHO_ASM)
  const char* directory = getenv("CRTWEB_GPU_MAP_AUDIT_DIR");
  if (!check(directory != nullptr, "native resource map observer configured")) return false;
  std::string ready = std::string(directory) + "/ready", temporary = ready + ".tmp";
  std::string release = std::string(directory) + "/release";
  FILE* file = fopen(temporary.c_str(), "w");
  if (!check(file != nullptr, "resource map checkpoint")) return false;
  fprintf(file, "GPUWorker %ld\n", static_cast<long>(worker));
  std::set<std::string> roles;
  for (const auto& host : crtweb_test::children(worker)) {
    if (host.name != "crtweb_host") continue;
    observed.push_back(host.pid); roles.insert("HostProcess");
    fprintf(file, "HostProcess %ld\n", static_cast<long>(host.pid));
    for (const auto& child : crtweb_test::children(host.pid)) {
      if (child.name != "WebProcess" && child.name != "NetworkProcess") continue;
      observed.push_back(child.pid); roles.insert(child.name);
      fprintf(file, "%s %ld\n", child.name.c_str(), static_cast<long>(child.pid));
    }
  }
  bool closed = fclose(file) == 0;
  if (!check(closed && rename(temporary.c_str(), ready.c_str()) == 0, "complete resource map checkpoint")) return false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (access(release.c_str(), F_OK) != 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  return check(roles.size() == 3 && access(release.c_str(), F_OK) == 0, "resource map observer released all browser roles");
#else
  (void)worker; (void)observed;
  return false; // Native observers belong to their future resource-backend replay.
#endif
}
struct Child {
  pid_t pid = 0;
  int listener = -1;
  std::string directory, socket, page, page_directory;
  ~Child() {
    if (listener >= 0) close(listener);
    if (pid > 0) {
      finish();
      if (pid > 0) { kill(pid, SIGKILL); waitpid(pid, nullptr, 0); }
    }
    if (!socket.empty()) unlink(socket.c_str());
    if (!directory.empty()) remove(directory.c_str());
    if (!page.empty()) unlink(page.c_str());
    if (!page_directory.empty()) remove(page_directory.c_str());
  }
  int launch(const char* worker, const char* browser) {
#if defined(_WIN32)
    // The socket path must stay short (AF_UNIX); the gate's work directory is the scratch space.
    char temp[] = "crtweb9b-sock-XXXXXX", fixture[] = "crtweb9b-page-XXXXXX", cwd[4096];
#else
    char temp[] = "/tmp/crtweb9bXXXXXX", fixture[] = "crtweb9b-page-XXXXXX", cwd[4096];
#endif
    if (!mkdtemp(temp) || !getcwd(cwd, sizeof cwd)) return -1;
    directory = temp; socket = directory + "/s";
    if (!mkdtemp(fixture)) return -1;
    page_directory = std::string(cwd) + "/" + fixture;
    page = page_directory + "/page.html";
    int page_fd = open(page.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (page_fd < 0) return -1;
    const char* html = "<!doctype html><title>shared GPU</title><body style='margin:0;background:#00ff00'>";
    bool written = write(page_fd, html, strlen(html)) == static_cast<ssize_t>(strlen(html)); close(page_fd);
    if (!written) return -1;
    listener = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    sockaddr_un address {}; address.sun_family = AF_UNIX;
    memcpy(address.sun_path, socket.c_str(), socket.size() + 1);
    if (listener < 0 || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || listen(listener, 1) != 0) return -1;
    std::string url = "file://" + page;
    char* argv[] = {const_cast<char*>(worker), const_cast<char*>(socket.c_str()), const_cast<char*>(browser),
      const_cast<char*>(url.c_str()), nullptr};
    if (posix_spawn(&pid, worker, nullptr, nullptr, argv, environ) != 0) { pid = 0; return -1; }
    struct pollfd pfd {listener, POLLIN, 0};
    if (poll(&pfd, 1, 30000) <= 0) return -1;
    int fd = accept(listener, nullptr, nullptr);
    if (fd >= 0) fcntl(fd, F_SETFD, FD_CLOEXEC);
    close(listener); listener = -1;
    return fd;
  }
  bool finish(bool abnormal = false) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int status = 0;
    while (pid > 0) {
      if (waitpid(pid, &status, WNOHANG) == pid) {
        pid = 0;
        bool clean = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        return abnormal ? !clean : clean;
      }
      if (std::chrono::steady_clock::now() >= end) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
  }
};

struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget below = CRTUI_INVALID_WIDGET, clip = CRTUI_INVALID_WIDGET, view = CRTUI_INVALID_WIDGET, above = CRTUI_INVALID_WIDGET;
  ~Scene() { if (ui) crtui_context_destroy(ui); }
  bool size(unsigned w, unsigned h) {
    return crtui_window_set_size(ui, window, w, h, 1) == CRTUI_OK
        && crtui_widget_set_bounds(ui, below, 0, 0, w, h) == CRTUI_OK
        && crtui_widget_set_bounds(ui, clip, 40, 20, w - 90, h - 50) == CRTUI_OK
        && crtui_widget_set_bounds(ui, view, -10, -10, w - 70, h - 30) == CRTUI_OK;
  }
  bool create() {
    if (crtui_context_create(&ui) != CRTUI_OK || crtui_window_create(ui, &window) != CRTUI_OK
        || crtui_container_create(ui, window, &below) != CRTUI_OK
        || crtui_container_create(ui, window, &clip) != CRTUI_OK
        || crtui_surface_view_create(ui, clip, &view) != CRTUI_OK
        || crtui_container_create(ui, window, &above) != CRTUI_OK) return false;
    crtui_style style {}; style.mask = CRTUI_STYLE_BACKGROUND; style.background_rgb = 0xff0000;
    if (crtui_widget_set_style(ui, below, &style) != CRTUI_OK) return false;
    style.background_rgb = 0x0000ff;
    if (crtui_widget_set_style(ui, above, &style) != CRTUI_OK
        || crtui_widget_set_bounds(ui, above, 60, 30, 40, 30) != CRTUI_OK) return false;
    style = {}; style.mask = CRTUI_STYLE_OPACITY; style.opacity = 128;
    return crtui_widget_set_style(ui, view, &style) == CRTUI_OK && size(160, 100);
  }
};

bool draw(crtweb::GpuSurfaceClient& client, Scene& scene, GrDirectContext* context, SkSurface* target,
          bool magenta, bool* matches, bool acknowledge = true) {
  auto provider = client.surface_provider();
  SkImage* image = provider.acquire(provider.user, nullptr);
  if (!check(image && image->isTextureBacked(), "GPU source, no consumer CPU upload")) return false;
  provider.release(provider.user, nullptr, image);
  if (!check(target && target->recordingContext() == context, "GPU target/context")
      || !check(crtui_skia_compose(scene.ui, scene.window, target, &provider) == CRTUI_OK, "GPU external-surface compose")) return false;
  context->flush(target);
  if (!check(context->submit(GrSyncCpu::kYes), "GPU submit")) return false;
  size_t stride = static_cast<size_t>(target->width()) * 4;
  std::vector<uint8_t> pixels(stride * target->height());
  auto info = SkImageInfo::Make(target->width(), target->height(), kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  if (!check(target->readPixels(info, pixels.data(), stride, 0, 0), "acceptance GPU readback")) return false;
  auto rgb = [&](unsigned x, unsigned y, int r, int g, int b) {
    const uint8_t* p = pixels.data() + y * stride + x * 4;
    return abs(p[2] - r) <= 3 && abs(p[1] - g) <= 3 && abs(p[0] - b) <= 3 && p[3] >= 250;
  };
  check(rgb(20, 20, 255, 0, 0), "UI below"); check(rgb(35, 25, 255, 0, 0), "ancestor clip");
  check(rgb(70, 40, 0, 0, 255), "UI above");
  *matches = magenta ? rgb(50, 50, 255, 0, 128) : rgb(50, 50, 127, 128, 0);
  if (!*matches) {
    const uint8_t* p = pixels.data() + 50 * stride + 50 * 4;
    fprintf(stderr, "crtweb_gpu_resource_test: waiting serial=%llu pixel_bgra=%u,%u,%u,%u\n",
        static_cast<unsigned long long>(client.frame_serial()), p[0], p[1], p[2], p[3]);
  }
  return !acknowledge || client.frame_presented();
}

bool await_colour(crtweb::GpuSurfaceClient& client, Scene& scene, GrDirectContext* context, SkSurface* target,
                  bool magenta, uint64_t after = 0, unsigned width = 0, unsigned height = 0) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  uint64_t drawn = 0;
  while (!client.closed() && std::chrono::steady_clock::now() < end) {
    client.poll(10);
    if (!client.has_frame() || drawn == client.frame_serial()) continue;
    drawn = client.frame_serial();
    bool matches = false;
    if (!draw(client, scene, context, target, magenta, &matches)) return false;
    if (matches && drawn > after && (!width || (client.frame_width() == width && client.frame_height() == height))) return true;
  }
  return false;
}

bool live(crtweb::GpuSurfaceClient& client, Scene& scene, crtgfx_gpu_device* device, GrDirectContext* context, unsigned frames) {
  crtgfx_window* window = nullptr; crtgfx_gpu_surface* gpu = nullptr;
  crtgfx_window_desc desc {"CRT shared GPU WebView", 320, 200, CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION};
  bool ok = check(crtgfx_window_create(&desc, &window) == CRTGFX_OK, "native window");
  if (ok) ok = check(crtgfx_gpu_surface_create(device, window, &gpu) == CRTGFX_OK, "native GPU surface");
  uint32_t w = 0, h = 0;
  if (ok) ok = check(crtgfx_gpu_surface_get_size(gpu, &w, &h) == CRTGFX_OK && scene.size(w, h), "native dimensions");
  unsigned presented = 0;
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (ok && presented < frames && !crtgfx_window_should_close(window) && std::chrono::steady_clock::now() < end) {
    crtgfx_result acquired = crtgfx_gpu_surface_acquire(gpu, 1000000);
    if (acquired == CRTGFX_ERROR_TIMEOUT) { crtgfx_window_pump_events(1); continue; }
    if (!check(acquired == CRTGFX_OK, "native acquire")) { ok = false; break; }
    auto target = crtgfx_skia_wrap_gpu_surface(context, gpu);
    auto provider = client.surface_provider();
    ok = check(target && crtui_skia_compose(scene.ui, scene.window, target.get(), &provider) == CRTUI_OK, "native GPU compose");
    if (ok) ok = check(crtgfx_skia_gpu_surface_present(context, target.get(), gpu) == CRTGFX_OK, "native GPU present");
    if (ok) { ++presented; client.frame_presented(); }
    client.poll(0); crtgfx_window_pump_events(1);
  }
  if (gpu) crtgfx_gpu_surface_release(gpu);
  if (window) crtgfx_window_destroy(window);
  printf("crtweb_gpu_resource_test: presented=%u requested=%u\n", presented, frames);
  return ok && presented == frames;
}

// Negotiation negative control, not a claim of a second physical GPU: a real
// native worker must reject a peer that declares a different device identity.
bool declared_mismatch(const char* worker, const char* browser, crtgfx_shared_image_desc identity) {
  Child child;
  int fd = child.launch(worker, browser);
  if (!check(fd >= 0, "mismatched peer launch")) return false;
  auto hello = crtweb_gpu_message(CRTWEB_GPU_HELLO);
  identity.width = 90; identity.height = 70; identity.device_uuid[0] ^= 1;
  hello.payload.image = identity;
  bool ok = crtweb_gpu_send(fd, &hello, nullptr, 0) == 0;
  struct pollfd ready {fd, POLLIN, 0};
  ok = ok && poll(&ready, 1, 10000) > 0;
  crtweb_gpu_packet config; int handles[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
  if (ok) {
    ok = crtweb_gpu_recv(fd, &config, handles) == 1;
    crtweb_gpu_close_handles(handles);
    ok = ok && config.type == CRTWEB_GPU_CONFIG && config.flags == CRTWEB_GPU_MODE_CPU;
  }
  close(fd);
  ok = check(ok, "declared identity mismatch negotiates CPU mode without resources") && child.finish() && ok;
  if (ok) puts("crtweb_gpu_resource_test: fallback mode=cpu resources=0 reason=declared-identity-mismatch");
  return ok;
}

bool run(const char* worker, const char* browser, crtgfx_gpu_device* device, GrDirectContext* context,
         unsigned frames, bool fallback) {
  Child child;
  int fd = child.launch(worker, browser);
  if (!check(fd >= 0, "independent GPU worker launch")) return false;
  bool ok = true;
  std::vector<pid_t> observed;
  {
    crtweb::GpuSurfaceClient client(fd, device, context);
    bool configured = client.handshake(90, 70, !fallback);
    if (fallback) {
      ok = check(configured && client.mode() == CRTWEB_GPU_MODE_CPU && !client.has_frame()
          && client.imports() == 0, "explicit CPU fallback, no GPU evidence");
    } else {
      if (!check(configured && client.mode() == CRTWEB_GPU_MODE_SHARED, "negotiate shared GPU channel")) return false;
      Scene scene; ok = check(scene.create(), "UI scene");
      auto target = crtgfx_skia_make_gpu_offscreen_surface(context, 160, 100);
      ok = ok && check(await_colour(client, scene, context, target.get(), false), "initial browser GPU pixels");
      uint64_t held = client.frame_serial();
      ok = ok && check(client.command(CRTWEB_GPU_EVALUATE, "document.body.style.background='#ff00ff'"), "change browser colour");
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      bool unchanged = false;
      ok = ok && draw(client, scene, context, target.get(), false, &unchanged)
          && check(unchanged && client.frame_serial() == held, "held GPU image not overwritten while consumer stalled");
      ok = ok && check(await_colour(client, scene, context, target.get(), true, held), "changed pixels over native GPU memory");
      uint32_t old_generation = client.generation();
      uint64_t before = client.frame_serial();
      check(scene.size(200, 120), "UI resize"); target = crtgfx_skia_make_gpu_offscreen_surface(context, 200, 120);
      ok = ok && check(client.resize(130, 90), "browser resize requested")
          && check(await_colour(client, scene, context, target.get(), true, before, 130, 90), "new resource generation/extent")
          && check(client.generation() > old_generation, "generation advances");
      auto baseline = crtweb_test::resources(); auto worker_before = crtweb_test::resources(child.pid);
      unsigned updates = 0;
      for (unsigned i = 0; ok && i < 20; ++i) {
        before = client.frame_serial(); bool magenta = (i & 1) != 0;
        std::string script = std::string("document.body.style.background='") + (magenta ? "#ff00ff" : "#00ff00") + "'";
        ok = check(client.command(CRTWEB_GPU_EVALUATE, script), "browser update command")
            && check(await_colour(client, scene, context, target.get(), magenta, before), "fresh shared GPU epoch");
        check(client.live_buffers() <= CRTWEB_GPU_MAX_BUFFERS, "bounded import pool");
        if (ok) ++updates;
      }
      auto after = crtweb_test::resources(); auto worker_after = crtweb_test::resources(child.pid);
      printf("crtweb_gpu_resource_test: updates=%u ui_rss=%+ld KiB ui_threads=%+ld ui_descriptors=%+ld "
          "worker_rss=%+ld KiB worker_threads=%+ld worker_descriptors=%+ld\n",
          updates, after.memoryKiB - baseline.memoryKiB, after.threads - baseline.threads, after.descriptors - baseline.descriptors,
          worker_after.memoryKiB - worker_before.memoryKiB, worker_after.threads - worker_before.threads,
          worker_after.descriptors - worker_before.descriptors);
      auto bounded = [](crtweb_test::Resources a, crtweb_test::Resources b) {
        return a.memoryKiB >= 0 && b.memoryKiB >= 0 && b.memoryKiB - a.memoryKiB <= 24 * 1024
            && a.threads >= 0 && b.threads >= 0 && b.threads - a.threads <= 2
            && a.descriptors >= 0 && b.descriptors >= 0 && b.descriptors - a.descriptors <= 2;
      };
      ok = check(bounded(baseline, after) && bounded(worker_before, worker_after), "UI/worker resources bounded") && ok;
      check(client.imports() == 6 && client.live_buffers() <= 4 && client.releases() >= 20, "real pool reuse and retirement");
      ok = report_browser_maps(child.pid, observed) && ok;
      if (ok && frames) ok = live(client, scene, device, context, frames);
      printf("crtweb_gpu_resource_test: imports=%u releases=%u live_buffers=%u generation=%u\n",
          client.imports(), client.releases(), client.live_buffers(), client.generation());
      fflush(stdout); // The child's teardown report shares this output stream.
      target.reset();
    }
  }
  ok = check(child.finish(), "GPU worker exits cleanly") && ok;
  for (pid_t pid : observed)
    ok = check(kill(pid, 0) < 0 && errno == ESRCH, "observed browser descendant stopped and reaped") && ok;
  check(crtweb_test::children().empty(), "owned worker reaped");
  return ok && failures == 0;
}

enum class Fault { Normal, Disconnect, Worker, Host, Web, Context, Resize };
const char* fault_name(Fault fault) {
  switch (fault) {
    case Fault::Normal: return "normal";
    case Fault::Disconnect: return "consumer-disconnect";
    case Fault::Worker: return "worker-kill";
    case Fault::Host: return "host-kill";
    case Fault::Web: return "web-kill";
    case Fault::Context: return "context-loss";
    case Fault::Resize: return "held-resize";
  }
  return "invalid";
}
std::vector<crtweb_test::Process> browser_tree(pid_t worker) {
  std::vector<crtweb_test::Process> result;
  for (const auto& host : crtweb_test::children(worker)) {
    if (host.name != "crtweb_host") continue;
    result.push_back(host);
    for (const auto& child : crtweb_test::children(host.pid))
      if (child.name == "WebProcess" || child.name == "NetworkProcess") result.push_back(child);
  }
  return result;
}
bool gone(const std::vector<crtweb_test::Process>& peers) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(6);
  do {
    bool all = true;
    for (const auto& peer : peers) if (kill(peer.pid, 0) == 0 || errno != ESRCH) all = false;
    if (all) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (std::chrono::steady_clock::now() < end);
  for (const auto& peer : peers)
    if (kill(peer.pid, 0) == 0) fprintf(stderr, "remaining owned descendant: %ld %s\n", static_cast<long>(peer.pid), peer.name.c_str());
  return check(false, "all observed browser descendants stopped/reaped after fault");
}
bool lifecycle_session(const char* worker, const char* browser, crtgfx_gpu_device* device,
                       GrDirectContext* context, Fault fault, bool maps = false) {
  Child child;
  int fd = child.launch(worker, browser);
  if (!check(fd >= 0, "lifecycle worker launch")) return false;
  auto started = std::chrono::steady_clock::now();
  std::vector<crtweb_test::Process> peers;
  bool ok = true;
  {
    crtweb::GpuSurfaceClient client(fd, device, context);
    Scene scene;
    auto target = crtgfx_skia_make_gpu_offscreen_surface(context, 160, 100);
    ok = check(client.handshake(90, 70, true) && client.mode() == CRTWEB_GPU_MODE_SHARED, "lifecycle GPU negotiation")
        && check(scene.create(), "lifecycle scene")
        && check(await_colour(client, scene, context, target.get(), false), "fresh initial GPU pixels in new session");
    peers = browser_tree(child.pid);
    ok = check(peers.size() >= 3, "owned browser tree observed before fault") && ok;
    if (ok && maps) {
      std::vector<pid_t> observed;
      ok = report_browser_maps(child.pid, observed);
      fflush(stdout); // Finish map records before the child writes its shutdown report.
    }
    uint64_t before = client.frame_serial();
    bool preserved = false;
    if (ok && (fault == Fault::Worker || fault == Fault::Host || fault == Fault::Disconnect)) {
      if (fault == Fault::Worker) ok = check(child.pid > 0 && kill(child.pid, SIGKILL) == 0, "kill exact owned GPU worker");
      if (fault == Fault::Host) {
        auto host = peers.front();
        ok = check(host.name == "crtweb_host" && host.pid > 0 && kill(host.pid, SIGKILL) == 0, "kill exact owned browser host");
      }
      if (fault == Fault::Disconnect) ok = check(::shutdown(fd, SHUT_RDWR) == 0, "abrupt consumer socket disconnect");
      auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
      while (!client.closed() && std::chrono::steady_clock::now() < deadline) client.poll(10);
      ok = check(client.closed(), "loss observed within deadline") && ok;
      ok = draw(client, scene, context, target.get(), false, &preserved, false)
          && check(preserved, "imported last image survives producer/connection death") && ok;
    }
    if (ok && fault == Fault::Web) {
      ok = check(client.command(CRTWEB_GPU_EVALUATE, "document.body.style.background='#ff00ff'"), "paint before browser loss")
          && check(await_colour(client, scene, context, target.get(), true, before), "magenta before browser loss");
      before = client.frame_serial();
      bool recovered = false;
      for (const auto& peer : peers) {
        if (!ok || peer.name != "WebProcess") continue;
        ok = check(peer.pid > 0 && kill(peer.pid, SIGKILL) == 0, "kill owned Web process candidate");
        // A prewarmed process is not necessarily the active page. A fresh green
        // frame can only be a real reload of the fixture after the active loss.
        if (ok && await_colour(client, scene, context, target.get(), false, before)) { recovered = true; break; }
      }
      ok = check(recovered, "replacement browser process paints new shared GPU pixels") && ok;
      auto replacement = browser_tree(child.pid); peers.insert(peers.end(), replacement.begin(), replacement.end());
    }
    if (ok && fault == Fault::Resize) {
      ok = check(client.resize(130, 90), "resize while old GPU image is retained");
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      ok = draw(client, scene, context, target.get(), false, &preserved, false)
          && check(preserved && client.frame_serial() == before, "old generation immutable while resize queued") && ok;
      ok = check(await_colour(client, scene, context, target.get(), false, before, 130, 90), "replacement resource generation") && ok;
      ok = check(client.generation() == 2 && client.live_buffers() <= CRTWEB_GPU_MAX_BUFFERS, "bounded old/new pools") && ok;
    }
    if (ok && fault == Fault::Context) {
      // Controlled context-loss boundary, not an OS-wide physical GPU reset.
      context->abandonContext(); client.poll(0);
      auto provider = client.surface_provider();
      SkImage* denied = provider.acquire(provider.user, nullptr);
      ok = check(client.gpu_lost() && client.closed() && !client.has_frame() && !denied
          && !client.frame_presented(), "lost context exposes no GPU image or ACK");
      if (denied) provider.release(provider.user, nullptr, denied);
    }
    target.reset();
    bool drained = client.shutdown();
    // A killed/disconnected peer cannot receive BYE, but received descriptors
    // must still be drained and local objects must have zero remaining slots.
    ok = check(drained && client.live_buffers() == 0, "channel shutdown drains descriptors and clears all imports") && ok;
  }
  ok = check(child.finish(fault == Fault::Worker || fault == Fault::Host), "fault-specific owned worker exit/reap") && ok;
  ok = gone(peers) && ok;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  printf("crtweb_gpu_lifecycle_test: case=%s result=%s elapsed_ms=%lld\n", fault_name(fault), ok ? "pass" : "fail", static_cast<long long>(ms));
  fflush(stdout);
  return ok && failures == 0;
}
bool drain_unread_transfers(crtgfx_gpu_device* device, GrDirectContext* context) {
#if defined(_WIN32)
  // The first CRT socketpair starts process-wide Winsock services. Their
  // persistent handles are transport initialization, not leaked GPU transfers:
  // a GPU-free open/close probe grows once, then stays flat. Initialize that
  // transport before the baseline, retaining the exact same +2 cleanup bound.
  int warm[2];
  if (!check(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, warm) == 0,
             "initialize Windows socket transport before transfer baseline")) return false;
  bool closed_first = close(warm[0]) == 0, closed_second = close(warm[1]) == 0;
  if (!check(closed_first && closed_second, "transport warm-up sockets closed")) return false;
#endif
  auto baseline = crtweb_test::resources();
  int sockets[2];
  if (!check(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) == 0, "queued-transfer socket pair")) return false;
  bool ok = true;
  std::unique_ptr<crtgfx_skia_shared_image> images[CRTWEB_GPU_POOL_SIZE];
  for (unsigned i = 0; i < CRTWEB_GPU_POOL_SIZE; ++i) {
    if (crtgfx_skia_shared_create(device, 32, 32, &images[i]) != CRTGFX_OK) { ok = false; break; }
    int handles[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
    if (images[i]->export_handles(handles) != CRTGFX_OK) { ok = false; break; }
    auto packet = crtweb_gpu_message(CRTWEB_GPU_BUFFER_ADD);
    packet.generation = 1; packet.buffer_id = i + 1; packet.payload.image = images[i]->description();
    ok = crtweb_gpu_send(sockets[0], &packet, handles, crtgfx_shared_image_handle_count(&packet.payload.image)) == 0;
    crtweb_gpu_close_handles(handles);
    if (!ok) break;
  }
  bool goodbye = false;
  std::thread sender([&] {
    struct pollfd ready {sockets[0], POLLIN, 0};
    if (poll(&ready, 1, 6000) > 0) {
      crtweb_gpu_packet packet; int handles[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
      goodbye = crtweb_gpu_recv(sockets[0], &packet, handles) == 1 && packet.type == CRTWEB_GPU_BYE;
      crtweb_gpu_close_handles(handles);
    }
    close(sockets[0]);
  });
  {
    // Deterministically prequeued real native handles, not a timing race with
    // browser painting. In particular Windows already owns the duplicated NT
    // handles here. A retiring client must drain even an unnegotiated channel.
    crtweb::GpuSurfaceClient client(sockets[1], device, context);
    ok = check(client.shutdown() && client.discarded_imports() == CRTWEB_GPU_POOL_SIZE
        && client.imports() == 0 && client.live_buffers() == 0, "queued native transfer handles closed without import") && ok;
  }
  sender.join();
  ok = check(goodbye, "queued-transfer peer received BYE") && ok;
  for (auto& image : images) image.reset();
  auto settled = crtweb_test::resources();
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((settled.descriptors > baseline.descriptors + 2 || settled.threads > baseline.threads + 2)
      && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    settled = crtweb_test::resources();
  }
  printf("crtweb_gpu_lifecycle_test: queued-transfer resources before=%ld/%ld/%ld after=%ld/%ld/%ld (KiB/threads/handles)\n",
      baseline.memoryKiB, baseline.threads, baseline.descriptors,
      settled.memoryKiB, settled.threads, settled.descriptors);
  ok = check(baseline.descriptors >= 0 && settled.descriptors >= 0 && settled.descriptors <= baseline.descriptors + 2
      && baseline.threads >= 0 && settled.threads >= 0 && settled.threads <= baseline.threads + 2,
      "queued-transfer native handles/lease threads are reclaimed") && ok;
  if (ok) puts("crtweb_gpu_lifecycle_test: queued_transfers=3 imported=0 cleanup=pass");
  return ok;
}
bool lifecycle(const char* worker, const char* browser, crtgfx_gpu_device*& device, sk_sp<GrDirectContext>& context) {
  const Fault cases[] = {Fault::Normal, Fault::Disconnect, Fault::Worker, Fault::Host, Fault::Web, Fault::Context, Fault::Resize};
  auto session = [&](Fault fault, bool maps) {
    bool ok = lifecycle_session(worker, browser, device, context.get(), fault, maps);
    if (ok && fault == Fault::Context) {
      // The destructive backend hook is safe ONLY after all borrowed images,
      // surfaces and the abandoned context have been destroyed.
      context.reset();
      ok = check(crtgfx_gpu_test_force_device_loss(device) == CRTGFX_OK, "owner-side native device teardown");
      crtgfx_gpu_device_release(device); device = nullptr;
      ok = check(crtgfx_gpu_device_create(0, &device) == CRTGFX_OK, "fresh native device after controlled loss") && ok;
      if (device) context = crtgfx_skia_make_gpu_context(device);
      ok = check(context != nullptr, "fresh Ganesh context after native device recreation") && ok;
    }
    return ok;
  };
  if (!drain_unread_transfers(device, context.get())) return false;
  // Warm every exit/fault policy once before measuring complete lifetimes.
  for (Fault fault : cases) if (!session(fault, false)) return false;
  auto baseline = crtweb_test::resources();
  unsigned counts[7] {};
  for (unsigned i = 0; i < 20; ++i) {
    unsigned index = i % 7;
    if (!session(cases[index], i == 0)) return false;
    ++counts[index];
    if (!check(crtweb_test::children().empty(), "no worker or browser remains between lifetimes")) return false;
  }
  // In particular prove fresh GPU pixels after the final controlled loss.
  if (!session(Fault::Normal, false)) return false;
  auto after = crtweb_test::resources();
  bool bounded = baseline.memoryKiB >= 0 && after.memoryKiB >= 0 && after.memoryKiB - baseline.memoryKiB <= 24 * 1024
      && baseline.threads >= 0 && after.threads - baseline.threads <= 2 && after.threads >= 0
      && baseline.descriptors >= 0 && after.descriptors >= 0 && after.descriptors - baseline.descriptors <= 2;
  printf("crtweb_gpu_lifecycle_test: cycles=20 rss=%+ld KiB threads=%+ld descriptors=%+ld\n",
      after.memoryKiB - baseline.memoryKiB, after.threads - baseline.threads, after.descriptors - baseline.descriptors);
  for (unsigned i = 0; i < 7; ++i) printf("crtweb_gpu_lifecycle_test: coverage=%s count=%u\n", fault_name(cases[i]), counts[i]);
  return check(bounded, "complete GPU/worker lifetimes have bounded resources") && failures == 0;
}
}

extern "C" int main(int argc, char** argv) {
  bool required = false, fallback = false, mismatch = false, lifecycle_mode = false; unsigned frames = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--require-gpu")) required = true;
    else if (!strcmp(argv[i], "--probe-fallback")) fallback = true;
    else if (!strcmp(argv[i], "--probe-declared-mismatch")) mismatch = true;
    else if (!strcmp(argv[i], "--lifecycle")) lifecycle_mode = true;
    else if (!strcmp(argv[i], "--window-frames") && i + 1 < argc) {
      char* end = nullptr; unsigned long n = strtoul(argv[++i], &end, 10);
      if (!end || *end || !n || n > 300) return 2;
      frames = static_cast<unsigned>(n);
    } else return 2;
  }
  if (lifecycle_mode && (frames || fallback || mismatch)) return 2;
  const char* worker = getenv("CRTWEB_GPU_WORKER"), *browser = getenv("CRTWEB_HOST");
  if (!worker || !browser || !*worker || !*browser) return required ? 1 : 77;
  crtgfx_gpu_device* device = nullptr;
  // Negative control only: an application device that is not the worker's (the worker always uses
  // device 0) makes the identity negotiation fail honestly, so --probe-fallback exercises the real
  // "unsupported pair -> explicit CPU mode, zero resources" path instead of a simulated one.
  unsigned device_index = 0;
  if (const char* index = getenv("CRTWEB_GPU_TEST_APP_DEVICE_INDEX")) device_index = static_cast<unsigned>(atoi(index));
  if (crtgfx_gpu_device_create(device_index, &device) != CRTGFX_OK) return required ? 1 : 77;
  crtgfx_shared_image_desc identity {};
  bool supported = crtgfx_skia_shared_query(device, &identity) == CRTGFX_OK;
  if (supported) {
    char device_id[33], driver_id[33];
    for (unsigned i = 0; i < 16; ++i) {
      snprintf(device_id + 2 * i, 3, "%02x", identity.device_uuid[i]);
      snprintf(driver_id + 2 * i, 3, "%02x", identity.driver_uuid[i]);
    }
    printf("crtweb_gpu_resource_test: app_device=%u backend=%u device_id=%s driver_id=%s\n", device_index, identity.backend,
        device_id, driver_id);
  }
  if (!supported && !fallback) { crtgfx_gpu_device_release(device); return required ? 1 : 77; }
  if (mismatch) {
    bool ok = supported && declared_mismatch(worker, browser, identity);
    crtgfx_gpu_device_release(device);
    return ok && !required ? 0 : 1;
  }
  auto context = crtgfx_skia_make_gpu_context(device);
  bool ok = context && (lifecycle_mode ? lifecycle(worker, browser, device, context)
                                      : run(worker, browser, device, context.get(), frames, fallback));
  context.reset(); crtgfx_gpu_device_release(device);
  if (!ok) return 1;
  if (lifecycle_mode) {
    printf("crtweb_gpu_lifecycle_test: ok backend=%u cycles=20 device_loss=controlled-context-and-owner "
        "worker_loss=pass browser_loss=pass disconnect=pass held_resize=pass recovery=pass cleanup=pass\n", identity.backend);
    return 0;
  }
  // backend= is the crtgfx backend enum of the negotiated descriptor (Vulkan 3, D3D12 1); sync= names what
  // the native handles actually were: Vulkan semaphore + fence pairs, or one shared D3D12 fence per direction.
  const char* sync = identity.backend == CRTGFX_GPU_BACKEND_D3D12 ? "native-shared-fence" :
      identity.backend == CRTGFX_GPU_BACKEND_METAL ? "native-shared-event" : "native-fence-semaphore";
  if (fallback) printf("crtweb_gpu_resource_test: fallback mode=cpu resources=0\n");
  else printf("crtweb_gpu_resource_test: ok backend=%u source=browser-cpu worker_upload=1 consumer_upload=0 "
      "transport=shared-gpu sync=%s updates=20 resize=pass ownership=pass cleanup=pass\n", identity.backend, sync);
  return 0;
}
