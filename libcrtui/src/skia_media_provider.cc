#include "crtui/skia_media.h"

#include "include/core/SkImage.h"

#include <stdlib.h>
#include <string.h>

struct crtui_skia_media_entry {
  crtui_widget view;
  SkImage* image; /* one provider-owned reference */
};

struct crtui_skia_media_provider {
  crtui_context* ui;
  GrDirectContext* skia;
  const crtgfx_gpu_device* device;
  crtui_skia_media_entry* entries;
  size_t count;
  size_t capacity;
};

namespace {

crtui_result validate_media_view(
    crtui_skia_media_provider* provider, crtui_widget view) {
  if (provider == nullptr || view == CRTUI_INVALID_WIDGET) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  crtui_widget_kind kind = static_cast<crtui_widget_kind>(0);
  crtui_result result = crtui_widget_get_kind(provider->ui, view, &kind);
  if (result != CRTUI_OK) return result;
  return kind == CRTUI_WIDGET_MEDIA_VIEW ? CRTUI_OK
                                         : CRTUI_ERROR_INVALID_ARGUMENT;
}

size_t find_entry(const crtui_skia_media_provider* provider, crtui_widget view) {
  for (size_t i = 0; i < provider->count; ++i) {
    if (provider->entries[i].view == view) return i;
  }
  return provider->count;
}

crtui_result reserve_entry(crtui_skia_media_provider* provider) {
  if (provider->count < provider->capacity) return CRTUI_OK;
  size_t capacity = provider->capacity == 0 ? 4 : provider->capacity * 2;
  void* grown = realloc(provider->entries, capacity * sizeof(*provider->entries));
  if (grown == nullptr) return CRTUI_ERROR_IO;
  provider->entries = static_cast<crtui_skia_media_entry*>(grown);
  provider->capacity = capacity;
  return CRTUI_OK;
}

void damage_whole_view(crtui_skia_media_provider* provider, crtui_widget view) {
  int32_t x = 0, y = 0, width = 0, height = 0;
  if (crtui_widget_get_bounds(provider->ui, view, &x, &y, &width, &height) == CRTUI_OK &&
      width > 0 && height > 0) {
    (void)crtui_surface_view_damage(provider->ui, view, 0, 0, width, height);
  }
}

SkImage* acquire_media_image(void* user, const crtui_surface_layer* layer) {
  auto* provider = static_cast<crtui_skia_media_provider*>(user);
  if (provider == nullptr || layer == nullptr) return nullptr;
  size_t index = find_entry(provider, layer->view);
  if (index == provider->count || provider->entries[index].image == nullptr) {
    return nullptr;
  }
  provider->entries[index].image->ref();
  return provider->entries[index].image;
}

void release_media_image(void*, const crtui_surface_layer*, SkImage* image) {
  if (image != nullptr) image->unref();
}

}  // namespace

crtui_result crtui_skia_media_provider_create(
    crtui_context* context, GrDirectContext* skia_context,
    const crtgfx_gpu_device* device, crtui_skia_media_provider** out_provider) {
  if (out_provider == nullptr) return CRTUI_ERROR_INVALID_ARGUMENT;
  *out_provider = nullptr;
  if (context == nullptr || skia_context == nullptr || device == nullptr) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  auto* provider = static_cast<crtui_skia_media_provider*>(
      calloc(1, sizeof(crtui_skia_media_provider)));
  if (provider == nullptr) return CRTUI_ERROR_IO;
  provider->ui = context;
  provider->skia = skia_context;
  provider->device = device;
  *out_provider = provider;
  return CRTUI_OK;
}

void crtui_skia_media_provider_destroy(crtui_skia_media_provider* provider) {
  if (provider == nullptr) return;
  for (size_t i = 0; i < provider->count; ++i) {
    if (provider->entries[i].image != nullptr) {
      provider->entries[i].image->unref();
    }
  }
  free(provider->entries);
  free(provider);
}

crtui_result crtui_skia_media_provider_submit(
    crtui_skia_media_provider* provider, crtui_widget media_view,
    crtmedia_gpu_frame* frame) {
  crtui_result result = validate_media_view(provider, media_view);
  if (result != CRTUI_OK) return result;
  if (frame == nullptr || frame->memory_kind != CRTMEDIA_GPU_MEMORY_GPU) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  size_t index = find_entry(provider, media_view);
  if (index == provider->count) {
    result = reserve_entry(provider);
    if (result != CRTUI_OK) return result;
  }
  sk_sp<SkImage> imported =
      crtgfx_skia_import_media_frame(provider->skia, provider->device, frame);
  if (imported == nullptr) return CRTUI_ERROR_UNSUPPORTED;
  if (index == provider->count) {
    provider->entries[index].view = media_view;
    provider->entries[index].image = nullptr;
    ++provider->count;
  }
  SkImage* previous = provider->entries[index].image;
  provider->entries[index].image = imported.release();
  if (previous != nullptr) previous->unref();
  damage_whole_view(provider, media_view);
  return CRTUI_OK;
}

crtui_result crtui_skia_media_provider_clear(
    crtui_skia_media_provider* provider, crtui_widget media_view) {
  crtui_result result = validate_media_view(provider, media_view);
  if (result != CRTUI_OK) return result;
  size_t index = find_entry(provider, media_view);
  if (index == provider->count) return CRTUI_OK;
  SkImage* image = provider->entries[index].image;
  if (index + 1 < provider->count) {
    memmove(&provider->entries[index], &provider->entries[index + 1],
            (provider->count - index - 1) * sizeof(*provider->entries));
  }
  --provider->count;
  if (image != nullptr) image->unref();
  damage_whole_view(provider, media_view);
  return CRTUI_OK;
}

crtui_result crtui_skia_media_provider_get_surface_provider(
    crtui_skia_media_provider* provider,
    crtui_skia_surface_provider* out_surface_provider) {
  if (provider == nullptr || out_surface_provider == nullptr) {
    return CRTUI_ERROR_INVALID_ARGUMENT;
  }
  out_surface_provider->user = provider;
  out_surface_provider->acquire = acquire_media_image;
  out_surface_provider->release = release_media_image;
  return CRTUI_OK;
}
