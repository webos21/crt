#pragma once

/* Fixed-width resource metadata, not a native GPU or Skia ABI. Version 1:
 * one BGRA8 image; plane strides/modifiers are opaque and must not be guessed by
 * a CPU consumer.
 *  - Vulkan, OPAQUE_FD: optimal-tiled dedicated allocation, five descriptors
 *    (memory, ready/release semaphores, ready/release fences).
 *  - D3D12, NT_HANDLE (Windows): one committed shared texture and two shared
 *    fences, three CRT descriptors adopted from NT handles (resource, ready
 *    fence, release fence). A D3D12 fence is both the GPU wait and the CPU wait
 *    and counts epochs monotonically, so no second object per direction exists.
 *    device_uuid is the adapter LUID + vendor + device id, driver_uuid the user-
 *    mode driver version + subsystem id + revision; memory_type stays 0. */
#include "crtgfx/gpu.h"
#include <stdint.h>

#define CRTGFX_SHARED_IMAGE_VERSION 1u
#define CRTGFX_SHARED_IMAGE_BGRA8 1u
#define CRTGFX_SHARED_IMAGE_OPAQUE_FD 1u
#define CRTGFX_SHARED_IMAGE_NT_HANDLE 2u
/* The most any backend transfers (array size); a descriptor's own count is
 * crtgfx_shared_image_handle_count(). */
#define CRTGFX_SHARED_IMAGE_HANDLE_COUNT 5u
#define CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT 3u
#define CRTGFX_SHARED_IMAGE_MAX_EXTENT 8192u

typedef struct crtgfx_shared_image_desc {
  uint32_t version, backend, handle_kind, format;
  uint32_t width, height, memory_type, reserved;
  uint64_t allocation_size;
  uint8_t device_uuid[16], driver_uuid[16];
} crtgfx_shared_image_desc;

/* Descriptors a BUFFER_ADD of this image carries; 0 for a shape no backend implements. */
static inline unsigned crtgfx_shared_image_handle_count(const crtgfx_shared_image_desc* d) {
  if (!d) return 0;
  if (d->backend == CRTGFX_GPU_BACKEND_VULKAN && d->handle_kind == CRTGFX_SHARED_IMAGE_OPAQUE_FD)
    return CRTGFX_SHARED_IMAGE_HANDLE_COUNT;
  if (d->backend == CRTGFX_GPU_BACKEND_D3D12 && d->handle_kind == CRTGFX_SHARED_IMAGE_NT_HANDLE)
    return CRTGFX_SHARED_IMAGE_D3D12_HANDLE_COUNT;
  return 0;
}

static inline int crtgfx_shared_image_descriptor_valid(const crtgfx_shared_image_desc* d) {
  return d && d->version == CRTGFX_SHARED_IMAGE_VERSION && crtgfx_shared_image_handle_count(d)
      && (d->backend != CRTGFX_GPU_BACKEND_D3D12 || !d->memory_type) && d->format == CRTGFX_SHARED_IMAGE_BGRA8
      && d->width && d->height && d->width <= CRTGFX_SHARED_IMAGE_MAX_EXTENT
      && d->height <= CRTGFX_SHARED_IMAGE_MAX_EXTENT && d->memory_type < 32 && !d->reserved
      && d->allocation_size >= (uint64_t)d->width * d->height * 4
      && d->allocation_size <= (1ull << 30);
}
