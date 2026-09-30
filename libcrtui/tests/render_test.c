/* crtui Tranche 1 acceptance (docs/crtui_acceptance.md): LVGL software draw
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
  test_lifecycle();
  if (failures != 0) {
    fprintf(stderr, "crtui_render_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtui_render_test: ok pixels=pass background=pass label=pass button=pass slider=pass progress=pass "
         "rerender=pass stride=pass resize=pass lifecycle=pass\n");
  return 0;
}
