#pragma once

/* Optional crtgfx/Skia final compositor for SurfaceView (Tranche 5B).
 * This is deliberately a separate C++ companion library: crtui/ui.h remains
 * renderer- and producer-neutral and libcrtui itself does not acquire a GPU
 * dependency. */

#include "crtui/ui.h"

#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"

/* A non-null acquire() result carries one retained SkImage reference. The
 * compositor never directly unrefs or caches it; it calls the producer-defined
 * release() exactly once after the synchronous draw, and release() consumes
 * that reference. Returning null means no frame is currently available and
 * makes compose return CRTUI_WOULD_BLOCK. The layer pointer is valid only
 * during the callback. */
typedef SkImage* (*crtui_skia_surface_acquire_fn)(
    void* user, const crtui_surface_layer* layer);
typedef void (*crtui_skia_surface_release_fn)(
    void* user, const crtui_surface_layer* layer, SkImage* image);

typedef struct crtui_skia_surface_provider {
  void* user;
  crtui_skia_surface_acquire_fn acquire;
  crtui_skia_surface_release_fn release;
} crtui_skia_surface_provider;

/* Composes the window into target in scene order:
 *   UI below -> producer SurfaceView image -> UI above.
 * Producer images are drawn directly by Skia and are never copied through
 * LVGL or a CPU framebuffer. The caller owns target, GPU synchronization and
 * final presentation. */
crtui_result crtui_skia_compose(
    crtui_context* context, crtui_window window, SkSurface* target,
    const crtui_skia_surface_provider* provider);
