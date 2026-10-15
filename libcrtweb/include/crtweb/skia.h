#pragma once

/* Optional C++17 GPU final-compositor companion. WebKit's private Skia objects
 * never cross the process boundary: the v1 surface transport carries CPU BGRA
 * shared memory and Ganesh uploads it into the application's GPU context.
 * This is GPU composition, NOT GPU-rendered WebCore or zero-copy transport.
 */
#include "crtweb/webview.h"
#include "include/gpu/ganesh/GrDirectContext.h"

namespace crtweb {

/* UI/context-owner thread only. context and target belong to the application;
 * target must be a GPU surface of that exact context (offscreen or an acquired
 * crtgfx swapchain). Waits for GPU completion before returning: the next poll,
 * resize or shutdown may release/retire the shared-memory source image. This
 * conservative CPU wait is intentional until a separate GPU buffer/fence
 * transport exists. No implicit CPU target or lost-device fallback.
 *
 * The caller still owns presentation: after success, present the acquired
 * surface if any, then call view.frame_presented(). Failure never ACKs a frame.
 * On GPU submission failure, recover/tear down the GPU path before retiring
 * source images; no completion guarantee is made for a failed submission.
 * Destroy target/context before the underlying crtgfx device, as usual.
 */
inline crtui_result compose_gpu(WebView& view, crtui_context* ui, crtui_window window,
                                GrDirectContext* context, SkSurface* target) {
  if (!ui || !context || !target || target->recordingContext() != context)
    return CRTUI_ERROR_INVALID_ARGUMENT;
  if (context->abandoned()) return CRTUI_ERROR_IO;
  if (!view.has_frame()) return CRTUI_WOULD_BLOCK;
  auto provider = view.surface_provider();
  crtui_result result = crtui_skia_compose(ui, window, target, &provider);
  // Even a partially composed scene may have queued source-image reads.
  context->flush(target);
  bool submitted = context->submit(GrSyncCpu::kYes);
  return !submitted || context->abandoned() ? CRTUI_ERROR_IO : result;
}

}  // namespace crtweb
