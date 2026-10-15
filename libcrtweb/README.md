# libcrtweb

The CRT-owned Web Runtime API (stage `06-web`): a WebKit CRT Port (`PlatformCRT`)
plus the `crtweb` runtime/view API and the `crtui` WebView on top of it. Contract,
tranche order and evidence: [`docs/acceptance/crtweb_acceptance.md`](../docs/acceptance/crtweb_acceptance.md);
upstream mapping: [`docs/porting/crtweb_porting.md`](../docs/porting/crtweb_porting.md).

The public C API (`include/crtweb/crtweb.h`, `src/crtweb.cpp`) and out-of-process
`crtweb_host` (`tools/crtweb_host.cpp`) serve the `crtui` WebView
`crtweb::WebView` (`include/crtweb/webview.h`) over the web surface/control protocol.
The product also registers CRT-owned playback and permission-gated video-camera
sources over the installed `crtmedia` APIs. Host acceptance, lifecycle/resource
measurements and media limits live in the acceptance document linked above;
active work lives in [`TODO.md`](../TODO.md), not a second checklist here.
A packaged `06-web` SDK remains the separate Tranche 10 deliverable.

## Layout

- `tests/jsc/` -- the JavaScriptCore acceptance: `jsc_acceptance.js` and
  `jsc_jit_acceptance.js` (run through the `jsc` shell), `jsc_context_cycle.cpp` (VM
  create/use/release on several threads) and `jsc_watchdog_test.cpp` (terminating compiled
  code with a signal-based trap).
- `platform/` -- the Tranche 3A contracts and prototype: `crtweb_surface_wire.h` (the frozen
  wire protocol, plain C), `crtweb_surface_client.h` (the consumer adapter for a `crtui`
  SurfaceView) and `wpe-crt/wpe_crt_platform.c` (a GIO module registering the `crt` WPEDisplay).
- `tests/platform-crt/` -- `surface_probe.c` (CRT-built consumer), `wpe_crt_host.c` (native
  WebKitWebView on the `crt` display), `crt_surface.html`, and `web_surface_test.cc` (the
  hermetic `crtui` SurfaceView test run by CTest). Driver: `tools/build_webkit_wpe_crt.py`.
- `tests/webkit/crtweb_media_test.cpp` -- HTML media through the CRT playback engine (Tranche 8B); `platform/va-stub/` -- the
  VA-API stubs that keep a host driver library out of the Web process.
- `tests/webcore/` and `tests/webkit/` -- product-port graph/paint/text and
  multi-process acceptance programs used by B3 and B4. Driver:
  `tools/build_webkit_crt_port.py`.
  `crtweb_api_lifecycle_test` covers public API recovery, shutdown, navigation while
  resizing and 100x runtime/view cycles; `tests/platform-crt/webview_test.cc` covers
  recovery and resource ownership through the `crtui` WebView and its host.
- `tests/wpe-reference/` -- the Tranche 2 native Linux fixture and headless WPE
  harness. It is deliberately a host program, not a CRT program.
- `port/` -- the CRT-owned files of the `PlatformCRT` port, mirroring the WebKit tree (`Source/cmake/
  OptionsCRT.cmake`, per-module `PlatformCRT.cmake`); copied over the pinned commit by
  `tools/build_webkit_jsc.py --port CRT`. Declared under `new_files` in the patch manifest.
- `patches/` -- carried WebKit patches and `manifest.json` (schema 2: per-file license class,
  `new_files`), checked by `tools/check_webkit_patch_manifest.py`.
- `third_party/webkit/` -- the pinned WPE WebKit 2.54.0 reference: `recipe.json`
  (URL, size, SHA-256, signed tag and commit, license and security policy),
  `license-inventory.json` (the archive's 64 license/notice files and bundled
  third-party trees), `license-scan.json` (per-file header classes of the product source) and
  the `product_source` pin of the full commit (`tools/fetch_webkit_commit.py`) and `README.md` (provenance). The source itself is never
  vendored; `tools/fetch_webkit.py` downloads and verifies it.

Current and planned layout, following the tranche order:

- `include/crtweb/` -- the public API: `crtweb.h` (plain C runtime/view API) and `webview.h` (the
  header-only `crtui` WebView). No WebKit, WPE or GLib type may appear here; the driver's webview gate
  checks the headers and `libcrtweb.so`'s exports, and `tools/verify_dist.py` will enforce it for `06-web`
  as it does for LVGL in `05-ui`. Rules: `docs/design/libcrtweb_api_policy.md`.
- `src/` -- the runtime/view implementation over `PlatformCRT` (`crtweb.cpp`, the only file that
  sees WebKit); built by the port's CMake into `libcrtweb.so`.
- `tools/` -- `crtweb_host.cpp`, one web view as a process speaking the web surface protocol.
- `port/` -- `PlatformCRT`, the new WebKit CRT port (not a WPEPlatform backend),
  composed over the pinned full WebKit source. It is being connected to `crtgfx`,
  `crtui` external surfaces, `crtmedia` and the CRT PAL tranche by tranche.
  WPEPlatform appears only in the Linux first-green prototype (Tranche 3A) and
  is not the cross-platform architecture; see `docs/porting/crtweb_porting.md`.
- `tests/` and `tools/` -- per-tranche acceptance tests and demos, as in the other
  libraries.

## Tools

- `tools/build_webkit_jsc.py` -- fetch, extract, configure, build and accept JavaScriptCore
  (JSCOnly) with the CRT toolchain; writes `webkit-jsc-config.json` (the inputs of a run) and
  a host-ABI audit of what the binaries load.
- `tools/build_webkit_wpe_reference.py` -- build the pristine WPE port with the
  native Linux toolchain, install it into a private work prefix, and validate a
  local HTML DOM/canvas proof plus a 640x480 headless snapshot.
- `tools/fetch_webkit.py` -- download and verify the pinned archive
  (`tools/test_fetch_webkit.py`).
- `tools/scan_webkit_licenses.py` -- regenerate the license inventory from the
  verified archive.
