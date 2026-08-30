/* Deterministic, no-real-OS-input-needed regression coverage for the
 * crtgfx_event queue contract itself (Phase 2 of the window/event API
 * completion plan, added 2026-08-30) -- ordering, overflow/drop-newest
 * policy, and multi-window routing, none of which need a human typing or
 * a synthetic-input tool this project's own hosts do not reliably have
 * (no wtype/ydotool-equivalent on Wayland/Win32/Cocoa -- see window_
 * keyboard_interactive_test.cc's own top comment). Uses crtgfx_window_
 * inject_event() (crtgfx/window.h), a testing-only hook that pushes an
 * event onto a window's own queue exactly the way a real backend already
 * does internally, without needing crtgfx_window_pump_events() or any
 * real native input delivery at all.
 *
 * Still needs a real host backend connection for crtgfx_window_create()
 * itself, same as crtgfx_window_smoke -- synthetic injection replaces the
 * need for real *input*, not the need for a live display connection.
 * Headless Linux CI (no reachable Wayland compositor) is expected to
 * report CRTGFX_ERROR_UNSUPPORTED here exactly as crtgfx_window_smoke
 * does; that is a real, separate, already-covered path (STATUS.md's own
 * "Graphics Checks" table), not something this file re-verifies. */
#include "crtgfx/window.h"

#include <stdio.h>
#include <string.h>

#if defined(CRT_TARGET_OS_LINUX)
#include <dirent.h>
#endif

static int fail(const char* message, int code) {
  fprintf(stderr, "crtgfx_synthetic_event: %s (%d)\n", message, code);
  return 1;
}

static int events_equal(const crtgfx_event* a, const crtgfx_event* b) {
  if (a->type != b->type) {
    return 0;
  }
  switch (a->type) {
    case CRTGFX_EVENT_KEY_DOWN:
    case CRTGFX_EVENT_KEY_UP:
      return a->data.key.keycode == b->data.key.keycode &&
             a->data.key.modifiers == b->data.key.modifiers;
    case CRTGFX_EVENT_TEXT:
      return strcmp(a->data.text.utf8, b->data.text.utf8) == 0;
    case CRTGFX_EVENT_POINTER_MOTION:
      return a->data.pointer_motion.x == b->data.pointer_motion.x &&
             a->data.pointer_motion.y == b->data.pointer_motion.y;
    case CRTGFX_EVENT_POINTER_BUTTON_DOWN:
    case CRTGFX_EVENT_POINTER_BUTTON_UP:
      return a->data.pointer_button.button == b->data.pointer_button.button;
    default:
      return 1; /* payload-less event types (CLOSE_REQUESTED, FOCUS_IN, FOCUS_OUT, EXPOSE, ...) */
  }
}

/* Test A: strict FIFO ordering across a real mix of event types (the
 * exact keyboard/modifier/text/pointer sequence a real typed keystroke +
 * click actually produces on a live backend), verified end to end
 * against crtgfx_window_poll_event() with no pump_events() call at all
 * in between. */
static int test_ordering(crtgfx_window* window) {
  crtgfx_event sent[6];
  crtgfx_event got;
  int i;
  int rc;

  memset(sent, 0, sizeof(sent));
  sent[0].type = CRTGFX_EVENT_KEY_DOWN;
  sent[0].data.key.keycode = 30u; /* KEY_A, evdev */
  sent[0].data.key.modifiers = CRTGFX_MOD_SHIFT;
  sent[1].type = CRTGFX_EVENT_TEXT;
  strcpy(sent[1].data.text.utf8, "A");
  sent[2].type = CRTGFX_EVENT_KEY_UP;
  sent[2].data.key.keycode = 30u;
  sent[3].type = CRTGFX_EVENT_POINTER_MOTION;
  sent[3].data.pointer_motion.x = 12.5;
  sent[3].data.pointer_motion.y = 34.5;
  sent[4].type = CRTGFX_EVENT_POINTER_BUTTON_DOWN;
  sent[4].data.pointer_button.button = CRTGFX_POINTER_BUTTON_LEFT;
  sent[5].type = CRTGFX_EVENT_POINTER_BUTTON_UP;
  sent[5].data.pointer_button.button = CRTGFX_POINTER_BUTTON_LEFT;

  for (i = 0; i < 6; ++i) {
    rc = crtgfx_window_inject_event(window, &sent[i]);
    if (rc != CRTGFX_OK) {
      return fail("test_ordering inject", rc);
    }
  }
  for (i = 0; i < 6; ++i) {
    rc = crtgfx_window_poll_event(window, &got);
    if (rc != CRTGFX_OK || got.type == CRTGFX_EVENT_NONE) {
      return fail("test_ordering poll (queue drained early)", rc);
    }
    if (!events_equal(&sent[i], &got)) {
      fprintf(stderr, "crtgfx_synthetic_event: test_ordering mismatch at index %d (expected type %d, got %d)\n",
              i, (int)sent[i].type, (int)got.type);
      return 1;
    }
  }
  rc = crtgfx_window_poll_event(window, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_NONE) {
    return fail("test_ordering queue not empty after draining exactly what was sent", rc);
  }
  return 0;
}

/* Test B: overflow/drop-newest policy (crtgfx/window.h's own documented
 * contract). Injects far more events than any reasonable queue capacity
 * and confirms whatever comes back out is a real, unbroken *prefix* of
 * what was sent (oldest events kept, newest ones silently dropped once
 * full) -- not the exact capacity number, which is a wayland_weston.c
 * implementation detail (CRTGFX_EVENT_QUEUE_CAPACITY) this black-box test
 * deliberately does not depend on. Each injected event carries its own
 * send-order index in pointer_motion.x, used only to identify it here --
 * it has no other special meaning. */
static int test_overflow_drop_newest(crtgfx_window* window) {
  enum { SEND_COUNT = 500 };
  int i;
  int received = 0;
  double last_index = -1.0;

  for (i = 0; i < SEND_COUNT; ++i) {
    crtgfx_event event;
    int rc;

    memset(&event, 0, sizeof(event));
    event.type = CRTGFX_EVENT_POINTER_MOTION;
    event.data.pointer_motion.x = (double)i;
    event.data.pointer_motion.y = 0.0;
    rc = crtgfx_window_inject_event(window, &event);
    if (rc != CRTGFX_OK) {
      return fail("test_overflow inject", rc);
    }
  }
  for (;;) {
    crtgfx_event got;
    int rc = crtgfx_window_poll_event(window, &got);

    if (rc != CRTGFX_OK) {
      return fail("test_overflow poll", rc);
    }
    if (got.type == CRTGFX_EVENT_NONE) {
      break;
    }
    if (got.type != CRTGFX_EVENT_POINTER_MOTION) {
      return fail("test_overflow unexpected event type", (int)got.type);
    }
    /* Strictly increasing by exactly 1 each time -- a real, unbroken
     * prefix starting at index 0, not just "some subset". */
    if (got.data.pointer_motion.x != last_index + 1.0) {
      fprintf(stderr,
              "crtgfx_synthetic_event: test_overflow non-contiguous prefix (expected %.0f, got %.0f)\n",
              last_index + 1.0, got.data.pointer_motion.x);
      return 1;
    }
    last_index = got.data.pointer_motion.x;
    ++received;
  }
  if (received == 0 || received > SEND_COUNT) {
    return fail("test_overflow implausible received count", received);
  }
  printf("crtgfx_synthetic_event: overflow test kept %d of %d injected events\n", received, SEND_COUNT);
  return 0;
}

/* Test C: multi-window queue isolation (the exact separation Phase 1's
 * multi-window work was for) -- events injected into one window must
 * never appear when polling a different one. */
static int test_multi_window_isolation(crtgfx_window* window_a) {
  crtgfx_window_desc desc_b;
  crtgfx_window* window_b;
  crtgfx_event event;
  crtgfx_event got;
  int rc;

  desc_b.title = "crtgfx synthetic event (window B)";
  desc_b.width = 320;
  desc_b.height = 200;
  desc_b.flags = 0;
  rc = crtgfx_window_create(&desc_b, &window_b);
  if (rc != CRTGFX_OK) {
    return fail("test_multi_window second create", rc);
  }

  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_KEY_DOWN;
  event.data.key.keycode = 1u; /* KEY_ESC, evdev -- window A's own marker */
  rc = crtgfx_window_inject_event(window_a, &event);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window inject A", rc);
  }
  event.data.key.keycode = 2u; /* KEY_1, evdev -- window B's own marker */
  rc = crtgfx_window_inject_event(window_b, &event);
  if (rc != CRTGFX_OK) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window inject B", rc);
  }

  rc = crtgfx_window_poll_event(window_a, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_KEY_DOWN || got.data.key.keycode != 1u) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window A saw wrong event", rc);
  }
  rc = crtgfx_window_poll_event(window_a, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_NONE) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window A queue not isolated from B", rc);
  }
  rc = crtgfx_window_poll_event(window_b, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_KEY_DOWN || got.data.key.keycode != 2u) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window B saw wrong event", rc);
  }
  rc = crtgfx_window_poll_event(window_b, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_NONE) {
    crtgfx_window_destroy(window_b);
    return fail("test_multi_window B queue not isolated from A", rc);
  }

  crtgfx_window_destroy(window_b);
  return 0;
}

#if defined(CRT_TARGET_OS_LINUX)
/* Real, direct fd-count check (Linux only -- /proc/self/fd is a Linux/
 * procfs-specific mechanism, no cross-platform equivalent this project
 * already has a helper for). Counts real directory entries, not a name
 * pattern -- "." and ".." plus one entry per genuinely open fd. */
static int count_open_fds(void) {
  DIR* dir = opendir("/proc/self/fd");
  int count = 0;
  struct dirent* entry;

  if (dir == 0) {
    return -1;
  }
  while ((entry = readdir(dir)) != 0) {
    ++count;
  }
  closedir(dir);
  return count;
}
#endif

/* Test D: repeated create/destroy, checked for a real fd leak on Linux
 * (where a cheap, direct check is available via /proc/self/fd) and, on
 * every host, checked simply for not crashing/hanging across a real
 * repeated-lifecycle loop -- the same class of regression a real long-
 * running application (not just a single demo window) would actually
 * exercise. */
static int test_repeated_create_destroy(void) {
  enum { ITERATIONS = 50 };
  int i;
#if defined(CRT_TARGET_OS_LINUX)
  int fds_before = count_open_fds();
#endif

  for (i = 0; i < ITERATIONS; ++i) {
    crtgfx_window_desc desc;
    crtgfx_window* window;
    int rc;

    desc.title = "crtgfx synthetic event (repeated)";
    desc.width = 200;
    desc.height = 150;
    desc.flags = 0;
    rc = crtgfx_window_create(&desc, &window);
    if (rc != CRTGFX_OK) {
      return fail("test_repeated_create_destroy create", rc);
    }
    crtgfx_window_destroy(window);
  }

#if defined(CRT_TARGET_OS_LINUX)
  {
    int fds_after = count_open_fds();

    if (fds_before >= 0 && fds_after >= 0 && fds_after > fds_before) {
      fprintf(stderr,
              "crtgfx_synthetic_event: test_repeated_create_destroy leaked fds (before=%d after=%d over %d iterations)\n",
              fds_before, fds_after, (int)ITERATIONS);
      return 1;
    }
  }
#endif
  return 0;
}

/* Test E: injected events survive/queue correctly interleaved with real
 * begin_frame()/end_frame() calls on the same window -- a modest,
 * honestly-scoped stand-in for "resize during frame acquire/submit":
 * this cannot synthesize a *real* resize (that still has to come from
 * the real host backend's own native resize notification, which is
 * exactly the input this file deliberately does not need), but it does
 * prove the event queue and the frame-lifecycle state machine are
 * genuinely independent of each other, not accidentally coupled. */
static int test_events_survive_frame_cycle(crtgfx_window* window) {
  crtgfx_framebuffer framebuffer;
  crtgfx_event event;
  crtgfx_event got;
  int rc;

  memset(&event, 0, sizeof(event));
  event.type = CRTGFX_EVENT_RESIZE;
  event.data.resize.width = 640;
  event.data.resize.height = 480;
  rc = crtgfx_window_inject_event(window, &event);
  if (rc != CRTGFX_OK) {
    return fail("test_events_survive_frame_cycle inject before begin_frame", rc);
  }

  rc = crtgfx_window_begin_frame(window, &framebuffer);
  if (rc != CRTGFX_OK) {
    /* A real backend can legitimately fail this in a headless/unusual
     * environment even when window creation itself succeeded (e.g. a
     * transient present-path issue unrelated to this test's own event-
     * queue purpose) -- not this test's concern either way, skip the
     * rest rather than mis-blaming the event queue for it. */
    printf("crtgfx_synthetic_event: begin_frame unavailable (%d), skipping frame-cycle interleave\n", rc);
    return 0;
  }

  event.type = CRTGFX_EVENT_KEY_DOWN;
  event.data.key.keycode = 57u; /* KEY_SPACE, evdev */
  rc = crtgfx_window_inject_event(window, &event);
  if (rc != CRTGFX_OK) {
    crtgfx_window_end_frame(window);
    return fail("test_events_survive_frame_cycle inject during frame_pending", rc);
  }

  rc = crtgfx_window_end_frame(window);
  if (rc != CRTGFX_OK) {
    /* Same reasoning as begin_frame's own failure path above -- a real
     * present_software() failure (e.g. this project's own already-
     * tracked WSL-shell-context present_software quirk, see TODO.md/
     * HISTORY.md) is a real environment limitation independent of
     * whether the event queue itself works correctly. Logged, not
     * treated as fatal, and deliberately does NOT skip the two poll_
     * event() checks below -- unlike begin_frame failing (which leaves
     * no valid frame_pending state to safely call end_frame() against
     * at all), the event queue is fully independent of whatever end_
     * frame()'s own presentation path did, so those checks still mean
     * something here. */
    printf("crtgfx_synthetic_event: end_frame unavailable (%d), continuing (event queue is independent of presentation)\n", rc);
  }

  rc = crtgfx_window_poll_event(window, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_RESIZE || got.data.resize.width != 640u) {
    return fail("test_events_survive_frame_cycle lost the pre-frame event", rc);
  }
  rc = crtgfx_window_poll_event(window, &got);
  if (rc != CRTGFX_OK || got.type != CRTGFX_EVENT_KEY_DOWN || got.data.key.keycode != 57u) {
    return fail("test_events_survive_frame_cycle lost the mid-frame event", rc);
  }
  return 0;
}

int main(void) {
  crtgfx_window_desc desc;
  crtgfx_window* window;
  int rc;

  desc.title = "crtgfx synthetic event";
  desc.width = 320;
  desc.height = 200;
  desc.flags = 0;

  rc = crtgfx_window_create(&desc, &window);
  if (rc == CRTGFX_ERROR_UNSUPPORTED) {
    /* No usable host backend in this environment -- matches crtgfx_
     * window_smoke's own established graceful-skip convention exactly
     * (see this file's own top comment). */
    puts("crtgfx_synthetic_event: ok (unsupported)");
    return 0;
  }
  if (rc != CRTGFX_OK) {
    return fail("create", rc);
  }

  if (test_ordering(window) != 0) {
    crtgfx_window_destroy(window);
    return 1;
  }
  if (test_overflow_drop_newest(window) != 0) {
    crtgfx_window_destroy(window);
    return 1;
  }
  if (test_multi_window_isolation(window) != 0) {
    crtgfx_window_destroy(window);
    return 1;
  }
  if (test_events_survive_frame_cycle(window) != 0) {
    crtgfx_window_destroy(window);
    return 1;
  }
  crtgfx_window_destroy(window);

  if (test_repeated_create_destroy() != 0) {
    return 1;
  }

  puts("crtgfx_synthetic_event: ok");
  return 0;
}
