#pragma once

/* crtweb::WebView: the crtui WebView companion (Web Tranche 4).
 *
 * C++17, header-only, POSIX. A WebView owns one `crtweb_host` child process (the crtweb runtime and one
 * view, libcrtweb/tools/crtweb_host.cpp), the connection to it over the CRT web surface protocol, and the
 * crtweb::SurfaceClient that turns that connection into what crtui composes: a Skia image behind a
 * crtui SurfaceView, input forwarding, resize, focus. On top it carries the page control of the
 * protocol's control extension: navigation commands and load/title/URL/history/script events, in
 * the same vocabulary as the crtweb.h view API.
 *
 * The application therefore links crtui, crtgfx and Skia as it already does and never loads WebKit:
 * the web runtime stays in its own process (docs/design/libcrtweb_api_policy.md, "Why a separate
 * process"). No WebKit, WPE or GLib type appears here.
 *
 * Threading: one UI thread, as crtui. poll() delivers every callback.
 *
 *   crtweb::WebView web;
 *   web.on_title = [](const std::string& t) { ... };
 *   web.launch("/path/to/crtweb_host", 640, 480);
 *   web.load_url("https://example.org/");
 *   // per frame: web.poll(0); crtui_skia_provider p = web.surface_provider(); crtui_skia_compose(..., &p);
 *   //            web.frame_presented();  and for crtui input: web.forward(input, view_bounds);
 */

#include "crtweb_surface_client.h"
#include "crtweb_surface_control.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

extern char** environ;

namespace crtweb {

class WebView {
 public:
  enum class LoadEvent { Started = 1, Committed = 2, Finished = 3, Failed = 4 };

  /* Callbacks, delivered from poll() on the calling thread. Any may be left empty. */
  std::function<void(LoadEvent, const std::string& url, const std::string& message)> on_load;
  std::function<void(const std::string& title)> on_title;
  std::function<void(const std::string& url)> on_url;
  std::function<void()> on_history;
  std::function<void()> on_terminated;
  /* The host process ended or the connection broke; the view shows its last picture. */
  std::function<void()> on_closed;

  WebView() = default;
  WebView(const WebView&) = delete;
  WebView& operator=(const WebView&) = delete;
  ~WebView() { shutdown(); }

  /* Starts `host_path` (the crtweb_host executable), waits for it to connect and completes the
   * protocol handshake. width/height: logical pixels. Returns false (and cleans up) when the host
   * cannot be started or does not answer within timeout_ms. The host inherits the environment: it
   * finds its Web and Network process executables next to itself, or in CRTWEB_PROCESS_DIR. */
  bool launch(const char* host_path, uint32_t width, uint32_t height, const char* initial_url = nullptr,
              int timeout_ms = 60000) {
    shutdown();
    const char* base = getenv("TMPDIR");
    std::string pattern = std::string(base && *base ? base : "/tmp") + "/crtwebXXXXXX";
    std::string dir = pattern;
    if (mkdtemp(&dir[0]) == nullptr) return false;
    std::string path = dir + "/s";

    int listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (listener < 0) {
      rmdir(dir.c_str());
      return false;
    }
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    if (path.size() >= sizeof address.sun_path) {
      close(listener);
      rmdir(dir.c_str());
      return false;
    }
    memcpy(address.sun_path, path.c_str(), path.size() + 1);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || listen(listener, 1) != 0) {
      close(listener);
      rmdir(dir.c_str());
      return false;
    }

    std::vector<char*> argv = {const_cast<char*>(host_path), const_cast<char*>(path.c_str())};
    if (initial_url != nullptr) argv.push_back(const_cast<char*>(initial_url));
    argv.push_back(nullptr);
    pid_t child = 0;
    int spawned = posix_spawn(&child, host_path, nullptr, nullptr, argv.data(), environ);

    int connection = -1;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (spawned == 0) {
      struct pollfd pfd = {listener, POLLIN, 0};
      int ready = ::poll(&pfd, 1, 100);
      if (ready > 0) {
        connection = accept(listener, nullptr, nullptr);
        if (connection >= 0) fcntl(connection, F_SETFD, FD_CLOEXEC);
        break;
      }
      int status = 0;
      if (waitpid(child, &status, WNOHANG) == child) {
        child = 0;
        break;
      }
      if (std::chrono::steady_clock::now() >= deadline) break;
    }
    close(listener);
    unlink(path.c_str());
    rmdir(dir.c_str());
    if (connection < 0) {
      if (spawned == 0 && child > 0) {
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
      }
      return false;
    }
    child_ = child;
    client_ = std::make_unique<SurfaceClient>(connection);
    client_->on_event = [this](uint32_t code, uint32_t id, uint32_t flags, const std::string& argument) {
      handle_event(code, id, flags, argument);
    };
    if (!client_->handshake(width, height, 100, timeout_ms)) {
      shutdown();
      return false;
    }
    width_ = width;
    height_ = height;
    closed_reported_ = false;
    return true;
  }

  /* Ends the page: BYE, then waits for the host to exit (killing it after a few seconds). */
  void shutdown() {
    if (client_) {
      client_->close_connection();
      client_.reset();
    }
    if (child_ > 0) {
      auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      int status = 0;
      while (waitpid(child_, &status, WNOHANG) == 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
          kill(child_, SIGKILL);
          waitpid(child_, &status, 0);
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      child_ = 0;
    }
    scripts_.clear();
  }

  bool running() const { return client_ && !client_->closed(); }

  /* Reads and dispatches what the host sent, waiting up to timeout_ms for the first message. */
  bool poll(int timeout_ms = 0) {
    if (!client_) return false;
    std::string title_before = client_->title();
    bool any = client_->poll(timeout_ms);
    if (client_->title() != title_before && on_title) on_title(client_->title());
    if (client_->closed() && !closed_reported_) {
      closed_reported_ = true;
      if (on_closed) on_closed();
    }
    return any;
  }

  /* ---- crtui composition and input: the SurfaceClient's, unchanged ---- */
  SurfaceClient* surface() { return client_.get(); }
  crtui_skia_surface_provider surface_provider() { return client_->surface_provider(); }
  void frame_presented() {
    if (client_) client_->frame_presented();
  }
  void resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    if (client_) client_->resize(width, height, 100);
  }
  void focus(bool focused) {
    if (client_) client_->focus(focused);
  }
  bool forward(const crtui_input& input, const crtui_rect& view) { return client_ && client_->forward(input, view); }
  bool has_frame() const { return client_ && client_->has_frame(); }
  uint64_t frame_serial() const { return client_ ? client_->frame_serial() : 0; }

  /* ---- navigation ---- */
  bool load_url(const std::string& url) { return command(CRTWEB_COMMAND_LOAD_URL, 0, url); }
  bool load_html(const std::string& html, const std::string& base_url = std::string()) {
    std::string argument = html;
    argument.push_back('\0');
    argument += base_url;
    return command(CRTWEB_COMMAND_LOAD_HTML, 0, argument);
  }
  bool stop() { return command(CRTWEB_COMMAND_STOP, 0, std::string()); }
  bool reload() { return command(CRTWEB_COMMAND_RELOAD, 0, std::string()); }
  bool go_back() { return command(CRTWEB_COMMAND_GO_BACK, 0, std::string()); }
  bool go_forward() { return command(CRTWEB_COMMAND_GO_FORWARD, 0, std::string()); }
  bool can_go_back() const { return (history_ & 1u) != 0; }
  bool can_go_forward() const { return (history_ & 2u) != 0; }
  const std::string& title() const { return client_ ? client_->title() : empty_; }
  const std::string& url() const { return url_; }

  /* Evaluates script in the main frame. callback(result, error) runs from poll(). */
  bool evaluate(const std::string& script, std::function<void(const std::string& result, bool error)> callback) {
    uint32_t id = ++next_script_;
    if (!command(CRTWEB_COMMAND_EVALUATE, id, script)) return false;
    scripts_[id] = std::move(callback);
    return true;
  }

 private:
  bool command(uint32_t code, uint32_t id, const std::string& argument) {
    return client_ && client_->send_command(code, id, argument.data(), argument.size());
  }

  void handle_event(uint32_t code, uint32_t id, uint32_t flags, const std::string& argument) {
    size_t split = argument.find('\0');
    std::string first = argument.substr(0, split);
    std::string second = split == std::string::npos ? std::string() : argument.substr(split + 1);
    switch (code) {
      case CRTWEB_EVENT_LOAD_STARTED:
      case CRTWEB_EVENT_LOAD_COMMITTED:
      case CRTWEB_EVENT_LOAD_FINISHED:
      case CRTWEB_EVENT_LOAD_FAILED:
        if (on_load) on_load(static_cast<LoadEvent>(code), first, second);
        break;
      case CRTWEB_EVENT_URL_CHANGED:
        url_ = first;
        if (on_url) on_url(url_);
        break;
      case CRTWEB_EVENT_HISTORY:
        history_ = flags;
        if (on_history) on_history();
        break;
      case CRTWEB_EVENT_SCRIPT_RESULT: {
        auto found = scripts_.find(id);
        if (found != scripts_.end()) {
          auto callback = std::move(found->second);
          scripts_.erase(found);
          if (callback) callback(argument, (flags & 1u) != 0);
        }
        break;
      }
      case CRTWEB_EVENT_PROCESS_TERMINATED:
        if (on_terminated) on_terminated();
        break;
      default:
        break;
    }
  }

  std::unique_ptr<SurfaceClient> client_;
  pid_t child_ = 0;
  uint32_t width_ = 0, height_ = 0;
  uint32_t history_ = 0;
  uint32_t next_script_ = 0;
  bool closed_reported_ = false;
  std::string url_;
  std::string empty_;
  std::map<uint32_t, std::function<void(const std::string&, bool)>> scripts_;
};

}  // namespace crtweb
