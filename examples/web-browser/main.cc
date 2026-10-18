/* Minimal separate-process browser. CPU shared frames and software presentation
 * are deliberate: WebKit's private Skia never enters the application. Camera,
 * downloads, popups and dialogs are denied by default. Not a secure browser. */
#include "crtweb/webview.h"
#include "crtgfx/window.h"
#include "crtui/crtgfx.h"
#include "include/core/SkImageInfo.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static crtui_key key(uint32_t code) {
  switch (code) {
    case 15: return CRTUI_KEY_TAB;
    case 28: case 96: return CRTUI_KEY_ENTER;
    case 57: return CRTUI_KEY_SPACE;
    case 105: return CRTUI_KEY_LEFT;
    case 106: return CRTUI_KEY_RIGHT;
    case 103: return CRTUI_KEY_UP;
    case 108: return CRTUI_KEY_DOWN;
    case 1: return CRTUI_KEY_ESCAPE;
    case 14: return CRTUI_KEY_BACKSPACE;
    case 111: return CRTUI_KEY_DELETE;
    case 102: return CRTUI_KEY_HOME;
    case 107: return CRTUI_KEY_END;
    default: return CRTUI_KEY_NONE;
  }
}

extern "C" int main(int argc, char** argv) {
  bool smoke = argc == 2 && !strcmp(argv[1], "--smoke");
  if (argc > 2) return 2;
  const char* host = getenv("CRTWEB_HOST");
  std::string owned_host;
  if (!host) {
    // Portable explicit path fallback: invoke from the SDK examples/bin
    // directory, or set CRTWEB_HOST. No search of a system WebKit installation.
    std::string executable = argv[0];
    auto slash = executable.find_last_of("/\\");
    if (slash == std::string::npos) {
      fprintf(stderr, "invoke by path or set CRTWEB_HOST\n"); return 2;
    }
    owned_host = executable.substr(0, slash) + "/../../libexec/crtweb/crtweb_host";
#if defined(_WIN32)
    owned_host += ".exe";
#endif
    host = owned_host.c_str();
  }
  if (!getenv("FONTCONFIG_FILE")) {
    std::string path = host;
    auto slash = path.find_last_of("/\\");
    path = path.substr(0, slash) + "/../../share/crt/web-fonts.conf";
    setenv("FONTCONFIG_FILE", path.c_str(), 1);
  }
  crtweb::WebView web;
  crtweb::WebView::LaunchOptions options;
  if (const char* ca = getenv("CRTWEB_CA_PEM_FILE")) options.ca_pem_file = ca;
  if (!web.launch(host, 640, 480, nullptr, 60000, options)) return 1;
  bool loaded = false, failed = false, scripted = false;
  web.on_load = [&](crtweb::WebView::LoadEvent e, const std::string&, const std::string&) {
    loaded = e == crtweb::WebView::LoadEvent::Finished;
    if (e == crtweb::WebView::LoadEvent::Failed) failed = true;
  };
  web.on_title = [&](const std::string& title) {
    scripted = title == "CRT Web ready";
    printf("title: %s\n", title.c_str());
  };
  if (smoke || argc == 1) {
    web.load_html("<!doctype html><title>CRT Web</title><body style='margin:0;background:#2468ac'>"
                  "<h1>CRT Web</h1><p>Separate-process WebKit CRT Port</p>"
                  "<button onclick=\"this.textContent='Clicked'\">Click me</button>"
                  "<script>document.title='CRT Web ready'</script>");
  } else web.load_url(argv[1]);
  crtgfx_window* window = nullptr;
  if (!smoke) {
    crtgfx_window_desc desc = {"CRT Web (development, no sandbox)", 640, 480, CRTGFX_WINDOW_VISIBLE};
    if (crtgfx_window_create(&desc, &window) != CRTGFX_OK) return 1;
  }
  int pointer_x = 0, pointer_y = 0;
  int width = 640, height = 480;
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  bool painted = false;
  while (web.running() && !failed && (!window || !crtgfx_window_should_close(window))) {
    web.poll(10);
    if (web.has_frame()) {
      auto provider = web.surface_provider();
      SkImage* image = provider.acquire(provider.user, nullptr);
      if (image) {
        if (smoke) {
          unsigned char rgba[4] = {};
          painted = image->readPixels(SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
                                      rgba, 4, 600, 440)
              && rgba[0] == 0x24 && rgba[1] == 0x68 && rgba[2] == 0xac && rgba[3] == 255;
        } else {
          crtgfx_framebuffer fb = {};
          if (crtgfx_window_begin_frame(window, &fb) == CRTGFX_OK) {
            width = fb.width; height = fb.height;
            web.resize(fb.width, fb.height);
            image->readPixels(SkImageInfo::Make(fb.width, fb.height, kBGRA_8888_SkColorType,
                                               kPremul_SkAlphaType), fb.pixels, fb.stride, 0, 0);
            crtgfx_window_end_frame(window);
          }
        }
        provider.release(provider.user, nullptr, image);
        web.frame_presented();
      }
    }
    if (smoke && ((loaded && painted && scripted) || std::chrono::steady_clock::now() >= end)) break;
    if (window) {
      crtgfx_event event = {};
      while (crtgfx_window_poll_event(window, &event) == CRTGFX_OK && event.type != CRTGFX_EVENT_NONE) {
        crtui_input input = {};
        bool send = true;
        switch (event.type) {
          case CRTGFX_EVENT_POINTER_MOTION:
            input.type = CRTUI_INPUT_POINTER_MOVE;
            pointer_x = event.data.pointer_motion.x; pointer_y = event.data.pointer_motion.y; break;
          case CRTGFX_EVENT_POINTER_BUTTON_DOWN:
          case CRTGFX_EVENT_POINTER_BUTTON_UP:
            input.type = event.type == CRTGFX_EVENT_POINTER_BUTTON_DOWN ? CRTUI_INPUT_POINTER_DOWN : CRTUI_INPUT_POINTER_UP;
            pointer_x = event.data.pointer_button.x; pointer_y = event.data.pointer_button.y; break;
          case CRTGFX_EVENT_KEY_DOWN:
          case CRTGFX_EVENT_KEY_UP:
            input.type = event.type == CRTGFX_EVENT_KEY_DOWN ? CRTUI_INPUT_KEY_DOWN : CRTUI_INPUT_KEY_UP;
            input.key = key(event.data.key.keycode);
            input.modifiers = event.data.key.modifiers & CRTUI_MOD_SHIFT;
            send = input.key != CRTUI_KEY_NONE; break;
          case CRTGFX_EVENT_POINTER_SCROLL:
            input.type = CRTUI_INPUT_WHEEL;
            input.wheel_delta = event.data.pointer_scroll.dy; break;
          case CRTGFX_EVENT_TEXT:
            input.type = CRTUI_INPUT_TEXT;
            memcpy(input.text, event.data.text.utf8, sizeof(input.text) - 1); break;
          default: send = false; break;
        }
        if (send) {
          input.x = pointer_x; input.y = pointer_y;
          crtui_rect bounds = {0, 0, width, height};
          web.forward(input, bounds);
        }
      }
      crtgfx_window_pump_events(1);
    }
  }
  web.shutdown();
  if (window) crtgfx_window_destroy(window);
  if (smoke) {
    printf("crtweb_browser: load=%s pixels=%s script=%s\n", loaded ? "pass" : "FAIL", painted ? "pass" : "FAIL", scripted ? "pass" : "FAIL");
    return loaded && painted && scripted && !failed ? 0 : 1;
  }
  return failed ? 1 : 0;
}
