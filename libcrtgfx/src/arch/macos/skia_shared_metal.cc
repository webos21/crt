#include "crtgfx/skia_shared.h"

#if defined(CRTGFX_HAVE_METAL)
#include "../../gpu_internal.h"
#include "metal_shared_native.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkImageGanesh.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"
#include <algorithm>
#include <cstring>

namespace {
struct Record {
  void* native = nullptr;
  ~Record() { if (native) crtgfx_metal_share_release(native); }
};

crtgfx_result identity(const crtgfx_gpu_device* device, crtgfx_shared_image_desc* out,
                      crtgfx_gpu_metal_device_view* view) {
  if (!crtgfx_gpu_metal_borrow_device(device, view)) return CRTGFX_ERROR_UNSUPPORTED;
  *out = {};
  if (!crtgfx_metal_share_identity(view->device, out->device_uuid, out->driver_uuid)) return CRTGFX_ERROR_UNSUPPORTED;
  out->version = CRTGFX_SHARED_IMAGE_VERSION; out->backend = CRTGFX_GPU_BACKEND_METAL;
  out->handle_kind = CRTGFX_SHARED_IMAGE_MACH_CAPABILITY; out->format = CRTGFX_SHARED_IMAGE_BGRA8;
  return CRTGFX_OK;
}

/* A capability has repeatable metadata plus a lease socket. The offer owns
 * actual Mach send-right references until all transferred lease copies close.
 * Each import receives independent kernel references; no image data or GPU
 * completion signal travels through this thread/socket. */
class SharedImage final : public crtgfx_skia_shared_image {
 public:
  Record record;
  crtgfx_gpu_metal_device_view device {};
  crtgfx_shared_image_desc desc {};
  GrBackendTexture texture;
  GrDirectContext* context = nullptr;
  uint64_t epochs = 0;
  bool producer = false, writing = false, reading = false, poisoned = false;
  ~SharedImage() override {
    if (record.native && epochs) crtgfx_metal_share_wait(record.native, producer ? 1 : 0, epochs, 2000);
  }
  const crtgfx_shared_image_desc& description() const override { return desc; }
  crtgfx_result export_handles(int out[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) out[i] = -1;
    if (!producer) return CRTGFX_ERROR_INVALID_ARGUMENT;
    return crtgfx_metal_share_export(record.native, out) ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  bool bind(GrDirectContext* ctx) {
    if (!ctx || ctx->abandoned() || poisoned || (context && context != ctx)) return false;
    context = ctx; return true;
  }
  void make_texture() {
    GrMtlTextureInfo info;
    info.fTexture.retain(crtgfx_metal_share_texture(record.native));
    texture = GrBackendTextures::MakeMtl(static_cast<int>(desc.width), static_cast<int>(desc.height),
                                         skgpu::Mipmapped::kNo, info);
  }
  crtgfx_result wait(int ready, uint64_t value, uint32_t ms) {
    return crtgfx_metal_share_wait(record.native, ready, value, ms) ? CRTGFX_OK : CRTGFX_ERROR_TIMEOUT;
  }
  bool enqueue(int ready, uint64_t value, bool signal) {
    if (crtgfx_metal_share_enqueue(record.native, device.command_queue, ready, value, signal)) return true;
    poisoned = true; return false;
  }
  crtgfx_result wait_ready(uint32_t ms) override { return wait(1, producer ? std::max<uint64_t>(epochs, 1) : epochs + 1, ms); }
  crtgfx_result wait_released(uint32_t ms) override { return wait(0, std::max<uint64_t>(epochs, 1), ms); }
  crtgfx_result begin_write(GrDirectContext* ctx, uint32_t ms, sk_sp<SkSurface>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (!producer || reading || writing || !bind(ctx) || epochs == UINT64_MAX) return CRTGFX_ERROR_INVALID_ARGUMENT;
    if (epochs) {
      crtgfx_result result = wait(0, epochs, ms);
      if (result != CRTGFX_OK) return result;
      if (!enqueue(0, epochs, false)) return CRTGFX_ERROR_HOST;
    }
    make_texture();
    *out = SkSurfaces::WrapBackendTexture(ctx, texture, kTopLeft_GrSurfaceOrigin, 1, kBGRA_8888_SkColorType,
                                         nullptr, nullptr);
    writing = *out != nullptr;
    return writing ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result end_write(GrDirectContext* ctx, SkSurface* surface) override {
    if (!writing || !surface || surface->recordingContext() != ctx || !bind(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush(surface); writing = false;
    if (!ctx->submit(GrSyncCpu::kNo) || !enqueue(1, epochs + 1, true)) { poisoned = true; return CRTGFX_ERROR_HOST; }
    ++epochs; return CRTGFX_OK;
  }
  crtgfx_result begin_read(GrDirectContext* ctx, uint32_t ms, sk_sp<SkImage>* out) override {
    if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
    out->reset();
    if (producer || reading || writing || !bind(ctx) || epochs == UINT64_MAX) return CRTGFX_ERROR_INVALID_ARGUMENT;
    crtgfx_result result = wait(1, epochs + 1, ms);
    if (result != CRTGFX_OK) return result;
    ++epochs;
    if (!enqueue(1, epochs, false)) return CRTGFX_ERROR_HOST;
    make_texture();
    *out = SkImages::BorrowTextureFrom(ctx, texture, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
                                     kPremul_SkAlphaType, nullptr);
    reading = *out != nullptr;
    return reading ? CRTGFX_OK : CRTGFX_ERROR_HOST;
  }
  crtgfx_result end_read(GrDirectContext* ctx) override {
    if (!reading || !bind(ctx)) return CRTGFX_ERROR_INVALID_ARGUMENT;
    ctx->flush(); reading = false;
    if (!ctx->submit(GrSyncCpu::kNo) || !enqueue(0, epochs, true)) { poisoned = true; return CRTGFX_ERROR_HOST; }
    return CRTGFX_OK;
  }
};
}

crtgfx_result crtgfx_skia_shared_query(const crtgfx_gpu_device* device, crtgfx_shared_image_desc* out) {
  if (!device || !out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  *out = {};
  crtgfx_gpu_metal_device_view view {};
  crtgfx_result result = identity(device, out, &view);
  if (result != CRTGFX_OK) return result;
  Record probe; probe.native = crtgfx_metal_share_create(view.device, 16, 16);
  crtgfx_metal_share_ticket ticket {};
  void* offer = probe.native ? crtgfx_metal_share_offer(probe.native, &ticket) : nullptr;
  if (!offer) { *out = {}; return CRTGFX_ERROR_UNSUPPORTED; }
  crtgfx_metal_share_offer_release(offer);
  return CRTGFX_OK;
}
crtgfx_result crtgfx_skia_shared_create(const crtgfx_gpu_device* device, uint32_t width, uint32_t height,
    std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !width || !height || width > CRTGFX_SHARED_IMAGE_MAX_EXTENT || height > CRTGFX_SHARED_IMAGE_MAX_EXTENT)
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  std::unique_ptr<SharedImage> image(new SharedImage);
  crtgfx_result result = identity(device, &image->desc, &image->device);
  if (result != CRTGFX_OK) return result;
  image->record.native = crtgfx_metal_share_create(image->device.device, width, height);
  if (!image->record.native) return CRTGFX_ERROR_UNSUPPORTED;
  image->producer = true; image->desc.width = width; image->desc.height = height;
  image->desc.allocation_size = crtgfx_metal_share_allocation(image->record.native);
  if (!crtgfx_shared_image_desc_valid(image->desc)) return CRTGFX_ERROR_UNSUPPORTED;
  *out = std::move(image); return CRTGFX_OK;
}
crtgfx_result crtgfx_skia_shared_import(const crtgfx_gpu_device* device, const crtgfx_shared_image_desc& desc,
    const int* fds, std::unique_ptr<crtgfx_skia_shared_image>* out) {
  if (!out) return CRTGFX_ERROR_INVALID_ARGUMENT;
  out->reset();
  if (!device || !fds || !crtgfx_shared_image_desc_valid(desc)) return CRTGFX_ERROR_INVALID_ARGUMENT;
  if (desc.backend != CRTGFX_GPU_BACKEND_METAL) return CRTGFX_ERROR_UNSUPPORTED;
  crtgfx_shared_image_desc local {};
  std::unique_ptr<SharedImage> image(new SharedImage);
  crtgfx_result result = identity(device, &local, &image->device);
  if (result != CRTGFX_OK) return result;
  if (memcmp(desc.device_uuid, local.device_uuid, 16) || memcmp(desc.driver_uuid, local.driver_uuid, 16))
    return CRTGFX_ERROR_UNSUPPORTED;
  image->record.native = crtgfx_metal_share_import(image->device.device, fds);
  if (!image->record.native) return CRTGFX_ERROR_INVALID_ARGUMENT;
  if (!crtgfx_metal_share_shape(image->record.native, desc.width, desc.height, desc.allocation_size))
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  image->desc = desc; *out = std::move(image); return CRTGFX_OK;
}
#endif
