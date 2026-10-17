#include "crtgfx/skia_shared.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#if defined(CRTGFX_HAVE_METAL)
#include "../src/arch/macos/metal_shared_native.h"
#include <chrono>
#include <thread>
#endif

namespace {
int failures = 0;
bool check(bool ok, const char* name) {
  if (!ok) { fprintf(stderr, "crtgfx_skia_shared_image_test: FAILED %s\n", name); ++failures; }
  return ok;
}
bool run(crtgfx_gpu_device* a, crtgfx_gpu_device* b) {
  auto producer_context = crtgfx_skia_make_gpu_context(a);
  auto consumer_context = crtgfx_skia_make_gpu_context(b);
  if (!check(producer_context && consumer_context, "independent Ganesh contexts")) return false;
  std::unique_ptr<crtgfx_skia_shared_image> producer, consumer;
  if (!check(crtgfx_skia_shared_create(a, 128, 96, &producer) == CRTGFX_OK, "create exportable image")) return false;
  int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
  if (!check(producer->export_handles(fds) == CRTGFX_OK, "native resource handles")) return false;
  auto desc = producer->description();
  const unsigned handles = crtgfx_shared_image_handle_count(&desc);
  check(handles == CRTGFX_SHARED_IMAGE_HANDLE_COUNT || handles == CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT
      || handles == CRTGFX_SHARED_IMAGE_METAL_HANDLE_COUNT,
      "the backend's descriptor count");
  for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i)
    check((i < handles) == (fds[i] >= 0), "exactly the descriptor count of handles is exported");
  std::unique_ptr<crtgfx_skia_shared_image> invalid;
  auto bad = desc; bad.device_uuid[0] ^= 1;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_UNSUPPORTED && !invalid, "reject device mismatch");
  bad = desc; bad.driver_uuid[0] ^= 1;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_UNSUPPORTED && !invalid, "reject driver mismatch");
  bad = desc; ++bad.allocation_size;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_INVALID_ARGUMENT && !invalid, "reject allocation shape mismatch");
  bool imported = check(crtgfx_skia_shared_import(b, desc, fds, &consumer) == CRTGFX_OK, "import independent logical device");
  for (unsigned i = 0; i < handles; ++i) {
    check(fcntl(fds[i], F_GETFD) >= 0, "import borrows caller handles"); close(fds[i]);
  }
  if (!imported) return false;
  check(consumer->wait_ready(0) == CRTGFX_ERROR_TIMEOUT, "unsignaled native ready fence times out");
  check(producer->wait_released(0) == CRTGFX_ERROR_TIMEOUT, "unsignaled native release fence times out");
  sk_sp<SkImage> unavailable;
  check(consumer->begin_read(consumer_context.get(), 0, &unavailable) == CRTGFX_ERROR_TIMEOUT && !unavailable,
      "timeout grants no GPU ownership");
  auto target = crtgfx_skia_make_gpu_offscreen_surface(consumer_context.get(), 128, 96);
  if (!check(target != nullptr, "consumer GPU target")) return false;
  for (unsigned i = 0; i < 20 && failures == 0; ++i) {
    sk_sp<SkSurface> surface;
    if (!check(producer->begin_write(producer_context.get(), 2000, &surface) == CRTGFX_OK, "acquire producer ownership")) break;
    bool green = (i & 1) == 0;
    surface->getCanvas()->clear(green ? SK_ColorGREEN : SK_ColorMAGENTA);
    if (!check(producer->end_write(producer_context.get(), surface.get()) == CRTGFX_OK, "submit GPU write and ready signal")) break;
    surface.reset();
    sk_sp<SkImage> image;
    if (!check(consumer->begin_read(consumer_context.get(), 2000, &image) == CRTGFX_OK, "native fence/semaphore acquisition")) break;
    check(image->isTextureBacked(), "import is texture backed, no CPU fallback");
    sk_sp<SkSurface> blocked;
    check(producer->begin_write(producer_context.get(), 0, &blocked) == CRTGFX_ERROR_TIMEOUT && !blocked,
        "no overwrite before native release");
    target->getCanvas()->drawImage(image, 0, 0);
    consumer_context->flushAndSubmit(target.get(), GrSyncCpu::kYes);
    uint8_t pixel[4] {};
    auto info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    check(target->readPixels(info, pixel, 4, 40, 40)
        && pixel[0] == (green ? 0 : 255) && pixel[1] == (green ? 255 : 0)
        && pixel[2] == (green ? 0 : 255) && pixel[3] == 255, "shared GPU pixel identity");
    check(consumer->end_read(consumer_context.get()) == CRTGFX_OK, "release ownership and signal native fence");
    image.reset();
    check(consumer->begin_read(consumer_context.get(), 0, &unavailable) == CRTGFX_ERROR_TIMEOUT && !unavailable,
        "consumed ready fence cannot authorize another read epoch");
    check(producer->wait_released(2000) == CRTGFX_OK, "producer observes consumer GPU completion");
  }
  consumer_context->flushAndSubmit(GrSyncCpu::kYes);
  target.reset(); consumer.reset(); producer.reset();
  return failures == 0;
}
bool retained_export(crtgfx_gpu_device* a, crtgfx_gpu_device* b) {
  auto producer_context = crtgfx_skia_make_gpu_context(a), consumer_context = crtgfx_skia_make_gpu_context(b);
  std::unique_ptr<crtgfx_skia_shared_image> producer, consumer;
  if (!check(crtgfx_skia_shared_create(a, 64, 32, &producer) == CRTGFX_OK, "retained export create")) return false;
  sk_sp<SkSurface> surface;
  if (!check(producer->begin_write(producer_context.get(), 2000, &surface) == CRTGFX_OK, "retained export begin")) return false;
  surface->getCanvas()->clear(SK_ColorBLUE);
  if (!check(producer->end_write(producer_context.get(), surface.get()) == CRTGFX_OK
      && producer->wait_ready(2000) == CRTGFX_OK, "retained export write")) return false;
  surface.reset();
  int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT];
  auto desc = producer->description();
  if (!check(producer->export_handles(fds) == CRTGFX_OK, "retained export handles")) return false;
  producer.reset(); // The exported capability owns the native references, not this wrapper.
  bool ok = check(crtgfx_skia_shared_import(b, desc, fds, &consumer) == CRTGFX_OK, "import after producer wrapper destruction");
  for (unsigned i = 0; i < crtgfx_shared_image_handle_count(&desc); ++i) close(fds[i]);
  if (!ok) return false;
  sk_sp<SkImage> image;
  if (!check(consumer->begin_read(consumer_context.get(), 2000, &image) == CRTGFX_OK, "retained native ready signal")) return false;
  auto target = crtgfx_skia_make_gpu_offscreen_surface(consumer_context.get(), 64, 32);
  if (!check(target != nullptr, "retained GPU target")) return false;
  target->getCanvas()->drawImage(image, 0, 0); consumer_context->flushAndSubmit(target.get(), GrSyncCpu::kYes);
  uint8_t pixel[4] {};
  check(target->readPixels(SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType), pixel, 4, 10, 10)
      && pixel[0] == 255 && !pixel[1] && !pixel[2] && pixel[3] == 255, "retained native pixels");
  check(consumer->end_read(consumer_context.get()) == CRTGFX_OK && consumer->wait_released(2000) == CRTGFX_OK,
        "retained export release");
  image.reset(); target.reset(); consumer.reset();
  return failures == 0;
}
}

extern "C" int main(int argc, char** argv) {
  bool required = argc == 2 && !strcmp(argv[1], "--require-gpu");
  crtgfx_gpu_device *a = nullptr, *b = nullptr;
  crtgfx_shared_image_desc identity {};
  if (crtgfx_gpu_device_create(0, &a) != CRTGFX_OK || crtgfx_skia_shared_query(a, &identity) != CRTGFX_OK) {
    if (a) crtgfx_gpu_device_release(a);
    printf("crtgfx_skia_shared_image_test: %s -- external memory/semaphore/fence unavailable\n", required ? "FAILED" : "skipped");
    return required ? 1 : 77;
  }
  bool ok = check(crtgfx_gpu_device_create(0, &b) == CRTGFX_OK, "second logical device") && run(a, b);
  if (ok && identity.backend == CRTGFX_GPU_BACKEND_METAL) ok = retained_export(a, b);
#if defined(CRTGFX_HAVE_METAL)
  for (unsigned i = 0; i < 100 && crtgfx_metal_share_live_offers(); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  ok = check(crtgfx_metal_share_live_offers() == 0, "all capability leases revoke native offers and threads") && ok;
#endif
  if (b) crtgfx_gpu_device_release(b);
  crtgfx_gpu_device_release(a);
  if (!ok) return 1;
  printf("crtgfx_skia_shared_image_test: ok epochs=20 memory=shared-gpu sync=%s pixels=pass backend=%u handles=%u\n",
      identity.backend == CRTGFX_GPU_BACKEND_D3D12 ? "native-shared-fence" :
      identity.backend == CRTGFX_GPU_BACKEND_METAL ? "native-shared-event" : "native-fence-and-semaphore",
      identity.backend, crtgfx_shared_image_handle_count(&identity));
  return 0;
}
