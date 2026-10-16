#pragma once

/* Optional Ganesh shared-resource companion (Web Tranche 9B). No native GPU
 * pointer or Skia object crosses IPC. Metadata is fixed-width; handles are CRT
 * descriptors, not public Vulkan types. Initial implementation: Vulkan opaque
 * FD, BGRA8 optimal-tiled dedicated allocation, same device AND driver UUID.
 */
#include "crtgfx/skia.h"
#include "crtgfx/shared_image.h"
#include "include/core/SkImage.h"
#include <stdint.h>
#include <memory>
class GrDirectContext;

/* Device-free shape validation; native capability/UUID/allocation requirements
 * are checked again by import. Optimal tiling is opaque: no CPU plane stride or
 * modifier may be guessed. This version has exactly one dedicated BGRA image.
 */
inline bool crtgfx_shared_image_desc_valid(const crtgfx_shared_image_desc& d) {
  return crtgfx_shared_image_descriptor_valid(&d);
}

/* One UI/GPU-owner thread. The context must be created from the supplied device.
 * D3D12 epochs are the monotonic values of the two shared fences: a consumer
 * waits for ready >= its read count + 1, a producer for release >= its write
 * count; a stale signal cannot satisfy a later epoch. Resources are left in the
 * COMMON state when ownership moves.
 * device and context must outlive the image and every
 * returned Skia reference. Producer/consumer never access a resource together.
 * CPU ACKs pace frames only: each reuse waits on a real imported VkFence, then
 * inserts the corresponding GPU semaphore wait and EXTERNAL queue acquisition.
 * Fences and semaphores are shared by reference, with one signal/wait per epoch.
 * Timeouts never enqueue an unsignaled semaphore wait or grant ownership.
 */
class crtgfx_skia_shared_image {
 public:
  virtual ~crtgfx_skia_shared_image() = default;
  virtual const crtgfx_shared_image_desc& description() const = 0;
  /* New owned FDs, crtgfx_shared_image_handle_count(description()) of them (the
   * rest of the array is set to -1). Vulkan: memory, ready semaphore, release
   * semaphore, ready fence, release fence. D3D12: resource, ready fence, release
   * fence (NT handles adopted as CRT descriptors). Caller closes them. Import
   * borrows/duplicates its inputs. */
  virtual crtgfx_result export_handles(int out_fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT]) = 0;
  virtual crtgfx_result begin_write(GrDirectContext*, uint32_t timeout_ms, sk_sp<SkSurface>*) = 0;
  virtual crtgfx_result end_write(GrDirectContext*, SkSurface*) = 0;
  virtual crtgfx_result wait_ready(uint32_t timeout_ms) = 0;
  virtual crtgfx_result begin_read(GrDirectContext*, uint32_t timeout_ms, sk_sp<SkImage>*) = 0;
  virtual crtgfx_result end_read(GrDirectContext*) = 0;
  virtual crtgfx_result wait_released(uint32_t timeout_ms) = 0;
};

/* query returns UNSUPPORTED rather than inventing CPU synchronization on an
 * unavailable backend. On success out_identity has zero extent/allocation.
 * Factories clear out_image on entry; invalid metadata/UUID fails before FD
 * import. An import never closes the caller's descriptors, including failure.
 */
crtgfx_result crtgfx_skia_shared_query(const crtgfx_gpu_device*, crtgfx_shared_image_desc* out_identity);
crtgfx_result crtgfx_skia_shared_create(const crtgfx_gpu_device*, uint32_t width, uint32_t height,
    std::unique_ptr<crtgfx_skia_shared_image>* out_image);
crtgfx_result crtgfx_skia_shared_import(const crtgfx_gpu_device*, const crtgfx_shared_image_desc&,
    const int fds[CRTGFX_SHARED_IMAGE_HANDLE_COUNT], std::unique_ptr<crtgfx_skia_shared_image>* out_image);
