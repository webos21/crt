/* Tranche 9A: real browser output through the CRT GPU final compositor.
 * No WebKit/private Skia type is linked into this application. The source is
 * CPU shared memory (explicitly not zero-copy); target/render/present are GPU.
 * Default: offscreen. --window-frames N additionally presents a bounded live
 * window. --require-gpu turns unavailable prerequisites into failures.
 */
#include "crtweb/skia.h"
#include "crtgfx/skia.h"
#include "../common/crtweb_test_process.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <set>
#include <thread>
#include <vector>

namespace {
#if defined(_WIN32)
// Acceptance observer only, matching the existing Windows lifecycle probe.
// Native declarations never enter the CRT public surface or the GPU companion.
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
int failures = 0;
bool check(bool ok, const char* name) {
  if (!ok) { fprintf(stderr, "crtweb_webview_gpu_test: FAILED %s\n", name); ++failures; }
  return ok;
}

struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget below = CRTUI_INVALID_WIDGET, clip = CRTUI_INVALID_WIDGET;
  crtui_widget view = CRTUI_INVALID_WIDGET, above = CRTUI_INVALID_WIDGET;
  ~Scene() { if (ui) crtui_context_destroy(ui); }
  bool size(int width, int height) {
    return crtui_window_set_size(ui, window, width, height, 1) == CRTUI_OK
        && crtui_widget_set_bounds(ui, below, 0, 0, width, height) == CRTUI_OK
        && crtui_widget_set_bounds(ui, clip, 40, 20, width - 90, height - 50) == CRTUI_OK
        && crtui_widget_set_bounds(ui, view, -10, -10, width - 70, height - 30) == CRTUI_OK;
  }
  bool create() {
    if (crtui_context_create(&ui) != CRTUI_OK || crtui_window_create(ui, &window) != CRTUI_OK
        || crtui_container_create(ui, window, &below) != CRTUI_OK
        || crtui_container_create(ui, window, &clip) != CRTUI_OK
        || crtui_surface_view_create(ui, clip, &view) != CRTUI_OK
        || crtui_container_create(ui, window, &above) != CRTUI_OK) return false;
    crtui_style style = {};
    style.mask = CRTUI_STYLE_BACKGROUND; style.background_rgb = 0xff0000;
    if (crtui_widget_set_style(ui, below, &style) != CRTUI_OK) return false;
    style.background_rgb = 0x0000ff;
    if (crtui_widget_set_style(ui, above, &style) != CRTUI_OK
        || crtui_widget_set_bounds(ui, above, 60, 30, 40, 30) != CRTUI_OK) return false;
    style = {}; style.mask = CRTUI_STYLE_OPACITY; style.opacity = 128;
    return crtui_widget_set_style(ui, view, &style) == CRTUI_OK && size(160, 100);
  }
};

const char* page = "<!doctype html><title>gpu:green</title>"
    "<body style='margin:0;background:#00ff00'>"
    "<div style='width:24px;height:24px;background:white' "
    "onclick=\"document.body.style.background='#ff00ff';document.title='gpu:clicked'\"></div>";

bool wait_for(crtweb::WebView& web, const std::function<bool()>& done) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!done()) {
    if (!web.running() || std::chrono::steady_clock::now() >= end) return false;
    web.poll(10);
    // No ACK here: only a completed GPU composition/present acknowledges frames.
  }
  return true;
}

bool source_is(crtweb::WebView& web, bool magenta) {
  if (!web.has_frame()) return false;
  auto provider = web.surface_provider();
  SkImage* image = provider.acquire(provider.user, nullptr);
  if (!image) return false;
  uint8_t pixel[4] = {};
  auto info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  bool ok = image->readPixels(info, pixel, 4, 40, 40)
      && pixel[0] == (magenta ? 255 : 0) && pixel[1] == (magenta ? 0 : 255)
      && pixel[2] == (magenta ? 255 : 0) && pixel[3] == 255;
  provider.release(provider.user, nullptr, image);
  return ok;
}

void report_browser_maps() {
#if defined(_WIN32)
  auto report = [](pid_t pid, const char* role) {
    void* snapshot = crtweb_test::CreateToolhelp32Snapshot(0x8 | 0x10, static_cast<unsigned>(pid));
    if (!check(snapshot != reinterpret_cast<void*>(-1), "browser module snapshot")) return;
    std::set<std::string> files;
    GpuModuleEntry entry = {};
    entry.size = sizeof entry;
    for (int more = Module32First(snapshot, &entry); more; more = Module32Next(snapshot, &entry)) {
      std::string path = entry.path;
      for (char& c : path) if (c == '\\') c = '/';
      files.insert(path);
    }
    crtweb_test::CloseHandle(snapshot);
    check(!files.empty(), "browser modules observed");
    for (const auto& file : files) printf("MAPPED %s %s\n", role, file.c_str());
  };
#elif defined(__linux__)
  auto report = [](pid_t pid, const char* role) {
    std::string name = "/proc/" + std::to_string(pid) + "/maps";
    FILE* maps = fopen(name.c_str(), "r");
    if (!maps) return;
    std::set<std::string> files;
    char line[8192];
    while (fgets(line, sizeof line, maps)) {
      char* path = strchr(line, '/');
      if (!path || !strncmp(path, "/memfd:", 7)) continue;
      path[strcspn(path, "\r\n")] = 0;
      char permissions[5] = {};
      std::string own_memfd = "/crt_memfd_" + std::to_string(pid) + "_";
      if (sscanf(line, "%*lx-%*lx %4s", permissions) == 1 && !strchr(permissions, 'x')
          && strstr(path, own_memfd.c_str()) && strstr(path, ".tmp (deleted)")) {
        // CRT memfd_create uses an immediately unlinked file (mman.c). This
        // non-executable shared data is not a mapped host library.
        printf("ANON_SHM %s %s\n", role, path);
        continue;
      }
      files.insert(path);
    }
    fclose(maps);
    for (const auto& file : files) printf("MAPPED %s %s\n", role, file.c_str());
  };
#endif
#if defined(_WIN32) || defined(__linux__)
  for (const auto& host : crtweb_test::children()) {
    if (host.name != "crtweb_host") continue;
    report(host.pid, "HostProcess");
    for (const auto& child : crtweb_test::children(host.pid))
      if (child.name == "WebProcess" || child.name == "NetworkProcess") report(child.pid, child.name.c_str());
  }
#endif
}

bool pixels_are(SkSurface* target, bool magenta) {
  size_t stride = static_cast<size_t>(target->width()) * 4;
  std::vector<uint8_t> pixels(stride * target->height());
  auto info = SkImageInfo::Make(target->width(), target->height(), kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  if (!check(target->readPixels(info, pixels.data(), stride, 0, 0), "GPU readback")) return false;
  auto rgb = [&](int x, int y, int r, int g, int b) {
    const uint8_t* p = pixels.data() + y * stride + x * 4;
    auto near = [](int a, int v) { return abs(a - v) <= 3; };
    return near(p[2], r) && near(p[1], g) && near(p[0], b) && p[3] >= 250;
  };
  return check(rgb(20, 20, 255, 0, 0), "UI below")
      && check(rgb(35, 25, 255, 0, 0), "ancestor clipping")
      && check(magenta ? rgb(50, 50, 255, 0, 128) : rgb(50, 50, 127, 128, 0), "web pixel/opacity")
      && check(rgb(70, 40, 0, 0, 255), "UI above");
}

bool compose(crtweb::WebView& web, Scene& scene, GrDirectContext* context, SkSurface* target, bool magenta) {
  if (!check(target && target->recordingContext() == context, "GPU target/context identity")) return false;
  if (!check(crtweb::compose_gpu(web, scene.ui, scene.window, context, target) == CRTUI_OK, "GPU compose")) return false;
  bool ok = pixels_are(target, magenta);  // acceptance readback only, never a production round trip
  if (ok) web.frame_presented();
  return ok;
}

bool live(crtweb::WebView& web, Scene& scene, crtgfx_gpu_device* device, GrDirectContext* context, unsigned frames) {
  crtgfx_window_desc desc = {"CRT WebView GPU composition (CPU upload)", 320, 200,
      CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION};
  crtgfx_window* window = nullptr;
  crtgfx_gpu_surface* gpu = nullptr;
  bool ok = check(crtgfx_window_create(&desc, &window) == CRTGFX_OK, "live window");
  if (ok) ok = check(crtgfx_gpu_surface_create(device, window, &gpu) == CRTGFX_OK, "live GPU surface");
  uint32_t width = 0, height = 0;
  if (ok) ok = check(crtgfx_gpu_surface_get_size(gpu, &width, &height) == CRTGFX_OK, "live size");
  uint64_t before = web.frame_serial();
  if (ok) {
    ok = check(scene.size(width, height), "live UI size");
    web.resize(width - 70, height - 30);
    ok = ok && check(wait_for(web, [&] { return web.frame_serial() > before
        && web.surface()->frame_width() == width - 70 && web.surface()->frame_height() == height - 30; }), "live viewport");
  }
  unsigned presented = 0;
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (ok && presented < frames && !crtgfx_window_should_close(window)
      && std::chrono::steady_clock::now() < end) {
    crtgfx_result acquired = crtgfx_gpu_surface_acquire(gpu, 1000000);
    if (acquired == CRTGFX_ERROR_TIMEOUT) { crtgfx_window_pump_events(1); continue; }
    if (!check(acquired == CRTGFX_OK, "live acquire")) { ok = false; break; }
    auto target = crtgfx_skia_wrap_gpu_surface(context, gpu);
    ok = check(target != nullptr && target->recordingContext() == context, "live GPU wrap");
    if (ok) ok = check(crtweb::compose_gpu(web, scene.ui, scene.window, context, target.get()) == CRTUI_OK, "live compose");
    if (ok) ok = check(crtgfx_skia_gpu_surface_present(context, target.get(), gpu) == CRTGFX_OK, "live present");
    if (ok) { ++presented; web.frame_presented(); }
    web.poll(0);
    crtgfx_window_pump_events(1);
  }
  if (gpu) crtgfx_gpu_surface_release(gpu);
  if (window) crtgfx_window_destroy(window);
  printf("crtweb_webview_gpu_test: presented=%u requested=%u\n", presented, frames);
  return ok && check(presented == frames, "bounded live presentation count");
}

bool run(const char* host, crtgfx_gpu_device* device, GrDirectContext* context, unsigned window_frames) {
  Scene scene;
  if (!check(scene.create(), "UI scene")) return false;
  crtweb::WebView web;
  auto target = crtgfx_skia_make_gpu_offscreen_surface(context, 160, 100);
  auto raster = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(160, 100));
  if (!check(target != nullptr && raster != nullptr, "initial targets")) return false;
  check(crtweb::compose_gpu(web, scene.ui, scene.window, context, raster.get()) == CRTUI_ERROR_INVALID_ARGUMENT, "reject raster target");
  check(crtweb::compose_gpu(web, scene.ui, scene.window, nullptr, target.get()) == CRTUI_ERROR_INVALID_ARGUMENT, "reject null GPU context");
  check(crtweb::compose_gpu(web, scene.ui, scene.window, context, target.get()) == CRTUI_WOULD_BLOCK, "no frame before launch");
  unsigned terminations = 0;
  web.on_terminated = [&] { ++terminations; };
  if (!check(web.launch(host, 90, 70), "browser host launch")) return false;
  if (!check(web.load_html(page), "load fixture") || !check(wait_for(web, [&] {
      return web.title() == "gpu:green" && source_is(web, false); }), "first browser frame")) return false;
  // A real browser-produced raster image must not masquerade as a GPU export.
  auto provider = web.surface_provider();
  SkImage* held = provider.acquire(provider.user, nullptr);
  if (!check(held != nullptr, "retained source image")) return false;
  check(!held->isTextureBacked(), "source classified CPU-upload, not zero-copy");
  bool ok = compose(web, scene, context, target.get(), false);

  crtui_input input = {}; input.type = CRTUI_INPUT_POINTER_DOWN; input.x = 50; input.y = 25;
  crtui_rect bounds = {30, 10, 90, 70};
  uint64_t before = web.frame_serial();
  check(web.forward(input, bounds), "pointer down"); input.type = CRTUI_INPUT_POINTER_UP;
  check(web.forward(input, bounds), "pointer up");
  ok = check(wait_for(web, [&] { return web.title() == "gpu:clicked" && web.frame_serial() > before
      && source_is(web, true); }), "input changes browser pixels") && ok;
  if (ok) ok = compose(web, scene, context, target.get(), true);
  check(scene.size(200, 120), "UI resize");
  before = web.frame_serial(); web.resize(130, 90);
  ok = check(wait_for(web, [&] { return web.frame_serial() > before
      && web.surface()->frame_width() == 130 && web.surface()->frame_height() == 90
      && source_is(web, true); }), "fresh resized frame") && ok;
  target = crtgfx_skia_make_gpu_offscreen_surface(context, 200, 120);
  if (ok) ok = compose(web, scene, context, target.get(), true);
  uint8_t sample[4] = {};
  auto sample_info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  check(held->readPixels(sample_info, sample, 4, 40, 40) && sample[0] == 0 && sample[1] == 255 && sample[2] == 0,
      "retained old buffer immutable across update/resize");
  provider.release(provider.user, nullptr, held);

  // Exercise new frames and release/retire ownership without recreating the GPU context.
  auto baseline = crtweb_test::resources();
  for (unsigned i = 0; ok && i < 20; ++i) {
    before = web.frame_serial();
    bool magenta = (i & 1) != 0;
    std::string script = std::string("document.body.style.background='") + (magenta ? "#ff00ff" : "#00ff00") + "'";
    ok = check(web.evaluate(script, nullptr), "update queued")
        && check(wait_for(web, [&] { return web.frame_serial() > before && source_is(web, magenta); }), "fresh GPU-composed update")
        && compose(web, scene, context, target.get(), magenta);
  }
  auto final = crtweb_test::resources();
  printf("crtweb_webview_gpu_test: updates=20 rss=%+ld KiB threads=%+ld descriptors=%+ld\n",
      final.memoryKiB - baseline.memoryKiB, final.threads - baseline.threads, final.descriptors - baseline.descriptors);
  check(baseline.memoryKiB >= 0 && final.memoryKiB >= 0 && final.memoryKiB - baseline.memoryKiB <= 24 * 1024,
      "GPU update RSS bounded");
  check(baseline.threads >= 0 && final.threads >= 0 && final.threads - baseline.threads <= 2, "GPU update threads bounded");
  check(baseline.descriptors >= 0 && final.descriptors >= 0 && final.descriptors - baseline.descriptors <= 2,
      "GPU update descriptors/handles bounded");
  report_browser_maps();
  // Active process loss must not invalidate the last GPU-composed picture.
  auto hosts = crtweb_test::children();
  for (const auto& child : hosts) {
    if (child.name != "crtweb_host") continue;
    for (const auto& process : crtweb_test::children(child.pid)) {
      if (process.name != "WebProcess") continue;
      check(kill(process.pid, SIGKILL) == 0, "terminate browser producer");
      // Cached/prewarmed processes can exist on replay hosts. Stop at the
      // active page's callback rather than killing the whole pool at once.
      if (wait_for(web, [&] { return terminations != 0; })) break;
    }
  }
  ok = check(wait_for(web, [&] { return terminations != 0; }), "producer termination event") && ok;
  check(terminations == 1, "one active producer termination");
  if (ok) ok = compose(web, scene, context, target.get(), true);
  before = web.frame_serial();
  check(web.load_html(page), "load after process loss");
  ok = check(wait_for(web, [&] { return web.title() == "gpu:green" && web.frame_serial() > before
      && source_is(web, false); }), "replacement browser frame") && ok;
  if (ok) ok = compose(web, scene, context, target.get(), false);
  if (ok && window_frames) ok = live(web, scene, device, context, window_frames);
  // Completion is explicit before polling or releasing CPU shared-memory images.
  context->flushAndSubmit(GrSyncCpu::kYes);
  target.reset(); raster.reset();
  web.shutdown();
  auto remaining = crtweb_test::children();
  for (const auto& child : remaining)
    fprintf(stderr, "crtweb_webview_gpu_test: remaining child pid=%ld name=%s\n",
        static_cast<long>(child.pid), child.name.c_str());
  check(!web.running() && remaining.empty(), "browser children reaped");
  return ok && failures == 0;
}
}  // namespace

extern "C" int main(int argc, char** argv) {
  bool required = false;
  unsigned frames = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--require-gpu")) required = true;
    else if (!strcmp(argv[i], "--window-frames") && i + 1 < argc) {
      char* end = nullptr;
      unsigned long parsed = strtoul(argv[++i], &end, 10);
      if (!end || *end || parsed == 0 || parsed > 300) return 2;
      frames = static_cast<unsigned>(parsed);
    } else return 2;
  }
  const char* host = getenv("CRTWEB_HOST");
  crtgfx_gpu_capabilities caps = {};
  if (!host || !*host || crtgfx_gpu_query_capabilities(&caps) != CRTGFX_OK || caps.device_count == 0) {
    printf("crtweb_webview_gpu_test: %s -- browser host or GPU unavailable\n", required ? "FAILED" : "skipped");
    return required ? 1 : 77;
  }
  crtgfx_gpu_device* device = nullptr;
  if (!check(crtgfx_gpu_device_create(0, &device) == CRTGFX_OK, "GPU device")) return 1;
  auto context = crtgfx_skia_make_gpu_context(device);
  bool ok = check(context != nullptr, "Ganesh context") && run(host, device, context.get(), frames);
  context.reset();
  crtgfx_gpu_device_release(device);
  if (!ok || failures) return 1;
  printf("crtweb_webview_gpu_test: ok backend=%u source=cpu-shm interop=cpu-upload gpu_target=pass "
      "pixels=pass clip=pass opacity=pass ordering=pass input=pass resize=pass updates=20 recovery=pass cleanup=pass\n",
      static_cast<unsigned>(caps.backend));
  return 0;
}
