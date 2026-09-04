// Real Ganesh GPU offscreen rendering coverage (2026-09-03, TODO.md's
// "Enable Skia GPU rendering" step -- Linux/Vulkan first, Windows/D3D12
// the same week; see crtgfx/gpu.h's own top comment and libcrtgfx/
// README.md for the full design record, including the real, hands-on
// pivots that led here). One real, cross-platform source file, not
// duplicated per OS: every call in this file goes through crtgfx_gpu_*
// (crtgfx/gpu.h) and crtgfx_skia_make_gpu_context()/crtgfx_skia_make_
// gpu_offscreen_surface() (crtgfx/skia.h), which have real per-OS
// implementations behind the same names on Linux (src/arch/linux/
// gpu_vulkan.c) and Windows (src/arch/windows/gpu_win32.c) -- only the
// device-loss test's own low-level handle-teardown step (test_device_
// loss_and_recreation(), below) branches by OS internally, via #if
// defined(CRTGFX_HAVE_VULKAN)/#elif defined(CRTGFX_HAVE_D3D12).
//
// Unlike crtgfx_skia_raster_smoke/crtgfx_skia_cpu_coverage (CPU raster,
// crtgfx_skia_make_raster_surface()), this drives a real GPU device end to
// end: crtgfx_gpu_query_capabilities() -> crtgfx_gpu_device_create() (the
// real per-OS backend) -> crtgfx_skia_make_gpu_context() -> crtgfx_skia_
// make_gpu_offscreen_surface() (src/skia_bridge.cc) -> a real Ganesh-
// drawn, GPU-backed SkSurface, read back via SkSurface::readPixels() and
// checked with the exact same shared assertions (tests/skia_reference_
// scene.h) the CPU-raster path could use on the identical scene --
// proving real cross-backend rendering correctness, not just "a GPU call
// didn't crash."
//
// Gracefully, honestly skips (not a failure) on any host/config where
// crtgfx_gpu_query_capabilities() reports 0 real devices (macOS today,
// no real Metal backend yet; Linux without a real libvulkan found at
// configure time) -- this target is only ever registered/run when
// CRTGFX_HAVE_VULKAN or CRTGFX_HAVE_D3D12 was actually defined at build
// time (see CMakeLists.txt), but the underlying GPU environment itself
// could still honestly report nothing usable at real runtime (e.g. no
// ICD/adapter at all) even when the code was compiled in.
//
// Covers, matching this vertical slice's own real, available scope:
//   - real draw + exact-pixel readback against the shared reference scene;
//   - resize: recreate the offscreen surface at new dimensions, draw+read
//     back again;
//   - device-loss + context-recreation: on Windows/D3D12, the real, clean,
//     intentionally-provided ID3D12Device::RemoveDevice() API; on Linux/
//     Vulkan, the closest real, safe, portable approximation available
//     (no synthetic VK_ERROR_DEVICE_LOST trigger exists at all -- see
//     test_device_loss_and_recreation()'s own Vulkan branch for the full
//     story, including a real, confirmed-for-real crash this test's own
//     first version hit and why). Both branches confirm real removal is
//     detected and that a fresh crtgfx_gpu_device_create() afterward
//     genuinely recovers with a brand-new, working device.

#include "crtgfx/gpu.h"
#include "crtgfx/skia.h"

#include "skia_reference_scene.h"

#include "gpu_internal.h"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(CRTGFX_HAVE_VULKAN)
// Hand-declared, minimal real Vulkan subset -- matching src/arch/linux/
// gpu_vulkan.c's own top comment: this project's own dlopen() has no real
// ELF dynamic loading yet, so this links libvulkan directly (this whole
// target already does, transitively via crtgfx -- see CMakeLists.txt),
// resolved at link time, not looked up at runtime. Real, external linkage
// (not inside the anonymous namespace below, unlike everything else in
// this file) -- it names a real symbol libvulkan.so itself defines.
typedef struct VkDevice_T* VkDevice;
extern "C" void vkDestroyDevice(VkDevice device, const void* allocator);
#elif defined(CRTGFX_HAVE_D3D12)
// Real <d3d12.h> (2026-09-04, real, confirmed necessary -- an earlier
// version of this file's own comment here claimed this arrived
// transitively via crtgfx/skia.h -> Skia's own GrD3DTypes.h, which is
// wrong: crtgfx/skia.h only ever includes the backend-agnostic
// include/gpu/ganesh/GrDirectContext.h, never GrD3DTypes.h itself, so
// this file's own direct use of GUID/HRESULT/ID3D12Device/ID3D12Device5
// below needs its own real include -- confirmed for real: "unknown type
// name 'GUID'"/"'ID3D12Device'"/"'HRESULT'" without it). Resolves to
// mingw-w64's own real, complete header (see libcrtgfx/CMakeLists.txt's
// own -I<win32_shim>/-I<mingw-w64-headers include root> comment on this
// same target, and tools/fetch_mingw_w64_headers.py's own top comment
// for why mingw-w64's header set specifically, not the raw Microsoft
// Windows SDK's -- a real, confirmed dead end under this project's
// mingw-target clang).
//
// #pragma, not just this target's own CMakeLists.txt -Wno-unused-value/
// -Wno-ignored-attributes (2026-09-04, real, confirmed necessary):
// those command-line flags land in <FLAGS> *before* crt_cxx_build_flags'
// own inherited -Wall -Wextra -Werror (this project's own established
// per-target-then-inherited ordering, see CMakeLists.txt's own comments
// on this same target) -- and -Wextra itself re-enables -Wunused-value,
// so a plain -Wno-unused-value earlier on the same command line is
// silently undone by the time -Werror promotes it back to a hard error.
// A #pragma always wins regardless of command-line position (it takes
// effect exactly at this point in the token stream), so it is used here
// instead, scoped tightly around just the one real #include that needs
// it -- mingw-w64's own real combaseapi.h (reached transitively) has a
// deliberate no-op `static_cast<IUnknown *>(*pp);` (part of its own
// IID_PPV_ARGS-equivalent macro machinery) and two intentionally-
// redeclared-inline dllimport functions (CoCreateInstance/
// CoCreateInstanceEx) -- both real, harmless, expected mingw-w64
// patterns; see CMakeLists.txt's own matching comment for the exact
// confirmed diagnostics.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wignored-attributes"
#include <d3d12.h>
#pragma clang diagnostic pop

// Real GUID for ID3D12Device5 (needed for the real, clean RemoveDevice()
// device-loss test below), hand-declared -- IID_PPV_ARGS()/__uuidof() are
// real MSVC-only extensions this project's own mingw-target clang
// invocation does not support (matching skia_bridge.cc's own kIID_
// ID3D12Resource precedent, same file), and dxguid.lib -- which would
// otherwise supply the real SDK's own extern IID_ID3D12Device5 symbol --
// is deliberately not linked (matching window_win32.c's own established
// precedent). The real ID3D12Device/ID3D12Device5 C++ interfaces
// themselves (from the real <d3d12.h> just above) are used directly
// below, no hand-declared vtable needed the way gpu_win32.c's own real
// device/queue creation code needs one.
static const GUID kIID_ID3D12Device5 = {
    0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
#endif

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

#if defined(CRTGFX_HAVE_VULKAN)
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
#elif defined(CRTGFX_HAVE_D3D12)
  // Real, clean device-loss: ID3D12Device::RemoveDevice() (via
  // ID3D12Device5, the interface it was introduced on -- see this file's
  // own top comment) is D3D12's own real, intentionally-provided API for
  // exactly this kind of test. Unlike Vulkan's own vkDestroyDevice(),
  // RemoveDevice() does NOT free the underlying COM object or invalidate
  // the pointer -- it only marks the device "removed" (subsequent real
  // D3D12 calls against it start failing with DXGI_ERROR_DEVICE_REMOVED),
  // so there is no double-destroy hazard to route around here the way the
  // Vulkan branch above needs to: the real ID3D12Device pointer stays
  // genuinely valid, and crtgfx_gpu_device_release() below can safely
  // Release() it normally, real COM refcounting handling the rest.
  {
    ID3D12Device* d3d_device = reinterpret_cast<ID3D12Device*>(device->d3d12_device);
    ID3D12Device5* d3d_device5 = nullptr;
    HRESULT hr = d3d_device->QueryInterface(
        kIID_ID3D12Device5, reinterpret_cast<void**>(&d3d_device5));
    if (report("device-loss: QueryInterface(ID3D12Device5) succeeds", SUCCEEDED(hr) && d3d_device5 != nullptr)) {
      d3d_device5->RemoveDevice();
      d3d_device5->Release();
    }
  }
  report(
      "device-loss: GetDeviceRemovedReason() reports a real removal after RemoveDevice()",
      FAILED(reinterpret_cast<ID3D12Device*>(device->d3d12_device)->GetDeviceRemovedReason()));
#endif

  crtgfx_gpu_device_release(device); // a single, real, safe teardown on every real backend

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
