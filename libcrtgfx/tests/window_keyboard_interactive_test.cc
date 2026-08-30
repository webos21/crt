/* Manual, interactive verification for real keyboard/pointer/window-state
 * input, originally written for Linux/Wayland keyboard input (wl_seat/
 * wl_keyboard + libxkbcommon wiring in src/arch/linux/window_wayland.c)
 * and since reused as-is on every host: event_type_name()/the main event
 * switch below print every crtgfx_event_type this project defines, not
 * just keyboard, so the same binary also verifies RESIZE/FOCUS_IN/
 * FOCUS_OUT/POINTER_SCROLL/DPI_SCALE_CHANGED/CLOSE_REQUESTED delivery
 * against a real, live window on whatever host it's built for -- added
 * 2026-08-30 verifying the Phase 1 window/event contract on real macOS
 * hardware for the first time (multi-window/focus/scroll/DPI-scale work
 * had, until then, only ever been reasoned from Apple's own AppKit docs
 * and compile/object-code checked, never run against a live NSWindow --
 * see HISTORY.md's 2026-08-30 entries). NOT part of the automated ctest
 * suite -- it requires a human (or host automation like AppleScript/
 * System Events) to actually interact with the window that appears
 * (this environment has no synthetic-input tool like wtype/ydotool
 * available to inject key events programmatically), so it is built and
 * run ad hoc instead of being registered as a CMake test.
 *
 * Opens a real window against whatever live compositor/window server is
 * reachable (WSLg's own Weston on a WSL host, real Wayland/X11 on Linux,
 * a real Win32 message loop on Windows, a real NSApplication run loop on
 * macOS, ...), then drains crtgfx_window_poll_event() every 50ms for up
 * to ~60 seconds, printing every event it receives with a line-buffered,
 * unbuffered stdout write so a `tail -f` on the redirected output shows
 * events live. Exits early on Escape (evdev KEY_ESC=1) or a window-close
 * request.
 *
 * Real Skia+FreeType text rendering (added 2026-08-25, verifying Linux
 * end to end for the first time -- see TODO.md item 5/HISTORY.md's
 * matching entry): typed characters accumulate into an on-screen text
 * buffer, drawn every frame via crtgfx -> Skia -> this project's own
 * FreeType port, the exact same real pipeline crtgfx_skia_raster_smoke.
 * cc already proved on Windows/macOS (see that file's own comments for
 * the "why legacyMakeTypeface, why -1 for the mmap NUL, ..." reasoning,
 * not repeated here). Compile-time-gated behind crtgfx/skia.h's own
 * CRTGFX_HAS_SKIA_HEADERS (true only when CRTGFX_ENABLE_SKIA=ON and
 * libskia.a has actually been built -- see libcrtgfx/CMakeLists.txt's
 * own crt_wire_skia_executable() call for this target): with Skia
 * unavailable, this file still builds and runs exactly as before,
 * falling back to the plain per-pixel gradient fill it always had, so
 * this target's own "always buildable regardless of CRTGFX_ENABLE_SKIA"
 * property (crtgfx_window_demo's own established shape) is unchanged. */
#include "crtgfx/window.h"
#include "crtgfx/skia.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if CRTGFX_HAS_SKIA_HEADERS
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_directory.h"

#ifndef CRT_SKIA_FONTS_DIR
#error "CRT_SKIA_FONTS_DIR must be defined (see libcrtgfx/CMakeLists.txt)"
#endif
#endif

/* Real wall-clock deadline, not an iteration count: found for real that
 * crtgfx_window_pump_events(50)'s own poll(2) call frequently returns
 * almost instantly rather than actually blocking the full 50ms (a live
 * compositor keeps sending frame-done/ping traffic in response to this
 * test's own per-iteration present calls), so a fixed loop count of 1200
 * iterations finished in ~2 real seconds instead of the intended 60 --
 * nowhere near enough time for a human to switch windows and type. */
static double monotonic_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static const char* event_type_name(crtgfx_event_type type) {
  switch (type) {
    case CRTGFX_EVENT_NONE:
      return "NONE";
    case CRTGFX_EVENT_KEY_DOWN:
      return "KEY_DOWN";
    case CRTGFX_EVENT_KEY_UP:
      return "KEY_UP";
    case CRTGFX_EVENT_TEXT:
      return "TEXT";
    case CRTGFX_EVENT_POINTER_MOTION:
      return "POINTER_MOTION";
    case CRTGFX_EVENT_POINTER_BUTTON_DOWN:
      return "POINTER_BUTTON_DOWN";
    case CRTGFX_EVENT_POINTER_BUTTON_UP:
      return "POINTER_BUTTON_UP";
    case CRTGFX_EVENT_RESIZE:
      return "RESIZE";
    case CRTGFX_EVENT_CLOSE_REQUESTED:
      return "CLOSE_REQUESTED";
    case CRTGFX_EVENT_FOCUS_IN:
      return "FOCUS_IN";
    case CRTGFX_EVENT_FOCUS_OUT:
      return "FOCUS_OUT";
    case CRTGFX_EVENT_EXPOSE:
      return "EXPOSE";
    case CRTGFX_EVENT_POINTER_SCROLL:
      return "POINTER_SCROLL";
    case CRTGFX_EVENT_DPI_SCALE_CHANGED:
      return "DPI_SCALE_CHANGED";
    default:
      return "?";
  }
}

/* Fixed-size, not std::string: keeps this file's own dependency surface
 * identical to before Skia was wired in when CRTGFX_HAS_SKIA_HEADERS is
 * false (no <string>/<vector>), and a notepad-style typing demo never
 * needs more than a page of text anyway. */
#define CRTGFX_TEXT_BUFFER_CAPACITY 4096u
static char text_buffer[CRTGFX_TEXT_BUFFER_CAPACITY];
static size_t text_buffer_len;

static void text_buffer_append(const char* utf8) {
  size_t add_len = strlen(utf8);
  if (text_buffer_len + add_len >= CRTGFX_TEXT_BUFFER_CAPACITY) {
    return; /* full -- silently drop further typing rather than overflow */
  }
  memcpy(text_buffer + text_buffer_len, utf8, add_len);
  text_buffer_len += add_len;
  text_buffer[text_buffer_len] = 0;
}

static void text_buffer_backspace(void) {
  /* Remove exactly one UTF-8 codepoint, not one byte -- scan back past
   * any continuation bytes (0x80-0xBF, the real UTF-8 encoding rule for
   * "this byte is a continuation of the previous codepoint") so deleting
   * a non-ASCII character (e.g. a composed accented letter) doesn't leave
   * a truncated, invalid partial encoding in the buffer. */
  if (text_buffer_len == 0u) {
    return;
  }
  text_buffer_len--;
  while (text_buffer_len > 0u &&
         ((unsigned char)text_buffer[text_buffer_len] & 0xc0u) == 0x80u) {
    text_buffer_len--;
  }
  text_buffer[text_buffer_len] = 0;
}

static void text_buffer_newline(void) {
  text_buffer_append("\n");
}

#if CRTGFX_HAS_SKIA_HEADERS
/* Draws text_buffer's own current contents onto `canvas`, one Skia
 * drawString() call per '\n'-separated line -- SkFont/drawString have no
 * built-in multi-line wrapping, matching how any real, minimal text
 * widget has to split lines itself before handing single lines to the
 * font-shaping layer. */
static void draw_text_buffer(SkCanvas* canvas, const SkFont& font, const SkPaint& paint) {
  const char* line_start = text_buffer;
  float y = 32.0f;
  const float line_height = 28.0f;

  for (;;) {
    const char* newline = strchr(line_start, '\n');
    size_t line_len = newline ? (size_t)(newline - line_start) : strlen(line_start);
    canvas->drawSimpleText(line_start, line_len, SkTextEncoding::kUTF8, 16.0f, y, font, paint);
    if (newline == nullptr) {
      break;
    }
    line_start = newline + 1;
    y += line_height;
  }
}
#endif

extern "C" int main() {
  crtgfx_window_desc desc;
  crtgfx_window* window;
  crtgfx_framebuffer framebuffer;
  crtgfx_event event;
  unsigned char* row;
  uint32_t x, y;
  int rc;
  int iteration;
  int should_quit;
  double start_time;
  double deadline_seconds;
#if CRTGFX_HAS_SKIA_HEADERS
  sk_sp<SkFontMgr> font_mgr;
  sk_sp<SkTypeface> typeface;
  int skia_ready = 0;
#endif

  desc.title = "crtgfx keyboard test - type here, Esc to quit";
  desc.width = 480;
  desc.height = 320;
  desc.flags = CRTGFX_WINDOW_VISIBLE;

  setvbuf(stdout, NULL, _IONBF, 0);

  rc = crtgfx_window_create(&desc, &window);
  if (rc == CRTGFX_ERROR_UNSUPPORTED) {
    printf("keyboard_interactive: no usable host backend (no compositor reachable) -- skipped\n");
    return 0;
  }
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "keyboard_interactive: create failed (%d)\n", rc);
    return 1;
  }

  rc = crtgfx_window_show(window);
  if (rc != CRTGFX_OK) {
    fprintf(stderr, "keyboard_interactive: show failed (%d)\n", rc);
    crtgfx_window_destroy(window);
    return 1;
  }

#if CRTGFX_HAS_SKIA_HEADERS
  /* Real font manager set up once, outside the frame loop -- resolves the
   * project's own bundled default typeface via crtgfx_skia_default_
   * typeface() (crtgfx/skia.h), which tries "Pretendard GOV" first, then
   * "DejaVu Sans Mono", then legacyMakeTypeface(nullptr, ...) as a last
   * resort. See that function's doc comment for why a plain nullptr
   * lookup alone is no longer trustworthy now that the fonts directory
   * holds more than one real bundled family. Failure here is non-fatal to
   * this manual demo -- falls back to the plain gradient fill instead of
   * refusing to run at all, since the whole point of this binary is
   * verifying *input*, and text rendering is this session's own added
   * bonus check, not this binary's original job. */
  font_mgr = SkFontMgr_New_Custom_Directory(CRT_SKIA_FONTS_DIR);
  if (font_mgr) {
    typeface = crtgfx_skia_default_typeface(font_mgr.get(), SkFontStyle());
  }
  skia_ready = (typeface != nullptr);
  printf("keyboard_interactive: skia text rendering %s\n", skia_ready ? "enabled" : "unavailable");
#else
  printf("keyboard_interactive: built without Skia (CRTGFX_ENABLE_SKIA=OFF) -- gradient fill only\n");
#endif

  printf("keyboard_interactive: window open -- type into it now (up to 60s, Esc to quit early)\n");

  should_quit = 0;
  start_time = monotonic_seconds();
  deadline_seconds = 60.0;
  for (iteration = 0; (monotonic_seconds() - start_time) < deadline_seconds && !should_quit; ++iteration) {
    rc = crtgfx_window_pump_events(50);
    if (rc != CRTGFX_OK) {
      fprintf(stderr, "keyboard_interactive: pump failed (%d)\n", rc);
      break;
    }
    if (crtgfx_window_should_close(window)) {
      printf("keyboard_interactive: window close requested\n");
      break;
    }
    for (;;) {
      rc = crtgfx_window_poll_event(window, &event);
      if (rc != CRTGFX_OK) {
        fprintf(stderr, "keyboard_interactive: poll_event failed (%d)\n", rc);
        should_quit = 1;
        break;
      }
      if (event.type == CRTGFX_EVENT_NONE) {
        break;
      }
      switch (event.type) {
        case CRTGFX_EVENT_KEY_DOWN:
          printf("event: KEY_DOWN keycode=%u modifiers=0x%x\n", event.data.key.keycode,
                 event.data.key.modifiers);
          if (event.data.key.keycode == 1u) { /* evdev KEY_ESC */
            printf("keyboard_interactive: Esc pressed, quitting\n");
            should_quit = 1;
          } else if (event.data.key.keycode == 14u) { /* evdev KEY_BACKSPACE */
            text_buffer_backspace();
          } else if (event.data.key.keycode == 28u) { /* evdev KEY_ENTER */
            text_buffer_newline();
          }
          break;
        case CRTGFX_EVENT_KEY_UP:
          printf("event: KEY_UP keycode=%u modifiers=0x%x\n", event.data.key.keycode,
                 event.data.key.modifiers);
          break;
        case CRTGFX_EVENT_TEXT:
          printf("event: TEXT utf8=\"%s\"\n", event.data.text.utf8);
          text_buffer_append(event.data.text.utf8);
          break;
        case CRTGFX_EVENT_RESIZE:
          printf("event: RESIZE width=%u height=%u\n", event.data.resize.width,
                 event.data.resize.height);
          break;
        case CRTGFX_EVENT_POINTER_SCROLL:
          printf("event: POINTER_SCROLL dx=%.3f dy=%.3f\n", event.data.pointer_scroll.dx,
                 event.data.pointer_scroll.dy);
          break;
        case CRTGFX_EVENT_DPI_SCALE_CHANGED:
          printf("event: DPI_SCALE_CHANGED scale=%.3f\n", event.data.dpi_scale.scale);
          break;
        default:
          /* CLOSE_REQUESTED/FOCUS_IN/FOCUS_OUT/EXPOSE carry no payload
           * (see crtgfx_event's own doc comment, window.h) -- the event
           * type name alone, via event_type_name(), is the whole message. */
          printf("event: %s\n", event_type_name(event.type));
          break;
      }
    }
    /* Keep the window visibly alive/repainting so it's obvious which
     * window to click into and type at. */
    rc = crtgfx_window_begin_frame(window, &framebuffer);
    if (rc == CRTGFX_OK) {
      for (y = 0; y < framebuffer.height; ++y) {
        row = (unsigned char*)framebuffer.pixels + y * framebuffer.stride;
        for (x = 0; x < framebuffer.width; ++x) {
          row[x * 4u + 0u] = (unsigned char)(x & 0xffu);
          row[x * 4u + 1u] = (unsigned char)(y & 0xffu);
          row[x * 4u + 2u] = (unsigned char)((iteration * 2) & 0xffu);
          row[x * 4u + 3u] = 0xffu;
        }
      }
#if CRTGFX_HAS_SKIA_HEADERS
      if (skia_ready) {
        sk_sp<SkSurface> surface = crtgfx_skia_make_raster_surface(&framebuffer);
        if (surface) {
          SkCanvas* canvas = surface->getCanvas();
          SkFont font(typeface, 22.0f);
          SkPaint paint;
          paint.setColor(SK_ColorWHITE);
          draw_text_buffer(canvas, font, paint);
        }
      }
#endif
      crtgfx_window_end_frame(window);
    }
    /* poll(2) inside pump_events doesn't reliably block the full 50ms
     * requested (see monotonic_seconds()'s own comment above) -- a small
     * floor sleep keeps this from busy-looping the CPU at ~100% for the
     * full 60s deadline while still being far shorter than any human
     * reaction time. */
    {
      struct timespec sleep_ts;
      sleep_ts.tv_sec = 0;
      sleep_ts.tv_nsec = 5000000; /* 5ms; unistd.h has no usleep() here */
      nanosleep(&sleep_ts, 0);
    }
  }

  crtgfx_window_destroy(window);
  printf("keyboard_interactive: done\n");
  return 0;
}
