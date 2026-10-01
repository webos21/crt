/* crtui Tranche 5A acceptance: the producer-neutral External Surface scene
 * boundary. This is intentionally headless and LVGL-free. It proves the
 * composition metadata a real crtgfx compositor consumes without pretending
 * that metadata alone displays a producer GPU frame. */

#include "crtui/ui.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    if (!(cond)) {                                                   \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);  \
      ++failures;                                                    \
    }                                                                \
  } while (0)

static int rect_is(crtui_rect r, int x, int y, int w, int h) {
  return r.x == x && r.y == y && r.width == w && r.height == h;
}

typedef struct thread_probe {
  crtui_context* ctx;
  crtui_window win;
  crtui_widget view;
  crtui_result snapshot_result;
  crtui_result damage_result;
  crtui_result clear_result;
} thread_probe;

static void* probe_wrong_thread(void* raw) {
  thread_probe* probe = (thread_probe*)raw;
  size_t count = 0;
  probe->snapshot_result = crtui_window_get_surface_layers(probe->ctx, probe->win, NULL, 0, &count);
  probe->damage_result = crtui_surface_view_damage(probe->ctx, probe->view, 0, 0, 1, 1);
  probe->clear_result = crtui_surface_view_clear_damage(probe->ctx, probe->view);
  return NULL;
}

int main(void) {
  crtui_context* ctx = NULL;
  crtui_window win = CRTUI_INVALID_WIDGET;
  crtui_widget below, clipper, surface_a, surface_b;
  CHECK(crtui_context_create(&ctx) == CRTUI_OK, "create context");
  CHECK(crtui_window_create(ctx, &win) == CRTUI_OK, "create window");
  CHECK(crtui_window_set_size(ctx, win, 300, 200, 1.0f) == CRTUI_OK, "size window");

  CHECK(crtui_button_create(ctx, win, "below", &below) == CRTUI_OK, "create lower sibling");
  CHECK(crtui_widget_set_bounds(ctx, below, 200, 10, 50, 20) == CRTUI_OK, "position lower sibling");
  CHECK(crtui_container_create(ctx, win, &clipper) == CRTUI_OK, "create clipping parent");
  CHECK(crtui_widget_set_bounds(ctx, clipper, 10, 10, 100, 80) == CRTUI_OK, "position clipping parent");
  CHECK(crtui_surface_view_create(ctx, clipper, &surface_a) == CRTUI_OK, "create clipped SurfaceView");
  CHECK(crtui_widget_set_bounds(ctx, surface_a, 70, 50, 80, 60) == CRTUI_OK, "position clipped SurfaceView");
  CHECK(crtui_surface_view_create(ctx, win, &surface_b) == CRTUI_OK, "create top SurfaceView");
  CHECK(crtui_widget_set_bounds(ctx, surface_b, 150, 100, 40, 30) == CRTUI_OK, "position top SurfaceView");

  crtui_style half;
  memset(&half, 0, sizeof(half));
  half.mask = CRTUI_STYLE_OPACITY;
  half.opacity = 128;
  CHECK(crtui_widget_set_style(ctx, clipper, &half) == CRTUI_OK, "parent opacity");
  CHECK(crtui_widget_set_style(ctx, surface_a, &half) == CRTUI_OK, "view opacity");

  size_t count = 99;
  CHECK(crtui_window_get_surface_layers(ctx, win, NULL, 0, &count) == CRTUI_OK && count == 2,
        "two-call query reports two visible surfaces");
  crtui_surface_layer one;
  memset(&one, 0xA5, sizeof(one));
  CHECK(crtui_window_get_surface_layers(ctx, win, &one, 1, &count) == CRTUI_WOULD_BLOCK && count == 2,
        "short capacity reports required count");
  CHECK(one.view == UINT64_C(0xA5A5A5A5A5A5A5A5), "short capacity writes no partial layer");

  crtui_surface_layer layers[2];
  memset(layers, 0, sizeof(layers));
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, &count) == CRTUI_OK && count == 2,
        "collect surface layers");
  CHECK(layers[0].view == surface_a && rect_is(layers[0].bounds, 80, 60, 80, 60), "absolute bounds");
  CHECK(rect_is(layers[0].clip, 80, 60, 30, 30), "clip is intersected through ancestors");
  CHECK(layers[0].opacity == 64, "ancestor and view opacity multiply");
  CHECK(layers[0].has_damage && rect_is(layers[0].damage, 80, 60, 30, 30),
        "new view starts fully damaged but reports only visible damage");
  CHECK(layers[1].view == surface_b && layers[1].z_order > layers[0].z_order, "scene order is stable z-order");

  crtui_widget hit = CRTUI_INVALID_WIDGET;
  CHECK(crtui_context_hit_test(ctx, win, 85, 65, &hit) == CRTUI_OK && hit == surface_a,
        "SurfaceView participates in hit-testing");
  CHECK(crtui_context_hit_test(ctx, win, 115, 65, &hit) == CRTUI_OK && hit != surface_a,
        "ancestor clip also limits SurfaceView hit-testing");

  CHECK(crtui_surface_view_clear_damage(ctx, surface_a) == CRTUI_OK, "clear consumed damage");
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, &count) == CRTUI_OK && !layers[0].has_damage,
        "cleared damage stays clear without a scene mutation");
  uint64_t serial = layers[0].damage_serial;
  CHECK(crtui_surface_view_damage(ctx, surface_a, 5, 6, 7, 8) == CRTUI_OK, "producer damage");
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, &count) == CRTUI_OK && layers[0].has_damage &&
            rect_is(layers[0].damage, 85, 66, 7, 8) && layers[0].damage_serial != serial,
        "producer damage is translated to window coordinates and advances serial");

  CHECK(crtui_widget_set_visible(ctx, clipper, 0) == CRTUI_OK, "hide ancestor");
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, &count) == CRTUI_OK && count == 1 &&
            layers[0].view == surface_b,
        "hidden ancestry removes a surface from composition");
  CHECK(crtui_widget_set_visible(ctx, clipper, 1) == CRTUI_OK, "show ancestor");
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, &count) == CRTUI_OK && count == 2 &&
            layers[0].has_damage,
        "scene mutation conservatively restores full damage");

  CHECK(crtui_surface_view_damage(ctx, below, 0, 0, 1, 1) == CRTUI_ERROR_INVALID_ARGUMENT,
        "damage rejects a non-SurfaceView");
  CHECK(crtui_surface_view_damage(ctx, surface_a, -1, 0, 1, 1) == CRTUI_ERROR_INVALID_ARGUMENT,
        "damage rejects a negative origin");
  CHECK(crtui_surface_view_damage(ctx, surface_a, 500, 0, 1, 1) == CRTUI_ERROR_INVALID_ARGUMENT,
        "damage rejects a rectangle outside the view");
  CHECK(crtui_window_get_surface_layers(ctx, below, layers, 2, &count) == CRTUI_ERROR_INVALID_ARGUMENT,
        "snapshot requires a window");
  CHECK(crtui_window_get_surface_layers(ctx, win, layers, 2, NULL) == CRTUI_ERROR_INVALID_ARGUMENT,
        "snapshot requires out_count");
  thread_probe probe = {ctx, win, surface_a, CRTUI_OK, CRTUI_OK, CRTUI_OK};
  pthread_t thread;
  CHECK(pthread_create(&thread, NULL, probe_wrong_thread, &probe) == 0, "create wrong-thread probe");
  CHECK(pthread_join(thread, NULL) == 0, "join wrong-thread probe");
  CHECK(probe.snapshot_result == CRTUI_ERROR_WRONG_THREAD && probe.damage_result == CRTUI_ERROR_WRONG_THREAD &&
            probe.clear_result == CRTUI_ERROR_WRONG_THREAD,
        "every SurfaceView operation remains UI-thread owned");
  CHECK(crtui_context_destroy(ctx) == CRTUI_OK, "destroy context");

  if (failures != 0) {
    fprintf(stderr, "crtui_surface_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("crtui_surface_test: ok create=pass geometry=pass clip=pass opacity=pass z=pass damage=pass "
         "hit_test=pass visibility=pass thread=pass\n");
  return 0;
}
