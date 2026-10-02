#pragma once

/* Optional MediaView binding for the accepted crtmedia GPU-frame -> Skia ->
 * crtui final-compositor path (Tranche 6). This C++ companion keeps
 * crtmedia, crtgfx and Skia out of core libcrtui. */

#include "crtui/skia.h"

#include "crtgfx/gpu.h"
#include "crtgfx/skia_media.h"
#include "crtmedia/gpu_frame.h"

typedef struct crtui_skia_media_provider crtui_skia_media_provider;

/* context and device are borrowed and must outlive provider. All functions
 * except get_surface_provider() are UI-thread-only; destroy provider before
 * destroying context or the GPU context/device. */
crtui_result crtui_skia_media_provider_create(
    crtui_context* context, GrDirectContext* skia_context,
    const crtgfx_gpu_device* device, crtui_skia_media_provider** out_provider);
void crtui_skia_media_provider_destroy(crtui_skia_media_provider* provider);

/* Submit a hardware-decoded GPU frame for one MediaView. On success ownership
 * of *frame moves into the imported SkImage and *frame is cleared, exactly as
 * crtgfx_skia_import_media_frame() specifies. On failure *frame is untouched.
 * Replacing or clearing a view releases the previous image only after any
 * synchronous compositor acquire has released its temporary reference. */
crtui_result crtui_skia_media_provider_submit(
    crtui_skia_media_provider* provider, crtui_widget media_view,
    crtmedia_gpu_frame* frame);
crtui_result crtui_skia_media_provider_clear(
    crtui_skia_media_provider* provider, crtui_widget media_view);

/* Returns callbacks suitable for crtui_skia_compose(). The callback table
 * borrows provider and becomes invalid when provider is destroyed. Multiple
 * MediaViews may be registered in the same provider and are selected by their
 * stable widget ids. */
crtui_result crtui_skia_media_provider_get_surface_provider(
    crtui_skia_media_provider* provider,
    crtui_skia_surface_provider* out_surface_provider);

