/* A synthetic "host library" for TODO.md's "Allocator baseline validation
 * before Upper Runtime" tranche 7 ("freeze the Host ABI firewall before
 * hardware decode") -- see docs/host_abi_firewall.md's own "Mechanical
 * detection" section for the full design.
 *
 * fake_host_object is deliberately opaque here: this header is the only
 * thing "CRT adapter" code (host_abi_firewall_test.c/host_abi_firewall_
 * fault_victim.c) ever includes, matching the real boundary this
 * synthesizes -- a real Wayland/Vulkan/FFmpeg object's private layout is
 * likewise never visible to this project's own adapter code, only an
 * opaque pointer/handle and that library's own API to act on it. */

#ifndef HOST_ABI_FIREWALL_FAKE_HOST_H
#define HOST_ABI_FIREWALL_FAKE_HOST_H

typedef struct fake_host_object fake_host_object;

fake_host_object* fake_host_create(void);
void fake_host_retain(fake_host_object* obj);
void fake_host_release(fake_host_object* obj);

unsigned fake_host_create_count(void);
unsigned fake_host_destroy_count(void);
unsigned fake_host_live_count(void);

#endif
