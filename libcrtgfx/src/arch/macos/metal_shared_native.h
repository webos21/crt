#pragma once
#include <stdint.h>

/* Private Objective-C/Mach boundary. No native object or port name is public ABI. */
struct crtgfx_metal_share_ticket {
  uint64_t magic;
  char service[96];
};

#ifdef __cplusplus
extern "C" {
#endif
int crtgfx_metal_share_identity(void* device, uint8_t device_id[16], uint8_t driver_id[16]);
void* crtgfx_metal_share_create(void* device, uint32_t width, uint32_t height);
void crtgfx_metal_share_release(void* record);
void* crtgfx_metal_share_texture(void* record);
uint64_t crtgfx_metal_share_allocation(void* record);
int crtgfx_metal_share_shape(void* record, uint32_t width, uint32_t height, uint64_t allocation);
int crtgfx_metal_share_wait(void* record, int ready, uint64_t value, uint32_t timeout_ms);
int crtgfx_metal_share_enqueue(void* record, void* queue, int ready, uint64_t value, int signal);
void* crtgfx_metal_share_offer(void* record, struct crtgfx_metal_share_ticket* ticket);
int crtgfx_metal_share_serve(void* offer, uint32_t timeout_ms);
void crtgfx_metal_share_offer_release(void* offer);
void* crtgfx_metal_share_open(void* device, const struct crtgfx_metal_share_ticket* ticket);
int crtgfx_metal_share_export(void* record, int out_fds[2]);
void* crtgfx_metal_share_import(void* device, const int fds[2]);
uint32_t crtgfx_metal_share_live_offers(void); /* Private lifetime-test observer. */
#ifdef __cplusplus
}
#endif
