/* Manual, interactive verification for real Linux/Wayland keyboard input
 * (wl_seat/wl_keyboard + libxkbcommon wiring in
 * src/arch/linux/window_wayland.c). NOT part of the automated ctest
 * suite -- it requires a human to actually type into the window that
 * appears (this environment has no synthetic-input tool like wtype/
 * ydotool available to inject key events programmatically), so it is
 * built and run ad hoc instead of being registered as a CMake test.
 *
 * Opens a real window against whatever live compositor is reachable
 * (WSLg's own Weston on this session's WSL host), then drains
 * crtgfx_window_poll_event() every 50ms for up to ~60 seconds, printing
 * every event it receives with a line-buffered, unbuffered stdout write
 * so a `tail -f` on the redirected output shows keystrokes live. Exits
 * early on Escape (evdev KEY_ESC=1) or a window-close request.
 */
#include "crtgfx/window.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

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
    default:
      return "?";
  }
}

int main(void) {
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
          }
          break;
        case CRTGFX_EVENT_KEY_UP:
          printf("event: KEY_UP keycode=%u modifiers=0x%x\n", event.data.key.keycode,
                 event.data.key.modifiers);
          break;
        case CRTGFX_EVENT_TEXT:
          printf("event: TEXT utf8=\"%s\"\n", event.data.text.utf8);
          break;
        default:
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
