#include "crtgfx/skia_shared.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

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
  if (!check(producer->export_handles(fds) == CRTGFX_OK, "five native resource handles")) return false;
  auto desc = producer->description();
  std::unique_ptr<crtgfx_skia_shared_image> invalid;
  auto bad = desc; bad.device_uuid[0] ^= 1;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_UNSUPPORTED && !invalid, "reject device mismatch");
  bad = desc; bad.driver_uuid[0] ^= 1;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_UNSUPPORTED && !invalid, "reject driver mismatch");
  bad = desc; ++bad.allocation_size;
  check(crtgfx_skia_shared_import(b, bad, fds, &invalid) == CRTGFX_ERROR_INVALID_ARGUMENT && !invalid, "reject allocation shape mismatch");
  bool imported = check(crtgfx_skia_shared_import(b, desc, fds, &consumer) == CRTGFX_OK, "import independent logical device");
  for (unsigned i = 0; i < CRTGFX_SHARED_IMAGE_HANDLE_COUNT; ++i) {
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
  if (b) crtgfx_gpu_device_release(b);
  crtgfx_gpu_device_release(a);
  if (!ok) return 1;
  printf("crtgfx_skia_shared_image_test: ok epochs=20 memory=shared-gpu sync=native-fence-and-semaphore pixels=pass\n");
  return 0;
}
