/* Tranche 5B: a synthetic GPU producer composed between CRT UI planes.
 * Default mode is headless/offscreen and belongs to CTest. Passing a frame
 * count opens a real crtgfx GPU-presentation window and exercises the same
 * compositor against the swapchain-backed SkSurface. */

#include "crtui/skia.h"

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"
#include "crtgfx/window.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct Scene {
  crtui_context* ui = nullptr;
  crtui_window window = CRTUI_INVALID_WIDGET;
  crtui_widget surface = CRTUI_INVALID_WIDGET;
};

struct Producer {
  sk_sp<SkImage> image;
  int acquire_count = 0;
  int release_count = 0;
  crtui_surface_layer last_layer = {};
};

struct EmptyProducer {
  int acquire_count = 0;
  int release_count = 0;
};

int failures = 0;

bool check(bool ok, const char* name) {
  if (!ok) {
    fprintf(stderr, "crtui_surface_compositor_test: FAILED %s\n", name);
    ++failures;
  }
  return ok;
}

SkImage* acquire_image(void* user, const crtui_surface_layer* layer) {
  Producer* producer = static_cast<Producer*>(user);
  ++producer->acquire_count;
  producer->last_layer = *layer;
  if (producer->image == nullptr) return nullptr;
  producer->image->ref();  // the matching release callback owns this ref
  return producer->image.get();
}

void release_image(void* user, const crtui_surface_layer*, SkImage* image) {
  Producer* producer = static_cast<Producer*>(user);
  ++producer->release_count;
  image->unref();
}

SkImage* acquire_no_frame(void* user, const crtui_surface_layer*) {
  ++static_cast<EmptyProducer*>(user)->acquire_count;
  return nullptr;
}

void release_no_frame(void* user, const crtui_surface_layer*, SkImage*) {
  ++static_cast<EmptyProducer*>(user)->release_count;
}

void set_background(crtui_context* ui, crtui_widget widget, uint32_t rgb) {
  crtui_style style = {};
  style.mask = CRTUI_STYLE_BACKGROUND;
  style.background_rgb = rgb;
  check(crtui_widget_set_style(ui, widget, &style) == CRTUI_OK, "set background");
}

bool make_scene(int width, int height, Scene* scene) {
  crtui_widget below = CRTUI_INVALID_WIDGET;
  crtui_widget clip = CRTUI_INVALID_WIDGET;
  crtui_widget above = CRTUI_INVALID_WIDGET;
  if (!check(crtui_context_create(&scene->ui) == CRTUI_OK, "context create") ||
      !check(crtui_window_create(scene->ui, &scene->window) == CRTUI_OK, "window create") ||
      !check(crtui_window_set_size(scene->ui, scene->window, width, height, 1.0f) == CRTUI_OK, "window size") ||
      !check(crtui_container_create(scene->ui, scene->window, &below) == CRTUI_OK, "below create") ||
      !check(crtui_widget_set_bounds(scene->ui, below, 10, 10, width - 20, height - 20) == CRTUI_OK,
             "below bounds") ||
      !check(crtui_container_create(scene->ui, scene->window, &clip) == CRTUI_OK, "clip create") ||
      !check(crtui_widget_set_bounds(scene->ui, clip, 40, 20, width - 90, height - 50) == CRTUI_OK,
             "clip bounds") ||
      !check(crtui_surface_view_create(scene->ui, clip, &scene->surface) == CRTUI_OK, "surface create") ||
      !check(crtui_widget_set_bounds(scene->ui, scene->surface, -10, -10, width - 70, height - 30) == CRTUI_OK,
             "surface bounds") ||
      !check(crtui_container_create(scene->ui, scene->window, &above) == CRTUI_OK, "above create") ||
      !check(crtui_widget_set_bounds(scene->ui, above, 60, 30, 40, 30) == CRTUI_OK, "above bounds")) {
    return false;
  }
  set_background(scene->ui, below, 0xFF0000u);
  set_background(scene->ui, above, 0x0000FFu);
  crtui_style opacity = {};
  opacity.mask = CRTUI_STYLE_OPACITY;
  opacity.opacity = 128;
  check(crtui_widget_set_style(scene->ui, scene->surface, &opacity) == CRTUI_OK, "surface opacity");
  check(crtui_surface_view_clear_damage(scene->ui, scene->surface) == CRTUI_OK, "clear damage");
  check(crtui_surface_view_damage(scene->ui, scene->surface, 15, 12, 20, 16) == CRTUI_OK, "surface damage");
  return failures == 0;
}

sk_sp<SkImage> make_producer_image(GrDirectContext* context) {
  sk_sp<SkSurface> producer = crtgfx_skia_make_gpu_offscreen_surface(context, 96, 72);
  if (producer == nullptr) return nullptr;
  producer->getCanvas()->clear(SK_ColorGREEN);
  context->flushAndSubmit(producer.get(), GrSyncCpu::kYes);
  return producer->makeImageSnapshot();
}

bool near(uint8_t value, int expected) {
  int delta = static_cast<int>(value) - expected;
  return delta >= -3 && delta <= 3;
}

bool pixel_is(const uint8_t* pixels, size_t stride, int x, int y, int b, int g, int r) {
  const uint8_t* p = pixels + static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4u;
  return near(p[0], b) && near(p[1], g) && near(p[2], r) && p[3] >= 250;
}

bool compose_and_check(GrDirectContext* context, SkSurface* target, Scene* scene, Producer* producer) {
  crtui_skia_surface_provider provider = {producer, acquire_image, release_image};
  int before_acquire = producer->acquire_count;
  int before_release = producer->release_count;
  if (!check(crtui_skia_compose(scene->ui, scene->window, target, &provider) == CRTUI_OK, "compose")) return false;
  context->flushAndSubmit(target, GrSyncCpu::kYes);
  check(producer->acquire_count == before_acquire + 1, "one acquire per frame");
  check(producer->release_count == before_release + 1, "one release per frame");
  check(producer->last_layer.view == scene->surface, "stable SurfaceView id");
  check(producer->last_layer.has_damage != 0, "damage reaches producer boundary");

  const int width = target->width();
  const int height = target->height();
  const size_t stride = static_cast<size_t>(width) * 4u;
  uint8_t* pixels = static_cast<uint8_t*>(calloc(static_cast<size_t>(height), stride));
  if (!check(pixels != nullptr, "readback allocation")) return false;
  SkImageInfo info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  bool read = target->readPixels(info, pixels, stride, 0, 0);
  check(read, "readPixels");
  if (read) {
    check(pixel_is(pixels, stride, 20, 20, 0, 0, 255), "UI below surface");
    check(pixel_is(pixels, stride, 35, 25, 0, 0, 255), "surface ancestor clip");
    check(pixel_is(pixels, stride, 50, 30, 0, 128, 127), "surface opacity over UI below");
    check(pixel_is(pixels, stride, 70, 40, 255, 0, 0), "UI above surface");
  }
  free(pixels);
  return failures == 0;
}

bool run_offscreen(GrDirectContext* context, Producer* producer) {
  Scene scene;
  if (!make_scene(160, 100, &scene)) return false;
  sk_sp<SkSurface> target = crtgfx_skia_make_gpu_offscreen_surface(context, 160, 100);
  check(target != nullptr, "initial target");
  if (target != nullptr) compose_and_check(context, target.get(), &scene, producer);

  check(crtui_window_set_size(scene.ui, scene.window, 200, 120, 1.0f) == CRTUI_OK, "UI resize");
  sk_sp<SkSurface> resized = crtgfx_skia_make_gpu_offscreen_surface(context, 200, 120);
  check(resized != nullptr, "resized target");
  if (resized != nullptr) compose_and_check(context, resized.get(), &scene, producer);
  if (resized != nullptr) {
    EmptyProducer empty;
    crtui_skia_surface_provider unavailable = {&empty, acquire_no_frame, release_no_frame};
    check(crtui_skia_compose(scene.ui, scene.window, resized.get(), &unavailable) == CRTUI_WOULD_BLOCK,
          "unavailable frame returns WOULD_BLOCK");
    check(empty.acquire_count == 1 && empty.release_count == 0,
          "unavailable frame is not released");
  }
  check(crtui_context_destroy(scene.ui) == CRTUI_OK, "context destroy");
  return failures == 0;
}

bool run_window(
    crtgfx_gpu_device* device, GrDirectContext* context, Producer* producer, unsigned long frame_limit) {
  crtgfx_window_desc desc = {"crtui SurfaceView final compositor", 320, 200,
                             CRTGFX_WINDOW_VISIBLE | CRTGFX_WINDOW_GPU_PRESENTATION};
  crtgfx_window* window = nullptr;
  crtgfx_gpu_surface* surface = nullptr;
  if (!check(crtgfx_window_create(&desc, &window) == CRTGFX_OK, "real window create") ||
      !check(crtgfx_gpu_surface_create(device, window, &surface) == CRTGFX_OK, "presentation surface create")) {
    if (window != nullptr) crtgfx_window_destroy(window);
    return false;
  }
  uint32_t width = 0, height = 0;
  check(crtgfx_gpu_surface_get_size(surface, &width, &height) == CRTGFX_OK, "presentation size");
  Scene scene;
  make_scene(static_cast<int>(width), static_cast<int>(height), &scene);
  unsigned long presented = 0;
  while (failures == 0 && presented < frame_limit && !crtgfx_window_should_close(window)) {
    if (crtgfx_gpu_surface_acquire(surface, 1000000u) == CRTGFX_OK) {
      sk_sp<SkSurface> target = crtgfx_skia_wrap_gpu_surface(context, surface);
      if (!check(target != nullptr, "wrap live swapchain")) break;
      crtui_skia_surface_provider provider = {producer, acquire_image, release_image};
      if (!check(crtui_skia_compose(scene.ui, scene.window, target.get(), &provider) == CRTUI_OK,
                 "live compose")) break;
      if (!check(crtgfx_skia_gpu_surface_present(context, target.get(), surface) == CRTGFX_OK,
                 "live present")) break;
      ++presented;
    }
    crtgfx_window_pump_events(1);
  }
  crtui_context_destroy(scene.ui);
  crtgfx_gpu_surface_release(surface);
  crtgfx_window_destroy(window);
  printf("crtui_surface_compositor_test: window presented=%lu ownership=%s\n",
         presented, producer->acquire_count == producer->release_count ? "pass" : "fail");
  return presented == frame_limit && producer->acquire_count == producer->release_count && failures == 0;
}

}  // namespace

extern "C" int main(int argc, char** argv) {
  crtgfx_gpu_capabilities caps = {};
  if (crtgfx_gpu_query_capabilities(&caps) != CRTGFX_OK || caps.device_count == 0) {
    printf("crtui_surface_compositor_test: skipped -- no usable GPU device\n");
    return 0;
  }
  crtgfx_gpu_device* device = nullptr;
  if (!check(crtgfx_gpu_device_create(0, &device) == CRTGFX_OK, "GPU device")) return 1;
  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  check(context != nullptr, "Ganesh context");
  Producer producer;
  if (context != nullptr) producer.image = make_producer_image(context.get());
  check(producer.image != nullptr, "synthetic producer image");
  if (producer.image != nullptr) check(producer.image->isTextureBacked(), "producer remains GPU texture-backed");

  bool ok = context != nullptr && producer.image != nullptr && run_offscreen(context.get(), &producer);
  if (ok && argc > 1) {
    unsigned long frames = strtoul(argv[1], nullptr, 10);
    ok = frames != 0 && run_window(device, context.get(), &producer, frames);
  }
  check(producer.acquire_count == producer.release_count, "balanced final ownership");
  producer.image.reset();
  context.reset();
  crtgfx_gpu_device_release(device);
  printf("crtui_surface_compositor_test: %s gpu=pass ordering=pass clip=pass opacity=pass damage=pass resize=pass ownership=pass\n",
         ok && failures == 0 ? "ok" : "FAILED");
  return ok && failures == 0 ? 0 : 1;
}
