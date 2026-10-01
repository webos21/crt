/* crtui Tranche 4 acceptance (docs/crtui_acceptance.md): the layout engine, the
 * style properties and the v1 widget behaviors, checked against the CRT-owned
 * model -- no window, display or LVGL, so it runs on every host. The pixels
 * these produce are checked by crtui_render_test. */

#include "crtui/ui.h"

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

typedef struct rect {
  int32_t x, y, w, h;
} rect;

typedef struct harness {
  crtui_context* ctx;
  crtui_window win;
  int value_changed;
  int32_t last_value;
  int text_changed;
  int32_t last_length;
  int activated;
} harness;

static int on_event(crtui_widget current, const crtui_event* e, void* user) {
  harness* h = (harness*)user;
  if (current != e->target && e->type != CRTUI_EVENT_RESIZED) return CRTUI_EVENT_IGNORED; /* count each event once */
  if (e->type == CRTUI_EVENT_VALUE_CHANGED) {
    ++h->value_changed;
    h->last_value = e->value;
  } else if (e->type == CRTUI_EVENT_TEXT_CHANGED) {
    ++h->text_changed;
    h->last_length = e->value;
  } else if (e->type == CRTUI_EVENT_ACTIVATE) {
    ++h->activated;
  }
  return CRTUI_EVENT_IGNORED;
}

static void begin(harness* h, int32_t width, int32_t height) {
  memset(h, 0, sizeof(*h));
  crtui_context_create(&h->ctx);
  crtui_window_create(h->ctx, &h->win);
  crtui_window_set_size(h->ctx, h->win, width, height, 1.0f);
  crtui_widget_set_callback(h->ctx, h->win, on_event, h);
}

static void finish(harness* h) {
  CHECK(crtui_context_destroy(h->ctx) == CRTUI_OK, "destroy");
}

static rect bounds(harness* h, crtui_widget w) {
  rect r = {-9999, -9999, -9999, -9999};
  crtui_widget_get_bounds(h->ctx, w, &r.x, &r.y, &r.w, &r.h);
  return r;
}

static int is(rect r, int32_t x, int32_t y, int32_t w, int32_t h) {
  return r.x == x && r.y == y && r.w == w && r.h == h;
}

static void send_key(harness* h, crtui_key key, uint32_t modifiers) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = CRTUI_INPUT_KEY_DOWN;
  in.window = h->win;
  in.key = key;
  in.modifiers = modifiers;
  crtui_context_send_input(h->ctx, &in);
}

static void send_text(harness* h, const char* utf8) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = CRTUI_INPUT_TEXT;
  in.window = h->win;
  strncpy(in.text, utf8, sizeof(in.text) - 1);
  crtui_context_send_input(h->ctx, &in);
}

static void send_pointer(harness* h, crtui_input_type type, int32_t x, int32_t y, int32_t wheel) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = type;
  in.window = h->win;
  in.x = x;
  in.y = y;
  in.wheel_delta = wheel;
  crtui_context_send_input(h->ctx, &in);
}

static void click(harness* h, int32_t x, int32_t y) {
  send_pointer(h, CRTUI_INPUT_POINTER_DOWN, x, y, 0);
  send_pointer(h, CRTUI_INPUT_POINTER_UP, x, y, 0);
}

/* ---- column ------------------------------------------------------------- */

static void test_column(void) {
  harness h;
  begin(&h, 300, 200);
  crtui_widget col, a, b, c;
  crtui_column_create(h.ctx, h.win, &col);
  crtui_widget_set_size(h.ctx, col, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_set_padding(h.ctx, col, 10, 10, 10, 10);
  crtui_container_set_gap(h.ctx, col, 5);
  crtui_container_create(h.ctx, col, &a);
  crtui_container_create(h.ctx, col, &b);
  crtui_container_create(h.ctx, col, &c);
  crtui_widget_set_size(h.ctx, a, 100, 30);
  crtui_widget_set_size(h.ctx, b, 0, 0);
  crtui_widget_set_grow(h.ctx, b, 1);
  crtui_widget_set_alignment(h.ctx, b, CRTUI_ALIGN_STRETCH, CRTUI_ALIGN_START);
  crtui_widget_set_size(h.ctx, c, 100, 30);
  crtui_widget_set_alignment(h.ctx, c, CRTUI_ALIGN_CENTER, CRTUI_ALIGN_START);

  CHECK(is(bounds(&h, col), 0, 0, 300, 200), "a FILL column fills the window (window has no padding)");
  CHECK(is(bounds(&h, a), 10, 10, 100, 30), "first child sits at the padded content origin");
  /* 180 (content) - 30 - 30 (fixed) - 10 (two gaps) = 110 for the growing child. */
  CHECK(is(bounds(&h, b), 10, 45, 280, 110), "the grow child takes the leftover height and stretches across");
  CHECK(is(bounds(&h, c), 100, 160, 100, 30), "a centered child is centered in the content width; the column ends flush");

  /* Resize re-lays out by itself: the grow child absorbs the extra height. */
  crtui_window_set_size(h.ctx, h.win, 400, 300, 1.0f);
  CHECK(is(bounds(&h, b), 10, 45, 380, 210), "a window resize re-lays out the tree");
  CHECK(is(bounds(&h, c), 150, 260, 100, 30), "and re-centers the centered child");

  /* Hidden children take no space. */
  crtui_widget_set_visible(h.ctx, a, 0);
  CHECK(is(bounds(&h, b), 10, 10, 380, 245), "a hidden child releases its space");
  crtui_widget_set_visible(h.ctx, a, 1);

  /* Margins surround a child. */
  crtui_widget_set_margin(h.ctx, a, 4, 6, 8, 2);
  rect ra = bounds(&h, a);
  CHECK(ra.x == 14 && ra.y == 16 && ra.w == 100 && ra.h == 30, "margin offsets the child inside its slot");
  CHECK(bounds(&h, b).y == 10 + 6 + 30 + 2 + 5, "and pushes its siblings by the full margin box");
  finish(&h);
}

/* ---- row and main-axis distribution ---------------------------------------- */

static void test_row(void) {
  harness h;
  begin(&h, 300, 200);
  crtui_widget row, x, y, z;
  crtui_row_create(h.ctx, h.win, &row);
  crtui_widget_set_size(h.ctx, row, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_set_gap(h.ctx, row, 10);
  crtui_container_create(h.ctx, row, &x);
  crtui_container_create(h.ctx, row, &y);
  crtui_container_create(h.ctx, row, &z);
  crtui_widget_set_size(h.ctx, x, 50, 20);
  crtui_widget_set_alignment(h.ctx, x, CRTUI_ALIGN_START, CRTUI_ALIGN_END);
  crtui_widget_set_size(h.ctx, y, 0, 20);
  crtui_widget_set_grow(h.ctx, y, 1);
  crtui_widget_set_size(h.ctx, z, 0, 20);
  crtui_widget_set_grow(h.ctx, z, 2);
  crtui_widget_set_alignment(h.ctx, z, CRTUI_ALIGN_START, CRTUI_ALIGN_CENTER);
  /* 300 - 50 - 20 (gaps) = 230, shared 1:2 -> 76 and (last grow child takes the remainder) 154. */
  CHECK(is(bounds(&h, x), 0, 180, 50, 20), "END aligns to the cross axis' far edge");
  CHECK(is(bounds(&h, y), 60, 0, 76, 20), "grow 1 of 3 parts");
  CHECK(is(bounds(&h, z), 146, 90, 154, 20), "grow 2 of 3 parts, centered across; the row fills exactly");

  /* FILL on the main axis behaves as grow 1. */
  crtui_widget_set_size(h.ctx, y, CRTUI_SIZE_FILL, 20);
  crtui_widget_set_grow(h.ctx, y, 0);
  crtui_widget_set_grow(h.ctx, z, 0);
  crtui_widget_set_size(h.ctx, z, 40, 20);
  CHECK(is(bounds(&h, y), 60, 0, 190, 20), "FILL on the main axis takes what is left (300 - 90 - 20)");
  finish(&h);

  /* main_align with fixed children. */
  begin(&h, 300, 200);
  crtui_widget r2, k[3];
  crtui_row_create(h.ctx, h.win, &r2);
  crtui_widget_set_size(h.ctx, r2, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  for (int i = 0; i < 3; ++i) {
    crtui_container_create(h.ctx, r2, &k[i]);
    crtui_widget_set_size(h.ctx, k[i], 50, 10);
  }
  static const struct {
    crtui_main_align align;
    int32_t xs[3];
  } cases[] = {{CRTUI_MAIN_START, {0, 50, 100}},
               {CRTUI_MAIN_CENTER, {75, 125, 175}},
               {CRTUI_MAIN_END, {150, 200, 250}},
               {CRTUI_MAIN_SPACE_BETWEEN, {0, 125, 250}}};
  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
    crtui_container_set_main_align(h.ctx, r2, cases[c].align);
    int ok = 1;
    for (int i = 0; i < 3; ++i) ok = ok && bounds(&h, k[i]).x == cases[c].xs[i];
    CHECK(ok, "main_align distributes the leftover main-axis space");
  }
  finish(&h);
}

/* ---- stack and free containers ---------------------------------------------- */

static void test_stack_and_free(void) {
  harness h;
  begin(&h, 200, 100);
  crtui_widget stack, fill, center, corner;
  crtui_stack_create(h.ctx, h.win, &stack);
  crtui_widget_set_size(h.ctx, stack, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_set_padding(h.ctx, stack, 10, 10, 10, 10);
  crtui_container_create(h.ctx, stack, &fill);
  crtui_widget_set_size(h.ctx, fill, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_create(h.ctx, stack, &center);
  crtui_widget_set_size(h.ctx, center, 40, 20);
  crtui_widget_set_alignment(h.ctx, center, CRTUI_ALIGN_CENTER, CRTUI_ALIGN_CENTER);
  crtui_container_create(h.ctx, stack, &corner);
  crtui_widget_set_size(h.ctx, corner, 30, 10);
  crtui_widget_set_alignment(h.ctx, corner, CRTUI_ALIGN_END, CRTUI_ALIGN_END);
  CHECK(is(bounds(&h, fill), 10, 10, 180, 80), "a FILL child of a Stack fills the padded content box");
  CHECK(is(bounds(&h, center), 80, 40, 40, 20), "a centered child overlays the middle");
  CHECK(is(bounds(&h, corner), 160, 80, 30, 10), "an END/END child overlays the far corner");
  finish(&h);

  /* A free container: manual bounds win, FILL snaps an axis to the padded box. */
  begin(&h, 200, 100);
  crtui_widget free_c, manual, snapped;
  crtui_container_set_padding(h.ctx, h.win, 5, 5, 5, 5);
  crtui_container_create(h.ctx, h.win, &free_c);
  crtui_widget_set_bounds(h.ctx, free_c, 20, 30, 100, 50);
  crtui_container_create(h.ctx, free_c, &manual);
  crtui_widget_set_bounds(h.ctx, manual, 7, 9, 11, 13);
  crtui_container_create(h.ctx, h.win, &snapped);
  crtui_widget_set_bounds(h.ctx, snapped, 99, 99, CRTUI_SIZE_FILL, 20);
  CHECK(is(bounds(&h, free_c), 20, 30, 100, 50), "a free container's bounds are the application's");
  CHECK(is(bounds(&h, manual), 7, 9, 11, 13), "so are its children's");
  CHECK(is(bounds(&h, snapped), 5, 99, 190, 20), "FILL snaps only the filled axis to the window's padded box");
  finish(&h);
}

/* ---- scroll view and list ------------------------------------------------------- */

static void test_scroll(void) {
  harness h;
  begin(&h, 200, 200);
  crtui_widget view, items[10];
  crtui_scroll_view_create(h.ctx, h.win, &view);
  crtui_widget_set_bounds(h.ctx, view, 0, 0, 100, 100);
  for (int i = 0; i < 10; ++i) {
    crtui_button_create(h.ctx, view, "item", &items[i]);
    crtui_widget_set_size(h.ctx, items[i], 100, 30);
  }
  int32_t offset = -1, content = -1;
  crtui_scroll_view_get_offset(h.ctx, view, &offset, &content);
  CHECK(offset == 0 && content == 300, "content height is the sum of the children");
  CHECK(is(bounds(&h, items[3]), 0, 90, 100, 30), "children stack vertically");

  send_pointer(&h, CRTUI_INPUT_WHEEL, 10, 10, -1);
  send_pointer(&h, CRTUI_INPUT_WHEEL, 10, 10, -1);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 48, "wheel down scrolls the content up, 24 px per notch");
  CHECK(bounds(&h, items[3]).y == 90 - 48, "children move with the offset");
  send_pointer(&h, CRTUI_INPUT_WHEEL, 10, 10, 1);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 24, "wheel up scrolls back");
  for (int i = 0; i < 3; ++i) send_pointer(&h, CRTUI_INPUT_WHEEL, 10, 10, 1);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 0, "the offset is clamped at the top");
  crtui_scroll_view_set_offset(h.ctx, view, 1000);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 200, "and at the bottom (content 300 - view 100)");

  /* Focusing a child scrolls it into view. */
  crtui_widget_focus(h.ctx, items[0]);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 0, "focusing the first item scrolls it into view");
  crtui_widget_focus(h.ctx, items[9]);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  CHECK(offset == 200 && bounds(&h, items[9]).y == 70, "focusing the last item scrolls it into view");
  crtui_widget_focus(h.ctx, items[5]);
  crtui_scroll_view_get_offset(h.ctx, view, &offset, NULL);
  int32_t y5 = bounds(&h, items[5]).y;
  CHECK(y5 >= 0 && y5 + 30 <= 100, "focusing a middle item keeps it fully visible");

  /* The scroll view clips: an item scrolled out of view cannot be hit. */
  crtui_scroll_view_set_offset(h.ctx, view, 0);
  crtui_widget hit = CRTUI_INVALID_WIDGET;
  crtui_context_hit_test(h.ctx, h.win, 50, 150, &hit);
  CHECK(hit == h.win, "below the scroll view is the window, not a clipped item");
  crtui_context_hit_test(h.ctx, h.win, 50, 95, &hit);
  CHECK(hit == items[3], "inside the view the item under the point is hit");
  crtui_scroll_view_set_offset(h.ctx, view, 30);
  crtui_context_hit_test(h.ctx, h.win, 50, 95, &hit);
  CHECK(hit == items[4], "after scrolling, hit-testing follows the moved children");
  CHECK(crtui_scroll_view_set_offset(h.ctx, items[0], 1) == CRTUI_ERROR_INVALID_ARGUMENT, "only scroll views scroll");
  finish(&h);

  /* List: full-width button rows. */
  begin(&h, 200, 200);
  crtui_widget list, row1, row2;
  crtui_list_create(h.ctx, h.win, &list);
  crtui_widget_set_bounds(h.ctx, list, 10, 10, 180, 100);
  crtui_list_add_item(h.ctx, list, "first", &row1);
  crtui_list_add_item(h.ctx, list, "second", &row2);
  CHECK(is(bounds(&h, row1), 10, 10, 180, 32) || is(bounds(&h, row1), 0, 0, 180, 32),
        "a list item is a full-width 32 px row");
  CHECK(bounds(&h, row2).y == bounds(&h, row1).y + 32, "rows stack");
  crtui_widget_kind kind;
  crtui_widget_get_kind(h.ctx, row1, &kind);
  CHECK(kind == CRTUI_WIDGET_BUTTON, "an item is a button (ACTIVATE = selected)");
  crtui_widget_set_callback(h.ctx, row2, on_event, &h);
  click(&h, 20, 10 + 32 + 10);
  CHECK(h.activated == 1, "clicking a list row activates it");
  row1 = 1234;
  CHECK(crtui_list_add_item(h.ctx, h.win, "x", &row1) == CRTUI_ERROR_INVALID_ARGUMENT &&
            row1 == CRTUI_INVALID_WIDGET,
        "only lists take items, and a failed create clears its output");
  finish(&h);
}

/* ---- style ------------------------------------------------------------------------ */

static void test_style(void) {
  harness h;
  begin(&h, 100, 100);
  crtui_widget b;
  crtui_button_create(h.ctx, h.win, "b", &b);
  crtui_style s, got;
  memset(&s, 0, sizeof(s));
  crtui_widget_get_style(h.ctx, b, &got);
  CHECK(got.mask == 0, "a new widget has no overrides (the CRT default look)");
  s.mask = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_RADIUS;
  s.background_rgb = 0x123456;
  s.radius = 9;
  CHECK(crtui_widget_set_style(h.ctx, b, &s) == CRTUI_OK, "set style");
  memset(&s, 0, sizeof(s));
  s.mask = CRTUI_STYLE_OPACITY | CRTUI_STYLE_FONT | CRTUI_STYLE_TEXT_ALIGN | CRTUI_STYLE_BORDER_WIDTH |
           CRTUI_STYLE_BORDER_COLOR | CRTUI_STYLE_FOREGROUND;
  s.opacity = 128;
  s.font = CRTUI_FONT_LARGE;
  s.text_align = CRTUI_TEXT_ALIGN_CENTER;
  s.border_width = 3;
  s.border_rgb = 0xABCDEF;
  s.foreground_rgb = 0xFF00FF;
  crtui_widget_set_style(h.ctx, b, &s);
  crtui_widget_get_style(h.ctx, b, &got);
  CHECK(got.mask == (CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_RADIUS | CRTUI_STYLE_OPACITY | CRTUI_STYLE_FONT |
                     CRTUI_STYLE_TEXT_ALIGN | CRTUI_STYLE_BORDER_WIDTH | CRTUI_STYLE_BORDER_COLOR |
                     CRTUI_STYLE_FOREGROUND),
        "masks accumulate: only the fields named are changed");
  CHECK(got.background_rgb == 0x123456 && got.radius == 9 && got.opacity == 128 && got.font == CRTUI_FONT_LARGE &&
            got.text_align == CRTUI_TEXT_ALIGN_CENTER && got.border_width == 3 && got.border_rgb == 0xABCDEF &&
            got.foreground_rgb == 0xFF00FF,
        "every property reads back");
  memset(&s, 0, sizeof(s));
  s.mask = CRTUI_STYLE_BACKGROUND;
  s.background_rgb = 0xFF654321; /* the alpha byte is not part of the color */
  crtui_widget_set_style(h.ctx, b, &s);
  crtui_widget_get_style(h.ctx, b, &got);
  CHECK(got.background_rgb == 0x654321 && got.radius == 9, "colors are 0xRRGGBB; other fields are untouched");

  memset(&s, 0, sizeof(s));
  s.mask = 1u << 20;
  CHECK(crtui_widget_set_style(h.ctx, b, &s) == CRTUI_ERROR_INVALID_ARGUMENT, "an unknown property bit is refused");
  s.mask = CRTUI_STYLE_RADIUS;
  s.radius = -1;
  CHECK(crtui_widget_set_style(h.ctx, b, &s) == CRTUI_ERROR_INVALID_ARGUMENT, "a negative radius is refused");
  s.mask = CRTUI_STYLE_BORDER_WIDTH;
  s.border_width = -1;
  CHECK(crtui_widget_set_style(h.ctx, b, &s) == CRTUI_ERROR_INVALID_ARGUMENT, "a negative border is refused");
  s.mask = CRTUI_STYLE_FONT;
  s.font = (crtui_font)9;
  CHECK(crtui_widget_set_style(h.ctx, b, &s) == CRTUI_ERROR_INVALID_ARGUMENT, "an unknown font is refused");
  CHECK(crtui_widget_set_style(h.ctx, b, NULL) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL style");
  crtui_widget_get_style(h.ctx, b, &got);
  CHECK(got.font == CRTUI_FONT_LARGE, "a refused call changes nothing");
  crtui_widget_clear_style(h.ctx, b);
  crtui_widget_get_style(h.ctx, b, &got);
  CHECK(got.mask == 0, "clear_style restores the default look");
  finish(&h);

  /* Layout property validation. */
  begin(&h, 100, 100);
  crtui_widget c;
  crtui_container_create(h.ctx, h.win, &c);
  CHECK(crtui_widget_set_margin(h.ctx, c, -1, 0, 0, 0) == CRTUI_ERROR_INVALID_ARGUMENT, "negative margin");
  CHECK(crtui_container_set_padding(h.ctx, c, 0, -1, 0, 0) == CRTUI_ERROR_INVALID_ARGUMENT, "negative padding");
  CHECK(crtui_container_set_gap(h.ctx, c, -1) == CRTUI_ERROR_INVALID_ARGUMENT, "negative gap");
  CHECK(crtui_widget_set_grow(h.ctx, c, -1) == CRTUI_ERROR_INVALID_ARGUMENT, "negative grow");
  CHECK(crtui_widget_set_size(h.ctx, c, -2, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "only FILL may be negative");
  CHECK(crtui_widget_set_size(h.ctx, h.win, 10, 10) == CRTUI_ERROR_INVALID_ARGUMENT, "a window is sized by set_size()");
  crtui_widget lab;
  crtui_text_create(h.ctx, c, "t", &lab);
  CHECK(crtui_container_set_padding(h.ctx, lab, 1, 1, 1, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "padding needs a container");
  CHECK(crtui_container_set_main_align(h.ctx, c, (crtui_main_align)7) == CRTUI_ERROR_INVALID_ARGUMENT, "bad main_align");
  CHECK(crtui_widget_set_alignment(h.ctx, lab, (crtui_align)9, CRTUI_ALIGN_START) == CRTUI_ERROR_INVALID_ARGUMENT,
        "bad alignment");
  finish(&h);
}

/* ---- toggles ---------------------------------------------------------------------------- */

static void test_toggles(void) {
  harness h;
  begin(&h, 200, 100);
  crtui_widget sw, cb, btn;
  crtui_switch_create(h.ctx, h.win, &sw);
  crtui_widget_set_bounds(h.ctx, sw, 10, 10, 50, 24);
  crtui_checkbox_create(h.ctx, h.win, "Remember me", &cb);
  crtui_widget_set_bounds(h.ctx, cb, 10, 50, 150, 24);
  crtui_button_create(h.ctx, h.win, "x", &btn);
  crtui_widget_set_callback(h.ctx, sw, on_event, &h);
  crtui_widget_set_callback(h.ctx, cb, on_event, &h);

  int checked = -1;
  CHECK(crtui_toggle_get_checked(h.ctx, sw, &checked) == CRTUI_OK && checked == 0, "a switch starts off");
  click(&h, 20, 20);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 1 && h.value_changed == 1 && h.last_value == 1, "a click turns it on and emits VALUE_CHANGED(1)");
  click(&h, 20, 20);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 0 && h.value_changed == 2 && h.last_value == 0, "and off again");
  crtui_widget focused_w = CRTUI_INVALID_WIDGET;
  crtui_context_get_focus(h.ctx, &focused_w);
  CHECK(focused_w == sw, "toggles are focusable and a click focuses them");
  send_key(&h, CRTUI_KEY_SPACE, 0);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 1 && h.value_changed == 3, "Space toggles the focused switch");
  send_key(&h, CRTUI_KEY_ENTER, 0);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 0 && h.value_changed == 4, "Enter toggles it too");

  h.value_changed = 0;
  crtui_toggle_set_checked(h.ctx, sw, 1);
  CHECK(h.value_changed == 0, "programmatic changes emit no event");
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 1, "but they do change the state");

  /* Press on the switch, release elsewhere: no toggle. */
  send_pointer(&h, CRTUI_INPUT_POINTER_DOWN, 20, 20, 0);
  send_pointer(&h, CRTUI_INPUT_POINTER_UP, 20, 60, 0);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 1 && h.value_changed == 0, "press and release on different widgets never toggles");
  crtui_widget_set_enabled(h.ctx, sw, 0);
  click(&h, 20, 20);
  crtui_toggle_get_checked(h.ctx, sw, &checked);
  CHECK(checked == 1, "a disabled switch ignores clicks");

  /* Checkbox. */
  click(&h, 20, 60);
  crtui_toggle_get_checked(h.ctx, cb, &checked);
  CHECK(checked == 1 && h.value_changed == 1, "a click toggles a checkbox");
  char text[32];
  size_t length = 0;
  CHECK(crtui_widget_get_text(h.ctx, cb, text, sizeof(text), &length) == CRTUI_OK && strcmp(text, "Remember me") == 0,
        "a checkbox carries its label");
  crtui_widget_set_text(h.ctx, cb, "Stay signed in");
  crtui_widget_get_text(h.ctx, cb, text, sizeof(text), &length);
  CHECK(strcmp(text, "Stay signed in") == 0, "and the label can change");
  CHECK(crtui_toggle_set_checked(h.ctx, btn, 1) == CRTUI_ERROR_INVALID_ARGUMENT, "only switches and checkboxes toggle");
  CHECK(crtui_toggle_get_checked(h.ctx, btn, &checked) == CRTUI_ERROR_INVALID_ARGUMENT, "and read back");
  finish(&h);
}

/* ---- text input ------------------------------------------------------------------------- */

static void text_is(harness* h, crtui_widget w, const char* expected, const char* message) {
  char buffer[2048];
  size_t length = 0;
  crtui_widget_get_text(h->ctx, w, buffer, sizeof(buffer), &length);
  CHECK(strcmp(buffer, expected) == 0, message);
}

static size_t caret_of(harness* h, crtui_widget w) {
  size_t c = 999999;
  crtui_text_input_get_caret(h->ctx, w, &c);
  return c;
}

static void test_text_input(void) {
  harness h;
  begin(&h, 200, 100);
  crtui_widget in, other;
  crtui_text_input_create(h.ctx, h.win, "ab", &in);
  crtui_widget_set_bounds(h.ctx, in, 10, 10, 150, 26);
  crtui_button_create(h.ctx, h.win, "x", &other);
  crtui_widget_set_bounds(h.ctx, other, 10, 60, 50, 20);
  crtui_widget_set_callback(h.ctx, in, on_event, &h);
  CHECK(caret_of(&h, in) == 2, "the caret starts at the end of the initial text");

  send_text(&h, "z");
  text_is(&h, in, "ab", "text goes only to a focused TextInput");
  CHECK(crtui_widget_focus(h.ctx, in) == CRTUI_OK, "a text input is focusable");
  send_text(&h, "c");
  text_is(&h, in, "abc", "typing inserts at the caret");
  CHECK(h.text_changed == 1 && h.last_length == 3 && caret_of(&h, in) == 3, "TEXT_CHANGED reports the length");
  send_key(&h, CRTUI_KEY_LEFT, 0);
  send_text(&h, "X");
  text_is(&h, in, "abXc", "Left moves the caret; typing inserts there");
  CHECK(caret_of(&h, in) == 3, "the caret follows the insertion");
  send_key(&h, CRTUI_KEY_BACKSPACE, 0);
  text_is(&h, in, "abc", "Backspace deletes before the caret");
  send_key(&h, CRTUI_KEY_DELETE, 0);
  text_is(&h, in, "ab", "Delete deletes after the caret");
  send_key(&h, CRTUI_KEY_HOME, 0);
  CHECK(caret_of(&h, in) == 0, "Home");
  send_key(&h, CRTUI_KEY_BACKSPACE, 0);
  text_is(&h, in, "ab", "Backspace at the start does nothing");
  send_key(&h, CRTUI_KEY_END, 0);
  CHECK(caret_of(&h, in) == 2, "End");
  send_key(&h, CRTUI_KEY_DELETE, 0);
  text_is(&h, in, "ab", "Delete at the end does nothing");
  send_key(&h, CRTUI_KEY_RIGHT, 0);
  CHECK(caret_of(&h, in) == 2, "Right at the end stays put");

  /* UTF-8: one character is one caret step, whatever its byte length. */
  send_text(&h, "\xC3\xA9"); /* e acute, 2 bytes */
  text_is(&h, in, "ab\xC3\xA9", "a multi-byte character is inserted whole");
  CHECK(caret_of(&h, in) == 4, "the caret advances by its byte length");
  send_key(&h, CRTUI_KEY_LEFT, 0);
  CHECK(caret_of(&h, in) == 2, "Left steps over the whole character");
  send_key(&h, CRTUI_KEY_RIGHT, 0);
  send_key(&h, CRTUI_KEY_BACKSPACE, 0);
  text_is(&h, in, "ab", "Backspace removes the whole character, never half of it");
  crtui_text_input_set_caret(h.ctx, in, 1);
  send_text(&h, "\xE2\x82\xAC"); /* euro sign, 3 bytes */
  crtui_text_input_set_caret(h.ctx, in, 2);
  CHECK(caret_of(&h, in) == 1, "set_caret never lands inside a multi-byte character");
  crtui_text_input_set_caret(h.ctx, in, 999);
  CHECK(caret_of(&h, in) == 5, "and clamps to the end");

  /* Not text. */
  int before = h.text_changed;
  send_text(&h, "\n");
  send_text(&h, "\x01");
  send_text(&h, "\x7f");
  CHECK(h.text_changed == before, "control characters are not inserted");
  crtui_widget_set_text(h.ctx, in, "");
  CHECK(caret_of(&h, in) == 0, "set_text moves the caret to the end of the new text");

  /* Submit and navigation. */
  send_key(&h, CRTUI_KEY_ENTER, 0);
  CHECK(h.activated == 1, "Enter submits (ACTIVATE)");
  send_key(&h, CRTUI_KEY_DOWN, 0);
  crtui_widget f = CRTUI_INVALID_WIDGET;
  crtui_context_get_focus(h.ctx, &f);
  CHECK(f == other, "Down still navigates away from a text input");
  send_key(&h, CRTUI_KEY_TAB, CRTUI_MOD_SHIFT);
  crtui_context_get_focus(h.ctx, &f);
  CHECK(f == in, "Shift+Tab comes back");

  /* Limits. */
  char big[1100];
  memset(big, 'a', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';
  CHECK(crtui_widget_set_text(h.ctx, in, big) == CRTUI_ERROR_INVALID_ARGUMENT, "text over 1024 bytes is refused");
  crtui_widget too_big = 1234;
  CHECK(crtui_text_input_create(h.ctx, h.win, big, &too_big) == CRTUI_ERROR_INVALID_ARGUMENT &&
            too_big == CRTUI_INVALID_WIDGET,
        "an oversized initial value is refused and clears the output");
  big[1024] = '\0';
  CHECK(crtui_widget_set_text(h.ctx, in, big) == CRTUI_OK, "exactly 1024 bytes is accepted");
  before = h.text_changed;
  send_text(&h, "b");
  CHECK(h.text_changed == before, "typing past the limit is refused");

  CHECK(crtui_text_input_set_placeholder(h.ctx, in, "Search...") == CRTUI_OK, "placeholder");
  CHECK(crtui_text_input_set_placeholder(h.ctx, other, "x") == CRTUI_ERROR_INVALID_ARGUMENT, "only text inputs have one");
  CHECK(crtui_text_input_set_caret(h.ctx, other, 0) == CRTUI_ERROR_INVALID_ARGUMENT, "or a caret");
  finish(&h);
}

/* ---- image --------------------------------------------------------------------------- */

static void test_image(void) {
  harness h;
  begin(&h, 100, 100);
  crtui_widget img;
  crtui_image_create(h.ctx, h.win, &img);
  uint8_t px[2 * 2 * 4];
  memset(px, 0x80, sizeof(px));
  CHECK(crtui_image_set_pixels(h.ctx, img, px, 2, 2, 8) == CRTUI_OK, "set pixels");
  CHECK(is(bounds(&h, img), 0, 0, 2, 2), "an unsized image takes its natural size");
  crtui_widget_set_size(h.ctx, img, 40, 40);
  CHECK(is(bounds(&h, img), 0, 0, 40, 40), "but can be sized explicitly (the pixels are stretched)");
  CHECK(crtui_image_set_pixels(h.ctx, img, NULL, 2, 2, 8) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL pixels");
  CHECK(crtui_image_set_pixels(h.ctx, img, px, 0, 2, 8) == CRTUI_ERROR_INVALID_ARGUMENT, "zero width");
  CHECK(crtui_image_set_pixels(h.ctx, img, px, 2, 2, 4) == CRTUI_ERROR_INVALID_ARGUMENT, "stride shorter than a row");
  CHECK(crtui_image_set_pixels(h.ctx, img, px, 5000, 2, 20000) == CRTUI_ERROR_INVALID_ARGUMENT, "absurd width");
  crtui_widget not_image;
  crtui_container_create(h.ctx, h.win, &not_image);
  CHECK(crtui_image_set_pixels(h.ctx, not_image, px, 2, 2, 8) == CRTUI_ERROR_INVALID_ARGUMENT, "only images take pixels");
  finish(&h);
}

int main(void) {
  test_column();
  test_row();
  test_stack_and_free();
  test_scroll();
  test_style();
  test_toggles();
  test_text_input();
  test_image();
  if (failures != 0) {
    fprintf(stderr, "crtui_layout_test: %d failure(s)\n", failures);
    return 1;
  }
  printf(
      "crtui_layout_test: ok column=pass row=pass main_align=pass stack=pass free=pass scroll=pass list=pass "
      "style=pass toggles=pass text_input=pass image=pass\n");
  return 0;
}
