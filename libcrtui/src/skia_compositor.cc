#include "crtui/skia.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSamplingOptions.h"

#include <limits.h>
#include <stdlib.h>

namespace {

crtui_result draw_ui_plane(
    crtui_context* context, crtui_window window, SkCanvas* canvas,
    uint32_t z_begin, uint32_t z_end, bool transparent, int width, int height) {
  const size_t stride = static_cast<size_t>(width) * 4u;
  void* pixels = calloc(static_cast<size_t>(height), stride);
  if (pixels == nullptr) return CRTUI_ERROR_IO;
  crtui_result result = crtui_window_render_plane(
      context, window, z_begin, z_end, transparent ? 1 : 0,
      pixels, stride, width, height);
  if (result == CRTUI_OK) {
    SkImageInfo info = SkImageInfo::Make(
        width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    SkPixmap pixmap(info, pixels, stride);
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(pixmap);
    if (image == nullptr) {
      result = CRTUI_ERROR_IO;
    } else {
      canvas->drawImage(image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest));
    }
  }
  free(pixels);
  return result;
}

}  // namespace

crtui_result crtui_skia_compose(
    crtui_context* context, crtui_window window, SkSurface* target,
    const crtui_skia_surface_provider* provider) {
  if (context == nullptr || window == CRTUI_INVALID_WIDGET || target == nullptr ||
      provider == nullptr || provider->acquire == nullptr || provider->release == nullptr) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  int32_t width = 0;
  int32_t height = 0;
  float dpi_scale = 0.0f;
  crtui_result result = crtui_window_get_size(context, window, &width, &height, &dpi_scale);
  if (result != CRTUI_OK) return result;
  if (target->width() != width || target->height() != height) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }

  size_t count = 0;
  result = crtui_window_get_surface_layers(context, window, nullptr, 0, &count);
  if (result != CRTUI_OK) return result;
  crtui_surface_layer* layers = nullptr;
  if (count != 0) {
    layers = static_cast<crtui_surface_layer*>(calloc(count, sizeof(*layers)));
    if (layers == nullptr) return CRTUI_ERROR_IO;
    result = crtui_window_get_surface_layers(context, window, layers, count, &count);
    if (result != CRTUI_OK) {
      free(layers);
      return result;
    }
  }

  SkCanvas* canvas = target->getCanvas();
  uint32_t next_z = 0;
  for (size_t i = 0; i < count && result == CRTUI_OK; ++i) {
    const crtui_surface_layer* layer = &layers[i];
    if (next_z < layer->z_order) {
      result = draw_ui_plane(
          context, window, canvas, next_z, layer->z_order, next_z != 0,
          width, height);
      if (result != CRTUI_OK) break;
    }

    SkImage* image = provider->acquire(provider->user, layer);
    if (image == nullptr) {
      result = CRTUI_WOULD_BLOCK;
      break;
    }
    SkPaint paint;
    paint.setAlphaf(static_cast<float>(layer->opacity) / 255.0f);
    canvas->save();
    canvas->clipRect(SkRect::MakeXYWH(
        static_cast<float>(layer->clip.x), static_cast<float>(layer->clip.y),
        static_cast<float>(layer->clip.width), static_cast<float>(layer->clip.height)));
    canvas->drawImageRect(
        image,
        SkRect::MakeXYWH(
            static_cast<float>(layer->bounds.x), static_cast<float>(layer->bounds.y),
            static_cast<float>(layer->bounds.width), static_cast<float>(layer->bounds.height)),
        SkSamplingOptions(SkFilterMode::kNearest), &paint);
    canvas->restore();
    provider->release(provider->user, layer, image);
    next_z = layer->z_order == UINT32_MAX ? UINT32_MAX : layer->z_order + 1u;
  }
  if (result == CRTUI_OK && next_z != UINT32_MAX) {
    result = draw_ui_plane(
        context, window, canvas, next_z, UINT32_MAX, next_z != 0,
        width, height);
  }
  free(layers);
  return result;
}
