/* crtui Tranche 0 acceptance (docs/crtui_acceptance.md): the frozen contract of
 * crtui/ui.h, checked by a resource-free test -- no window, GPU, display or
 * LVGL, only the headless model in libcrtui/src/core.c. Covers thread
 * ownership, handle lifetime, event ordering/bubbling/re-entrancy, default
 * keyboard actions, focus traversal and recovery, modal routing, hit-testing
 * and clipping, geometry/resize, and the error codes. */

#include "crtui/ui.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

/* ---- event log ---------------------------------------------------------- */

typedef struct log_entry {
  int tag; /* which callback (the widget's user tag) */
  crtui_event_type type;
  crtui_widget target;
  crtui_widget current;
  int32_t value;
} log_entry;

static log_entry event_log[256];
static int event_count = 0;

static void log_reset(void) { event_count = 0; }

static int count_type(crtui_event_type type) {
  int n = 0;
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].type == type) ++n;
  }
  return n;
}

/* Default recording callback: user is a small int tag; never handles. */
static int record_cb(crtui_widget current, const crtui_event* event, void* user) {
  if (event_count < 256) {
    log_entry* e = &event_log[event_count++];
    e->tag = (int)(intptr_t)user;
    e->type = event->type;
    e->target = event->target;
    e->current = current;
    e->value = event->value;
  }
  return CRTUI_EVENT_IGNORED;
}

static int handled_cb(crtui_widget current, const crtui_event* event, void* user) {
  record_cb(current, event, user);
  return CRTUI_EVENT_HANDLED;
}

/* ---- scene -------------------------------------------------------------- */

typedef struct scene {
  crtui_context* ctx;
  crtui_window win;
  crtui_widget container, button_a, button_b, slider, text;
} scene;

enum { TAG_WIN = 1, TAG_CONTAINER = 2, TAG_A = 3, TAG_B = 4, TAG_SLIDER = 5 };

static int build_scene(scene* s) {
  memset(s, 0, sizeof(*s));
  if (crtui_context_create(&s->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(s->ctx, &s->win) != CRTUI_OK) return -1;
  crtui_window_set_size(s->ctx, s->win, 400, 300, 1.0f);
  if (crtui_container_create(s->ctx, s->win, &s->container) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->container, 10, 10, 300, 200);
  if (crtui_button_create(s->ctx, s->container, "A", &s->button_a) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->button_a, 20, 20, 80, 30);
  if (crtui_button_create(s->ctx, s->container, "B", &s->button_b) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->button_b, 120, 20, 80, 30);
  if (crtui_slider_create(s->ctx, s->container, 0, 10, &s->slider) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->slider, 20, 80, 200, 20);
  if (crtui_text_create(s->ctx, s->container, "label", &s->text) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->text, 20, 120, 100, 20);
  crtui_widget_set_callback(s->ctx, s->win, record_cb, (void*)(intptr_t)TAG_WIN);
  crtui_widget_set_callback(s->ctx, s->container, record_cb, (void*)(intptr_t)TAG_CONTAINER);
  crtui_widget_set_callback(s->ctx, s->button_a, record_cb, (void*)(intptr_t)TAG_A);
  crtui_widget_set_callback(s->ctx, s->button_b, record_cb, (void*)(intptr_t)TAG_B);
  crtui_widget_set_callback(s->ctx, s->slider, record_cb, (void*)(intptr_t)TAG_SLIDER);
  return 0;
}

static void end_scene(scene* s) {
  CHECK(crtui_context_destroy(s->ctx) == CRTUI_OK, "context destroy");
}

static crtui_input pointer(scene* s, crtui_input_type type, int32_t x, int32_t y) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = type;
  in.window = s->win;
  in.x = x;
  in.y = y;
  return in;
}

static crtui_input key(scene* s, crtui_input_type type, crtui_key k, uint32_t modifiers) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = type;
  in.window = s->win;
  in.key = k;
  in.modifiers = modifiers;
  return in;
}

static void send(scene* s, crtui_input in) {
  CHECK(crtui_context_send_input(s->ctx, &in) == CRTUI_OK, "send input");
}

static void tap_key(scene* s, crtui_key k, uint32_t modifiers) {
  send(s, key(s, CRTUI_INPUT_KEY_DOWN, k, modifiers));
  send(s, key(s, CRTUI_INPUT_KEY_UP, k, modifiers));
}

static crtui_widget focused(scene* s) {
  crtui_widget f = CRTUI_INVALID_WIDGET;
  crtui_context_get_focus(s->ctx, &f);
  return f;
}

/* ---- 1. errors ---------------------------------------------------------- */

static void test_errors(void) {
  CHECK(CRTUI_OK == 0 && CRTUI_ERROR_INVALID_ARGUMENT == -1 && CRTUI_ERROR_UNSUPPORTED == -2 &&
            CRTUI_WOULD_BLOCK == -3 && CRTUI_ERROR_IO == -4 && CRTUI_ERROR_TIMEOUT == -5 &&
            CRTUI_ERROR_CANCELLED == -6 && CRTUI_ERROR_PROTOCOL == -7,
        "values 0..-7 mirror crtmedia_result");
  CHECK(CRTUI_ERROR_WRONG_THREAD == -8 && CRTUI_ERROR_INVALID_HANDLE == -9 && CRTUI_ERROR_STATE == -10,
        "crtui-specific codes");
  CHECK(crtui_context_create(NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "create(NULL)");
  CHECK(crtui_context_destroy(NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "destroy(NULL)");
  CHECK(crtui_window_create(NULL, NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "window_create(NULL ctx)");

  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_widget out = 1234;
  CHECK(crtui_window_create(s.ctx, NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "window_create(out NULL)");
  CHECK(crtui_button_create(s.ctx, s.container, NULL, &out) == CRTUI_ERROR_INVALID_ARGUMENT, "button text NULL");
  CHECK(crtui_button_create(s.ctx, s.text, "x", &out) == CRTUI_ERROR_INVALID_ARGUMENT,
        "a text widget cannot hold children");
  CHECK(out == CRTUI_INVALID_WIDGET, "failed create always clears *out");
  CHECK(crtui_button_create(s.ctx, (crtui_widget)0xdeadbeef, "x", &out) == CRTUI_ERROR_INVALID_HANDLE, "bogus parent");
  CHECK(crtui_slider_create(s.ctx, s.container, 5, 5, &out) == CRTUI_ERROR_INVALID_ARGUMENT, "slider min >= max");
  CHECK(crtui_surface_view_create(s.ctx, s.container, &out) == CRTUI_OK,
        "Tranche 5 SurfaceView is a real producer-neutral widget");
  crtui_widget_kind surface_kind = 0;
  CHECK(crtui_widget_get_kind(s.ctx, out, &surface_kind) == CRTUI_OK && surface_kind == CRTUI_WIDGET_SURFACE_VIEW,
        "SurfaceView kind");
  CHECK(crtui_media_view_create(s.ctx, s.container, &out) == CRTUI_OK,
        "Tranche 6 MediaView is a real producer-neutral widget");
  CHECK(crtui_widget_get_kind(s.ctx, out, &surface_kind) == CRTUI_OK && surface_kind == CRTUI_WIDGET_MEDIA_VIEW,
        "MediaView kind");
  CHECK(crtui_slider_set_value(s.ctx, s.button_a, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "slider op on a button");
  CHECK(crtui_widget_set_text(s.ctx, s.slider, "x") == CRTUI_ERROR_INVALID_ARGUMENT, "text op on a slider");
  CHECK(crtui_widget_set_bounds(s.ctx, s.win, 0, 0, 1, 1) == CRTUI_ERROR_INVALID_ARGUMENT,
        "windows are sized with crtui_window_set_size()");
  CHECK(crtui_widget_set_bounds(s.ctx, s.container, 0, 0, -2, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "negative size");
  CHECK(crtui_context_get_focus(s.ctx, &out) == CRTUI_ERROR_STATE && out == CRTUI_INVALID_WIDGET,
        "no focus -> STATE");

  char text[8];
  size_t length = 0;
  CHECK(crtui_widget_get_text(s.ctx, s.text, text, sizeof(text), &length) == CRTUI_OK && length == 5 &&
            strcmp(text, "label") == 0,
        "get_text");
  CHECK(crtui_widget_get_text(s.ctx, s.text, text, 3, &length) == CRTUI_OK && length == 5 && strcmp(text, "la") == 0,
        "get_text truncates to capacity-1 and reports the full length");
  CHECK(crtui_widget_set_text(s.ctx, s.text, "changed") == CRTUI_OK, "set_text");
  end_scene(&s);
}

/* ---- 2. lifetime -------------------------------------------------------- */

static void test_lifetime(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(s.ctx, s.button_a, &kind) == CRTUI_OK && kind == CRTUI_WIDGET_BUTTON, "kind");

  crtui_widget old_a = s.button_a;
  crtui_widget old_container = s.container;
  CHECK(crtui_widget_destroy(s.ctx, s.container) == CRTUI_OK, "destroy container");
  CHECK(crtui_widget_get_kind(s.ctx, old_container, &kind) == CRTUI_ERROR_INVALID_HANDLE, "container id is dead");
  CHECK(crtui_widget_get_kind(s.ctx, old_a, &kind) == CRTUI_ERROR_INVALID_HANDLE,
        "destroying a parent invalidates its whole subtree at once");
  CHECK(crtui_widget_get_kind(s.ctx, s.slider, &kind) == CRTUI_ERROR_INVALID_HANDLE, "grandchild id is dead");
  CHECK(crtui_widget_destroy(s.ctx, old_container) == CRTUI_ERROR_INVALID_HANDLE, "double destroy is detected");

  crtui_widget fresh = CRTUI_INVALID_WIDGET;
  CHECK(crtui_button_create(s.ctx, s.win, "again", &fresh) == CRTUI_OK, "create after destroy");
  CHECK(fresh != old_a && fresh != old_container, "a reused slot never yields an equal id");
  CHECK(crtui_widget_get_kind(s.ctx, old_a, &kind) == CRTUI_ERROR_INVALID_HANDLE, "stale id stays dead after reuse");

  crtui_window old_win = s.win;
  CHECK(crtui_widget_destroy(s.ctx, s.win) == CRTUI_OK, "destroy window");
  CHECK(crtui_widget_get_kind(s.ctx, fresh, &kind) == CRTUI_ERROR_INVALID_HANDLE, "window destroy kills children");
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = CRTUI_INPUT_POINTER_MOVE;
  in.window = old_win;
  CHECK(crtui_context_send_input(s.ctx, &in) == CRTUI_ERROR_INVALID_HANDLE, "input to a dead window is refused");
  end_scene(&s);
}

/* ---- 3. threads --------------------------------------------------------- */

typedef struct thread_probe {
  crtui_context* ctx;
  crtui_widget widget;
  crtui_result visible_result;
  crtui_result destroy_result;
  crtui_result context_destroy_result;
  crtui_result post_result;
} thread_probe;

static pthread_t ui_thread_id;
static int post_order[16];
static int post_count = 0;
static int posts_on_ui_thread = 1;

static void post_fn(void* user) {
  if (!pthread_equal(pthread_self(), ui_thread_id)) posts_on_ui_thread = 0;
  if (post_count < 16) post_order[post_count++] = (int)(intptr_t)user;
}

static void* worker(void* argument) {
  thread_probe* probe = (thread_probe*)argument;
  probe->visible_result = crtui_widget_set_visible(probe->ctx, probe->widget, 0);
  probe->destroy_result = crtui_widget_destroy(probe->ctx, probe->widget);
  probe->context_destroy_result = crtui_context_destroy(probe->ctx);
  probe->post_result = CRTUI_OK;
  for (int i = 10; i < 13; ++i) {
    if (crtui_context_post(probe->ctx, post_fn, (void*)(intptr_t)i) != CRTUI_OK) probe->post_result = CRTUI_ERROR_IO;
  }
  return NULL;
}

static void test_threads(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  ui_thread_id = pthread_self();
  post_count = 0;
  posts_on_ui_thread = 1;
  thread_probe probe;
  memset(&probe, 0, sizeof(probe));
  probe.ctx = s.ctx;
  probe.widget = s.button_a;
  pthread_t thread;
  CHECK(pthread_create(&thread, NULL, worker, &probe) == 0, "create worker");
  pthread_join(thread, NULL);
  CHECK(probe.visible_result == CRTUI_ERROR_WRONG_THREAD, "mutation from another thread is rejected");
  CHECK(probe.destroy_result == CRTUI_ERROR_WRONG_THREAD, "destroy from another thread is rejected");
  CHECK(probe.context_destroy_result == CRTUI_ERROR_WRONG_THREAD, "context destroy from another thread is rejected");
  CHECK(probe.post_result == CRTUI_OK, "post is the one thread-safe call");
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(s.ctx, s.button_a, &kind) == CRTUI_OK, "the rejected calls changed nothing");

  CHECK(crtui_context_post(s.ctx, post_fn, (void*)(intptr_t)1) == CRTUI_OK, "post from the UI thread");
  CHECK(post_count == 0, "post never runs the function inline");
  size_t ran = 0;
  CHECK(crtui_context_pump(s.ctx, &ran) == CRTUI_OK && ran == 4, "pump runs every posted function");
  CHECK(posts_on_ui_thread, "posted functions run on the UI thread");
  CHECK(post_count == 4 && post_order[0] == 10 && post_order[1] == 11 && post_order[2] == 12 && post_order[3] == 1,
        "posted functions run in FIFO order");
  end_scene(&s);
}

/* ---- 4. event order, bubbling, handled ---------------------------------- */

static void test_events(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");

  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40)); /* on button A */
  CHECK(event_count >= 4, "pointer down delivers FOCUS_IN and bubbles POINTER_DOWN");
  CHECK(focused(&s) == s.button_a, "pointer press focuses a focusable widget");
  int down_order[3];
  int n = 0;
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].type == CRTUI_EVENT_POINTER_DOWN && n < 3) down_order[n++] = event_log[i].tag;
  }
  CHECK(n == 3 && down_order[0] == TAG_A && down_order[1] == TAG_CONTAINER && down_order[2] == TAG_WIN,
        "bubbling goes target -> ancestors -> window");
  CHECK(count_type(CRTUI_EVENT_FOCUS_IN) == 1, "FOCUS_IN goes to the target only");
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].type == CRTUI_EVENT_POINTER_DOWN) {
      CHECK(event_log[i].target == s.button_a, "event->target stays the original target while bubbling");
    }
  }

  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_UP, 40, 40));
  CHECK(count_type(CRTUI_EVENT_POINTER_UP) == 3 && count_type(CRTUI_EVENT_ACTIVATE) == 3,
        "release over the same button activates it, and ACTIVATE bubbles");

  /* Press on A, release over B: no activation. */
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40));
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_UP, 140, 40));
  CHECK(count_type(CRTUI_EVENT_ACTIVATE) == 0, "press and release on different widgets never activates");

  /* A callback that handles the event stops the bubble. */
  crtui_widget_set_callback(s.ctx, s.container, handled_cb, (void*)(intptr_t)TAG_CONTAINER);
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 40, 40));
  int window_saw_move = 0;
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].tag == TAG_WIN && event_log[i].type == CRTUI_EVENT_POINTER_MOVE) window_saw_move = 1;
  }
  CHECK(count_type(CRTUI_EVENT_POINTER_MOVE) == 2 && !window_saw_move, "HANDLED stops the bubble");
  crtui_widget_set_callback(s.ctx, s.container, record_cb, (void*)(intptr_t)TAG_CONTAINER);

  /* Wheel delivers its delta. */
  log_reset();
  crtui_input wheel = pointer(&s, CRTUI_INPUT_WHEEL, 40, 40);
  wheel.wheel_delta = -3;
  send(&s, wheel);
  CHECK(count_type(CRTUI_EVENT_WHEEL) == 3 && event_log[0].value == -3, "wheel delta reaches the callback");

  /* Disabled widget (and anything under a disabled ancestor) gets nothing. */
  crtui_widget_set_enabled(s.ctx, s.button_b, 0);
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 140, 40));
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_UP, 140, 40));
  CHECK(event_count == 0, "input on a disabled widget is dropped, not passed through");
  crtui_widget_set_enabled(s.ctx, s.button_b, 1);
  crtui_widget_set_enabled(s.ctx, s.container, 0);
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40));
  CHECK(event_count == 0, "a disabled ancestor disables its subtree");
  crtui_widget_set_enabled(s.ctx, s.container, 1);
  end_scene(&s);
}

/* ---- 5. keyboard defaults and focus ------------------------------------- */

static void test_keyboard_and_focus(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");

  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.button_a, "Tab from no focus -> first focusable (pre-order)");
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.button_b, "Tab -> next");
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.slider, "Tab -> slider (text is not focusable)");
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.button_a, "Tab wraps");
  tap_key(&s, CRTUI_KEY_TAB, CRTUI_MOD_SHIFT);
  CHECK(focused(&s) == s.slider, "Shift+Tab goes backwards and wraps");

  crtui_widget_set_visible(s.ctx, s.button_b, 0);
  crtui_widget_focus(s.ctx, s.button_a);
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.slider, "a hidden widget is skipped");
  crtui_widget_set_visible(s.ctx, s.button_b, 1);
  crtui_widget_set_enabled(s.ctx, s.button_b, 0);
  crtui_widget_focus(s.ctx, s.button_a);
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.slider, "a disabled widget is skipped");
  crtui_widget_set_enabled(s.ctx, s.button_b, 1);
  CHECK(crtui_widget_focus(s.ctx, s.text) == CRTUI_ERROR_STATE, "a non-focusable widget cannot take focus");
  crtui_widget_set_focusable(s.ctx, s.text, 1);
  CHECK(crtui_widget_focus(s.ctx, s.text) == CRTUI_OK && focused(&s) == s.text, "set_focusable opts a widget in");
  crtui_widget_set_focusable(s.ctx, s.text, 0);
  CHECK(focused(&s) != s.text, "un-focusable focus moves on");

  /* Enter/Space activate the focused button. */
  crtui_widget_focus(s.ctx, s.button_a);
  log_reset();
  tap_key(&s, CRTUI_KEY_ENTER, 0);
  CHECK(count_type(CRTUI_EVENT_ACTIVATE) == 3, "Enter activates the focused button");
  log_reset();
  tap_key(&s, CRTUI_KEY_SPACE, 0);
  CHECK(count_type(CRTUI_EVENT_ACTIVATE) == 3, "Space activates the focused button");

  /* Slider arrows, clamped, VALUE_CHANGED only when the value changes. */
  crtui_widget_focus(s.ctx, s.slider);
  int32_t value = -1;
  log_reset();
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 2 && count_type(CRTUI_EVENT_VALUE_CHANGED) == 6, "Left/Right step the slider and bubble VALUE_CHANGED");
  for (int i = 0; i < 20; ++i) tap_key(&s, CRTUI_KEY_RIGHT, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 10, "clamped at max");
  log_reset();
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  CHECK(count_type(CRTUI_EVENT_VALUE_CHANGED) == 0, "no event when the value cannot change");
  for (int i = 0; i < 30; ++i) tap_key(&s, CRTUI_KEY_LEFT, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 0, "clamped at min");
  log_reset();
  CHECK(crtui_slider_set_value(s.ctx, s.slider, 99) == CRTUI_OK, "programmatic set");
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 10 && event_count == 0, "programmatic changes clamp and emit no event");

  /* A callback that handles KEY_DOWN suppresses the default action. */
  crtui_widget_focus(s.ctx, s.button_a);
  crtui_widget_set_callback(s.ctx, s.button_a, handled_cb, (void*)(intptr_t)TAG_A);
  log_reset();
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(focused(&s) == s.button_a, "a handled Tab does not move focus");
  tap_key(&s, CRTUI_KEY_ENTER, 0);
  CHECK(count_type(CRTUI_EVENT_ACTIVATE) == 0, "a handled Enter does not activate");
  crtui_widget_set_callback(s.ctx, s.button_a, record_cb, (void*)(intptr_t)TAG_A);

  /* Key events go to the window when nothing is focused. */
  crtui_widget_destroy(s.ctx, s.button_a);
  crtui_widget_destroy(s.ctx, s.button_b);
  crtui_widget_destroy(s.ctx, s.slider);
  CHECK(focused(&s) == CRTUI_INVALID_WIDGET, "no focusable widget left -> focus clears");
  log_reset();
  tap_key(&s, CRTUI_KEY_ENTER, 0);
  CHECK(count_type(CRTUI_EVENT_KEY_DOWN) == 1 && event_log[0].tag == TAG_WIN, "unfocused key input goes to the window");
  end_scene(&s);
}

/* ---- 6. focus recovery -------------------------------------------------- */

static void test_focus_recovery(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_widget_focus(s.ctx, s.button_b);
  log_reset();
  crtui_widget_destroy(s.ctx, s.button_b);
  CHECK(focused(&s) == s.slider, "destroying the focused widget moves focus to the next eligible widget");
  CHECK(count_type(CRTUI_EVENT_FOCUS_OUT) == 0 && count_type(CRTUI_EVENT_FOCUS_IN) == 1,
        "FOCUS_OUT is never sent to a destroyed widget");

  log_reset();
  crtui_widget_set_visible(s.ctx, s.slider, 0);
  CHECK(focused(&s) == s.button_a, "hiding the focused widget moves focus on, wrapping");
  CHECK(count_type(CRTUI_EVENT_FOCUS_OUT) == 1 && count_type(CRTUI_EVENT_FOCUS_IN) == 1,
        "a hidden (but alive) widget still gets FOCUS_OUT");

  crtui_widget_set_enabled(s.ctx, s.button_a, 0);
  CHECK(focused(&s) == CRTUI_INVALID_WIDGET, "no eligible widget -> focus clears");
  end_scene(&s);
}

/* ---- 7. re-entrancy ----------------------------------------------------- */

static scene* reentrant_scene;
static int last_activate_index = -1;

static int destroy_self_cb(crtui_widget current, const crtui_event* event, void* user) {
  (void)user;
  if (event->type != CRTUI_EVENT_ACTIVATE) {
    return CRTUI_EVENT_IGNORED;
  }
  record_cb(current, event, (void*)(intptr_t)TAG_A);
  last_activate_index = event_count - 1;
  /* Input sent from inside a callback is queued, never delivered nested. */
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = CRTUI_INPUT_KEY_UP;
  in.window = reentrant_scene->win;
  in.key = CRTUI_KEY_ENTER;
  CHECK(crtui_context_send_input(reentrant_scene->ctx, &in) == CRTUI_OK, "send from inside a callback");
  CHECK(count_type(CRTUI_EVENT_KEY_UP) == 0, "input sent from a callback is queued, not delivered nested");
  CHECK(crtui_widget_destroy(reentrant_scene->ctx, current) == CRTUI_OK, "a widget may destroy itself in its callback");
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(reentrant_scene->ctx, current, &kind) == CRTUI_ERROR_INVALID_HANDLE,
        "its id is invalid immediately");
  return CRTUI_EVENT_HANDLED;
}

static void test_reentrancy(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  reentrant_scene = &s;
  crtui_widget victim = s.button_a;
  crtui_widget_set_callback(s.ctx, victim, destroy_self_cb, NULL);
  crtui_widget_focus(s.ctx, victim);
  log_reset();
  send(&s, key(&s, CRTUI_INPUT_KEY_DOWN, CRTUI_KEY_ENTER, 0));
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(s.ctx, victim, &kind) == CRTUI_ERROR_INVALID_HANDLE, "the victim is gone");
  CHECK(count_type(CRTUI_EVENT_ACTIVATE) == 1, "the handled ACTIVATE did not bubble");
  int first_key_up = -1;
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].type == CRTUI_EVENT_KEY_UP) {
      first_key_up = i;
      break;
    }
  }
  CHECK(first_key_up > last_activate_index, "the queued input was delivered after the current event finished");
  CHECK(focused(&s) == s.button_b, "focus recovered onto the next widget after the self-destroy");
  CHECK(first_key_up >= 0 && event_log[first_key_up].target == s.button_b,
        "the queued key went to the new focus, not the dead widget");

  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40));
  for (int i = 0; i < event_count; ++i) {
    CHECK(event_log[i].tag != TAG_A, "no callback ever fires for a destroyed widget");
  }
  end_scene(&s);
}

/* ---- 8. modal routing --------------------------------------------------- */

static void test_modal(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_window w2 = CRTUI_INVALID_WIDGET;
  crtui_widget m = CRTUI_INVALID_WIDGET;
  CHECK(crtui_window_create(s.ctx, &w2) == CRTUI_OK, "second window");
  crtui_window_set_size(s.ctx, w2, 200, 100, 1.0f);
  CHECK(crtui_button_create(s.ctx, w2, "M", &m) == CRTUI_OK, "modal button");
  crtui_widget_set_bounds(s.ctx, m, 10, 10, 50, 20);
  crtui_widget_set_callback(s.ctx, m, record_cb, (void*)(intptr_t)9);

  crtui_widget_focus(s.ctx, s.button_a);
  CHECK(crtui_window_set_modal(s.ctx, w2, 1) == CRTUI_OK, "set_modal");
  CHECK(focused(&s) == m, "focus inside a now-blocked window moves into the modal window");
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40));
  tap_key(&s, CRTUI_KEY_TAB, 0);
  CHECK(event_count == 0 && focused(&s) == m, "input for any other window is dropped while a modal window exists");
  CHECK(crtui_widget_focus(s.ctx, s.button_a) == CRTUI_ERROR_STATE, "focus cannot enter a blocked window");

  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = CRTUI_INPUT_POINTER_DOWN;
  in.window = w2;
  in.x = 20;
  in.y = 20;
  CHECK(crtui_context_send_input(s.ctx, &in) == CRTUI_OK, "input to the modal window");
  CHECK(count_type(CRTUI_EVENT_POINTER_DOWN) == 1, "the modal window still receives input");

  crtui_window w3 = CRTUI_INVALID_WIDGET;
  crtui_window_create(s.ctx, &w3);
  crtui_window_set_size(s.ctx, w3, 100, 100, 1.0f);
  crtui_window_set_modal(s.ctx, w3, 1);
  log_reset();
  in.window = w2;
  crtui_context_send_input(s.ctx, &in);
  CHECK(event_count == 0, "with two modal windows only the most recently modal one takes input");

  crtui_window_set_modal(s.ctx, w3, 0);
  crtui_window_set_modal(s.ctx, w2, 0);
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 40, 40));
  CHECK(count_type(CRTUI_EVENT_POINTER_DOWN) == 3, "input flows to the first window again once no window is modal");
  end_scene(&s);
}

/* ---- 9. hit-testing and clipping ---------------------------------------- */

static void test_hit_testing(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_widget out = CRTUI_INVALID_WIDGET;
  CHECK(crtui_context_hit_test(s.ctx, s.win, 40, 40, &out) == CRTUI_OK && out == s.button_a, "hit a button");
  CHECK(crtui_context_hit_test(s.ctx, s.win, 15, 15, &out) == CRTUI_OK && out == s.container,
        "the container where no child covers");
  CHECK(crtui_context_hit_test(s.ctx, s.win, 350, 250, &out) == CRTUI_OK && out == s.win,
        "the window where nothing covers");
  CHECK(crtui_context_hit_test(s.ctx, s.win, 400, 10, &out) == CRTUI_OK && out == CRTUI_INVALID_WIDGET,
        "outside the window is nothing");
  CHECK(crtui_context_hit_test(s.ctx, s.win, -1, 10, &out) == CRTUI_OK && out == CRTUI_INVALID_WIDGET, "negative");

  /* Clipping: a child larger than its parent is only hittable inside the parent. */
  crtui_widget big = CRTUI_INVALID_WIDGET;
  CHECK(crtui_container_create(s.ctx, s.container, &big) == CRTUI_OK, "oversized child");
  crtui_widget_set_bounds(s.ctx, big, 250, 150, 200, 100); /* abs (260,160)-(460,260); container ends at (310,210) */
  CHECK(crtui_context_hit_test(s.ctx, s.win, 280, 170, &out) == CRTUI_OK && out == big, "inside both");
  CHECK(crtui_context_hit_test(s.ctx, s.win, 350, 180, &out) == CRTUI_OK && out == s.win,
        "a child is clipped to every ancestor");

  /* z-order: later siblings are on top. */
  crtui_widget y1 = CRTUI_INVALID_WIDGET;
  crtui_widget y2 = CRTUI_INVALID_WIDGET;
  crtui_button_create(s.ctx, s.container, "y1", &y1);
  crtui_button_create(s.ctx, s.container, "y2", &y2);
  crtui_widget_set_bounds(s.ctx, y1, 0, 150, 50, 40);
  crtui_widget_set_bounds(s.ctx, y2, 0, 150, 50, 40);
  crtui_context_hit_test(s.ctx, s.win, 20, 170, &out);
  CHECK(out == y2, "later siblings are on top");
  crtui_widget_set_visible(s.ctx, y2, 0);
  crtui_context_hit_test(s.ctx, s.win, 20, 170, &out);
  CHECK(out == y1, "a hidden widget is never hit");
  crtui_widget_set_visible(s.ctx, s.container, 0);
  crtui_context_hit_test(s.ctx, s.win, 20, 170, &out);
  CHECK(out == s.win, "hiding a parent hides its subtree");
  end_scene(&s);
}

/* ---- 10. geometry and resize -------------------------------------------- */

static void test_geometry(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  int32_t x, y, w, h;
  CHECK(crtui_widget_get_bounds(s.ctx, s.button_a, &x, &y, &w, &h) == CRTUI_OK && x == 20 && y == 20 && w == 80 &&
            h == 30,
        "bounds are parent-relative");
  log_reset();
  CHECK(crtui_window_set_size(s.ctx, s.win, 500, 400, 1.5f) == CRTUI_OK, "resize");
  CHECK(count_type(CRTUI_EVENT_RESIZED) == 1 && event_log[0].tag == TAG_WIN && event_log[0].value == 150,
        "RESIZED goes to the window only, carrying the DPI scale in percent");
  float scale = 0;
  CHECK(crtui_window_get_size(s.ctx, s.win, &w, &h, &scale) == CRTUI_OK && w == 500 && h == 400 && scale == 1.5f,
        "window size and scale");
  crtui_widget out = CRTUI_INVALID_WIDGET;
  crtui_context_hit_test(s.ctx, s.win, 450, 350, &out);
  CHECK(out == s.win, "the new size is what hit-testing uses");
  CHECK(crtui_window_set_size(s.ctx, s.win, 500, 400, 0.0f) == CRTUI_ERROR_INVALID_ARGUMENT, "scale must be > 0");
  CHECK(crtui_window_set_size(s.ctx, s.win, -1, 400, 1.0f) == CRTUI_ERROR_INVALID_ARGUMENT, "negative size");
  CHECK(crtui_window_set_size(s.ctx, s.button_a, 1, 1, 1.0f) == CRTUI_ERROR_INVALID_ARGUMENT, "only windows resize");
  end_scene(&s);
}

/* ---- 11. progress (Tranche 1 addition) ---------------------------------- */

static void test_progress(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  crtui_widget p = CRTUI_INVALID_WIDGET;
  CHECK(crtui_progress_create(s.ctx, s.container, &p) == CRTUI_OK, "progress create");
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(s.ctx, p, &kind) == CRTUI_OK && kind == CRTUI_WIDGET_PROGRESS, "kind");
  int32_t value = -1;
  CHECK(crtui_progress_get_value(s.ctx, p, &value) == CRTUI_OK && value == 0, "starts at 0");
  CHECK(crtui_progress_set_value(s.ctx, p, 250) == CRTUI_OK, "set");
  crtui_progress_get_value(s.ctx, p, &value);
  CHECK(value == 100, "clamped to 0..100");
  crtui_progress_set_value(s.ctx, p, -5);
  crtui_progress_get_value(s.ctx, p, &value);
  CHECK(value == 0, "clamped at 0");
  CHECK(crtui_widget_focus(s.ctx, p) == CRTUI_ERROR_STATE, "a progress bar is not focusable");
  CHECK(crtui_progress_set_value(s.ctx, s.slider, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "progress op on a slider");
  CHECK(crtui_slider_get_value(s.ctx, p, &value) == CRTUI_ERROR_INVALID_ARGUMENT, "slider op on a progress bar");
  CHECK(crtui_progress_create(s.ctx, s.text, &p) == CRTUI_ERROR_INVALID_ARGUMENT, "a text widget cannot hold it");
  CHECK(crtui_progress_create(s.ctx, s.container, NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "out NULL");
  end_scene(&s);
}

/* ---- 12. Tranche 2: arrow navigation, slider pointer/wheel ---------------- */

static void test_arrow_navigation(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  /* Absolute centers: A (70,45), B (170,45), slider (130,100). */
  tap_key(&s, CRTUI_KEY_LEFT, 0);
  CHECK(focused(&s) == s.button_a, "an arrow with nothing focused focuses the first eligible widget");
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  CHECK(focused(&s) == s.button_b, "Right moves to the widget on the right");
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  CHECK(focused(&s) == s.button_b, "no widget further right: focus stays (no wrap)");
  tap_key(&s, CRTUI_KEY_DOWN, 0);
  CHECK(focused(&s) == s.slider, "Down moves to the widget below");
  tap_key(&s, CRTUI_KEY_UP, 0);
  CHECK(focused(&s) == s.button_b, "Up from the slider picks the best-aligned widget above (B, not A)");
  tap_key(&s, CRTUI_KEY_LEFT, 0);
  CHECK(focused(&s) == s.button_a, "Left moves to the widget on the left");
  tap_key(&s, CRTUI_KEY_LEFT, 0);
  CHECK(focused(&s) == s.button_a, "no widget further left: focus stays");

  /* A focused slider consumes Left/Right but still lets Up/Down navigate. */
  crtui_widget_focus(s.ctx, s.slider);
  int32_t value = -1;
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 2 && focused(&s) == s.slider, "Right steps the focused slider and keeps focus");
  tap_key(&s, CRTUI_KEY_LEFT, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 1, "Left steps it back");
  tap_key(&s, CRTUI_KEY_UP, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(focused(&s) == s.button_b && value == 1, "Up leaves the slider without changing its value");

  /* A hidden or disabled widget is not a navigation target. */
  crtui_widget_set_enabled(s.ctx, s.button_b, 0);
  crtui_widget_focus(s.ctx, s.button_a);
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  CHECK(focused(&s) == s.slider, "a disabled widget is skipped: Right lands on the next candidate, not on B");
  crtui_widget_set_enabled(s.ctx, s.button_b, 1);

  /* A callback that handles the arrow suppresses navigation. */
  crtui_widget_focus(s.ctx, s.button_a);
  crtui_widget_set_callback(s.ctx, s.button_a, handled_cb, (void*)(intptr_t)TAG_A);
  tap_key(&s, CRTUI_KEY_RIGHT, 0);
  CHECK(focused(&s) == s.button_a, "a handled arrow does not navigate");
  end_scene(&s);
}

static void test_slider_pointer(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  /* Slider abs x 30..230 (width 200), range 0..10. */
  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 130, 100));
  int32_t value = -1;
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 5, "pressing the middle of the slider sets it to the middle");
  CHECK(focused(&s) == s.slider, "a press on a slider focuses it");
  CHECK(count_type(CRTUI_EVENT_VALUE_CHANGED) == 3, "VALUE_CHANGED bubbles");

  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 280, 300)); /* far outside the slider and the window's content */
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 10, "a captured drag past the right end clamps at max");
  int slider_saw_move = 0;
  for (int i = 0; i < event_count; ++i) {
    if (event_log[i].type == CRTUI_EVENT_POINTER_MOVE && event_log[i].tag == TAG_SLIDER) slider_saw_move = 1;
  }
  CHECK(slider_saw_move, "moves are delivered to the captured slider even outside its bounds");
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 10, 250));
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 0, "dragging past the left end clamps at min");
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 80, 250));
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 2 || value == 3, "dragging back inside sets the proportional value");

  log_reset();
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_UP, 180, 250));
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 7 || value == 8, "the release position is applied");
  CHECK(count_type(CRTUI_EVENT_POINTER_UP) == 3, "the release is delivered to the slider and bubbles");
  int32_t after_release = value;
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 40, 100));
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == after_release, "the capture ended with the release");

  /* Wheel. */
  crtui_slider_set_value(s.ctx, s.slider, 5);
  crtui_input wheel = pointer(&s, CRTUI_INPUT_WHEEL, 130, 100);
  wheel.wheel_delta = 3;
  send(&s, wheel);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 6, "wheel up steps the slider up by one");
  wheel.wheel_delta = -120;
  send(&s, wheel);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 5, "wheel down steps it down by one");
  wheel.wheel_delta = 0;
  send(&s, wheel);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 5, "a zero delta does nothing");
  crtui_widget_set_callback(s.ctx, s.slider, handled_cb, (void*)(intptr_t)TAG_SLIDER);
  wheel.wheel_delta = 1;
  send(&s, wheel);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == 5, "a handled wheel event suppresses the default step");
  crtui_widget_set_callback(s.ctx, s.slider, record_cb, (void*)(intptr_t)TAG_SLIDER);

  /* A capture ends if the slider becomes ineligible or is destroyed. */
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 130, 100));
  crtui_widget_set_enabled(s.ctx, s.slider, 0);
  crtui_slider_get_value(s.ctx, s.slider, &value);
  int32_t before = value;
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 220, 100));
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value == before, "disabling the slider mid-drag stops the drag");
  crtui_widget_set_enabled(s.ctx, s.slider, 1);
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_DOWN, 130, 100));
  crtui_widget_destroy(s.ctx, s.slider);
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_MOVE, 200, 100));
  send(&s, pointer(&s, CRTUI_INPUT_POINTER_UP, 200, 100));
  crtui_widget_kind kind;
  CHECK(crtui_widget_get_kind(s.ctx, s.slider, &kind) == CRTUI_ERROR_INVALID_HANDLE,
        "destroying the slider mid-drag is safe");
  end_scene(&s);
}

int main(void) {
  test_errors();
  test_lifetime();
  test_threads();
  test_events();
  test_keyboard_and_focus();
  test_focus_recovery();
  test_reentrancy();
  test_modal();
  test_hit_testing();
  test_geometry();
  test_progress();
  test_arrow_navigation();
  test_slider_pointer();
  if (failures != 0) {
    fprintf(stderr, "crtui_contract_test: %d failure(s)\n", failures);
    return 1;
  }
  printf(
      "crtui_contract_test: ok errors=pass lifetime=pass threads=pass events=pass keyboard_focus=pass "
      "focus_recovery=pass reentrancy=pass modal=pass hit_test=pass geometry=pass progress=pass arrow_nav=pass slider_pointer=pass\n");
  return 0;
}
