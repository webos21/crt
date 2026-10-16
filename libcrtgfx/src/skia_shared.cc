#include "crtgfx/skia_shared.h"

/* Native sharing is an optional backend capability, independent of ordinary
 * Ganesh rendering. Do not rename an upload or a CPU fence as native sharing. */
#if !defined(CRTGFX_HAVE_VULKAN) && !defined(CRTGFX_HAVE_D3D12)
crtgfx_result crtgfx_skia_shared_query(const crtgfx_gpu_device* device, crtgfx_shared_image_desc* out) {
  if (!out || !device) return CRTGFX_ERROR_INVALID_ARGUMENT;
  *out = {};
  return CRTGFX_ERROR_UNSUPPORTED;
}
crtgfx_result crtgfx_skia_shared_create(const crtgfx_gpu_device* device, uint32_t width, uint32_t height,
    std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !width || !height || width > CRTGFX_SHARED_IMAGE_MAX_EXTENT || height > CRTGFX_SHARED_IMAGE_MAX_EXTENT)
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  return CRTGFX_ERROR_UNSUPPORTED;
}
crtgfx_result crtgfx_skia_shared_import(const crtgfx_gpu_device* device, const crtgfx_shared_image_desc& desc,
    const int* fds, std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !fds || !crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_INVALID_ARGUMENT;
  return CRTGFX_ERROR_UNSUPPORTED;
}
#endif
