/* crtui Tranche 1 acceptance (docs/acceptance/crtui_acceptance.md): LVGL software draw
 * buffer -> CRT display adapter -> BGRA8888 pixels. Headless: renders a
 * Window > Container ("column") > Label/Button/Slider/Progress scene into
 * memory and checks real pixels (never just "it ran"), then cycles
 * create/render/destroy and audits native handles/threads for leaks. The
 * crtgfx present step is exercised separately by crtui_window_demo. */

#include "crtui/ui.h"

#include <dirent.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(CRT_TARGET_OS_WINDOWS)
__declspec(dllimport) void* GetCurrentProcess(void);
__declspec(dllimport) int GetProcessHandleCount(void* process, unsigned int* count);
#endif

static int failures = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

#define WIDTH 320
#define HEIGHT 200

#define COLOR_BG 0xF0F0F0u
#define COLOR_BUTTON 0x2D6CDFu
#define COLOR_DISABLED 0xB0B0B0u
#define COLOR_TRACK 0xC8C8C8u
#define COLOR_SLIDER_FILL 0x2D6CDFu
#define COLOR_PROGRESS_FILL 0x2ECC71u
#define COLOR_PRESSED 0x1E4FA8u
#define COLOR_FOCUS_RING 0xFF9800u

typedef struct scene {
  crtui_context* ctx;
  crtui_window win;
  crtui_widget column, label, button, slider, progress;
} scene;

/* Absolute layout: the container sits at (10,10) so children are offset by it.
 *   label    abs (10,10)  200x20
 *   button   abs (10,40)  120x32
 *   slider   abs (10,90)  200x12   value 30 of 0..100
 *   progress abs (10,120) 200x14   value 50 of 0..100 */
static int build_scene(scene* s) {
  memset(s, 0, sizeof(*s));
  if (crtui_context_create(&s->ctx) != CRTUI_OK) return -1;
  if (crtui_window_create(s->ctx, &s->win) != CRTUI_OK) return -1;
  crtui_window_set_size(s->ctx, s->win, WIDTH, HEIGHT, 1.0f);
  if (crtui_container_create(s->ctx, s->win, &s->column) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->column, 10, 10, 300, 180);
  if (crtui_text_create(s->ctx, s->column, "Hello crtui", &s->label) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->label, 0, 0, 200, 20);
  if (crtui_button_create(s->ctx, s->column, "OK", &s->button) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->button, 0, 30, 120, 32);
  if (crtui_slider_create(s->ctx, s->column, 0, 100, &s->slider) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->slider, 0, 80, 200, 12);
  crtui_slider_set_value(s->ctx, s->slider, 30);
  if (crtui_progress_create(s->ctx, s->column, &s->progress) != CRTUI_OK) return -1;
  crtui_widget_set_bounds(s->ctx, s->progress, 0, 110, 200, 14);
  crtui_progress_set_value(s->ctx, s->progress, 50);
  return 0;
}

static uint32_t rgb_at(const uint8_t* pixels, size_t stride, int x, int y) {
  const uint8_t* p = pixels + (size_t)y * stride + (size_t)x * 4u;
  return ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

static int alpha_at(const uint8_t* pixels, size_t stride, int x, int y) {
  return pixels[(size_t)y * stride + (size_t)x * 4u + 3u];
}

static int channel_close(uint32_t a, uint32_t b, int tolerance) {
  for (int shift = 0; shift <= 16; shift += 8) {
    int da = (int)((a >> shift) & 0xff) - (int)((b >> shift) & 0xff);
    if (da < 0) da = -da;
    if (da > tolerance) return 0;
  }
  return 1;
}

static int count_not_bg(const uint8_t* pixels, size_t stride, int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (!channel_close(rgb_at(pixels, stride, x, y), COLOR_BG, 8)) ++n;
    }
  }
  return n;
}

static int count_light(const uint8_t* pixels, size_t stride, int x0, int y0, int x1, int y1) {
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      uint32_t c = rgb_at(pixels, stride, x, y);
      if (((c >> 16) & 0xff) > 200 && ((c >> 8) & 0xff) > 200 && (c & 0xff) > 200) ++n;
    }
  }
  return n;
}

/* ---- resource audit (same signal as crtmedia_http_lifecycle_test) --------- */

#if !defined(CRT_TARGET_OS_WINDOWS)
static int count_directory(const char* path) {
  DIR* dir = opendir(path);
  if (dir == NULL) return -1;
  int count = 0;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] != '.') ++count;
  }
  closedir(dir);
  return count;
}

#endif

static void sample_resources(int* handles, int* threads) {
  *handles = -1;
  *threads = -1;
#if defined(CRT_TARGET_OS_WINDOWS)
  unsigned int count = 0;
  if (GetProcessHandleCount(GetCurrentProcess(), &count) != 0) *handles = (int)count;
#else
  *handles = count_directory("/proc/self/fd");
  if (*handles < 0) *handles = count_directory("/dev/fd");
  *threads = count_directory("/proc/self/task");
#endif
}

static void* render_from_other_thread(void* argument) {
  scene* s = (scene*)argument;
  uint8_t dummy[16];
  crtui_result r = crtui_window_render(s->ctx, s->win, dummy, 16, WIDTH, HEIGHT);
  return (void*)(intptr_t)r;
}

/* Optional visual aid: CRTUI_RENDER_DUMP=<path> writes the first rendered frame
 * as a 32-bit BMP so a human can look at it. Never part of the pass criteria. */
static void dump_bmp(const char* path, const uint8_t* pixels, size_t stride) {
  FILE* file = fopen(path, "wb");
  if (file == NULL) return;
  uint32_t image_size = (uint32_t)(WIDTH * 4 * HEIGHT);
  uint32_t file_size = 54 + image_size;
  uint8_t header[54];
  memset(header, 0, sizeof(header));
  header[0] = 'B';
  header[1] = 'M';
  memcpy(header + 2, &file_size, 4);
  uint32_t offset = 54, dib = 40;
  int32_t w = WIDTH, h = -HEIGHT; /* negative height: top-down rows */
  uint16_t planes = 1, bits = 32;
  memcpy(header + 10, &offset, 4);
  memcpy(header + 14, &dib, 4);
  memcpy(header + 18, &w, 4);
  memcpy(header + 22, &h, 4);
  memcpy(header + 26, &planes, 2);
  memcpy(header + 28, &bits, 2);
  memcpy(header + 34, &image_size, 4);
  fwrite(header, 1, sizeof(header), file);
  for (int y = 0; y < HEIGHT; ++y) fwrite(pixels + (size_t)y * stride, 1, (size_t)WIDTH * 4u, file);
  fclose(file);
}

/* ---- tests -------------------------------------------------------------- */

static void test_pixels(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  size_t stride = (size_t)WIDTH * 4u;
  uint8_t* pixels = (uint8_t*)malloc(stride * HEIGHT);
  memset(pixels, 0xAB, stride * HEIGHT);
  crtui_result r = crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  if (r == CRTUI_ERROR_UNSUPPORTED) {
    fprintf(stderr, "crtui_render_test: this build has no renderer (CRTUI_ENABLE_LVGL is OFF)\n");
    ++failures;
    free(pixels);
    crtui_context_destroy(s.ctx);
    return;
  }
  CHECK(r == CRTUI_OK, "render succeeds");
  if (getenv("CRTUI_RENDER_DUMP") != NULL) dump_bmp(getenv("CRTUI_RENDER_DUMP"), pixels, stride);

  CHECK(rgb_at(pixels, stride, 3, 3) == COLOR_BG && alpha_at(pixels, stride, 3, 3) == 0xFF,
        "window background is the fixed CRT color, fully opaque");
  CHECK(rgb_at(pixels, stride, WIDTH - 2, HEIGHT - 2) == COLOR_BG && alpha_at(pixels, stride, WIDTH - 2, HEIGHT - 2) == 0xFF,
        "the whole buffer was overwritten (no sentinel left in the last pixel)");
  int label_ink = count_not_bg(pixels, stride, 10, 10, 210, 30);
  CHECK(label_ink >= 25, "the label rendered real glyph pixels");
  CHECK(rgb_at(pixels, stride, 200, 15) == COLOR_BG, "the label draws no background box");

  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BUTTON, "button fill color");
  CHECK(rgb_at(pixels, stride, 124, 66) == COLOR_BUTTON, "button fill color near the far corner");
  CHECK(count_light(pixels, stride, 12, 42, 128, 70) >= 8, "the button label is drawn in white on the fill");
  CHECK(rgb_at(pixels, stride, 140, 56) == COLOR_BG, "just outside the button is background");

  CHECK(rgb_at(pixels, stride, 25, 96) == COLOR_SLIDER_FILL, "slider filled part (left of the knob)");
  CHECK(rgb_at(pixels, stride, 195, 96) == COLOR_TRACK, "slider track (right of the knob)");
  CHECK(rgb_at(pixels, stride, 100, 96) == COLOR_TRACK, "slider track beyond 30%");

  CHECK(rgb_at(pixels, stride, 30, 127) == COLOR_PROGRESS_FILL, "progress fill at 25%");
  CHECK(rgb_at(pixels, stride, 100, 127) == COLOR_PROGRESS_FILL, "progress fill just under 50%");
  CHECK(rgb_at(pixels, stride, 120, 127) == COLOR_TRACK, "progress track past 50%");
  CHECK(rgb_at(pixels, stride, 190, 127) == COLOR_TRACK, "progress track at 95%");

  /* Changing the model re-renders it. */
  crtui_progress_set_value(s.ctx, s.progress, 100);
  CHECK(crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT) == CRTUI_OK, "re-render");
  CHECK(rgb_at(pixels, stride, 190, 127) == COLOR_PROGRESS_FILL, "progress at 100% fills the track");
  crtui_widget_set_enabled(s.ctx, s.button, 0);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_DISABLED, "a disabled button is drawn gray");
  crtui_widget_set_enabled(s.ctx, s.button, 1);
  crtui_widget_set_visible(s.ctx, s.column, 0);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BG && rgb_at(pixels, stride, 25, 96) == COLOR_BG,
        "hiding the container hides every child");
  crtui_widget_set_visible(s.ctx, s.column, 1);

  /* Padded stride: identical image, padding bytes untouched. */
  size_t padded = stride + 32u;
  uint8_t* wide = (uint8_t*)malloc(padded * HEIGHT);
  memset(wide, 0x5C, padded * HEIGHT);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(crtui_window_render(s.ctx, s.win, wide, padded, WIDTH, HEIGHT) == CRTUI_OK, "render into a padded stride");
  int same = 1;
  int padding_untouched = 1;
  for (int y = 0; y < HEIGHT; ++y) {
    if (memcmp(pixels + (size_t)y * stride, wide + (size_t)y * padded, stride) != 0) same = 0;
    for (size_t i = stride; i < padded; ++i) {
      if (wide[(size_t)y * padded + i] != 0x5C) padding_untouched = 0;
    }
  }
  CHECK(same, "a padded stride yields the same image");
  CHECK(padding_untouched, "row padding is never written");
  free(wide);

  /* Resize: the renderer follows the window. */
  crtui_window_set_size(s.ctx, s.win, 200, 120, 1.0f);
  uint8_t* small = (uint8_t*)malloc((size_t)200 * 4u * 120);
  CHECK(crtui_window_render(s.ctx, s.win, small, 200 * 4u, 200, 120) == CRTUI_OK, "render after a resize");
  CHECK(rgb_at(small, 200 * 4u, 190, 110) == COLOR_BG, "resized window background");
  CHECK(rgb_at(small, 200 * 4u, 16, 46) == COLOR_BUTTON, "children keep their bounds after the resize");
  free(small);

  /* Argument validation. */
  crtui_window_set_size(s.ctx, s.win, WIDTH, HEIGHT, 1.0f);
  CHECK(crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH - 1, HEIGHT) == CRTUI_ERROR_INVALID_ARGUMENT,
        "size must match the window");
  CHECK(crtui_window_render(s.ctx, s.win, pixels, stride - 4, WIDTH, HEIGHT) == CRTUI_ERROR_INVALID_ARGUMENT,
        "stride must cover a row");
  CHECK(crtui_window_render(s.ctx, s.win, NULL, stride, WIDTH, HEIGHT) == CRTUI_ERROR_INVALID_ARGUMENT, "NULL pixels");
  CHECK(crtui_window_render(s.ctx, s.column, pixels, stride, WIDTH, HEIGHT) == CRTUI_ERROR_INVALID_ARGUMENT,
        "only windows render");
  pthread_t thread;
  CHECK(pthread_create(&thread, NULL, render_from_other_thread, &s) == 0, "thread");
  void* thread_result = NULL;
  pthread_join(thread, &thread_result);
  CHECK((crtui_result)(intptr_t)thread_result == CRTUI_ERROR_WRONG_THREAD, "rendering is UI-thread only");
  free(pixels);
  CHECK(crtui_context_destroy(s.ctx) == CRTUI_OK, "destroy");
}

/* ---- Tranche 2: interaction state is visible in the rendered image -------- */

static int relayout_on_resize(crtui_widget current, const crtui_event* e, void* user) {
  (void)current;
  scene* s = (scene*)user;
  if (e->type == CRTUI_EVENT_RESIZED) {
    /* The application's own re-layout: stretch the column and the progress bar. */
    crtui_widget_set_bounds(s->ctx, s->column, 10, 10, e->x - 20, e->y - 20);
    crtui_widget_set_bounds(s->ctx, s->progress, 0, 110, e->x - 40, 14);
  }
  return CRTUI_EVENT_IGNORED;
}

static crtui_result input_at(scene* s, crtui_input_type type, int32_t x, int32_t y) {
  crtui_input in;
  memset(&in, 0, sizeof(in));
  in.type = type;
  in.window = s->win;
  in.x = x;
  in.y = y;
  return crtui_context_send_input(s->ctx, &in);
}

static void test_interaction_render(void) {
  scene s;
  CHECK(build_scene(&s) == 0, "scene");
  size_t stride = (size_t)WIDTH * 4u;
  uint8_t* pixels = (uint8_t*)malloc(stride * HEIGHT);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 70, 37) == COLOR_BG, "no focus ring before anything is focused");

  /* Keyboard focus draws the ring just outside the widget. */
  crtui_widget_focus(s.ctx, s.button);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  /* The ring sits outside the widget, so a child touching its parent's edge has
   * that side clipped by the parent (the contract's clipping rule); the top
   * edge, away from the container, shows it: rows 37 and 38 above the button. */
  CHECK(rgb_at(pixels, stride, 70, 37) == COLOR_FOCUS_RING && rgb_at(pixels, stride, 70, 38) == COLOR_FOCUS_RING,
        "a focused button has a 2 px ring just above it");
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BUTTON, "the fill inside the ring is unchanged");
  crtui_widget_focus(s.ctx, s.slider);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 70, 37) == COLOR_BG, "moving focus removes the old ring");
  CHECK(rgb_at(pixels, stride, 100, 87) == COLOR_FOCUS_RING && rgb_at(pixels, stride, 100, 88) == COLOR_FOCUS_RING,
        "and draws it around the slider");

  /* A pointer press darkens the button; the release restores it and activates. */
  crtui_widget_focus(s.ctx, s.button);
  input_at(&s, CRTUI_INPUT_POINTER_DOWN, 60, 50);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_PRESSED, "a pressed button is drawn darker");
  input_at(&s, CRTUI_INPUT_POINTER_UP, 60, 50);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BUTTON, "released: back to the normal fill");
  input_at(&s, CRTUI_INPUT_POINTER_DOWN, 60, 50);
  input_at(&s, CRTUI_INPUT_POINTER_CANCEL, 0, 0);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BUTTON, "a cancelled press is no longer drawn pressed");

  /* Dragging the slider moves its fill (abs x 10..210: 50% ends near x=110). */
  input_at(&s, CRTUI_INPUT_POINTER_DOWN, 110, 96);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 60, 96) == COLOR_SLIDER_FILL, "the slider fill follows a press at 50%");
  CHECK(rgb_at(pixels, stride, 170, 96) == COLOR_TRACK, "and the track remains to the right");
  input_at(&s, CRTUI_INPUT_POINTER_MOVE, 190, 300);
  crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
  CHECK(rgb_at(pixels, stride, 150, 96) == COLOR_SLIDER_FILL, "dragging right extends the fill");
  input_at(&s, CRTUI_INPUT_POINTER_UP, 190, 300);
  int32_t value = -1;
  crtui_slider_get_value(s.ctx, s.slider, &value);
  CHECK(value >= 88 && value <= 92, "the released drag left the slider near 90");
  CHECK(rgb_at(pixels, stride, 100, 87) == COLOR_FOCUS_RING, "the dragged slider is focused, so it shows the ring");

  /* Resize / re-layout: the application reacts to RESIZED, the renderer follows. */
  crtui_widget_set_callback(s.ctx, s.win, relayout_on_resize, &s);
  crtui_window_set_size(s.ctx, s.win, 480, 240, 1.0f);
  uint8_t* wide = (uint8_t*)malloc((size_t)480 * 4u * 240);
  CHECK(crtui_window_render(s.ctx, s.win, wide, 480 * 4u, 480, 240) == CRTUI_OK, "render at the new size");
  CHECK(rgb_at(wide, 480 * 4u, 3, 3) == COLOR_BG && rgb_at(wide, 480 * 4u, 470, 230) == COLOR_BG, "new background");
  CHECK(rgb_at(wide, 480 * 4u, 200, 127) == COLOR_PROGRESS_FILL, "the re-laid-out progress bar fills to 50% of its new width");
  CHECK(rgb_at(wide, 480 * 4u, 300, 127) == COLOR_TRACK && rgb_at(wide, 480 * 4u, 440, 127) == COLOR_TRACK,
        "and its track spans the new width");
  free(wide);
  free(pixels);
  crtui_context_destroy(s.ctx);
}

/* ---- Tranche 4: layout, style and the v1 widgets, as real pixels -------------- */

typedef struct v1_scene {
  crtui_context* ctx;
  crtui_window win;
  crtui_widget col, title, row, sw, cb, go, input, img, prog, list, items[4];
} v1_scene;

/* 320x240 window, a Column with 10 px padding and 8 px gap; absolute layout:
 *   title (10,10)  300x24    row (10,42) 300x32 [switch 50x24 | checkbox 120x24 | button grow]
 *   input (10,82) 300x28     image (10,118) 32x32    progress (10,158) 300x14
 *   list  (10,180) 300x50 (4 rows of 32) */
static void build_v1(v1_scene* v) {
  memset(v, 0, sizeof(*v));
  crtui_context_create(&v->ctx);
  crtui_window_create(v->ctx, &v->win);
  crtui_window_set_size(v->ctx, v->win, 320, 240, 1.0f);
  crtui_column_create(v->ctx, v->win, &v->col);
  crtui_widget_set_size(v->ctx, v->col, CRTUI_SIZE_FILL, CRTUI_SIZE_FILL);
  crtui_container_set_padding(v->ctx, v->col, 10, 10, 10, 10);
  crtui_container_set_gap(v->ctx, v->col, 8);

  crtui_text_create(v->ctx, v->col, "Title", &v->title);
  crtui_widget_set_size(v->ctx, v->title, 300, 24);
  crtui_style st;
  memset(&st, 0, sizeof(st));
  st.mask = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_FOREGROUND | CRTUI_STYLE_OPACITY | CRTUI_STYLE_FONT;
  st.background_rgb = 0x000000;
  st.foreground_rgb = 0xFF0000;
  st.opacity = 128;
  st.font = CRTUI_FONT_LARGE;
  crtui_widget_set_style(v->ctx, v->title, &st);

  crtui_row_create(v->ctx, v->col, &v->row);
  crtui_widget_set_size(v->ctx, v->row, CRTUI_SIZE_FILL, 32);
  crtui_container_set_gap(v->ctx, v->row, 8);
  crtui_switch_create(v->ctx, v->row, &v->sw);
  crtui_widget_set_size(v->ctx, v->sw, 50, 24);
  crtui_toggle_set_checked(v->ctx, v->sw, 1);
  crtui_checkbox_create(v->ctx, v->row, "Opt", &v->cb);
  crtui_widget_set_size(v->ctx, v->cb, 120, 24);
  crtui_toggle_set_checked(v->ctx, v->cb, 1);
  crtui_button_create(v->ctx, v->row, "Go", &v->go);
  crtui_widget_set_size(v->ctx, v->go, 0, 32);
  crtui_widget_set_grow(v->ctx, v->go, 1);
  memset(&st, 0, sizeof(st));
  st.mask = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_RADIUS;
  st.background_rgb = 0x8E44AD;
  st.radius = 0;
  crtui_widget_set_style(v->ctx, v->go, &st);

  crtui_text_input_create(v->ctx, v->col, "hello", &v->input);
  crtui_widget_set_size(v->ctx, v->input, CRTUI_SIZE_FILL, 28);
  crtui_text_input_set_placeholder(v->ctx, v->input, "Type here");

  crtui_image_create(v->ctx, v->col, &v->img);
  /* An 8x8 source of four 4x4 quadrants (B,G,R,A), stretched 4x to 32x32. The
   * quadrant centers stay pure colors even though the scaler interpolates. */
  uint8_t px[8 * 8 * 4];
  static const uint8_t quadrant[4][4] = {
      {0x00, 0x00, 0xFF, 0xFF}, /* top-left: red */
      {0x00, 0xFF, 0x00, 0xFF}, /* top-right: green */
      {0xFF, 0x00, 0x00, 0xFF}, /* bottom-left: blue */
      {0xFF, 0xFF, 0xFF, 0xFF}  /* bottom-right: white */
  };
  for (int yy = 0; yy < 8; ++yy)
    for (int xx = 0; xx < 8; ++xx)
      memcpy(px + ((size_t)yy * 8 + xx) * 4, quadrant[(yy / 4) * 2 + xx / 4], 4);
  crtui_image_set_pixels(v->ctx, v->img, px, 8, 8, 32);
  crtui_widget_set_size(v->ctx, v->img, 32, 32);

  crtui_progress_create(v->ctx, v->col, &v->prog);
  crtui_widget_set_size(v->ctx, v->prog, CRTUI_SIZE_FILL, 14);
  crtui_progress_set_value(v->ctx, v->prog, 50);
  memset(&st, 0, sizeof(st));
  st.mask = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_FOREGROUND;
  st.background_rgb = 0x000000;
  st.foreground_rgb = 0xFFFF00;
  crtui_widget_set_style(v->ctx, v->prog, &st);

  crtui_list_create(v->ctx, v->col, &v->list);
  crtui_widget_set_size(v->ctx, v->list, CRTUI_SIZE_FILL, 50);
  static const uint32_t row_colors[4] = {0xE74C3C, 0x27AE60, 0x2980B9, 0xF1C40F};
  for (int i = 0; i < 4; ++i) {
    crtui_list_add_item(v->ctx, v->list, "row", &v->items[i]);
    memset(&st, 0, sizeof(st));
    st.mask = CRTUI_STYLE_BACKGROUND | CRTUI_STYLE_RADIUS;
    st.background_rgb = row_colors[i];
    st.radius = 0;
    crtui_widget_set_style(v->ctx, v->items[i], &st);
  }
}

static int near_rgb(uint32_t a, uint32_t b, int tolerance) { return channel_close(a, b, tolerance); }

static int count_color(const uint8_t* pixels, size_t stride, int x0, int y0, int x1, int y1, uint32_t rgb, int tol) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (near_rgb(rgb_at(pixels, stride, x, y), rgb, tol)) ++n;
  return n;
}

static void test_v1_render(void) {
  v1_scene v;
  build_v1(&v);
  const int W = 320, H = 240;
  size_t stride = (size_t)W * 4u;
  uint8_t* px = (uint8_t*)malloc(stride * H);
  CHECK(crtui_window_render(v.ctx, v.win, px, stride, W, H) == CRTUI_OK, "render the v1 scene");
  if (getenv("CRTUI_RENDER_DUMP_V1") != NULL) {
    FILE* f = fopen(getenv("CRTUI_RENDER_DUMP_V1"), "wb");
    if (f != NULL) {
      uint32_t image_size = (uint32_t)(W * 4 * H), file_size = 54 + image_size, offset = 54, dib = 40;
      int32_t w = W, h = -H;
      uint16_t planes = 1, bits = 32;
      uint8_t header[54];
      memset(header, 0, sizeof(header));
      header[0] = 'B';
      header[1] = 'M';
      memcpy(header + 2, &file_size, 4);
      memcpy(header + 10, &offset, 4);
      memcpy(header + 14, &dib, 4);
      memcpy(header + 18, &w, 4);
      memcpy(header + 22, &h, 4);
      memcpy(header + 26, &planes, 2);
      memcpy(header + 28, &bits, 2);
      memcpy(header + 34, &image_size, 4);
      fwrite(header, 1, sizeof(header), f);
      fwrite(px, 1, stride * H, f);
      fclose(f);
    }
  }

  /* Layout: where the Column put things (asserted on the model, then on pixels). */
  int32_t x, y, w, h;
  crtui_widget_get_bounds(v.ctx, v.go, &x, &y, &w, &h);
  CHECK(x == 186 && w == 114 && h == 32, "the grow button takes what the switch and checkbox leave (300 - 50 - 120 - 16)");
  crtui_widget_get_bounds(v.ctx, v.list, &x, &y, &w, &h);
  CHECK(y == 180 && w == 300 && h == 50, "the list sits at the end of the column (10 + 24 + 8 + 32 + 8 + 28 + 8 + 32 + 8 + 14 + 8)");

  /* Style: background, radius, foreground, opacity. */
  CHECK(near_rgb(rgb_at(px, stride, 200, 60), 0x8E44AD, 2), "a styled button uses the requested background (radius 0: corner too)");
  CHECK(near_rgb(rgb_at(px, stride, 197, 44), 0x8E44AD, 2), "a radius-0 button has a square corner");
  CHECK(near_rgb(rgb_at(px, stride, 300, 20), 0x787878, 4), "50% opacity black over the window background is mid gray");
  CHECK(count_not_bg(px, stride, 10, 10, 90, 34) > 100, "the large title text and its fill were drawn");
  CHECK(near_rgb(rgb_at(px, stride, 100, 165), 0xFFFF00, 2) && near_rgb(rgb_at(px, stride, 250, 165), 0x000000, 2),
        "progress: styled fill and track");

  /* Switch and checkbox. */
  CHECK(near_rgb(rgb_at(px, stride, 16, 54), 0x2D6CDF, 2), "a checked switch shows its on-track color");
  CHECK(near_rgb(rgb_at(px, stride, 47, 54), 0xFFFFFF, 30), "with the knob at the right end");
  CHECK(count_color(px, stride, 68, 42, 92, 66, 0x2D6CDF, 6) >= 60, "a checked checkbox shows a filled box");

  /* Text input. */
  CHECK(near_rgb(rgb_at(px, stride, 300, 96), 0xFFFFFF, 2), "text input background is white");
  CHECK(near_rgb(rgb_at(px, stride, 150, 82), 0xA0A0A0, 6), "and has the 1 px border");
  CHECK(count_not_bg(px, stride, 14, 86, 70, 108) > 30, "the entered text was drawn");

  /* Image: four quadrants of an 8x8 source stretched to 32x32. */
  CHECK(near_rgb(rgb_at(px, stride, 18, 126), 0xFF0000, 12), "image top-left");
  CHECK(near_rgb(rgb_at(px, stride, 34, 126), 0x00FF00, 12), "image top-right");
  CHECK(near_rgb(rgb_at(px, stride, 18, 142), 0x0000FF, 12), "image bottom-left");
  CHECK(near_rgb(rgb_at(px, stride, 34, 142), 0xFFFFFF, 12), "image bottom-right");

  /* List: rows stack, are clipped by the list, and scroll. */
  CHECK(near_rgb(rgb_at(px, stride, 100, 186), 0xE74C3C, 2), "list row 1");
  CHECK(near_rgb(rgb_at(px, stride, 100, 215), 0x27AE60, 2), "list row 2");
  CHECK(near_rgb(rgb_at(px, stride, 100, 232), 0xF0F0F0, 2), "the list clips: nothing below its 50 px");
  crtui_scroll_view_set_offset(v.ctx, v.list, 32);
  crtui_window_render(v.ctx, v.win, px, stride, W, H);
  CHECK(near_rgb(rgb_at(px, stride, 100, 186), 0x27AE60, 2), "after scrolling by a row, row 2 is on top");
  CHECK(near_rgb(rgb_at(px, stride, 100, 216), 0x2980B9, 2), "and row 3 follows");
  CHECK(near_rgb(rgb_at(px, stride, 100, 175), 0xF0F0F0, 2), "content scrolled above the list is clipped, not painted over the gap");

  /* State changes reach the pixels. */
  crtui_toggle_set_checked(v.ctx, v.sw, 0);
  crtui_window_render(v.ctx, v.win, px, stride, W, H);
  CHECK(near_rgb(rgb_at(px, stride, 52, 54), 0xC8C8C8, 2), "an unchecked switch shows the off track (knob now at the left)");
  crtui_widget_set_visible(v.ctx, v.row, 0);
  crtui_window_render(v.ctx, v.win, px, stride, W, H);
  CHECK(near_rgb(rgb_at(px, stride, 200, 60), 0xFFFFFF, 2), "hiding the row hides the button and the column re-flows: the text input is there now");
  /* Hidden row releases its space: the text input moves up by 32 + 8. */
  crtui_widget_get_bounds(v.ctx, v.input, &x, &y, &w, &h);
  CHECK(y == 42, "the hidden row gives its space back to the column");

  /* Resize: the column stretches the input and progress bar. */
  crtui_window_set_size(v.ctx, v.win, 400, 240, 1.0f);
  uint8_t* wide = (uint8_t*)malloc((size_t)400 * 4u * 240);
  CHECK(crtui_window_render(v.ctx, v.win, wide, 400 * 4u, 400, 240) == CRTUI_OK, "render after resize");
  crtui_widget_get_bounds(v.ctx, v.prog, &x, &y, &w, &h);
  CHECK(w == 380, "a FILL child follows the window width by itself");
  free(wide);

  free(px);
  crtui_context_destroy(v.ctx);
}

static void test_lifecycle(void) {
  size_t stride = (size_t)WIDTH * 4u;
  uint8_t* pixels = (uint8_t*)malloc(stride * HEIGHT);
  /* Warm-up: LVGL's one-time initialization is not a leak. */
  {
    scene s;
    build_scene(&s);
    crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT);
    crtui_context_destroy(s.ctx);
  }
  int handles_before, threads_before, handles_after, threads_after;
  sample_resources(&handles_before, &threads_before);
  const int cycles = 40;
  for (int i = 0; i < cycles; ++i) {
    scene s;
    if (build_scene(&s) != 0) {
      ++failures;
      break;
    }
    CHECK(crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT) == CRTUI_OK, "cycle render");
    /* Destroy a subtree, then render again from the reduced tree. */
    crtui_widget_destroy(s.ctx, s.button);
    CHECK(crtui_window_render(s.ctx, s.win, pixels, stride, WIDTH, HEIGHT) == CRTUI_OK, "cycle re-render");
    CHECK(rgb_at(pixels, stride, 16, 46) == COLOR_BG, "the destroyed button is gone from the image");
    if (i % 2 == 0) crtui_widget_destroy(s.ctx, s.win); /* window first, then the context */
    CHECK(crtui_context_destroy(s.ctx) == CRTUI_OK, "cycle destroy");
  }
  sample_resources(&handles_after, &threads_after);
  if (handles_before >= 0 && handles_after - handles_before > 4) {
    fprintf(stderr, "FAIL handle/fd count grew %d -> %d\n", handles_before, handles_after);
    ++failures;
  }
  if (threads_before >= 0 && threads_after > threads_before) {
    fprintf(stderr, "FAIL thread count grew %d -> %d\n", threads_before, threads_after);
    ++failures;
  }
  free(pixels);
  printf("crtui_render_test: lifecycle cycles=%d handles=%d->%d threads=%d->%d\n", cycles, handles_before,
         handles_after, threads_before, threads_after);
}

int main(void) {
  test_pixels();
  test_interaction_render();
  test_v1_render();
  test_lifecycle();
  if (failures != 0) {
    fprintf(stderr, "crtui_render_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtui_render_test: ok pixels=pass background=pass label=pass button=pass slider=pass progress=pass "
         "rerender=pass stride=pass resize=pass focus_ring=pass pressed=pass slider_drag=pass relayout=pass "
         "v1_layout=pass v1_style=pass v1_widgets=pass lifecycle=pass\n");
  return 0;
}
