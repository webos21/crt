/* Web Tranche 4, milestone C2: the crtui WebView (crtweb::WebView) against a real crtweb_host.
 *
 * Needs the host executable built by tools/build_webkit_crt_port.py (the PlatformCRT port), named by
 * the CRTWEB_HOST environment variable; without it the test reports "skipped" (the in-tree CTest
 * registration runs everywhere, the driver's webview gate runs it for real). A crtui window with a
 * SurfaceView at (40,20) 200x150 shows the web page through the final compositor; the test drives
 * navigation, page control events, history, script evaluation, crtui pointer input, resize and
 * shutdown through crtweb::WebView only. */

#include "crtweb/webview.h"

#include "../common/crtweb_test_fs.h"
#include "../common/crtweb_test_http_server.h"
#include "../common/crtweb_test_process.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <unistd.h>

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;

bool check(bool ok, const char* name) {
  if (!ok) {
    fprintf(stderr, "crtweb_webview_test: FAILED %s\n", name);
    ++failures;
  }
  return ok;
}

void write_file(const std::string& path, const std::string& content) {
  FILE* file = fopen(path.c_str(), "wb");
  if (file == nullptr) {
    fprintf(stderr, "crtweb_webview_test: cannot write %s\n", path.c_str());
    exit(2);
  }
  fwrite(content.data(), 1, content.size(), file);
  fclose(file);
}

struct Rgb { int r, g, b; };

bool near_rgb(Rgb c, int r, int g, int b) {
  auto near = [](int v, int e) { return v >= e - 3 && v <= e + 3; };
  return near(c.r, r) && near(c.g, g) && near(c.b, b);
}

struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget surface = CRTUI_INVALID_WIDGET;
};

bool make_scene(Scene* scene) {
  crtui_widget below = CRTUI_INVALID_WIDGET;
  if (crtui_context_create(&scene->ui) != CRTUI_OK || crtui_window_create(scene->ui, &scene->window) != CRTUI_OK ||
      crtui_window_set_size(scene->ui, scene->window, 320, 240, 1.0f) != CRTUI_OK ||
      crtui_container_create(scene->ui, scene->window, &below) != CRTUI_OK ||
      crtui_widget_set_bounds(scene->ui, below, 0, 0, 320, 240) != CRTUI_OK ||
      crtui_surface_view_create(scene->ui, scene->window, &scene->surface) != CRTUI_OK ||
      crtui_widget_set_bounds(scene->ui, scene->surface, 40, 20, 200, 150) != CRTUI_OK)
    return false;
  crtui_style style = {};
  style.mask = CRTUI_STYLE_BACKGROUND;
  style.background_rgb = 0xFF0000u;
  return crtui_widget_set_style(scene->ui, below, &style) == CRTUI_OK;
}

crtui_rect surface_bounds(Scene* scene) {
  crtui_surface_layer layer = {};
  size_t count = 0;
  crtui_rect none = {0, 0, 0, 0};
  if (crtui_window_get_surface_layers(scene->ui, scene->window, &layer, 1, &count) != CRTUI_OK || count != 1) return none;
  return layer.bounds;
}

}  // namespace

extern "C" int main() {
  const char* host = getenv("CRTWEB_HOST");
  if (host == nullptr || *host == '\0') {
    printf("crtweb_webview_test: skipped (CRTWEB_HOST is not set)\n");
    return 0;
  }
  char cwd[4096];
  if (getcwd(cwd, sizeof cwd) == nullptr) return 2;
  const std::string directory = cwd;
  const std::string page_a = directory + "/crtweb_webview_a.html";
  const std::string page_b = directory + "/crtweb_webview_b.html";
  write_file(page_a, "<!doctype html><title>view a</title><body style='margin:0;background:#00aa00'>"
                     "<div style='width:100px;height:100px;background:#ffffff' onclick=\"document.title='clicked'\"></div>");
  write_file(page_b, "<!doctype html><title>view b</title><body style='margin:0;background:#0000ff'>");

  Scene scene;
  if (!make_scene(&scene)) {
    fprintf(stderr, "crtweb_webview_test: cannot build the crtui scene\n");
    return 1;
  }
  sk_sp<SkSurface> target = SkSurfaces::Raster(SkImageInfo::Make(320, 240, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
  std::vector<uint8_t> pixels(320 * 240 * 4);
  SkImageInfo readback = SkImageInfo::Make(320, 240, kBGRA_8888_SkColorType, kPremul_SkAlphaType);

  crtweb::WebView web;
  struct Seen {
    std::vector<std::string> loads;
    std::string title, url;
    unsigned history = 0;
    bool closed = false;
  } seen;
  web.on_load = [&](crtweb::WebView::LoadEvent event, const std::string& url, const std::string& message) {
    seen.loads.push_back(std::to_string(static_cast<int>(event)) + ":" + url + (message.empty() ? "" : "|" + message));
  };
  web.on_title = [&](const std::string& title) { seen.title = title; };
  web.on_url = [&](const std::string& url) { seen.url = url; };
  web.on_history = [&]() { ++seen.history; };
  web.on_closed = [&]() { seen.closed = true; };

  auto has_load = [&](int event, const std::string& part) {
    const std::string prefix = std::to_string(event) + ":";
    for (const std::string& load : seen.loads) {
      if (load.compare(0, prefix.size(), prefix) == 0 && load.find(part) != std::string::npos) return true;
    }
    return false;
  };
  auto compose = [&]() {
    crtui_skia_surface_provider provider = web.surface_provider();
    crtui_skia_compose(scene.ui, scene.window, target.get(), &provider);
    target->readPixels(readback, pixels.data(), 320 * 4, 0, 0);
    web.frame_presented();
  };
  auto pixel = [&](int x, int y) {
    const uint8_t* p = pixels.data() + y * 320 * 4 + x * 4;
    return Rgb{p[2], p[1], p[0]};
  };
  auto pump_until = [&](const std::function<bool()>& done, int timeout_ms = 20000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!done()) {
      if (std::chrono::steady_clock::now() >= deadline) return false;
      web.poll(20);
      if (web.has_frame()) compose();
    }
    return true;
  };

  /* A host that cannot be started is an error, not a hang. */
  {
    crtweb::WebView broken;
    check(!broken.launch("/nonexistent/crtweb_host", 200, 150, nullptr, 3000), "a missing host fails to launch");
  }

  if (!check(web.launch(host, 200, 150), "host launched and handshake completed")) return 1;
  web.focus(true);
  check(web.running(), "running after launch");

  /* ---- load_html and composition ---- */
  const std::string html = "<!doctype html><title>wv</title><body style='margin:0;background:#00aa00'>"
                           "<div style='width:100px;height:100px;background:#ffffff' onclick=\"document.title='clicked'\"></div>";
  check(web.load_html(html), "load_html sent");
  check(pump_until([&] { return has_load(3, "about:blank") && seen.title == "wv"; }), "html finished and titled");
  check(pump_until([&] { return web.has_frame() && near_rgb(pixel(40 + 150, 20 + 100), 0, 170, 0) && near_rgb(pixel(40 + 50, 20 + 50), 255, 255, 255); }),
        "the page is composed under the crtui scene (white box, green page)");
  check(near_rgb(pixel(5, 5), 255, 0, 0), "crtui below shows around the view");

  /* ---- input: a crtui click lands in the page ---- */
  {
    crtui_input input = {};
    input.window = scene.window;
    input.x = 40 + 50;
    input.y = 20 + 50;
    input.type = CRTUI_INPUT_POINTER_MOVE;
    check(web.forward(input, surface_bounds(&scene)), "move forwarded");
    input.type = CRTUI_INPUT_POINTER_DOWN;
    check(web.forward(input, surface_bounds(&scene)), "down forwarded");
    input.type = CRTUI_INPUT_POINTER_UP;
    check(web.forward(input, surface_bounds(&scene)), "up forwarded");
    check(pump_until([&] { return seen.title == "clicked"; }, 10000), "the click reached the page (title callback)");
  }

  /* ---- script ---- */
  {
    std::string result;
    bool error = true, got = false;
    check(web.evaluate("6*7", [&](const std::string& value, bool is_error) { result = value; error = is_error; got = true; }), "evaluate sent");
    check(pump_until([&] { return got; }) && result == "42" && !error, "script result");
    got = false;
    check(web.evaluate("throw new Error('x')", [&](const std::string&, bool is_error) { error = is_error; got = true; }), "throwing script sent");
    check(pump_until([&] { return got; }) && error, "script error");
  }

  /* ---- navigation and history over file: URLs ---- */
  seen.loads.clear();
  check(web.load_url("file://" + page_a), "load page a");
  check(pump_until([&] { return has_load(3, "crtweb_webview_a.html") && seen.title == "view a"; }), "page a loaded");
  check(has_load(1, "crtweb_webview_a.html") && has_load(2, "crtweb_webview_a.html"), "started and committed events");
  check(pump_until([&] { return seen.url.find("crtweb_webview_a.html") != std::string::npos; }), "url event");
  seen.loads.clear();
  check(web.load_url("file://" + page_b), "load page b");
  check(pump_until([&] { return has_load(3, "crtweb_webview_b.html") && seen.title == "view b"; }), "page b loaded");
  check(pump_until([&] { return web.can_go_back() && !web.can_go_forward(); }), "history: can go back");
  check(pump_until([&] { return web.has_frame() && near_rgb(pixel(40 + 100, 20 + 100), 0, 0, 255); }), "page b is blue");
  seen.loads.clear();
  check(web.go_back(), "go back");
  check(pump_until([&] { return has_load(3, "crtweb_webview_a.html") && seen.title == "view a"; }), "back to page a");
  check(pump_until([&] { return web.can_go_forward(); }), "history: can go forward");
  seen.loads.clear();
  check(web.load_url("file:///crtweb_webview_missing.html"), "load a missing file");
  check(pump_until([&] { return has_load(4, "missing"); }), "load failure event");
  check(!web.load_url(std::string(2 * 1024 * 1024, 'x')), "an argument over the control limit is refused");

  /* ---- Tranche 4: website data, downloads, dialogs, popups, input method over the protocol ---- */
  {
    HttpServer server;
    auto evaluate_string = [&](crtweb::WebView& view, const std::string& script) {
      std::string result = "<none>";
      bool got = false;
      view.evaluate(script, [&](const std::string& value, bool is_error) {
        result = is_error ? "<error>" : value;
        got = true;
      });
      pump_until([&] { return got; });
      return result;
    };
    auto load_titled = [&](crtweb::WebView& view, std::string& title_slot, const std::string& url, const std::string& wanted) {
      view.load_url(url);
      return pump_until([&] { return title_slot == wanted; });
    };

    // Downloads: the application chooses the destination; the bytes arrive intact.
    std::string saved_path = directory + "/crtweb_webview_download.bin";
    struct { bool finished = false; crtweb::WebView::DownloadStatus status{}; uint64_t last = 0; std::string suggested; } download;
    web.on_download_request = [&](const crtweb::WebView::DownloadInfo& info) {
      download.suggested = info.suggested_filename;
      return saved_path;
    };
    web.on_download_progress = [&](uint32_t, uint64_t received, uint64_t) { download.last = received; };
    web.on_download_finished = [&](uint32_t, crtweb::WebView::DownloadStatus status, const std::string&) {
      download.finished = true;
      download.status = status;
    };
    check(load_titled(web, seen.title, server.url("/page"), "http page"), "http page through the WebView");
    web.load_url(server.url("/dl"));
    check(pump_until([&] { return download.finished; }), "download finished over the protocol");
    check(download.status == crtweb::WebView::DownloadStatus::Ok && download.suggested == "blob.bin" && download.last == kDownloadSize,
          "download status, filename and size");
    {
      std::string got;
      if (FILE* file = fopen(saved_path.c_str(), "rb")) {
        char buffer[8192];
        size_t length;
        while ((length = fread(buffer, 1, sizeof buffer, file)) > 0) got.append(buffer, length);
        fclose(file);
      }
      check(got == downloadBody(), "downloaded bytes match");
      unlink(saved_path.c_str());
    }

    // Dialogs: answered by the application.
    std::vector<std::string> dialogs;
    web.on_dialog = [&](crtweb::WebView::DialogKind kind, const std::string& message, const std::string& default_text,
                        std::function<void(bool, const std::string&)> complete) {
      dialogs.push_back(std::to_string(static_cast<int>(kind)) + ":" + message);
      if (kind == crtweb::WebView::DialogKind::Prompt) complete(true, "Lee " + default_text);
      else complete(true, std::string());
    };
    check(evaluate_string(web, "confirm('sure?')") == "true" && dialogs.size() == 1 && dialogs[0] == "2:sure?", "confirm over the protocol");
    check(evaluate_string(web, "prompt('who?', 'x')") == "Lee x", "prompt answer reaches the page");

    // Popups: the host blocks the window and reports the URL; the application opens a second WebView
    // (a second host process and runtime) for it.
    check(load_titled(web, seen.title, server.url("/opener"), "opener"), "opener page");
    std::string popup_url;
    web.on_popup_requested = [&](const std::string& url) { popup_url = url; };
    crtui_input click = {};
    click.window = scene.window;
    click.x = 40 + 50;
    click.y = 20 + 50;
    click.type = CRTUI_INPUT_POINTER_MOVE;
    web.forward(click, surface_bounds(&scene));
    click.type = CRTUI_INPUT_POINTER_DOWN;
    web.forward(click, surface_bounds(&scene));
    click.type = CRTUI_INPUT_POINTER_UP;
    web.forward(click, surface_bounds(&scene));
    check(pump_until([&] { return !popup_url.empty(); }), "popup request reported");
    check(popup_url.find("/popup-target") != std::string::npos, "popup url");
    {
      crtweb::WebView second;
      std::string second_title;
      second.on_title = [&](const std::string& title) { second_title = title; };
      check(second.launch(host, 120, 90, popup_url.c_str()), "a second WebView (second host and runtime)");
      auto both = [&](const std::function<bool()>& done) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20000);
        while (!done()) {
          if (std::chrono::steady_clock::now() >= deadline) return false;
          web.poll(10);
          second.poll(10);
        }
        return true;
      };
      check(both([&] { return second_title == "popup"; }), "the second WebView loads the popup page");
      check(seen.title == "opener", "the first WebView is unaffected");
      second.shutdown();
    }

    // Input method: Hangul composition and commit into a field.
    check(load_titled(web, seen.title, server.url("/ime"), "ime"), "ime page");
    click.x = 40 + 20;
    click.y = 20 + 12;
    click.type = CRTUI_INPUT_POINTER_DOWN;
    web.forward(click, surface_bounds(&scene));
    click.type = CRTUI_INPUT_POINTER_UP;
    web.forward(click, surface_bounds(&scene));
    pump_until([&] { return false; }, 150);
    check(web.ime_set_composition("\xED\x95\x9C", 1, 0), "composition sent");
    check(web.ime_commit("\xED\x95\x9C\xEA\xB8\x80"), "commit sent");
    check(pump_until([&] { return evaluate_string(web, "document.getElementById('t').value") == "\xED\x95\x9C\xEA\xB8\x80"; }),
          "the committed Hangul is the field value");
    check(evaluate_string(web, "window.log.join('|')").find("compositionend") != std::string::npos, "composition events reached the page");

    // Website data: ephemeral by default; a data directory persists cookies across host restarts; clearing works.
    check(load_titled(web, seen.title, server.url("/set"), "set"), "cookie set (ephemeral host)");
    check(load_titled(web, seen.title, server.url("/read"), "cookie:sid=abc123"), "cookie read back");
    bool cleared = false;
    check(web.clear_data(crtweb::WebView::kDataCookies | crtweb::WebView::kDataCache, [&] { cleared = true; }), "clear_data sent");
    check(pump_until([&] { return cleared; }), "clear_data completed");
    check(load_titled(web, seen.title, server.url("/page"), "http page") && load_titled(web, seen.title, server.url("/read"), "cookie:"),
          "no cookie after clearing");
    {
      std::string data = directory + "/crtweb_webview_data";
      if (!crtwebRemoveTree(data)) fprintf(stderr, "crtweb_webview_test: could not clear %s\n", data.c_str());
      crtweb::WebView::LaunchOptions options;
      options.data_directory = data;
      std::string title;
      {
        crtweb::WebView first;
        first.on_title = [&](const std::string& t) { title = t; };
        check(first.launch(host, 100, 75, nullptr, 60000, options), "host with a data directory");
        first.load_url(server.url("/set"));
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (title != "set" && std::chrono::steady_clock::now() < deadline) first.poll(20);
        check(title == "set", "cookie set in the persistent host");
        first.shutdown();
      }
      {
        crtweb::WebView again;
        title.clear();
        again.on_title = [&](const std::string& t) { title = t; };
        check(again.launch(host, 100, 75, nullptr, 60000, options), "host restarted on the same directory");
        again.load_url(server.url("/read"));
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (title != "cookie:sid=abc123" && std::chrono::steady_clock::now() < deadline) again.poll(20);
        check(title == "cookie:sid=abc123", "the cookie survived the host restart");
        again.shutdown();
      }
    }
  }

  /* ---- resize ---- */
  check(crtui_widget_set_bounds(scene.ui, scene.surface, 40, 20, 100, 75) == CRTUI_OK, "shrink the view");
  web.resize(100, 75);
  bool resized = pump_until([&] { return web.surface()->frame_width() == 100 && web.surface()->frame_height() == 75; }, 8000);
  if (!resized)
    fprintf(stderr, "crtweb_webview_test: frame is %ux%u serial %llu running %d\n", web.surface()->frame_width(),
            web.surface()->frame_height(), static_cast<unsigned long long>(web.frame_serial()), web.running() ? 1 : 0);
  check(resized, "frame at the new size");

  /* Tranche 5: kill the actual WebProcess behind crtweb_host, receive the termination event,
   * reload through the public control channel and compose a new frame. */
  unsigned terminated = 0;
  web.on_terminated = [&] { ++terminated; };
  check(web.load_url("file://" + page_a), "recovery page URL sent");
  check(pump_until([&] { return seen.title == "view a"; }), "recovery page loaded");
  auto host_children = [&] {
    std::vector<crtweb_test::Process> result;
    for (const auto& child : crtweb_test::children())
      if (child.name == "crtweb_host") result.push_back(child);
    return result;
  };
  auto web_pids = [&](pid_t parent) {
    std::vector<pid_t> found;
    for (const auto& child : crtweb_test::children(parent))
      if (child.name == "WebProcess") found.push_back(child.pid);
    return found;
  };
  auto hosts = host_children();
  check(hosts.size() == 1, "one owned host for recovery");
  if (hosts.size() == 1) {
    for (unsigned i = 0; i < 3; ++i) {
      unsigned terminated_before = terminated;
      auto loads_finished = [&] {
        unsigned count = 0;
        for (const auto& load : seen.loads) if (load.compare(0, 2, "3:") == 0) ++count;
        return count;
      };
      unsigned finished_before = loads_finished();
      // Earlier cross-site navigation can leave cached/prewarmed WebProcesses and the public
      // consumer deliberately cannot distinguish those implementation details from the active
      // page process. Probe the observed children one at a time and stop as soon as the page's
      // termination callback identifies the active one. Killing the whole process pool at once is
      // not the public recovery contract and can race WebKit's cache replenishment on Windows.
      auto old = web_pids(hosts[0].pid);
      check(!old.empty(), "WebView has a WebProcess");
      unsigned killed = 0;
      for (pid_t pid : old) {
        check(kill(pid, SIGKILL) == 0, "kill a WebView WebProcess candidate");
        ++killed;
        if (pump_until([&] { return terminated > terminated_before; }, 3000)) break;
      }
      check(terminated > terminated_before, "WebView termination callback received");
      check(terminated == terminated_before + 1, "WebView termination callback exactly once");
      uint64_t serial = web.frame_serial();
      check(web.reload(), "reload after termination sent");
      // A replacement starts every process killed above (the active page plus cached/prewarmed
      // processes).  On a loaded Windows acceptance host that cold launch can exceed the ordinary
      // 20-second interaction timeout; use the same finite 60-second bound as the initial host
      // handshake without weakening the required finished-load, fresh-frame or pixel checks.
      check(pump_until([&] { return loads_finished() > finished_before && web.frame_serial() > serial
          && near_rgb(pixel(40 + 20, 20 + 20), 255, 255, 255); }, 60000), "recovered WebView repaints");
      auto replacement = web_pids(hosts[0].pid);
      check(!replacement.empty(), "WebView replacement WebProcess");
      check(std::any_of(replacement.begin(), replacement.end(), [&](pid_t pid) {
        return std::find(old.begin(), old.end(), pid) == old.end();
      }), "WebView uses a new WebProcess PID");
      printf("crtweb_webview_test: recovery %u terminated=%u killed=%zu replacement=%zu\n", i + 1,
          terminated, static_cast<size_t>(killed), replacement.size());
    }
  }

  /* ---- shutdown ---- */
  web.shutdown();
  check(!web.running(), "not running after shutdown");
  check(crtweb_test::children().empty(), "host reaped after shutdown");

  /* The consumer also repeats launch/load/resize/shutdown: descriptor and SHM ownership must not
   * accumulate in the crtui application even though each browser runtime lives in its own host. */
  auto baseline = crtweb_test::resources();
  for (unsigned i = 0; i < 20; ++i) {
    check(web.launch(host, 100, 75), "repeat host launch");
    seen.title.clear();
    web.load_url("file://" + page_a);
    web.resize(120, 90);
    check(pump_until([&] { return seen.title == "view a" && web.surface()->frame_width() == 120
        && web.surface()->frame_height() == 90; }), "repeat load/resize frame");
    web.shutdown();
    check(crtweb_test::children().empty(), "repeat host reaped");
    if ((i + 1) % 5 == 0) {
      auto sample = crtweb_test::resources();
      printf("crtweb_webview_test: lifecycle cycle %u: rss=%+ld KiB threads=%+ld descriptors=%+ld\n",
          i + 1, sample.memoryKiB - baseline.memoryKiB, sample.threads - baseline.threads,
          sample.descriptors - baseline.descriptors);
    }
    if (failures) break;
  }
  auto final = crtweb_test::resources();
  /* Windows may finish closing AF_UNIX/file handles on a runtime worker just after the reaped host
   * disconnects.  Require the same strict bound after a finite settling interval; a real cumulative
   * leak still remains above the threshold and fails. */
  unsigned settled_ms = 0;
  while (settled_ms < 5000 && final.descriptors >= 0 && final.threads >= 0
      && (final.descriptors - baseline.descriptors > 2 || final.threads - baseline.threads > 2)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    settled_ms += 20;
    final = crtweb_test::resources();
  }
  printf("crtweb_webview_test: lifecycle 20 cycles: rss=%+ld KiB threads=%+ld descriptors=%+ld settled=%u ms\n",
      final.memoryKiB - baseline.memoryKiB, final.threads - baseline.threads,
      final.descriptors - baseline.descriptors, settled_ms);
  check(baseline.memoryKiB >= 0 && baseline.threads >= 0 && baseline.descriptors >= 0, "consumer resource observations available");
  check(final.descriptors >= 0 && final.descriptors - baseline.descriptors <= 2, "consumer descriptors/handles bounded");
  check(final.threads >= 0 && final.threads - baseline.threads <= 2, "consumer threads bounded");
  check(final.memoryKiB >= 0 && final.memoryKiB - baseline.memoryKiB <= 24 * 1024, "consumer RSS growth bounded");
  crtui_context_destroy(scene.ui);
  unlink(page_a.c_str());
  unlink(page_b.c_str());
  if (failures != 0) {
    fprintf(stderr, "crtweb_webview_test: %d check(s) failed\n", failures);
    return 1;
  }
  printf("crtweb_webview_test: ok\n");
  return 0;
}
