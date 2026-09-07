/* Real Ganesh/Metal offscreen vertical slice (TODO.md's "Enable Skia GPU
 * rendering" step, 2026-09-04) -- the macOS sibling of src/arch/linux/
 * gpu_vulkan.c and src/arch/windows/gpu_win32.c (both landed the same
 * week). See crtgfx/gpu.h's own top comment and libcrtgfx/README.md for
 * the full design record.
 *
 * Unlike either sibling, this file needs no hand-rolled vtable/struct
 * layout at all: Metal's real device-level C API (`MTLCreateSystemDefault
 * Device`/`MTLCopyAllDevices`) and the handful of Objective-C messages
 * this file sends (`count`/`objectAtIndex:`/`retain`/`release`/
 * `newCommandQueue`) are driven the same way window_cocoa.c already
 * drives AppKit/Foundation -- plain C calling the Objective-C runtime's
 * own C ABI (`objc_msgSend`/`sel_registerName`) directly, matching that
 * file's own established convention and this project's consistent no-
 * host-SDK-header policy (gpu_vulkan.c's own hand-rolled Vulkan subset,
 * gpu_win32.c's own hand-declared D3D12/DXGI COM surface). This file
 * itself never `#import`s a real Apple header at all -- every type/
 * function/selector below is a real, stable, public part of Metal's
 * documented ABI (`MTLCreateSystemDefaultDevice`/`MTLCopyAllDevices` are
 * real exported C symbols in Metal.framework; `id<MTLDevice>`/
 * `id<MTLCommandQueue>` are real Objective-C objects, opaque `id`
 * pointers here exactly like window_cocoa.c's own NSWindow/NSView/
 * CALayer handling), not third-party source needing the "third-party
 * source being ported" exception `-fcrt-real-apple-sdk`/skia_bridge.cc's
 * own Metal branch need (docs/libcrtgfx_api_policy.md's own Non-Goals
 * clause) -- confirmed for real (2026-09-04) via a standalone probe on
 * this real macOS host: `tools/crt-cc` (plain C, no special flags at all)
 * compiled and linked this exact `objc_msgSend`-driven `MTLCopyAllDevices`
 * / `count` / `objectAtIndex:` / `name` / `newCommandQueue` sequence
 * cleanly, and it ran correctly against the real default GPU ("Apple M1
 * Pro" on this machine).
 *
 * `MTLCopyAllDevices()` (real, macOS-only -- confirmed by reading Apple's
 * own MTLDevice.h documentation comment, not guessed: unlike iOS/tvOS,
 * a real Mac can have more than one real GPU, e.g. an external eGPU)
 * gives real, honest multi-device enumeration, matching gpu_vulkan.c's/
 * gpu_win32.c's own real "enumerate every real device" spirit exactly,
 * rather than only ever reporting the one default device. Each element
 * is a real, live `id<MTLDevice>` the caller does not own (Cocoa's
 * ordinary "not obtained via alloc/new/copy" convention) -- `retain`ed
 * explicitly before this function's own copy of the array is released,
 * matching window_cocoa.c's own manual (non-ARC) retain/release
 * discipline throughout.
 *
 * No custom `GrMtlMemoryAllocator`-equivalent exists at all here, unlike
 * gpu_vulkan.c's own `DumbVulkanMemoryAllocator`/gpu_win32.c's own
 * `DumbD3DMemoryAllocator` workarounds -- confirmed by reading Skia's own
 * public `include/gpu/ganesh/mtl/GrMtlBackendContext.h` directly: it
 * declares only `fDevice`/`fQueue`, no separate allocator field at all
 * (Ganesh's own Metal backend allocates through Metal's own real,
 * built-in resource/heap management instead, needing no caller-supplied
 * substitute the way Vulkan's/D3D12's own vertical slices both did). */

#include "gpu_internal.h"

#include <stdlib.h>

typedef void* id;
typedef void* SEL;
typedef unsigned long NSUInteger;

extern id objc_msgSend(id self, SEL op, ...);
extern SEL sel_registerName(const char* name);
extern id objc_getClass(const char* name);

/* Real, exported Metal.framework C functions -- MTLCreateSystemDefault
 * Device() is declared here too (not just MTLCopyAllDevices()) only for
 * documentation/completeness; this file's own device_create() below uses
 * MTLCopyAllDevices() exclusively, for the real multi-device enumeration
 * this contract's own device_index argument needs. */
extern id MTLCopyAllDevices(void);

static id metal_msg_id(id self, const char* selector_name) {
  SEL sel = sel_registerName(selector_name);
  return ((id (*)(id, SEL))objc_msgSend)(self, sel);
}

static NSUInteger metal_msg_uint(id self, const char* selector_name) {
  SEL sel = sel_registerName(selector_name);
  return ((NSUInteger (*)(id, SEL))objc_msgSend)(self, sel);
}

static id metal_msg_id_at_index(id self, const char* selector_name, NSUInteger index) {
  SEL sel = sel_registerName(selector_name);
  return ((id (*)(id, SEL, NSUInteger))objc_msgSend)(self, sel, index);
}

/* Enumerates every real Metal device on this host. `*out_devices` is the
 * real, live `NSArray` this function's own caller must `release` (via
 * metal_msg_id(array, "release")) once done -- every element inside it
 * is still owned by the array itself (Cocoa's ordinary "returned from a
 * plain accessor" convention), not individually retained here; a caller
 * that wants to keep one specific device beyond the array's own release
 * must `retain` it first (see crtgfx_gpu_metal_device_create() below).
 * Returns the real device count, or 0 if Metal is genuinely unavailable
 * on this host (`array` itself null, or a real empty array) -- matching
 * crtgfx_window_create()'s own "no usable host backend right now"
 * contract, not a crash. */
static NSUInteger metal_enumerate(id* out_devices) {
  id devices = MTLCopyAllDevices();
  NSUInteger count;
  if (devices == 0) {
    *out_devices = 0;
    return 0;
  }
  count = metal_msg_uint(devices, "count");
  if (count == 0) {
    metal_msg_id(devices, "release");
    *out_devices = 0;
    return 0;
  }
  *out_devices = devices;
  return count;
}

crtgfx_result crtgfx_gpu_metal_query_capabilities(crtgfx_gpu_capabilities* out_caps) {
  id devices = 0;
  NSUInteger count = metal_enumerate(&devices);
  if (devices != 0) {
    metal_msg_id(devices, "release");
  }
  out_caps->backend = (count > 0) ? CRTGFX_GPU_BACKEND_METAL : CRTGFX_GPU_BACKEND_NONE;
  out_caps->device_count = (uint32_t)count;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_metal_device_create(uint32_t device_index, struct crtgfx_gpu_device* device) {
  id devices = 0;
  NSUInteger count = metal_enumerate(&devices);
  id mtl_device;
  id command_queue;

  if (device_index >= count) {
    if (devices != 0) {
      metal_msg_id(devices, "release");
    }
    /* device_index out of [0, device_count) -- crtgfx_gpu_query_
     * capabilities()'s own real, current report is the only valid source
     * for that range (gpu.h's own documented contract). */
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  mtl_device = metal_msg_id_at_index(devices, "objectAtIndex:", (NSUInteger)device_index);
  /* A real, explicit retain -- `objectAtIndex:` does not transfer
   * ownership (see metal_enumerate()'s own comment), and this device
   * must outlive `devices` itself, released right below. */
  metal_msg_id(mtl_device, "retain");
  metal_msg_id(devices, "release");

  /* newCommandQueue: a real, owned (+1) reference per Cocoa's ordinary
   * "new"-prefixed method convention -- no separate retain needed. */
  command_queue = metal_msg_id(mtl_device, "newCommandQueue");
  if (command_queue == 0) {
    metal_msg_id(mtl_device, "release");
    return CRTGFX_ERROR_UNSUPPORTED;
  }

  device->mtl_device = mtl_device;
  device->mtl_command_queue = command_queue;
  return CRTGFX_OK;
}

void crtgfx_gpu_metal_device_destroy(struct crtgfx_gpu_device* device) {
  if (device->mtl_command_queue != 0) {
    metal_msg_id((id)device->mtl_command_queue, "release");
  }
  if (device->mtl_device != 0) {
    metal_msg_id((id)device->mtl_device, "release");
  }
}

/* Objective-C ABI subset from Apple's CAMetalLayer and Metal render-pass
 * documentation. Explicit argument types are required on Apple arm64;
 * sending these through a variadic declaration changes register placement.
 * Runtime verification requires a macOS host. */
typedef struct { double red, green, blue, alpha; } crtgfx_mtl_clear_color;
typedef struct { double width, height; } crtgfx_mtl_size;

static void metal_set_id(id object, const char* selector, id value) {
  ((void (*)(id, SEL, id))objc_msgSend)(object, sel_registerName(selector), value);
}

static void metal_set_uint(id object, const char* selector, NSUInteger value) {
  ((void (*)(id, SEL, NSUInteger))objc_msgSend)(object, sel_registerName(selector), value);
}

static void metal_drop_frame(struct crtgfx_gpu_surface* surface) {
  metal_msg_id(surface->mtl_command_buffer, "release");
  metal_msg_id(surface->mtl_drawable, "release");
  surface->mtl_command_buffer = NULL;
  surface->mtl_drawable = NULL;
  surface->mtl_drawable_texture = NULL;
  surface->mtl_drawable_acquired = 0;
}

crtgfx_result crtgfx_gpu_metal_surface_create(
    struct crtgfx_gpu_device* device, void* layer, uint32_t width, uint32_t height,
    struct crtgfx_gpu_surface* surface) {
  crtgfx_mtl_size size;
  if (layer == NULL || width == 0 || height == 0) return CRTGFX_ERROR_INVALID_ARGUMENT;
  size = ((crtgfx_mtl_size (*)(id, SEL))objc_msgSend)(layer, sel_registerName("drawableSize"));
  surface->device = device;
  surface->mtl_layer = metal_msg_id(layer, "retain");
  surface->width = (uint32_t)size.width;
  surface->height = (uint32_t)size.height;
  metal_set_id(layer, "setDevice:", device->mtl_device);
  metal_set_uint(layer, "setPixelFormat:", 80u); /* MTLPixelFormatBGRA8Unorm */
  ((void (*)(id, SEL, unsigned char))objc_msgSend)(layer, sel_registerName("setFramebufferOnly:"), 1);
  ((void (*)(id, SEL, unsigned char))objc_msgSend)(layer, sel_registerName("setAllowsNextDrawableTimeout:"), 1);
  return CRTGFX_OK;
}

void crtgfx_gpu_metal_surface_destroy(struct crtgfx_gpu_surface* surface) {
  metal_drop_frame(surface);
  if (surface->mtl_last_submission != NULL) {
    metal_msg_id(surface->mtl_last_submission, "waitUntilCompleted");
    metal_msg_id(surface->mtl_last_submission, "release");
  }
  metal_msg_id(surface->mtl_layer, "release");
}

crtgfx_result crtgfx_gpu_metal_surface_acquire(struct crtgfx_gpu_surface* surface, uint64_t timeout_us) {
  id pool, drawable;
  if (surface->mtl_drawable_acquired) return CRTGFX_ERROR_HOST;
  /* CAMetalLayer has a fixed ~1s timeout, not a caller-supplied deadline.
   * Refuse shorter budgets rather than silently block past them. */
  if (timeout_us < 1000000u) return CRTGFX_ERROR_UNSUPPORTED;
  if (surface->mtl_last_submission != NULL &&
      metal_msg_uint(surface->mtl_last_submission, "status") == 5u) return CRTGFX_ERROR_HOST;
  pool = metal_msg_id(metal_msg_id(objc_getClass("NSAutoreleasePool"), "alloc"), "init");
  drawable = metal_msg_id(surface->mtl_layer, "nextDrawable");
  if (drawable != NULL) {
    surface->mtl_drawable = metal_msg_id(drawable, "retain");
    surface->mtl_drawable_texture = metal_msg_id(drawable, "texture");
    surface->width = (uint32_t)metal_msg_uint(surface->mtl_drawable_texture, "width");
    surface->height = (uint32_t)metal_msg_uint(surface->mtl_drawable_texture, "height");
    surface->mtl_drawable_acquired = 1;
    surface->ganesh_wrapped = 0;
  }
  metal_msg_id(pool, "drain");
  return drawable != NULL ? CRTGFX_OK : CRTGFX_ERROR_TIMEOUT;
}

crtgfx_result crtgfx_gpu_metal_surface_clear(
    struct crtgfx_gpu_surface* surface, float r, float g, float b, float a) {
  id pool, pass, attachments, attachment, command, encoder;
  crtgfx_mtl_clear_color color = {r, g, b, a};
  if (!surface->mtl_drawable_acquired || surface->mtl_command_buffer != NULL) return CRTGFX_ERROR_HOST;
  if (surface->ganesh_wrapped) {
    /* Real, honest misuse guard (2026-09-07, the Ganesh-wrap vertical
     * slice): this frame's drawable was handed to crtgfx_skia_wrap_gpu_
     * surface() instead -- mixing the solid-color stand-in with a real
     * Ganesh-drawn frame is real, rejected misuse, matching every other
     * out-of-order guard in this file. */
    return CRTGFX_ERROR_HOST;
  }
  pool = metal_msg_id(metal_msg_id(objc_getClass("NSAutoreleasePool"), "alloc"), "init");
  command = metal_msg_id(surface->device->mtl_command_queue, "commandBuffer");
  pass = metal_msg_id(objc_getClass("MTLRenderPassDescriptor"), "renderPassDescriptor");
  attachments = metal_msg_id(pass, "colorAttachments");
  attachment = metal_msg_id_at_index(attachments, "objectAtIndexedSubscript:", 0);
  metal_set_id(attachment, "setTexture:", surface->mtl_drawable_texture);
  metal_set_uint(attachment, "setLoadAction:", 2u); /* Clear */
  metal_set_uint(attachment, "setStoreAction:", 1u); /* Store */
  ((void (*)(id, SEL, crtgfx_mtl_clear_color))objc_msgSend)(attachment, sel_registerName("setClearColor:"), color);
  encoder = ((id (*)(id, SEL, id))objc_msgSend)(command, sel_registerName("renderCommandEncoderWithDescriptor:"), pass);
  if (encoder == NULL) {
    metal_msg_id(pool, "drain");
    return CRTGFX_ERROR_HOST;
  }
  metal_msg_id(encoder, "endEncoding");
  metal_msg_id(surface->mtl_command_buffer, "release");
  surface->mtl_command_buffer = metal_msg_id(command, "retain");
  metal_msg_id(pool, "drain");
  return CRTGFX_OK;
}

/* Real drawable-size update (2026-09-07, closing the "resize on all GPU
 * hosts" gap this vertical slice's own follow-up work left open -- see
 * crtgfx/gpu.h's own crtgfx_gpu_surface_resize() comment for the full,
 * host-independent contract). Simpler than the Vulkan/D3D12 siblings --
 * a CAMetalLayer is not itself recreated on resize the way a
 * VkSwapchainKHR/IDXGISwapChain3 is; AppKit already keeps the layer's own
 * `bounds` in sync with its content view, but `drawableSize` itself is a
 * separate property real Metal apps must set explicitly on every live
 * resize (it does not track `bounds` automatically) -- this function is
 * that explicit set, real per-host work no different in kind from the
 * Vulkan/D3D12 siblings' own explicit recreation, just far cheaper
 * because Metal owns the drawable pool itself.
 *
 * Real unit mismatch this function must bridge, not just a naming detail:
 * `width`/`height` here arrive in the same units as CRTGFX_EVENT_RESIZE's
 * own payload (crtgfx/window.h) -- Cocoa *points* (crtgfx_weston_toplevel_
 * note_size()'s own callers in window_cocoa.c always pass a content
 * view's `-bounds` size directly, never multiplied by scale) -- but this
 * struct's own `width`/`height` fields (and crtgfx_gpu_surface_get_size()'s
 * own report) are real device *pixels*, matching crtgfx_gpu_metal_surface_
 * create()'s own established convention of reading the real, already-
 * scaled `-drawableSize` back from the layer rather than trusting its own
 * width/height parameters. window_cocoa.c's own crtgfx_cocoa_get_metal_
 * layer() (called once, at surface-create time) is the real source of
 * that scale (`-backingScaleFactor` at the time of that call, applied via
 * `-setContentsScale:`) -- this function reads the layer's own `-
 * contentsScale` back (unchanged since create(), since nothing yet wires
 * CRTGFX_EVENT_DPI_SCALE_CHANGED into an updated `-setContentsScale:`
 * call -- a real, separate, not-yet-wired gap, not silently assumed away)
 * rather than re-deriving it from a window this file has no handle to,
 * and applies it to `width`/`height` before comparing against or storing
 * into this struct's own real-pixel fields -- the same real conversion
 * crtgfx_cocoa_get_metal_layer() already performs, just read back instead
 * of recomputed. Reasoned-but-not-locally-verified this session (no
 * macOS hardware -- matches every other macOS-only addition's own
 * discipline, see docs/libcrtgfx_wayland_plan.md). */
crtgfx_result crtgfx_gpu_metal_surface_resize(struct crtgfx_gpu_surface* surface, uint32_t width, uint32_t height) {
  crtgfx_mtl_size size;
  double scale;
  uint32_t pixel_width;
  uint32_t pixel_height;

  if (surface->mtl_drawable_acquired) {
    /* Same real, honest misuse guard as every other out-of-order call this
     * contract already rejects. */
    return CRTGFX_ERROR_HOST;
  }

  scale = ((double (*)(id, SEL))objc_msgSend)(surface->mtl_layer, sel_registerName("contentsScale"));
  if (scale <= 0.0) scale = 1.0;
  pixel_width = (uint32_t)((double)width * scale);
  pixel_height = (uint32_t)((double)height * scale);
  if (pixel_width == surface->width && pixel_height == surface->height) {
    /* Real, cheap no-op -- see crtgfx/gpu.h's own comment on this
     * function. */
    return CRTGFX_OK;
  }

  size.width = (double)pixel_width;
  size.height = (double)pixel_height;
  ((void (*)(id, SEL, crtgfx_mtl_size))objc_msgSend)(
      surface->mtl_layer, sel_registerName("setDrawableSize:"), size);
  surface->width = pixel_width;
  surface->height = pixel_height;
  return CRTGFX_OK;
}

crtgfx_result crtgfx_gpu_metal_surface_present(struct crtgfx_gpu_surface* surface) {
  id command;
  if (!surface->mtl_drawable_acquired) return CRTGFX_ERROR_HOST;
  if (surface->ganesh_wrapped) {
    /* Real, honest misuse guard (2026-09-07, the Ganesh-wrap vertical
     * slice): this frame's drawable was handed to crtgfx_skia_wrap_gpu_
     * surface() -- a caller must present it via crtgfx_skia_gpu_surface_
     * present() (which itself calls crtgfx_gpu_metal_surface_prepare_
     * ganesh_present() and clears this flag before deferring to this
     * exact function), not this function directly. */
    return CRTGFX_ERROR_HOST;
  }
  if (surface->mtl_command_buffer == NULL) return CRTGFX_ERROR_HOST;
  command = surface->mtl_command_buffer;
  metal_set_id(command, "presentDrawable:", surface->mtl_drawable);
  metal_msg_id(command, "commit");
  metal_msg_id(surface->mtl_last_submission, "release");
  surface->mtl_last_submission = metal_msg_id(command, "retain");
  metal_drop_frame(surface);
  return metal_msg_uint(command, "status") == 5u ? CRTGFX_ERROR_HOST : CRTGFX_OK;
}

/* Real hook for crtgfx_skia_gpu_surface_present() (crtgfx/skia.h, src/
 * skia_bridge.cc, 2026-09-07 -- wiring the offscreen Ganesh pipeline onto
 * this surface's own acquired drawable) -- see this function's own
 * declaration in gpu_internal.h for the full "why here, not skia_
 * bridge.cc" reasoning. Creates one fresh command buffer from the same
 * shared command queue Ganesh's own GrDirectContext was built against
 * (crtgfx_skia_make_gpu_context()) and stores it into surface->mtl_
 * command_buffer, exactly mirroring crtgfx_gpu_metal_surface_clear()'s
 * own last step -- Metal's own same-queue submission-order guarantee
 * places this command buffer's own presentDrawable:/commit (crtgfx_gpu_
 * metal_surface_present(), unchanged, called right after this by the
 * caller) after every real draw Ganesh already submitted to that same
 * queue, with no image-layout/resource-state transition needed at all
 * (MTLTexture has no such concept, unlike VkImage/ID3D12Resource). */
crtgfx_result crtgfx_gpu_metal_surface_prepare_ganesh_present(struct crtgfx_gpu_surface* surface) {
  id pool, command;
  if (!surface->mtl_drawable_acquired || surface->mtl_command_buffer != NULL) return CRTGFX_ERROR_HOST;
  pool = metal_msg_id(metal_msg_id(objc_getClass("NSAutoreleasePool"), "alloc"), "init");
  command = metal_msg_id(surface->device->mtl_command_queue, "commandBuffer");
  if (command == NULL) {
    metal_msg_id(pool, "drain");
    return CRTGFX_ERROR_HOST;
  }
  surface->mtl_command_buffer = metal_msg_id(command, "retain");
  metal_msg_id(pool, "drain");
  return CRTGFX_OK;
}
