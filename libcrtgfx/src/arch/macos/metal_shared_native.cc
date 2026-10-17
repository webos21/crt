/* Objective-C++ confined to the native resource boundary, compiled with the
 * same real-SDK exception as Skia's Metal bridge. Only public SDK APIs are used.
 * The NSXPCCoder subclass delegates event encoding to Metal: its opaque key is
 * recorded, never guessed or exposed in the CRT protocol. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <sys/sysctl.h>
#include "metal_shared_native.h"

namespace {
constexpr uint64_t kMagic = UINT64_C(0x4352544d544c3031);
constexpr unsigned kPorts = 3;
struct EventCode {
  char port_key[64];
  char label_key[64];
  char label[256];
};
struct Request { mach_msg_header_t header; uint64_t magic; };
struct Reply {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t ports[kPorts];
  uint64_t magic;
  EventCode events[2];
};
struct ReceivedReply { Reply reply; mach_msg_max_trailer_t trailer; };
struct ReceivedRequest { Request request; mach_msg_max_trailer_t trailer; };

bool copy_string(char* out, size_t capacity, NSString* value) {
  if (!value) { out[0] = 0; return true; }
  const char* text = value.UTF8String;
  if (!text || strlen(text) >= capacity) return false;
  memcpy(out, text, strlen(text) + 1);
  return true;
}
void drop_port(mach_port_t port) {
  if (MACH_PORT_VALID(port)) mach_port_deallocate(mach_task_self(), port);
}
void drop_receive(mach_port_t port) {
  if (MACH_PORT_VALID(port)) mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
}
}

@interface CRTMetalEventCoder : NSXPCCoder {
 @public
  xpc_object_t dictionary;
  EventCode code;
  bool valid;
}
@end
@implementation CRTMetalEventCoder
- (instancetype)init {
  self = [super init];
  if (self) { dictionary = xpc_dictionary_create(nullptr, nullptr, 0); code = {}; valid = true; }
  return self;
}
- (void)dealloc { if (dictionary) xpc_release(dictionary); [super dealloc]; }
- (BOOL)allowsKeyedCoding { return YES; }
- (BOOL)requiresSecureCoding { return YES; }
- (void)encodeXPCObject:(xpc_object_t)value forKey:(NSString*)key {
  if (code.port_key[0] || !value || !copy_string(code.port_key, sizeof code.port_key, key)) { valid = false; return; }
  xpc_dictionary_set_value(dictionary, "reference", value);
}
- (xpc_object_t)decodeXPCObjectOfType:(xpc_type_t)type forKey:(NSString*)key {
  xpc_object_t value = xpc_dictionary_get_value(dictionary, "reference");
  if (!key || strcmp(key.UTF8String, code.port_key) || !value || xpc_get_type(value) != type) {
    valid = false; return nullptr;
  }
  return value;
}
- (void)encodeObject:(id)object forKey:(NSString*)key {
  if (code.label_key[0] || (object && ![object isKindOfClass:[NSString class]])
      || !copy_string(code.label_key, sizeof code.label_key, key)
      || !copy_string(code.label, sizeof code.label, object)) valid = false;
}
- (id)decodeObjectOfClass:(Class)type forKey:(NSString*)key {
  if (type != [NSString class] || !key || strcmp(key.UTF8String, code.label_key)) { valid = false; return nil; }
  return code.label[0] ? [NSString stringWithUTF8String:code.label] : nil;
}
@end

@interface CRTMetalSharedRecord : NSObject {
 @public
  IOSurfaceRef surface;
  id<MTLTexture> texture;
  id<MTLSharedEvent> ready;
  id<MTLSharedEvent> released;
}
@end
@implementation CRTMetalSharedRecord
- (void)dealloc {
  [texture release]; [ready release]; [released release];
  if (surface) CFRelease(surface);
  [super dealloc];
}
@end

namespace {
struct Offer {
  mach_port_t receive = MACH_PORT_NULL;
  mach_port_t ports[kPorts] = {};
  EventCode events[2] = {};
  char service[96] = {};
};

id<MTLTexture> make_texture(id<MTLDevice> device, IOSurfaceRef surface) {
  MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
      width:IOSurfaceGetWidth(surface) height:IOSurfaceGetHeight(surface) mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
  return [device newTextureWithDescriptor:desc iosurface:surface plane:0];
}

bool encode_event(id<MTLSharedEvent> event, EventCode* code, mach_port_t* port) {
  CRTMetalEventCoder* coder = [CRTMetalEventCoder new];
  MTLSharedEventHandle* handle = [event newSharedEventHandle];
  bool ok = false;
  @try {
    [handle encodeWithCoder:coder];
    *port = xpc_dictionary_copy_mach_send(coder->dictionary, "reference");
    ok = coder->valid && coder->code.port_key[0] && MACH_PORT_VALID(*port);
    if (ok) *code = coder->code;
  } @catch (NSException*) { ok = false; }
  [handle release]; [coder release];
  return ok;
}

id<MTLSharedEvent> decode_event(id<MTLDevice> device, const EventCode& code, mach_port_t port) {
  if (!code.port_key[0] || !memchr(code.port_key, 0, sizeof code.port_key)
      || !memchr(code.label_key, 0, sizeof code.label_key) || !memchr(code.label, 0, sizeof code.label)) return nil;
  CRTMetalEventCoder* coder = [CRTMetalEventCoder new]; coder->code = code;
  xpc_dictionary_set_mach_send(coder->dictionary, "reference", port);
  id<MTLSharedEvent> result = nil;
  @try {
    MTLSharedEventHandle* handle = [[MTLSharedEventHandle alloc] initWithCoder:coder];
    if (coder->valid && handle) result = [device newSharedEventWithHandle:handle];
    [handle release];
  } @catch (NSException*) { result = nil; }
  [coder release];
  return result;
}
}

extern "C" int crtgfx_metal_share_identity(void* opaque, uint8_t device_id[16], uint8_t driver_id[16]) {
  @autoreleasepool {
    id<MTLDevice> device = (id<MTLDevice>)opaque;
    if (!device || ![device respondsToSelector:@selector(newSharedEvent)]) return 0;
    uint64_t registry = device.registryID;
    if (!registry) return 0;
    memset(device_id, 0, 16); memcpy(device_id, &registry, sizeof registry);
    memcpy(device_id + 8, "MTL-BGRA", 8);
    char version[128] = {}; size_t size = sizeof version;
    if (sysctlbyname("kern.osversion", version, &size, nullptr, 0) != 0) return 0;
    uint64_t hashes[2] = {UINT64_C(14695981039346656037), UINT64_C(7809847782465536322)};
    for (unsigned char c : version) {
      hashes[0] = (hashes[0] ^ c) * UINT64_C(1099511628211);
      hashes[1] = (hashes[1] ^ c) * UINT64_C(1099511628211);
    }
    memcpy(driver_id, hashes, 16);
    return 1;
  }
}

extern "C" void* crtgfx_metal_share_create(void* opaque, uint32_t width, uint32_t height) {
  @autoreleasepool {
    id<MTLDevice> device = (id<MTLDevice>)opaque;
    CRTMetalSharedRecord* record = [CRTMetalSharedRecord new];
    NSDictionary* properties = @{
      (id)kIOSurfaceWidth: @(width), (id)kIOSurfaceHeight: @(height),
      (id)kIOSurfaceBytesPerElement: @4, (id)kIOSurfaceBytesPerRow: @(((uint64_t)width * 4 + 63) & ~UINT64_C(63)),
      (id)kIOSurfacePixelFormat: @(UINT32_C(0x42475241))
    };
    record->surface = IOSurfaceCreate((CFDictionaryRef)properties);
    if (record->surface) record->texture = make_texture(device, record->surface);
    record->ready = [device newSharedEvent]; record->released = [device newSharedEvent];
    if (!record->surface || !record->texture || !record->ready || !record->released
        || ![record->ready respondsToSelector:@selector(waitUntilSignaledValue:timeoutMS:)]) {
      [record release]; return nullptr;
    }
    return record;
  }
}
extern "C" void crtgfx_metal_share_release(void* record) { [(CRTMetalSharedRecord*)record release]; }
extern "C" void* crtgfx_metal_share_texture(void* record) { return ((CRTMetalSharedRecord*)record)->texture; }
extern "C" uint64_t crtgfx_metal_share_allocation(void* record) { return IOSurfaceGetAllocSize(((CRTMetalSharedRecord*)record)->surface); }
extern "C" int crtgfx_metal_share_shape(void* record, uint32_t width, uint32_t height, uint64_t allocation) {
  IOSurfaceRef surface = ((CRTMetalSharedRecord*)record)->surface;
  return IOSurfaceGetWidth(surface) == width && IOSurfaceGetHeight(surface) == height
      && IOSurfaceGetPixelFormat(surface) == UINT32_C(0x42475241) && IOSurfaceGetBytesPerElement(surface) == 4
      && IOSurfaceGetPlaneCount(surface) == 0 && IOSurfaceGetAllocSize(surface) == allocation
      && IOSurfaceGetBytesPerRow(surface) == (((uint64_t)width * 4 + 63) & ~UINT64_C(63));
}
extern "C" int crtgfx_metal_share_wait(void* opaque, int is_ready, uint64_t value, uint32_t timeout_ms) {
  @autoreleasepool {
    CRTMetalSharedRecord* record = (CRTMetalSharedRecord*)opaque;
    id<MTLSharedEvent> event = is_ready ? record->ready : record->released;
    return [event waitUntilSignaledValue:value timeoutMS:timeout_ms] ? 1 : 0;
  }
}
extern "C" int crtgfx_metal_share_enqueue(void* opaque, void* queue, int is_ready, uint64_t value, int signal) {
  @autoreleasepool {
    CRTMetalSharedRecord* record = (CRTMetalSharedRecord*)opaque;
    id<MTLSharedEvent> event = is_ready ? record->ready : record->released;
    id<MTLCommandBuffer> command = [(id<MTLCommandQueue>)queue commandBuffer];
    if (!command) return 0;
    if (signal) [command encodeSignalEvent:event value:value];
    else [command encodeWaitForEvent:event value:value];
    [command commit];
    return command.status != MTLCommandBufferStatusError;
  }
}

extern "C" void* crtgfx_metal_share_offer(void* opaque, crtgfx_metal_share_ticket* ticket) {
  @autoreleasepool {
    CRTMetalSharedRecord* record = (CRTMetalSharedRecord*)opaque;
    Offer* offer = new Offer;
    offer->ports[0] = IOSurfaceCreateMachPort(record->surface);
    bool ok = MACH_PORT_VALID(offer->ports[0]) && encode_event(record->ready, &offer->events[0], &offer->ports[1])
        && encode_event(record->released, &offer->events[1], &offer->ports[2]);
    if (ok) ok = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &offer->receive) == KERN_SUCCESS;
    if (ok) ok = mach_port_insert_right(mach_task_self(), offer->receive, offer->receive, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS;
    if (ok) {
      NSString* name = [@"org.crt.metal." stringByAppendingString:[NSUUID UUID].UUIDString];
      ok = copy_string(offer->service, sizeof offer->service, name)
          && bootstrap_register(bootstrap_port, offer->service, offer->receive) == KERN_SUCCESS;
    }
    if (!ok) { crtgfx_metal_share_offer_release(offer); return nullptr; }
    memset(ticket, 0, sizeof *ticket); ticket->magic = kMagic;
    memcpy(ticket->service, offer->service, sizeof ticket->service);
    return offer;
  }
}

extern "C" int crtgfx_metal_share_serve(void* opaque, uint32_t timeout_ms) {
  Offer* offer = (Offer*)opaque;
  ReceivedRequest incoming = {};
  kern_return_t result = mach_msg(&incoming.request.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
      0, sizeof incoming, offer->receive, timeout_ms, MACH_PORT_NULL);
  if (result == MACH_RCV_TIMED_OUT) return 1;
  if (result != KERN_SUCCESS) return 0;
  Request& request = incoming.request;
  if (request.header.msgh_size != sizeof(Request) || request.header.msgh_id != 0x4351 || request.magic != kMagic
      || (request.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) || !MACH_PORT_VALID(request.header.msgh_remote_port)) {
    mach_msg_destroy(&request.header); return 1;
  }
  Reply reply = {};
  reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) | MACH_MSGH_BITS_COMPLEX;
  reply.header.msgh_size = sizeof reply; reply.header.msgh_remote_port = request.header.msgh_remote_port;
  reply.header.msgh_id = 0x4352;
  reply.body.msgh_descriptor_count = kPorts; reply.magic = kMagic;
  memcpy(reply.events, offer->events, sizeof reply.events);
  for (unsigned i = 0; i < kPorts; ++i) {
    reply.ports[i].name = offer->ports[i]; reply.ports[i].disposition = MACH_MSG_TYPE_COPY_SEND;
    reply.ports[i].type = MACH_MSG_PORT_DESCRIPTOR;
  }
  result = mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof reply, 0, MACH_PORT_NULL, 100, MACH_PORT_NULL);
  if (result != KERN_SUCCESS) drop_port(request.header.msgh_remote_port);
  return 1;
}
extern "C" void crtgfx_metal_share_offer_release(void* opaque) {
  Offer* offer = (Offer*)opaque;
  if (!offer) return;
  if (offer->service[0]) {
    kern_return_t ignored = bootstrap_register(bootstrap_port, offer->service, MACH_PORT_NULL);
    (void)ignored;
  }
  for (mach_port_t port : offer->ports) drop_port(port);
  drop_port(offer->receive); drop_receive(offer->receive);
  delete offer;
}
extern "C" void* crtgfx_metal_share_open(void* device, const crtgfx_metal_share_ticket* ticket) {
  @autoreleasepool {
    if (!ticket || ticket->magic != kMagic || strncmp(ticket->service, "org.crt.metal.", 14)
        || !memchr(ticket->service, 0, sizeof ticket->service)) return nullptr;
    mach_port_t service = MACH_PORT_NULL, receive = MACH_PORT_NULL;
    if (bootstrap_look_up(bootstrap_port, ticket->service, &service) != KERN_SUCCESS) return nullptr;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &receive) != KERN_SUCCESS) { drop_port(service); return nullptr; }
    Request request = {};
    request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    request.header.msgh_size = sizeof request; request.header.msgh_remote_port = service;
    request.header.msgh_local_port = receive; request.header.msgh_id = 0x4351; request.magic = kMagic;
    kern_return_t result = mach_msg(&request.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof request, 0,
        MACH_PORT_NULL, 2000, MACH_PORT_NULL);
    drop_port(service);
    ReceivedReply incoming = {};
    if (result == KERN_SUCCESS) result = mach_msg(&incoming.reply.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
        0, sizeof incoming, receive, 2000, MACH_PORT_NULL);
    CRTMetalSharedRecord* record = nil;
    if (result == KERN_SUCCESS) {
      Reply& reply = incoming.reply;
      bool ok = reply.header.msgh_size == sizeof reply && reply.header.msgh_id == 0x4352 && reply.magic == kMagic
          && (reply.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) && reply.body.msgh_descriptor_count == kPorts;
      for (unsigned i = 0; ok && i < kPorts; ++i) ok = reply.ports[i].type == MACH_MSG_PORT_DESCRIPTOR
          && reply.ports[i].disposition == MACH_MSG_TYPE_PORT_SEND && MACH_PORT_VALID(reply.ports[i].name);
      if (ok) {
        record = [CRTMetalSharedRecord new];
        record->surface = IOSurfaceLookupFromMachPort(reply.ports[0].name);
        record->ready = decode_event((id<MTLDevice>)device, reply.events[0], reply.ports[1].name);
        record->released = decode_event((id<MTLDevice>)device, reply.events[1], reply.ports[2].name);
        if (record->surface) record->texture = make_texture((id<MTLDevice>)device, record->surface);
        if (!record->surface || !record->ready || !record->released || !record->texture) { [record release]; record = nil; }
      }
      mach_msg_destroy(&reply.header);
    }
    drop_receive(receive);
    return record;
  }
}
