# crtweb Acceptance (Stage `06-web`)

**Status: in progress -- Tranche 0 (scope, version and license freeze) is closed;
Tranche 1 (JavaScriptCore bring-up) is next. Nothing is built or imported yet.** Detailed contract and tranche order for the
WebKit-based web runtime. It depends on `05-ui`'s External Surface contract
([`crtui_acceptance.md`](crtui_acceptance.md)), which is accepted on all three
hosts (2026-10-03). Upstream mapping lives in [`crtweb_porting.md`](crtweb_porting.md). Evidence is recorded
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

**Status (2026-10-03): closed.** The reference is pinned in
`libcrtweb/third_party/webkit/recipe.json` (provenance in `libcrtweb/third_party/webkit/README.md`) and
verified: the archive was downloaded and its SHA-256 recomputed
(`efa9bcc3...eb452`, 46,202,080 bytes, equal to the release page); the signed tag
`wpewebkit-2.54.0` verifies with `gpg` (signer Adrian Perez de Castro, key
`5AA3BC33...123B`, not web-of-trust certified) and points at commit `73f39d84...`;
six sampled source files are byte-identical to that commit; and the 64 license and
notice files of the archive are inventoried in `libcrtweb/third_party/webkit/license-inventory.json`
(`tools/scan_webkit_licenses.py`). The upstream mapping is in `crtweb_porting.md`
and the security policy below was confirmed by the owner. Still unverified, and
carried as explicit items in the recipe: per-file license headers, tarball-equals-
commit beyond the sampled files, and upstream's security support window.

**Frozen scope and version.** Reference: WPE WebKit **2.54.0** (2026-09-16), the
current stable series. CRT targets the stable WPEPlatform API and does not use the
deprecated libwpe-based API (`ENABLE_WPE_LEGACY_API=OFF`). Build system: CMake with
Ninja (upstream now requires it). 2D rendering is Skia only; threaded painting is
mandatory upstream. The v1 scope above is unchanged.

**Licensing.** WebKit is licensed per file and bundles third-party components. The
archive carries LGPL-2 and LGPL-2.1 texts (WebCore, JavaScriptCore's `COPYING.LIB`),
an Apple BSD-style 2-clause notice, and license files for bundled Skia (BSD-3-style,
milestone 154), ANGLE, pdf.js, gtest and others (inventory above). Every `06-web` SDK
must carry the notices, the exact source revision and archive hash, and enough
information for source correspondence (the pinned recipe plus the carried-patch
list). Because parts are LGPL, the proposal is to keep WebKit as separately built,
separately replaceable libraries rather than merging it into application binaries;
that is confirmed or changed with the packaging decisions in Tranche 10.

**Security and update policy (confirmed by the owner 2026-10-03).**
1. Upstream publishes WebKitGTK/WPE advisories (`WSA-YYYY-NNNN`, the 2026 ones
   dated 2026-03-18, 03-28, 06-02, 07-10 and 09-29 at the time of writing) and
   fixes them in the current stable series. CRT pins one stable series and moves
   to the newest micro release of it.
2. Every new advisory is checked against the pin by version ("affected: before
   X"); an affected pin is bumped and re-verified (`tools/fetch_webkit.py`) before
   any further `06-web` release asset is published. A bump is a recipe change plus
   a re-run of the stage gates, never an in-place edit of a built tree.
3. The advisory feed (`https://wpewebkit.org/security/`) and the `webkit-wpe`
   mailing list are the tracked sources; a release asset records the WebKit
   version and the newest advisory it was checked against.
4. An SBOM (package, version, license, source URL, hash) is generated with each
   `06-web` SDK and declared in its manifest, like the other redistributed
   dependencies; the SBOM format is chosen at Tranche 10.
5. When the pinned series stops receiving upstream fixes, moving to the next
   stable series is a planned tranche item, not a silent bump. The length of
   upstream's support window is not documented on the pages consulted and must
   be confirmed before the first public `06-web` release.

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
