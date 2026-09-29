# crtweb Acceptance (Stage `06-web`)

**Status: planned, not started.** Detailed contract and tranche order for the
WebKit-based web runtime. It depends on `05-ui`'s External Surface contract
([`crtui_acceptance.md`](crtui_acceptance.md)); nothing here begins before that
stage is accepted, except the JavaScriptCore replay noted in Tranche 1. Upstream
mapping lives in [`crtweb_porting.md`](crtweb_porting.md). Evidence is recorded
here as each tranche closes; completed work goes to `../HISTORY.md` and the
current tranche state to `../TODO.md`.

## Purpose

Build a **WebKit CRT Port** (`PlatformCRT`) -- not a straight port of WPE to
three operating systems. WPE WebKit is the reference implementation to compare
against and to borrow structure from; WebKit already has separate ports (Apple,
GTK/WPE, PlayStation, Windows, JSCOnly), so a CRT port fits its architecture.

```text
                 WebKit
  +--------------------------------------+
  | WTF  JavaScriptCore  WebCore  WebKit |
  | WebProcess  NetworkProcess  GPUProcess |
  +------------------+-------------------+
                     |
                PlatformCRT
                     |
          +----------+----------+
          |          |          |
       CRT PAL     crtgfx    crtmedia
          |          |          |
          +------ CRT network --+
```

Application code sees only `libcrtweb` and the `crtui` WebView; no WebKit, WPE,
or GLib type appears in a public header.

## Scope

Required for v1: HTML, CSS, DOM, JavaScript, navigation, HTTP/HTTPS, images,
fonts/text, mouse/keyboard, scrolling, Canvas 2D, basic cookies/storage.

Deferred: WebRTC, WebGPU, WebXR, EME/DRM, camera/microphone, printing,
extensions, full accessibility, downloads.

**Version and licensing (verify at Tranche 0).** The reference candidate is the
current WPE WebKit stable series (2.54, where WPEPlatform is the stable
embedding API -- confirm against the release notes before pinning). Record the
exact pinned revision, source provenance, LGPL/BSD notices, and source
correspondence. WebKit ports own their release and security cadence, so a
security-update policy is a deliverable, not an afterthought.

## Tranches and gates

### 0. Scope, version, and license freeze

Pin the reference release and write `crtweb_porting.md`'s upstream mapping.
Decide the SBOM/CVE-tracking policy now.

### 1. JavaScriptCore / JSCOnly bring-up

Prerequisite bring-up, not an application scripting layer. Linux first, then a
quick replay on Windows and macOS, because it surfaces threads, TLS, virtual
and executable memory, signals/exceptions, GC, and JIT problems in the lower
runtime early. JIT-off first-green is acceptable; enabling the JIT follows.

### 2. Linux WPE reference baseline

Build upstream WPE unchanged and render local HTML with its own built-in
backend. No CRT integration; the purpose is a known-good baseline to diff
against when `PlatformCRT` misbehaves.

### 3. `PlatformCRT` graphics and input prototype

Shared-memory output first: WebKit rendering -> `PlatformCRT` adapter ->
`crtgfx` -> `crtui` external surface. Then GPU-buffer output through the same
external-surface contract. Use WPEPlatform as the structural reference.

### 4. `libcrtweb` and the WebView

`crtweb_runtime_create`, `crtweb_view_create/load_url/load_html/go_back/
go_forward/reload/set_size/set_focus`, and title/load/navigation callbacks,
plus `crtui`'s web view on top. Application code sees no WebKit type.

### 5. Multi-process lifecycle

UIProcess with WebProcess, NetworkProcess, and GPUProcess: launch, IPC, clean
shutdown, forced WebProcess termination and recovery/reload, 100x view
create/destroy, navigation while resizing, resource-leak audit. Closing this on
Linux is the proof that the port's architecture holds.

### 6. Windows/x64 replay

The same WebKit and `PlatformCRT` on the CRT PAL (not a WPE-for-Windows
effort). Focus on process spawning, IPC handles, shared memory, executable
pages, D3D12 surface integration, fonts, IME/input, TLS, DLL boundaries.

### 7. macOS/arm64 replay

Same WebKit, same `PlatformCRT`, same `libcrtweb` API. Apple's native WebKit
framework is deliberately not used as a shortcut -- that would not be a
cross-platform CRT port.

### 8. CRT subsystem substitution

Only after a working browser. Replace, one at a time, the components WPE
brought with it: WebKit's network backend -> CRT networking; WebCore's media
backend -> `crtmedia`; capture -> `crtmedia` capture. Do not remove
GLib/libsoup/GStreamer in one step; that mixes a browser port with subsystem
rewrites.

### 9. GPU integration

Start with WebKit's Skia output to an external surface consumed by CRT's Skia.
Later optimize to a shared/zero-copy GPU buffer import. Sharing one Skia
context between WebKit and CRT is explicitly not an initial acceptance item.

### 10. Distribution and security closure

Isolated `06-web` build from the installed `05-ui` SDK: JSC/WebCore/WebKit ->
`libcrtweb` -> a browser sample, `verify_dist.py`, dependency/RPATH audit, and
publication only after all three hosts pass. Include the provenance/SBOM/CVE
policy artifacts named in Tranche 0.

## Host order

Linux first (WPE is the reference), with an early three-host JavaScriptCore
replay; then Windows/x64; then macOS/arm64.
