/* crtui Tranche 2 acceptance (docs/acceptance/crtui_acceptance.md): CRT keyboard, pointer,
 * wheel and resize events -> crtui, through the crtgfx adapter, checked against
 * the widget model. Headless: crtgfx_event values are synthesized (the same
 * struct a real crtgfx window queues), so this needs no window, display or
 * LVGL and runs on every host. It covers: pointer hit-test and click, Tab focus
 * traversal, Enter/Space activation, arrow navigation, slider keyboard
 * adjustment and drag, wheel, resize/re-layout by the application, focus loss
 * (pointer cancel) and recovery. */

#include "crtui/crtgfx.h"

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

/* Linux evdev keycodes, as crtgfx defines them. */
enum {
  K_ESC = 1,
  K_BACKSPACE = 14,
  K_A = 30,
  K_HOME = 102,
  K_END = 107,
  K_DELETE = 111,
  K_TAB = 15,
  K_ENTER = 28,
  K_SPACE = 57,
  K_KPENTER = 96,
  K_UP = 103,
  K_LEFT = 105,
  K_RIGHT = 106,
  K_DOWN = 108
};

typedef struct app {
  crtui_context* ctx;
  crtui_window win;
  crtui_widget button_a, button_b, slider;
  crtui_crtgfx_state state;
  int activate_a, activate_b;
  int value_changes;
  int resized;
  int32_t resized_w, resized_h, resized_scale_percent;
  int last_key;   /* last KEY_DOWN key seen by the window callback */
  int key_events; /* KEY_DOWN events seen by the window callback */
} app;

/* The application's own re-layout: keep B glued to the right edge on resize. */
static int on_event(crtui_widget current, const crtui_event* e, void* user) {
  app* a = (app*)user;
  if (e->type == CRTUI_EVENT_ACTIVATE) {
    if (current == e->target && e->target == a->button_a) ++a->activate_a;
    if (current == e->target && e->target == a->button_b) ++a->activate_b;
  } else if (e->type == CRTUI_EVENT_VALUE_CHANGED && current == a->slider) {
    ++a->value_changes;
  } else if (e->type == CRTUI_EVENT_KEY_DOWN && current == a->win) {
    ++a->key_events;
    a->last_key = (int)e->key;
  } else if (e->type == CRTUI_EVENT_RESIZED) {
    ++a->resized;
    a->resized_w = e->x;
    a->resized_h = e->y;
    a->resized_scale_percent = e->value;
    crtui_widget_set_bounds(a->ctx, a->button_b, e->x - 100, 20, 80, 30);
    crtui_widget_set_bounds(a->ctx, a->slider, 20, 80, e->x - 40, 20);
  }
  return CRTUI_EVENT_IGNORED;
}

static int build(app* a) {
  memset(a, 0, sizeof(*a));
  if (crtui_context_create(&a->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(a->ctx, &a->win) != CRTUI_OK) return -1;
  crtui_window_set_size(a->ctx, a->win, 320, 200, 1.0f);
  if (crtui_button_create(a->ctx, a->win, "A", &a->button_a) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(a->ctx, a->button_a, 20, 20, 80, 30);
  if (crtui_button_create(a->ctx, a->win, "B", &a->button_b) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(a->ctx, a->button_b, 220, 20, 80, 30);
  if (crtui_slider_create(a->ctx, a->win, 0, 100, &a->slider) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(a->ctx, a->slider, 20, 80, 280, 20);
  crtui_widget_set_callback(a->ctx, a->win, on_event, a);
  crtui_widget_set_callback(a->ctx, a->button_a, on_event, a);
  crtui_widget_set_callback(a->ctx, a->button_b, on_event, a);
  crtui_widget_set_callback(a->ctx, a->slider, on_event, a);
  a->resized = 0;
  return 0;
}

static crtui_widget focus_of(app* a) {
  crtui_widget f = CRTUI_INVALID_WIDGET;
  crtui_context_get_focus(a->ctx, &f);
  return f;
}

static int32_t slider_value(app* a) {
  int32_t v = -1;
  crtui_slider_get_value(a->ctx, a->slider, &v);
  return v;
}

static crtui_result feed(app* a, const crtgfx_event* e) {
  return crtui_window_handle_crtgfx_event(a->ctx, a->win, &a->state, e);
}

static void key(app* a, int down, uint32_t code, uint32_t mods) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = down ? CRTGFX_EVENT_KEY_DOWN : CRTGFX_EVENT_KEY_UP;
  e.data.key.keycode = code;
  e.data.key.modifiers = mods;
  feed(a, &e);
}

static void tap(app* a, uint32_t code, uint32_t mods) {
  key(a, 1, code, mods);
  key(a, 0, code, mods);
}

static void motion(app* a, double x, double y) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_POINTER_MOTION;
  e.data.pointer_motion.x = x;
  e.data.pointer_motion.y = y;
  feed(a, &e);
}

static void button(app* a, int down, uint32_t which, double x, double y) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = down ? CRTGFX_EVENT_POINTER_BUTTON_DOWN : CRTGFX_EVENT_POINTER_BUTTON_UP;
  e.data.pointer_button.button = which;
  e.data.pointer_button.x = x;
  e.data.pointer_button.y = y;
  feed(a, &e);
}

static void click(app* a, double x, double y) {
  button(a, 1, CRTGFX_POINTER_BUTTON_LEFT, x, y);
  button(a, 0, CRTGFX_POINTER_BUTTON_LEFT, x, y);
}

static void scroll(app* a, double dy) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_POINTER_SCROLL;
  e.data.pointer_scroll.dy = dy;
  feed(a, &e);
}

static void simple(app* a, crtgfx_event_type type) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  feed(a, &e);
}

static void end(app* a) {
  CHECK(crtui_context_destroy(a->ctx) == CRTUI_OK, "destroy");
}

/* ---- tests -------------------------------------------------------------- */

static void test_keyboard(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  tap(&a, K_TAB, 0);
  CHECK(focus_of(&a) == a.button_a, "Tab focuses the first widget");
  tap(&a, K_TAB, 0);
  CHECK(focus_of(&a) == a.button_b, "Tab moves on");
  tap(&a, K_TAB, CRTGFX_MOD_SHIFT);
  CHECK(focus_of(&a) == a.button_a, "Shift+Tab goes back (the Shift modifier is carried)");
  tap(&a, K_ENTER, 0);
  CHECK(a.activate_a == 1, "Enter activates the focused button");
  tap(&a, K_SPACE, 0);
  CHECK(a.activate_a == 2, "Space activates it");
  tap(&a, K_KPENTER, 0);
  CHECK(a.activate_a == 3, "keypad Enter activates it");

  int before = a.key_events;
  tap(&a, K_A, 0);
  CHECK(a.key_events == before, "a key crtui has no meaning for yet is ignored");
  tap(&a, K_ESC, 0);
  CHECK(a.key_events == before + 1 && a.last_key == (int)CRTUI_KEY_ESCAPE, "Escape reaches the application");

  /* Arrow navigation: A (20,20) B (220,20) slider (20,80). */
  tap(&a, K_RIGHT, 0);
  CHECK(focus_of(&a) == a.button_b, "Right moves focus to B");
  tap(&a, K_DOWN, 0);
  CHECK(focus_of(&a) == a.slider, "Down moves focus to the slider");
  tap(&a, K_UP, 0);
  CHECK(focus_of(&a) == a.button_b || focus_of(&a) == a.button_a, "Up leaves the slider");
  crtui_widget_focus(a.ctx, a.slider);
  int32_t v0 = slider_value(&a);
  tap(&a, K_RIGHT, 0);
  tap(&a, K_RIGHT, 0);
  tap(&a, K_LEFT, 0);
  CHECK(slider_value(&a) == v0 + 1 && a.value_changes == 3, "Left/Right adjust the focused slider one step at a time");
  CHECK(focus_of(&a) == a.slider, "adjusting does not move focus");

  /* Key release without a press changes nothing. */
  int activations = a.activate_a + a.activate_b;
  crtui_widget_focus(a.ctx, a.button_a);
  key(&a, 0, K_ENTER, 0);
  CHECK(a.activate_a + a.activate_b == activations, "a lone key release activates nothing");
  end(&a);
}

static void test_pointer(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  motion(&a, 40.4, 35.6);
  CHECK(a.state.pointer_x == 40 && a.state.pointer_y == 36, "pointer positions are rounded to logical pixels");
  click(&a, 40, 35);
  CHECK(a.activate_a == 1 && focus_of(&a) == a.button_a, "a left click on A activates and focuses it");
  click(&a, 240, 35);
  CHECK(a.activate_b == 1 && focus_of(&a) == a.button_b, "a left click on B activates it");
  button(&a, 1, CRTGFX_POINTER_BUTTON_RIGHT, 40, 35);
  button(&a, 0, CRTGFX_POINTER_BUTTON_RIGHT, 40, 35);
  button(&a, 1, CRTGFX_POINTER_BUTTON_MIDDLE, 40, 35);
  button(&a, 0, CRTGFX_POINTER_BUTTON_MIDDLE, 40, 35);
  CHECK(a.activate_a == 1, "right and middle buttons never activate");
  click(&a, 150, 150);
  CHECK(a.activate_a == 1 && a.activate_b == 1, "a click on empty window area activates nothing");
  button(&a, 1, CRTGFX_POINTER_BUTTON_LEFT, 40, 35);
  button(&a, 0, CRTGFX_POINTER_BUTTON_LEFT, 240, 35);
  CHECK(a.activate_a == 1 && a.activate_b == 1, "press on A released over B activates neither");

  /* Slider: drag with the button held. Slider abs x 20..300 (width 280). */
  button(&a, 1, CRTGFX_POINTER_BUTTON_LEFT, 160, 90);
  CHECK(slider_value(&a) == 50, "pressing the middle of the slider sets 50");
  motion(&a, 300 + 60, 500);
  CHECK(slider_value(&a) == 100, "dragging past the end (and off the slider) clamps at 100");
  motion(&a, 90, 500);
  CHECK(slider_value(&a) == 25, "dragging back sets the proportional value");
  button(&a, 0, CRTGFX_POINTER_BUTTON_LEFT, 90, 500);
  motion(&a, 250, 90);
  CHECK(slider_value(&a) == 25, "motion after the release no longer drags");

  /* Wheel over the slider. */
  motion(&a, 160, 90);
  scroll(&a, 10.0);
  CHECK(slider_value(&a) == 26, "a scroll steps the slider one step in the delta's direction");
  scroll(&a, -10.0);
  CHECK(slider_value(&a) == 25, "and the other way");
  scroll(&a, 0.2);
  CHECK(slider_value(&a) == 26, "a small non-zero scroll is never rounded away");
  scroll(&a, 0.0);
  CHECK(slider_value(&a) == 26, "a zero scroll does nothing");
  end(&a);
}

static void test_resize(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_RESIZE;
  e.data.resize.width = 640;
  e.data.resize.height = 480;
  CHECK(feed(&a, &e) == CRTUI_OK, "resize event");
  int32_t w, h;
  float scale;
  CHECK(crtui_window_get_size(a.ctx, a.win, &w, &h, &scale) == CRTUI_OK && w == 640 && h == 480 && scale == 1.0f,
        "the window follows the resize and keeps its DPI scale");
  CHECK(a.resized == 1 && a.resized_w == 640 && a.resized_h == 480 && a.resized_scale_percent == 100,
        "RESIZED reaches the application with the new size");
  int32_t x, y, bw, bh;
  crtui_widget_get_bounds(a.ctx, a.button_b, &x, &y, &bw, &bh);
  CHECK(x == 540, "the application's RESIZED handler re-laid out B against the new width");

  /* Hit-testing follows the new layout. */
  click(&a, 240, 35);
  CHECK(a.activate_b == 0, "the old position of B no longer hits it");
  click(&a, 560, 35);
  CHECK(a.activate_b == 1, "the new position of B does");
  button(&a, 1, CRTGFX_POINTER_BUTTON_LEFT, 20 + 600 / 2, 90);
  CHECK(slider_value(&a) >= 49 && slider_value(&a) <= 51, "the slider spans the widened area");
  button(&a, 0, CRTGFX_POINTER_BUTTON_LEFT, 20 + 600 / 2, 90);

  /* DPI scale change keeps the size and reports the scale. */
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_DPI_SCALE_CHANGED;
  e.data.dpi_scale.scale = 1.5;
  CHECK(feed(&a, &e) == CRTUI_OK, "dpi event");
  crtui_window_get_size(a.ctx, a.win, &w, &h, &scale);
  CHECK(w == 640 && h == 480 && scale == 1.5f && a.resized_scale_percent == 150, "the scale changes, the size does not");

  /* Focus survives a resize. */
  crtui_widget_focus(a.ctx, a.slider);
  e.type = CRTGFX_EVENT_RESIZE;
  e.data.resize.width = 800;
  e.data.resize.height = 600;
  feed(&a, &e);
  CHECK(focus_of(&a) == a.slider, "a resize does not disturb focus");
  end(&a);
}

static void test_focus_loss_and_recovery(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  /* Losing keyboard focus mid-click cancels it. */
  button(&a, 1, CRTGFX_POINTER_BUTTON_LEFT, 40, 35);
  simple(&a, CRTGFX_EVENT_FOCUS_OUT);
  button(&a, 0, CRTGFX_POINTER_BUTTON_LEFT, 40, 35);
  CHECK(a.activate_a == 0, "a press interrupted by focus loss never activates");
  /* ... and cancels a slider drag. */
  button(&a, 1, CRTGFX_POINTER_BUTTON_LEFT, 160, 90);
  int32_t dragged = slider_value(&a);
  simple(&a, CRTGFX_EVENT_FOCUS_OUT);
  motion(&a, 290, 90);
  CHECK(slider_value(&a) == dragged, "a drag interrupted by focus loss stops following the pointer");
  simple(&a, CRTGFX_EVENT_FOCUS_IN);
  click(&a, 40, 35);
  CHECK(a.activate_a == 1, "interaction works again after focus returns");

  /* Recovery: the focused widget disappears while the user interacts. */
  crtui_widget_focus(a.ctx, a.button_b);
  crtui_widget_destroy(a.ctx, a.button_b);
  CHECK(focus_of(&a) == a.slider, "focus recovers onto the next eligible widget");
  tap(&a, K_TAB, 0);
  CHECK(focus_of(&a) == a.button_a, "traversal continues from the recovered focus");

  /* Events that crtui does not consume are harmless. */
  simple(&a, CRTGFX_EVENT_EXPOSE);
  simple(&a, CRTGFX_EVENT_FRAME_COMPLETE);
  simple(&a, CRTGFX_EVENT_CLOSE_REQUESTED);
  simple(&a, CRTGFX_EVENT_NONE);
  crtgfx_event text;
  memset(&text, 0, sizeof(text));
  text.type = CRTGFX_EVENT_TEXT;
  text.data.text.utf8[0] = 'a';
  CHECK(feed(&a, &text) == CRTUI_OK, "TEXT is ignored until text input exists");
  end(&a);
}

static void test_errors(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_POINTER_MOTION;
  CHECK(crtui_window_handle_crtgfx_event(NULL, a.win, &a.state, &e) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL context");
  CHECK(crtui_window_handle_crtgfx_event(a.ctx, a.win, NULL, &e) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL state");
  CHECK(crtui_window_handle_crtgfx_event(a.ctx, a.win, &a.state, NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL event");
  crtui_window dead = a.win;
  crtui_widget_destroy(a.ctx, a.win);
  CHECK(crtui_window_handle_crtgfx_event(a.ctx, dead, &a.state, &e) == CRTUI_ERROR_INVALID_HANDLE,
        "an event for a destroyed window is refused, not delivered");
  e.type = CRTGFX_EVENT_RESIZE;
  e.data.resize.width = 10;
  e.data.resize.height = 10;
  CHECK(crtui_window_handle_crtgfx_event(a.ctx, dead, &a.state, &e) == CRTUI_ERROR_INVALID_HANDLE,
        "resize for a destroyed window is refused");
  end(&a);
}

/* ---- Tranche 4: text entry, toggles and scrolling through the adapter --------- */

static void text_event(app* a, const char* utf8) {
  crtgfx_event e;
  memset(&e, 0, sizeof(e));
  e.type = CRTGFX_EVENT_TEXT;
  strncpy(e.data.text.utf8, utf8, sizeof(e.data.text.utf8) - 1);
  feed(a, &e);
}

static void text_is(app* a, crtui_widget w, const char* expected, const char* message) {
  char buffer[256];
  size_t length = 0;
  crtui_widget_get_text(a->ctx, w, buffer, sizeof(buffer), &length);
  CHECK(strcmp(buffer, expected) == 0, message);
}

static void test_v1_input(void) {
  app a;
  CHECK(build(&a) == 0, "app");
  crtui_widget input, sw, list, rows[6];
  crtui_text_input_create(a.ctx, a.win, "", &input);
  crtui_widget_set_bounds(a.ctx, input, 20, 120, 200, 26);
  crtui_switch_create(a.ctx, a.win, &sw);
  crtui_widget_set_bounds(a.ctx, sw, 240, 120, 50, 24);
  crtui_list_create(a.ctx, a.win, &list);
  crtui_widget_set_bounds(a.ctx, list, 20, 150, 120, 48);
  for (int i = 0; i < 6; ++i) crtui_list_add_item(a.ctx, list, "row", &rows[i]);

  /* Typing: TEXT events reach the focused text input; editing keys use evdev codes. */
  text_event(&a, "x");
  text_is(&a, input, "", "text with nothing focused goes nowhere");
  click(&a, 60, 130);
  text_event(&a, "h");
  text_event(&a, "i");
  text_is(&a, input, "hi", "clicking a text input focuses it; TEXT events type into it");
  tap(&a, K_HOME, 0);
  text_event(&a, "o");
  text_is(&a, input, "ohi", "Home moves the caret to the start");
  tap(&a, K_END, 0);
  tap(&a, K_BACKSPACE, 0);
  text_is(&a, input, "oh", "Backspace deletes before the caret");
  tap(&a, K_HOME, 0);
  tap(&a, K_DELETE, 0);
  text_is(&a, input, "h", "Delete deletes after it");
  text_event(&a, "\xC3\xA9");
  text_is(&a, input, "\xC3\xA9h", "a UTF-8 character typed through crtgfx TEXT is inserted whole");
  tap(&a, K_LEFT, 0);
  crtui_widget f = CRTUI_INVALID_WIDGET;
  crtui_context_get_focus(a.ctx, &f);
  CHECK(f == input, "Left stays in the text input (it moves the caret)");
  int activations = a.activate_a + a.activate_b;
  tap(&a, K_ENTER, 0);
  crtui_context_get_focus(a.ctx, &f);
  CHECK(f == input && a.activate_a + a.activate_b == activations, "Enter submits without leaving the field");
  tap(&a, K_TAB, 0);
  crtui_context_get_focus(a.ctx, &f);
  CHECK(f == sw || f == rows[0], "Tab moves on to the next focusable widget");

  /* Switch: click and Space. */
  int checked = -1;
  click(&a, 250, 130);
  crtui_toggle_get_checked(a.ctx, sw, &checked);
  CHECK(checked == 1, "a left click toggles a switch");
  tap(&a, K_SPACE, 0);
  crtui_toggle_get_checked(a.ctx, sw, &checked);
  CHECK(checked == 0, "Space toggles it back");
  button(&a, 1, CRTGFX_POINTER_BUTTON_RIGHT, 250, 130);
  button(&a, 0, CRTGFX_POINTER_BUTTON_RIGHT, 250, 130);
  crtui_toggle_get_checked(a.ctx, sw, &checked);
  CHECK(checked == 0, "a right click does not toggle");

  /* Scrolling: the wheel over the list, with crtgfx's dy passed through. */
  int32_t offset = -1, content = -1;
  crtui_scroll_view_get_offset(a.ctx, list, &offset, &content);
  CHECK(offset == 0 && content == 192, "six 32 px rows make 192 px of content in a 48 px list");
  motion(&a, 60, 170);
  scroll(&a, -10.0);
  crtui_scroll_view_get_offset(a.ctx, list, &offset, NULL);
  CHECK(offset == 24, "a negative scroll moves the content up by one notch");
  scroll(&a, 10.0);
  crtui_scroll_view_get_offset(a.ctx, list, &offset, NULL);
  CHECK(offset == 0, "and a positive one moves it back");
  crtui_widget_focus(a.ctx, rows[5]);
  crtui_scroll_view_get_offset(a.ctx, list, &offset, NULL);
  CHECK(offset == 144, "keyboard focus scrolls the focused row into view");
  end(&a);
}

int main(void) {
  test_keyboard();
  test_pointer();
  test_resize();
  test_focus_loss_and_recovery();
  test_errors();
  test_v1_input();
  if (failures != 0) {
    fprintf(stderr, "crtui_input_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtui_input_test: ok keyboard=pass pointer=pass slider_drag=pass wheel=pass resize=pass "
         "focus_loss=pass recovery=pass errors=pass text_input=pass toggles=pass scroll=pass\n");
  return 0;
}
