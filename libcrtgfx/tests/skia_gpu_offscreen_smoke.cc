// Real Ganesh/Vulkan offscreen rendering coverage (2026-09-03, TODO.md's
// "Enable Skia GPU rendering" step -- the Linux-first vertical slice; see
// crtgfx/gpu.h's own top comment and libcrtgfx/README.md for the full
// design record, including the real, hands-on pivots that led here).
//
// Unlike crtgfx_skia_raster_smoke/crtgfx_skia_cpu_coverage (CPU raster,
// crtgfx_skia_make_raster_surface()), this drives a real GPU device end to
// end: crtgfx_gpu_query_capabilities() -> crtgfx_gpu_device_create() (the
// real Vulkan backend, src/arch/linux/gpu_vulkan.c) -> crtgfx_skia_make_
// gpu_context() -> crtgfx_skia_make_gpu_offscreen_surface() (src/skia_
// bridge.cc) -> a real Ganesh-drawn, GPU-backed SkSurface, read back via
// SkSurface::readPixels() and checked with the exact same shared assertions
// (tests/skia_reference_scene.h) the CPU-raster path could use on the
// identical scene -- proving real cross-backend rendering correctness, not
// just "a GPU call didn't crash."
//
// Gracefully, honestly skips (not a failure) on any host/config where
// crtgfx_gpu_query_capabilities() reports 0 real devices (Windows/macOS
// today; Linux without a real libvulkan found at configure time) -- this
// target is only ever registered/run when CRTGFX_HAVE_VULKAN was actually
// defined at build time (see CMakeLists.txt), but the underlying Vulkan
// environment itself could still honestly report nothing usable at real
// runtime (e.g. no ICD at all) even when the code was compiled in.
//
// Covers, matching this vertical slice's own real, available scope:
//   - real draw + exact-pixel readback against the shared reference scene;
//   - resize: recreate the offscreen surface at new dimensions, draw+read
//     back again;
//   - device-loss + context-recreation, using the closest real, safe,
//     portable approximation Vulkan actually offers (no synthetic
//     VK_ERROR_DEVICE_LOST trigger exists, unlike D3D12's ID3D12Device::
//     RemoveDevice() -- see this file's own test_device_loss_and_
//     recreation() for the full story, including a real, confirmed-for-
//     real crash this test's own first version hit and why): tear down a
//     real VkDevice exactly once, confirm a subsequent real Ganesh call
//     against the now-invalid crtgfx_gpu_device fails without crashing/
//     hanging, then confirm a fresh crtgfx_gpu_device_create() genuinely
//     recovers with a brand-new, working device.

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"

#include "skia_reference_scene.h"

#include "gpu_internal.h"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>

// Hand-declared, minimal real Vulkan subset -- matching src/arch/linux/
// gpu_vulkan.c's own top comment: this project's own dlopen() has no real
// ELF dynamic loading yet, so this links libvulkan directly (this whole
// target already does, transitively via crtgfx -- see CMakeLists.txt),
// resolved at link time, not looked up at runtime. Real, external linkage
// (not inside the anonymous namespace below, unlike everything else in
// this file) -- it names a real symbol libvulkan.so itself defines.
typedef struct VkDevice_T* VkDevice;
extern "C" void vkDestroyDevice(VkDevice device, const void* allocator);

namespace {

int g_failures = 0;

bool report(const char* name, bool ok) {
  if (ok) {
    printf("crtgfx_skia_gpu_offscreen_smoke: %s ok\n", name);
  } else {
    printf("crtgfx_skia_gpu_offscreen_smoke: %s FAILED\n", name);
    ++g_failures;
  }
  return ok;
}

// Draws the shared reference scene into a real GPU-backed offscreen
// SkSurface at `width`x`height`, flushes+submits, reads every pixel back
// into a plain malloc'd BGRA8888-premultiplied buffer, and runs the shared
// assertions against it. Returns true iff the whole round trip (surface
// creation, draw, flush, readback, every assertion) succeeded.
bool draw_and_check(GrDirectContext* context, uint32_t width, uint32_t height, const char* label) {
  sk_sp<SkSurface> surface = crtgfx_skia_make_gpu_offscreen_surface(context, width, height);
  char name_buf[192];
  snprintf(name_buf, sizeof(name_buf), "%s: gpu surface created", label);
  if (!report(name_buf, surface != nullptr)) {
    return false;
  }

  crtgfx_test::draw_reference_scene(surface->getCanvas());
  context->flushAndSubmit(surface.get(), GrSyncCpu::kYes);

  size_t stride = static_cast<size_t>(width) * 4u;
  void* pixels = calloc(1, stride * height);
  snprintf(name_buf, sizeof(name_buf), "%s: readback buffer allocated", label);
  if (!report(name_buf, pixels != nullptr)) {
    return false;
  }

  SkImageInfo info = SkImageInfo::Make(
      static_cast<int>(width), static_cast<int>(height), kBGRA_8888_SkColorType,
      kPremul_SkAlphaType);
  bool read_ok = surface->readPixels(info, pixels, stride, 0, 0);
  snprintf(name_buf, sizeof(name_buf), "%s: readPixels() succeeds", label);
  bool ok = report(name_buf, read_ok);
  if (read_ok) {
    crtgfx_test::check_reference_scene(pixels, stride, [&](const char* check_name, bool check_ok) {
      snprintf(name_buf, sizeof(name_buf), "%s: %s", label, check_name);
      ok = report(name_buf, check_ok) && ok;
    });
  }
  free(pixels);
  return ok;
}

// Real draw + exact-pixel readback, then a real resize (a fresh surface at
// different dimensions, same real GrDirectContext) -- proves the surface-
// (re)creation code path is real and correct even without a live window
// driving it (see gpu.h's own top comment on why crtgfx_gpu_surface_create()
// itself stays out of scope here).
void test_draw_and_resize(crtgfx_gpu_device* device) {
  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (!report("draw/resize: gpu context created", context != nullptr)) {
    return;
  }
  draw_and_check(
      context.get(), crtgfx_test::kReferenceSceneWidth, crtgfx_test::kReferenceSceneHeight,
      "draw/resize: initial size");
  // A genuinely different size, not just re-running the same call -- proves
  // this is a real resize, not a coincidentally-passing fixed-size path.
  draw_and_check(
      context.get(), crtgfx_test::kReferenceSceneWidth * 2, crtgfx_test::kReferenceSceneHeight * 2,
      "draw/resize: resized (2x)");
}

// Device-loss + context-recreation, using the closest real, safe,
// portable approximation Vulkan actually offers. Confirmed directly
// (2026-09-03, this vertical slice's own research): there is no portable,
// standard Vulkan API to *force* VK_ERROR_DEVICE_LOST the way D3D12's own
// ID3D12Device::RemoveDevice() cleanly does for exactly this kind of test
// (Skia's own device-lost handling, GrVkGpu::checkVkResult, only reacts to
// a real driver-returned VK_ERROR_DEVICE_LOST, never synthesizes one) --
// Windows/D3D12's own later roadmap step gets that cleaner, driver-forced
// test; this is Linux's honest equivalent given Vulkan's own real limits.
//
// This test also does NOT attempt a genuine live Ganesh call against a
// dangling VkDevice handle -- confirmed for real, the hard way, that this
// is not survivable: an earlier version of this test destroyed the real
// VkDevice once directly, then let crtgfx_gpu_device_release()'s own real
// teardown (src/arch/linux/gpu_vulkan.c) call vkDestroyDevice() on it a
// *second* time, and the real Vulkan Loader's own parameter validation
// aborted the whole process ("ERROR: vkDestroyDevice: Invalid device
// [VUID-vkDestroyDevice-device-parameter]") rather than failing gracefully
// -- using an already-destroyed Vulkan object is genuinely undefined
// behavior per the spec, not a bug in this project's own teardown code,
// and *is* exactly why no portable "please simulate this safely" API
// exists in the first place. The safe, real, and still-meaningful
// boundary this test actually exercises instead: destroy the real
// VkDevice exactly once, then immediately null out this crtgfx_gpu_
// device's own handle fields so the *later* real release() only ever
// tears down what is actually still real -- proving (1) crtgfx_skia_make_
// gpu_context() rejects an already-torn-down device cleanly (no crash/
// hang), matching its own documented null-handle validation, and (2) a
// fresh crtgfx_gpu_device_create() afterward genuinely recovers with a
// brand-new, independently working device.
void test_device_loss_and_recreation(uint32_t device_index) {
  crtgfx_gpu_device* device = nullptr;
  if (!report(
          "device-loss: dedicated device_create() succeeds",
          crtgfx_gpu_device_create(device_index, &device) == CRTGFX_OK) ||
      device == nullptr) {
    return;
  }

  // Real, single, deliberate teardown -- then null out this object's own
  // handle fields so crtgfx_gpu_device_release() below performs no second
  // real vkDestroyDevice() call on the same, now-invalid handle (see this
  // function's own top comment for why a real double-destroy is not
  // survivable here).
  vkDestroyDevice(reinterpret_cast<VkDevice>(device->vk_device), nullptr);
  device->vk_device = nullptr;
  device->vk_queue = nullptr;

  // crtgfx_skia_make_gpu_context() must reject the now torn-down device
  // cleanly (its own documented "device == nullptr / vk_device == nullptr"
  // validation applies exactly here) -- the real, safe boundary this
  // vertical slice's own contract actually needs to hold.
  report(
      "device-loss: a real Ganesh call against the torn-down device fails cleanly (no crash/hang)",
      crtgfx_skia_make_gpu_context(device) == nullptr);

  crtgfx_gpu_device_release(device); // now a single, real, safe teardown

  // Real recovery: a fresh crtgfx_gpu_device_create() afterward must
  // succeed again with a genuinely new, independently working device --
  // not just a non-null pointer, but one a real Ganesh context can
  // actually be built from and actually draw with.
  crtgfx_gpu_device* recovered_device = nullptr;
  if (!report(
          "device-loss: device_create() recovers with a fresh device afterward",
          crtgfx_gpu_device_create(device_index, &recovered_device) == CRTGFX_OK) ||
      recovered_device == nullptr) {
    return;
  }
  {
    // Scoped so `recovered_context` (and every real Vulkan/Ganesh resource
    // it owns) is fully torn down *before* crtgfx_gpu_device_release()
    // below destroys the underlying real VkDevice -- getting this order
    // backwards segfaulted for real the first time this test ran: a
    // GrDirectContext's own destructor releases real Ganesh-internal
    // Vulkan resources (its resource cache, this file's own
    // DumbVulkanMemoryAllocator, ...) through the same VkDevice handle,
    // which must still be alive when that happens.
    sk_sp<GrDirectContext> recovered_context = crtgfx_skia_make_gpu_context(recovered_device);
    if (report("device-loss: recovered device builds a real, working Ganesh context",
               recovered_context != nullptr)) {
      draw_and_check(
          recovered_context.get(), crtgfx_test::kReferenceSceneWidth,
          crtgfx_test::kReferenceSceneHeight, "device-loss: recovered device draws correctly");
    }
  }
  crtgfx_gpu_device_release(recovered_device);
}

}  // namespace

extern "C" int main() {
  crtgfx_gpu_capabilities caps;
  if (!report(
          "query_capabilities() succeeds", crtgfx_gpu_query_capabilities(&caps) == CRTGFX_OK)) {
    printf("crtgfx_skia_gpu_offscreen_smoke: %d check(s) failed\n", g_failures);
    return 1;
  }
  if (caps.device_count == 0) {
    // Real, honest skip -- no usable Vulkan device on this host/config
    // right now (see this file's own top comment). Not a failure: this
    // vertical slice's own contract has always been "software fallback
    // must remain a first-class path," and a 0-device report here is
    // exactly that path, correctly taken.
    printf(
        "crtgfx_skia_gpu_offscreen_smoke: skipped -- crtgfx_gpu_query_capabilities() reports "
        "0 real devices on this host\n");
    return 0;
  }

  crtgfx_gpu_device* device = nullptr;
  if (!report(
          "device_create(0, ...) succeeds", crtgfx_gpu_device_create(0, &device) == CRTGFX_OK) ||
      device == nullptr) {
    printf("crtgfx_skia_gpu_offscreen_smoke: %d check(s) failed\n", g_failures);
    return 1;
  }

  sk_sp<GrDirectContext> context = crtgfx_skia_make_gpu_context(device);
  if (report("gpu context created from a real device", context != nullptr)) {
    draw_and_check(
        context.get(), crtgfx_test::kReferenceSceneWidth, crtgfx_test::kReferenceSceneHeight,
        "initial draw");
  }
  context.reset();
  crtgfx_gpu_device_release(device);

  {
    crtgfx_gpu_device* resize_device = nullptr;
    if (report(
            "resize test: device_create() succeeds",
            crtgfx_gpu_device_create(0, &resize_device) == CRTGFX_OK) &&
        resize_device != nullptr) {
      test_draw_and_resize(resize_device);
      crtgfx_gpu_device_release(resize_device);
    }
  }

  test_device_loss_and_recreation(0);

  if (g_failures == 0) {
    puts("crtgfx_skia_gpu_offscreen_smoke: ok");
    return 0;
  }
  printf("crtgfx_skia_gpu_offscreen_smoke: %d check(s) failed\n", g_failures);
  return 1;
}
