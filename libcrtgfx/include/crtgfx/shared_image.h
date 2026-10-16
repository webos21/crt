#pragma once

/* Fixed-width resource metadata, not a native GPU or Skia ABI. Version 1:
 * one optimal-tiled, dedicated BGRA8 image; plane strides/modifiers are opaque
 * and must not be guessed by a CPU consumer. */
#include "crtgfx/gpu.h"
#include <stdint.h>

#define CRTGFX_SHARED_IMAGE_VERSION 1u
#define CRTGFX_SHARED_IMAGE_BGRA8 1u
#define CRTGFX_SHARED_IMAGE_OPAQUE_FD 1u
#define CRTGFX_SHARED_IMAGE_HANDLE_COUNT 5u
#define CRTGFX_SHARED_IMAGE_MAX_EXTENT 8192u

typedef struct crtgfx_shared_image_desc {
  uint32_t version, backend, handle_kind, format;
  uint32_t width, height, memory_type, reserved;
  uint64_t allocation_size;
  uint8_t device_uuid[16], driver_uuid[16];
} crtgfx_shared_image_desc;

static inline int crtgfx_shared_image_descriptor_valid(const crtgfx_shared_image_desc* d) {
  return d && d->version == CRTGFX_SHARED_IMAGE_VERSION && d->backend == CRTGFX_GPU_BACKEND_VULKAN
      && d->handle_kind == CRTGFX_SHARED_IMAGE_OPAQUE_FD && d->format == CRTGFX_SHARED_IMAGE_BGRA8
      && d->width && d->height && d->width <= CRTGFX_SHARED_IMAGE_MAX_EXTENT
      && d->height <= CRTGFX_SHARED_IMAGE_MAX_EXTENT && d->memory_type < 32 && !d->reserved
      && d->allocation_size >= (uint64_t)d->width * d->height * 4
      && d->allocation_size <= (1ull << 30);
}
