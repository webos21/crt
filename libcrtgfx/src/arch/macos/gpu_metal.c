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
  return (NSUInteger)((id (*)(id, SEL))objc_msgSend)(self, sel);
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
