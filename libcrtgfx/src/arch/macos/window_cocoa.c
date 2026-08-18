/*
 * macOS host window backend for libcrtgfx: maps a Wayland-style toplevel
 * surface (src/wayland_weston.c) onto a real Cocoa NSWindow, the
 * same "one top-level surface maps to one host-native window" shape
 * docs/libcrtgfx_wayland_plan.md already established for Windows
 * (src/arch/windows/window_win32.c) and Linux (src/arch/linux/
 * window_wayland.c). This file follows Win32's own precedent of never
 * including a host SDK header at all: every AppKit/Foundation/
 * QuartzCore/CoreGraphics type, constant, and function used below is
 * hand-declared from Apple's own public, stable ABI, exactly the way
 * window_win32.c hand-declares Win32 rather than #include <windows.h>.
 *
 * Architecture reference: docs/libcrtgfx_wayland_plan.md names Wawona/
 * Wayoa/Cocoa-Way-style projects as the macOS/iOS reference for mapping
 * Wayland-shaped surfaces onto native Cocoa windows with GPU-composited
 * presentation. The concrete technique this file takes from that
 * reference class is "drive Cocoa from plain C via the Objective-C
 * runtime's own C ABI (objc_msgSend/objc_getClass/sel_registerName/
 * objc_allocateClassPair), never the Objective-C *language*" -- the same
 * approach Wawona-style native bridges use to keep a non-Objective-C-
 * language codebase talking to AppKit directly, and the same reason this
 * file needs no `.m` translation unit, no ARC, and no Xcode project: it
 * is a plain .c file compiled by this project's own tools/crt-cc,
 * linked against the real system frameworks (Foundation, AppKit,
 * QuartzCore, CoreGraphics, libobjc) the same way window_win32.c links
 * against real user32.lib/gdi32.lib.
 *
 * Every ObjC/AppKit/QuartzCore/CoreGraphics extern symbol, constant
 * value, and calling-convention detail below (NSWindowStyleMask bits,
 * NSBackingStoreBuffered, NSApplicationActivationPolicyRegular, the
 * exact CGBitmapInfo flags for BGRA8888-premultiplied, and -- most
 * importantly -- whether an NSRect-returning message send needs
 * objc_msgSend_stret) was cross-checked empirically on this real macOS
 * aarch64 host: a small standalone C program declaring the same externs
 * this file uses, linked against the real frameworks, that created a
 * real NSWindow/NSView/CALayer, queried -frame and -bounds, built a
 * CGImage from a raw pixel buffer, set it as layer.contents, and showed
 * the window -- all without crashing, before any of this landed here.
 * See HISTORY.md for the verification record.
 *
 * Performance shape (why this isn't the naive `-drawRect:` approach):
 * the content NSView is layer-backed (`-setWantsLayer:YES`), and each
 * presented frame becomes that CALayer's `contents` (a CGImage built
 * directly from the caller's BGRA8888 pixel buffer). Setting
 * `layer.contents` hands the frame straight to Core Animation's own
 * compositor, which the WindowServer composites via hardware the same
 * way it composites every other app's layers -- there is no `-drawRect:`
 * invalidation round trip, no synchronous Quartz `CGContextDrawImage`
 * call blocking the main thread on every frame, and no involvement of
 * the (much slower) legacy view-drawing machinery at all. This matches
 * the "software buffer path first, then GPU texture/direct-render
 * paths" staging in docs/libcrtgfx_wayland_plan.md: it is still a CPU-
 * built pixel buffer (no Metal/GPU texture yet), but it reaches the
 * screen through the same fast, hardware-composited presentation path a
 * real GPU-backed layer would use, which is the concrete "performance"
 * lesson taken from the Wawona/Cocoa-Way reference class -- those
 * projects present via CALayer/IOSurface-backed compositing rather than
 * routing every frame through the legacy drawing/invalidation pipeline.
 *
 * Event pumping (crtgfx_host_window_dispatch) is a genuine improvement
 * over this file's own Win32 sibling, not just a port of the same
 * pattern: Win32's PeekMessage has no "wait up to N milliseconds for the
 * next message" primitive, so window_win32.c's dispatch loop has to
 * busy-poll (drain, `Sleep(1)`, recheck elapsed time, repeat). Cocoa's
 * `-[NSApplication nextEventMatchingMask:untilDate:inMode:dequeue:]`
 * *does* take a real deadline, so this file's loop blocks the thread
 * efficiently in the OS's own run-loop wait instead of spin-polling,
 * while still draining every event and returning by the caller's
 * requested `timeout_ms` -- strictly less CPU for the same contract.
 *
 * Present-path safety: crtgfx_host_window_present_software() copies the
 * caller's pixel buffer into its own CGDataProvider-owned allocation
 * (freed via a real release callback once Core Graphics is done with
 * it) rather than wrapping the caller's `crtgfx_weston_toplevel`
 * software buffer in place. That buffer can be mutated or reallocated by
 * the very next crtgfx_window_begin_frame() call, and CALayer's own
 * consumption of `contents` is not synchronous with this call returning
 * (WindowServer compositing happens on its own schedule) -- copying
 * here avoids a real tear/use-after-free hazard a zero-copy wrap would
 * have, at the cost of one memcpy per frame. This mirrors Win32's own
 * `StretchDIBits()` call, which is itself synchronous/copying for the
 * exact same reason, and is more conservative than this project's own
 * Linux Wayland backend, whose "buffer torn down on the next present
 * rather than gated on wl_buffer::release" scope cut is explicitly
 * documented there as a real, currently-theoretical tear risk -- this
 * file simply doesn't take on that same risk in exchange for the one
 * extra copy.
 *
 * Frame-visibility bug (found live, not from documentation -- and the
 * real root cause took two attempts to find): the first real end-to-end
 * run showed one frame and then never visibly updated again, even
 * though crtgfx_window_demo kept running, drawing new pixel data, and
 * calling crtgfx_host_window_present_software() every ~16ms without any
 * error return -- a symptom process-health checks (still running, no
 * crash, stable CPU/memory) cannot distinguish from working correctly.
 * First hypothesis: a bare `[layer setContents:img]` only schedules an
 * *implicit* CATransaction that never gets flushed without
 * `-[NSApplication run]`'s own run-loop-idle observer. Fixing that
 * (wrapping `setContents:` in an *explicit* `[CATransaction begin]`/
 * `[CATransaction commit]` pair below, kept for its own genuine benefit
 * -- `setDisableActions:YES` also skips the default implicit-fade
 * animation on every frame -- but not because it was the real bug) did
 * *not* fix the freeze, caught by re-testing rather than trusting the
 * reasoning.
 *
 * Real root cause, found via a live `lldb attach`+`bt` on the still-
 * running (healthy-looking, 0% CPU) process: it was blocked inside
 * `-nextEventMatchingMask:untilDate:...` with `until_date` a real date
 * roughly two and a half weeks in the future. `crtgfx_now_ms()`
 * (below) had been calling `clock_gettime()` with Darwin's *real* raw
 * `CLOCK_MONOTONIC` value (6) -- correct for real libSystem, but
 * crtgfx_window_demo/crtgfx_window_smoke link this project's own libc
 * (`c`) as their actual C runtime, and that static archive's own
 * `clock_gettime()` (libc/src/time.c) wins symbol resolution over real
 * Darwin libSystem's at link time. This project's own macOS
 * `__crt_sys_clock_gettime()` only recognizes its own, differently-
 * numbered clock ids (include/time.h: CLOCK_REALTIME=0/
 * CLOCK_MONOTONIC=1) and returns -EINVAL for anything else -- leaving
 * the output struct timespec completely unwritten (stack garbage) on
 * every single call, confirmed directly via temporary dprintf()
 * instrumentation showing wildly inconsistent, non-advancing
 * timestamps. That garbage fed a bogus multi-day interval into
 * `-[NSDate dateWithTimeIntervalSinceNow:]`, which made the event wait
 * legitimately block far past the caller's real timeout_ms the first
 * time a real event backlog needed draining. Fixed by using this
 * project's own CLOCK_MONOTONIC value (1) below instead of Darwin's raw
 * one, since the symbol that actually links here is confirmed to be
 * this project's own implementation, not the real Darwin one the
 * original code assumed. See HISTORY.md for the full investigation,
 * including why the CATransaction fix looked plausible but wasn't it.
 *
 * Scope cuts (documented, not silent, matching this project's own
 * discipline elsewhere):
 *  - single window per process, matching Win32's own thread-global
 *    message queue shape (crtgfx_host_window_dispatch() takes no window
 *    parameter, same as the Win32/Linux backends);
 *  - no keyboard/mouse/trackpad input delivered to the caller yet
 *    (events are drained and dispatched to AppKit for correct window
 *    chrome/resize/close behavior, but this backend does not yet
 *    surface input events through the crtgfx public API -- matching
 *    Linux's own "no keyboard/pointer/wl_seat input" cut);
 *  - `-frame`/`-bounds` (both NSRect-returning) are the one place this
 *    file's calling convention differs between architectures: AAPCS64
 *    returns NSRect via the ordinary `objc_msgSend` entry point (Apple
 *    never shipped a separate `_stret` variant for arm64, since AAPCS64
 *    already handles a hidden indirect-result pointer transparently),
 *    while x86_64's SysV ABI needs the dedicated `objc_msgSend_stret`
 *    entry point for any struct return larger than 16 bytes. Both paths
 *    are implemented (msgsend_rect() below); only the arm64 path has
 *    been run on real hardware this session -- the x86_64 path is
 *    reasoned from Apple's own long-documented ABI split, not
 *    independently verified on a real Intel/Rosetta host.
 */

typedef void* id;
typedef void* Class;
typedef void* SEL;
typedef void* IMP;
typedef void* Ivar;
typedef signed char BOOL;
typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef double CGFloat;

typedef struct {
  CGFloat x;
  CGFloat y;
} NSPoint;

typedef struct {
  CGFloat width;
  CGFloat height;
} NSSize;

typedef struct {
  NSPoint origin;
  NSSize size;
} NSRect;

extern id objc_msgSend(id self, SEL op, ...);
extern void objc_msgSend_stret(void* out, id self, SEL op, ...);
extern Class objc_getClass(const char* name);
extern SEL sel_registerName(const char* name);
extern Class objc_allocateClassPair(Class superclass, const char* name, unsigned long extra_bytes);
extern void objc_registerClassPair(Class cls);
extern unsigned char class_addMethod(Class cls, SEL name, IMP imp, const char* types);
extern unsigned char class_addIvar(
    Class cls, const char* name, unsigned long size, unsigned char alignment, const char* types);
extern unsigned char object_setInstanceVariable(id obj, const char* name, void* value);
extern Ivar object_getInstanceVariable(id obj, const char* name, void** out_value);

/* Real NSString* constant exported as a data symbol by Foundation, not
 * something this file constructs -- see this file's own top comment for
 * how that was confirmed (a standalone probe linked and read this exact
 * symbol before it was relied on here). */
extern void* NSDefaultRunLoopMode;
/* Same shape, exported by QuartzCore. */
extern void* kCAGravityResize;

typedef void* CGColorSpaceRef;
typedef void* CGDataProviderRef;
typedef void* CGImageRef;

extern CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
extern void CGColorSpaceRelease(CGColorSpaceRef space);
extern CGDataProviderRef CGDataProviderCreateWithData(
    void* info, const void* data, unsigned long size,
    void (*release_callback)(void* info, const void* data, unsigned long size));
extern void CGDataProviderRelease(CGDataProviderRef provider);
extern CGImageRef CGImageCreate(
    unsigned long width, unsigned long height, unsigned long bits_per_component,
    unsigned long bits_per_pixel, unsigned long bytes_per_row, CGColorSpaceRef space,
    unsigned int bitmap_info, CGDataProviderRef provider, const CGFloat* decode,
    unsigned char should_interpolate, int rendering_intent);
extern void CGImageRelease(CGImageRef image);

/* clock_gettime()/struct timespec declared locally rather than pulled
 * from any header, matching this file's self-contained style elsewhere
 * -- but the CLOCK_MONOTONIC value below is deliberately this project's
 * own (include/time.h: CLOCK_REALTIME=0/CLOCK_MONOTONIC=1), NOT real
 * Darwin's raw clock_id (6), and that is not a stylistic choice, it is
 * required for correctness: crtgfx_window_demo/crtgfx_window_smoke link
 * this project's own libc (`c`) as their actual C runtime, and that
 * static archive's own clock_gettime() (libc/src/time.c) wins symbol
 * resolution over real Darwin libSystem's at link time -- confirmed
 * directly (`nm`/live debug prints), not assumed. This project's own
 * macOS __crt_sys_clock_gettime() only recognizes its own Bionic-
 * numbered clock ids and returns -EINVAL -- leaving the output struct
 * completely unwritten, i.e. stack garbage -- for anything else,
 * including Darwin's real raw value 6.
 *
 * This was a real, live bug, not a hypothetical: passing 6 produced
 * wildly inconsistent, non-advancing "now" timestamps (confirmed via
 * temporary dprintf() instrumentation and a live lldb backtrace), which
 * made crtgfx_host_window_dispatch()'s own timeout math occasionally
 * compute an `-[NSDate dateWithTimeIntervalSinceNow:]` many days in the
 * future. `-nextEventMatchingMask:untilDate:...` then legitimately
 * blocked against that bogus far-future deadline instead of returning
 * within the caller's real timeout_ms, which silently froze
 * crtgfx_window_demo's entire frame loop the first time a real event
 * backlog (e.g. ordinary mouse movement) needed draining and the
 * deadline check never correctly fired to cut it off -- reported
 * directly by the user watching the real window ("여전히 화면이
 * 멈춰 있다"), confirmed by two side-by-side screenshots a second apart
 * being byte-identical. See HISTORY.md. */
struct crtgfx_cocoa_timespec {
  long tv_sec;
  long tv_nsec;
};
extern int clock_gettime(int clock_id, struct crtgfx_cocoa_timespec* tp);
#define CRTGFX_CLOCK_MONOTONIC 1

/* Real libSystem allocator/memcpy -- declared locally for the same
 * self-contained-file reason as clock_gettime() above, rather than
 * #include <stdlib.h>/<string.h> (which would pull in this project's own
 * versions via its normal include search path, not host libSystem's --
 * see crtgfx/CMakeLists.txt's own comment on why this backend directory
 * stays outside crt_build_flags). */
extern void* malloc(unsigned long size);
extern void* calloc(unsigned long count, unsigned long size);
extern void free(void* ptr);
extern void* memcpy(void* dst, const void* src, unsigned long size);

#include "wayland_weston_internal.h"

#define CRTGFX_NSWINDOW_STYLE_TITLED 1u
#define CRTGFX_NSWINDOW_STYLE_CLOSABLE 2u
#define CRTGFX_NSWINDOW_STYLE_MINIATURIZABLE 4u
#define CRTGFX_NSWINDOW_STYLE_RESIZABLE 8u
#define CRTGFX_NSBACKINGSTORE_BUFFERED 2u
#define CRTGFX_NSAPPLICATIONACTIVATIONPOLICY_REGULAR 0

#define CRTGFX_CGIMAGE_ALPHA_PREMULTIPLIED_FIRST 2u
#define CRTGFX_CGBITMAP_BYTE_ORDER_32_LITTLE (2u << 12)

struct crtgfx_host_window {
  id window;
  id delegate;
  crtgfx_weston_toplevel* toplevel;
};

static Class crtgfx_delegate_class;

/* -frame/-bounds both return NSRect by value. AAPCS64 (arm64) resolves
 * this through the ordinary objc_msgSend entry point; the x86_64 SysV
 * ABI needs the dedicated stret entry point for a struct this size (32
 * bytes, over the 16-byte register-return threshold). See this file's
 * own top comment for what has and hasn't been verified here. */
#if defined(__x86_64__)
static NSRect crtgfx_msgsend_rect(id self, SEL op) {
  NSRect result;
  ((void (*)(NSRect*, id, SEL))objc_msgSend_stret)(&result, self, op);
  return result;
}
#else
static NSRect crtgfx_msgsend_rect(id self, SEL op) {
  return ((NSRect(*)(id, SEL))objc_msgSend)(self, op);
}
#endif

static id crtgfx_msgsend_class_op(Class cls, SEL op) {
  return ((id(*)(Class, SEL))objc_msgSend)(cls, op);
}

static void crtgfx_msgsend_class_void_bool(Class cls, SEL op, BOOL arg) {
  ((void (*)(Class, SEL, BOOL))objc_msgSend)(cls, op, arg);
}

static id crtgfx_msgsend_op(id self, SEL op) {
  return ((id(*)(id, SEL))objc_msgSend)(self, op);
}

static id crtgfx_msgsend_id(id self, SEL op, id arg) {
  return ((id(*)(id, SEL, id))objc_msgSend)(self, op, arg);
}

static void crtgfx_msgsend_void_id(id self, SEL op, id arg) {
  ((void (*)(id, SEL, id))objc_msgSend)(self, op, arg);
}

static void crtgfx_msgsend_void_bool(id self, SEL op, BOOL arg) {
  ((void (*)(id, SEL, BOOL))objc_msgSend)(self, op, arg);
}

static void crtgfx_msgsend_void_int(id self, SEL op, NSInteger arg) {
  ((void (*)(id, SEL, NSInteger))objc_msgSend)(self, op, arg);
}

static void crtgfx_msgsend_void(id self, SEL op) {
  ((void (*)(id, SEL))objc_msgSend)(self, op);
}

static id crtgfx_msgsend_string_with_utf8(Class ns_string_class, const char* utf8) {
  return ((id(*)(Class, SEL, const char*))objc_msgSend)(
      ns_string_class, sel_registerName("stringWithUTF8String:"), utf8);
}

static id crtgfx_msgsend_date_with_interval(Class ns_date_class, double seconds_from_now) {
  return ((id(*)(Class, SEL, double))objc_msgSend)(
      ns_date_class, sel_registerName("dateWithTimeIntervalSinceNow:"), seconds_from_now);
}

static id crtgfx_msgsend_next_event(
    id app, SEL op, NSUInteger mask, id until_date, id mode, BOOL dequeue) {
  return ((id(*)(id, SEL, NSUInteger, id, id, BOOL))objc_msgSend)(
      app, op, mask, until_date, mode, dequeue);
}

static id crtgfx_msgsend_init_window(
    id window, SEL op, NSRect rect, NSUInteger style, NSUInteger backing, BOOL defer) {
  return ((id(*)(id, SEL, NSRect, NSUInteger, NSUInteger, BOOL))objc_msgSend)(
      window, op, rect, style, backing, defer);
}

/* windowShouldClose:/windowWillClose:/windowDidResize: -- the same three
 * NSWindowDelegate hooks Win32's crtgfx_window_proc() handles via
 * WM_CLOSE/WM_DESTROY/WM_SIZE. Defining a real (if tiny) Objective-C
 * class at runtime via objc_allocateClassPair()/class_addMethod() is the
 * standard, supported way to receive AppKit delegate callbacks from
 * plain C -- verified directly on this host (a standalone probe
 * registered an equivalent class, stored a marker pointer in an ivar,
 * and confirmed both the ivar round-trip and each method actually
 * dispatching through the ObjC runtime before this landed here). */
static id crtgfx_delegate_toplevel(id self) {
  void* value = 0;
  object_getInstanceVariable(self, "crtgfxToplevel", &value);
  return (id)value;
}

static BOOL crtgfx_delegate_window_should_close(id self, SEL _cmd, id sender) {
  crtgfx_weston_toplevel* toplevel;
  (void)_cmd;
  (void)sender;
  toplevel = (crtgfx_weston_toplevel*)crtgfx_delegate_toplevel(self);
  crtgfx_weston_toplevel_note_close(toplevel);
  return 1;
}

static void crtgfx_delegate_window_will_close(id self, SEL _cmd, id notification) {
  crtgfx_weston_toplevel* toplevel;
  (void)_cmd;
  (void)notification;
  toplevel = (crtgfx_weston_toplevel*)crtgfx_delegate_toplevel(self);
  crtgfx_weston_toplevel_note_close(toplevel);
}

static void crtgfx_delegate_window_did_resize(id self, SEL _cmd, id notification) {
  crtgfx_weston_toplevel* toplevel;
  id window;
  id content_view;
  NSRect bounds;
  (void)_cmd;

  toplevel = (crtgfx_weston_toplevel*)crtgfx_delegate_toplevel(self);
  if (toplevel == 0) {
    return;
  }
  window = crtgfx_msgsend_id(notification, sel_registerName("object"), 0);
  content_view = crtgfx_msgsend_op(window, sel_registerName("contentView"));
  bounds = crtgfx_msgsend_rect(content_view, sel_registerName("bounds"));
  crtgfx_weston_toplevel_note_size(
      toplevel, (uint32_t)bounds.size.width, (uint32_t)bounds.size.height);
}

static Class crtgfx_ensure_delegate_class(void) {
  Class cls;

  if (crtgfx_delegate_class != 0) {
    return crtgfx_delegate_class;
  }
  cls = objc_allocateClassPair(objc_getClass("NSObject"), "CrtgfxWindowDelegate", 0);
  class_addIvar(cls, "crtgfxToplevel", sizeof(void*), 3, "^v");
  class_addMethod(
      cls, sel_registerName("windowShouldClose:"),
      (IMP)crtgfx_delegate_window_should_close, "c@:@");
  class_addMethod(
      cls, sel_registerName("windowWillClose:"),
      (IMP)crtgfx_delegate_window_will_close, "v@:@");
  class_addMethod(
      cls, sel_registerName("windowDidResize:"),
      (IMP)crtgfx_delegate_window_did_resize, "v@:@");
  objc_registerClassPair(cls);
  crtgfx_delegate_class = cls;
  return cls;
}

/* NSApplication setup is process-global and only needs doing once, the
 * same way crtgfx_register_window_class() lazily registers Win32's
 * window class once. setActivationPolicy:/finishLaunching are what let
 * a plain command-line-launched process (no .app bundle, no Info.plist)
 * behave like a normal foreground GUI app -- without this a window can
 * be created but never actually gains focus or receives input
 * correctly, a well-known requirement for any Cocoa app bootstrapped
 * outside a bundle. Must run on the main thread, same as every other
 * AppKit call in this file -- Cocoa itself enforces this. */
static id crtgfx_ensure_application(void) {
  static id app;
  static int ready;

  if (ready) {
    return app;
  }
  app = crtgfx_msgsend_class_op(objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
  crtgfx_msgsend_void_int(app, sel_registerName("setActivationPolicy:"),
                          CRTGFX_NSAPPLICATIONACTIVATIONPOLICY_REGULAR);
  crtgfx_msgsend_void(app, sel_registerName("finishLaunching"));
  ready = 1;
  return app;
}

int crtgfx_host_window_create(const crtgfx_window_desc* desc, crtgfx_weston_toplevel* toplevel) {
  id app;
  Class window_class;
  Class delegate_class;
  id window;
  id delegate;
  id content_view;
  id layer;
  id title;
  NSRect rect;
  NSUInteger style;
  crtgfx_host_window* host;

  if (desc == 0 || toplevel == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }

  app = crtgfx_ensure_application();
  if (app == 0) {
    return CRTGFX_ERROR_HOST;
  }

  host = (crtgfx_host_window*)calloc(1, sizeof(*host));
  if (host == 0) {
    return CRTGFX_ERROR_HOST;
  }
  host->toplevel = toplevel;

  rect.origin.x = 0;
  rect.origin.y = 0;
  rect.size.width = (CGFloat)desc->width;
  rect.size.height = (CGFloat)desc->height;
  style = CRTGFX_NSWINDOW_STYLE_TITLED | CRTGFX_NSWINDOW_STYLE_CLOSABLE |
          CRTGFX_NSWINDOW_STYLE_MINIATURIZABLE | CRTGFX_NSWINDOW_STYLE_RESIZABLE;

  window_class = objc_getClass("NSWindow");
  window = crtgfx_msgsend_class_op(window_class, sel_registerName("alloc"));
  window = crtgfx_msgsend_init_window(
      window, sel_registerName("initWithContentRect:styleMask:backing:defer:"), rect, style,
      CRTGFX_NSBACKINGSTORE_BUFFERED, 0);
  if (window == 0) {
    free(host);
    return CRTGFX_ERROR_HOST;
  }
  /* See this file's own top comment: without this, AppKit's default
   * "release the window when its close button is used" behavior can
   * deallocate `window` out from under crtgfx_host_window_destroy()'s
   * own, separately-owned release below. */
  crtgfx_msgsend_void_bool(window, sel_registerName("setReleasedWhenClosed:"), 0);

  title = crtgfx_msgsend_string_with_utf8(objc_getClass("NSString"), desc->title);
  crtgfx_msgsend_void_id(window, sel_registerName("setTitle:"), title);

  content_view = crtgfx_msgsend_op(window, sel_registerName("contentView"));
  crtgfx_msgsend_void_bool(content_view, sel_registerName("setWantsLayer:"), 1);
  layer = crtgfx_msgsend_op(content_view, sel_registerName("layer"));
  crtgfx_msgsend_void_id(layer, sel_registerName("setContentsGravity:"), (id)kCAGravityResize);

  delegate_class = crtgfx_ensure_delegate_class();
  delegate = crtgfx_msgsend_class_op(delegate_class, sel_registerName("alloc"));
  delegate = crtgfx_msgsend_op(delegate, sel_registerName("init"));
  object_setInstanceVariable(delegate, "crtgfxToplevel", (void*)toplevel);
  crtgfx_msgsend_void_id(window, sel_registerName("setDelegate:"), delegate);

  host->window = window;
  host->delegate = delegate;
  toplevel->host = host;

  crtgfx_weston_toplevel_note_size(toplevel, desc->width, desc->height);

  if ((desc->flags & CRTGFX_WINDOW_VISIBLE) != 0) {
    return crtgfx_host_window_show(host);
  }
  return CRTGFX_OK;
}

void crtgfx_host_window_destroy(crtgfx_host_window* host) {
  if (host->window != 0) {
    crtgfx_msgsend_void_id(host->window, sel_registerName("setDelegate:"), 0);
    crtgfx_msgsend_void(host->window, sel_registerName("close"));
    crtgfx_msgsend_void(host->window, sel_registerName("release"));
  }
  if (host->delegate != 0) {
    crtgfx_msgsend_void(host->delegate, sel_registerName("release"));
  }
  free(host);
}

int crtgfx_host_window_show(crtgfx_host_window* host) {
  id app;

  if (host == 0 || host->window == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  crtgfx_msgsend_void_id(host->window, sel_registerName("makeKeyAndOrderFront:"), 0);
  app = crtgfx_ensure_application();
  crtgfx_msgsend_void_bool(app, sel_registerName("activateIgnoringOtherApps:"), 1);
  return CRTGFX_OK;
}

static long crtgfx_now_ms(void) {
  struct crtgfx_cocoa_timespec ts;
  clock_gettime(CRTGFX_CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int crtgfx_host_window_dispatch(uint32_t timeout_ms) {
  id app;
  id pool;
  long deadline;
  NSUInteger any_event_mask;

  app = crtgfx_ensure_application();
  pool = crtgfx_msgsend_class_op(objc_getClass("NSAutoreleasePool"), sel_registerName("alloc"));
  pool = crtgfx_msgsend_op(pool, sel_registerName("init"));

  any_event_mask = ~(NSUInteger)0;
  deadline = crtgfx_now_ms() + (long)timeout_ms;
  for (;;) {
    long now;
    id until_date;
    id event;

    now = crtgfx_now_ms();
    if (now >= deadline) {
      until_date = crtgfx_msgsend_class_op(objc_getClass("NSDate"), sel_registerName("distantPast"));
    } else {
      double interval_seconds = (double)(deadline - now) / 1000.0;
      until_date = crtgfx_msgsend_date_with_interval(objc_getClass("NSDate"), interval_seconds);
    }
    event = crtgfx_msgsend_next_event(
        app, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"), any_event_mask,
        until_date, (id)NSDefaultRunLoopMode, 1);
    if (event == 0) {
      break;
    }
    crtgfx_msgsend_void_id(app, sel_registerName("sendEvent:"), event);
  }

  crtgfx_msgsend_void(pool, sel_registerName("release"));
  return CRTGFX_OK;
}

int crtgfx_host_window_get_size(crtgfx_host_window* host, uint32_t* out_width, uint32_t* out_height) {
  id content_view;
  NSRect bounds;

  if (host == 0 || out_width == 0 || out_height == 0) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  if (host->window != 0) {
    content_view = crtgfx_msgsend_op(host->window, sel_registerName("contentView"));
    if (content_view != 0) {
      bounds = crtgfx_msgsend_rect(content_view, sel_registerName("bounds"));
      crtgfx_weston_toplevel_note_size(
          host->toplevel, (uint32_t)bounds.size.width, (uint32_t)bounds.size.height);
    }
  }
  *out_width = host->toplevel->width;
  *out_height = host->toplevel->height;
  return CRTGFX_OK;
}

static void crtgfx_present_release_callback(void* info, const void* data, unsigned long size) {
  (void)info;
  (void)size;
  free((void*)data);
}

int crtgfx_host_window_present_software(
    crtgfx_host_window* host, const void* pixels, uint32_t width, uint32_t height, uint32_t stride) {
  id content_view;
  id layer;
  unsigned long buffer_size;
  void* copy;
  CGColorSpaceRef color_space;
  CGDataProviderRef provider;
  CGImageRef image;

  if (host == 0 || host->window == 0 || pixels == 0 || width == 0 || height == 0 ||
      stride < width * 4u) {
    return CRTGFX_ERROR_INVALID_ARGUMENT;
  }
  content_view = crtgfx_msgsend_op(host->window, sel_registerName("contentView"));
  if (content_view == 0) {
    return CRTGFX_ERROR_HOST;
  }
  layer = crtgfx_msgsend_op(content_view, sel_registerName("layer"));
  if (layer == 0) {
    return CRTGFX_ERROR_HOST;
  }

  /* See this file's own top comment ("Present-path safety") for why
   * this copies rather than wrapping the caller's buffer in place. */
  buffer_size = (unsigned long)stride * (unsigned long)height;
  copy = malloc(buffer_size);
  if (copy == 0) {
    return CRTGFX_ERROR_HOST;
  }
  memcpy(copy, pixels, buffer_size);

  color_space = CGColorSpaceCreateDeviceRGB();
  provider = CGDataProviderCreateWithData(0, copy, buffer_size, crtgfx_present_release_callback);
  if (color_space == 0 || provider == 0) {
    if (provider != 0) {
      CGDataProviderRelease(provider);
    } else {
      free(copy);
    }
    if (color_space != 0) {
      CGColorSpaceRelease(color_space);
    }
    return CRTGFX_ERROR_HOST;
  }
  image = CGImageCreate(
      width, height, 8, 32, stride, color_space,
      CRTGFX_CGBITMAP_BYTE_ORDER_32_LITTLE | CRTGFX_CGIMAGE_ALPHA_PREMULTIPLIED_FIRST, provider, 0, 1,
      0);
  CGColorSpaceRelease(color_space);
  CGDataProviderRelease(provider);
  if (image == 0) {
    return CRTGFX_ERROR_HOST;
  }

  /* Explicit CATransaction begin/commit, not a bare setContents: call:
   * flushes synchronously regardless of run-loop idle state and skips
   * the default implicit-fade animation on every frame. Kept for that
   * genuine benefit, but was NOT the fix for this file's real frame-
   * visibility bug (a clock_gettime() symbol collision elsewhere) --
   * see this file's own top comment ("Frame-visibility bug") and
   * HISTORY.md for the full account of what the real bug actually was. */
  crtgfx_msgsend_class_op(objc_getClass("CATransaction"), sel_registerName("begin"));
  crtgfx_msgsend_class_void_bool(
      objc_getClass("CATransaction"), sel_registerName("setDisableActions:"), 1);
  crtgfx_msgsend_void_id(layer, sel_registerName("setContents:"), (id)image);
  crtgfx_msgsend_class_op(objc_getClass("CATransaction"), sel_registerName("commit"));
  CGImageRelease(image);
  return CRTGFX_OK;
}
