# libcrtweb

The CRT-owned Web Runtime API (stage `06-web`): a WebKit CRT Port (`PlatformCRT`)
plus the `crtweb` runtime/view API and the `crtui` WebView on top of it. Contract,
tranche order and evidence: [`docs/acceptance/crtweb_acceptance.md`](../docs/acceptance/crtweb_acceptance.md);
upstream mapping: [`docs/porting/crtweb_porting.md`](../docs/porting/crtweb_porting.md).

**Status: bring-up.** Web Tranche 0 (scope, version and license freeze) is closed.
Tranche 1 (JavaScriptCore) is closed on Linux/x86_64, Linux/aarch64, macOS/arm64
and Windows/x64 through interpreter, Baseline/DFG/FTL, WebAssembly and the sampling
profiler. Tranche 2 is closed on Linux/x86_64: the verified, unmodified WPE WebKit
2.54.0 source renders a local fixture through WPEPlatform's built-in headless
backend and captures the accepted 640x480 reference frame. W^X remains separate
hardening. Tranche 3A (Linux/x86_64) has frozen the frame-producer and input contracts and
proves them with a WPEPlatform `crt` display module and a CRT-side consumer. No `PlatformCRT`
or `crtweb` API code exists yet; the root CMake build only adds the consumer-side test.

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
- `tests/wpe-reference/` -- the Tranche 2 native Linux fixture and headless WPE
  harness. It is deliberately a host program, not a CRT program.
- `patches/` -- carried WebKit patches and `manifest.json` (schema 2: per-file license class,
  `new_files`), checked by `tools/check_webkit_patch_manifest.py`.
- `third_party/webkit/` -- the pinned WPE WebKit 2.54.0 reference: `recipe.json`
  (URL, size, SHA-256, signed tag and commit, license and security policy),
  `license-inventory.json` (the archive's 64 license/notice files and bundled
  third-party trees), `license-scan.json` (per-file header classes of the product source) and
  the `product_source` pin of the full commit (`tools/fetch_webkit_commit.py`) and `README.md` (provenance). The source itself is never
  vendored; `tools/fetch_webkit.py` downloads and verifies it.

Current and planned layout, following the tranche order:

- `include/crtweb/` -- the public API. No WebKit, WPE or GLib type may appear here;
  `tools/verify_dist.py` will enforce it for `06-web` as it does for LVGL in `05-ui`.
- `src/` -- the runtime/view implementation over the CRT-owned platform.
- `platform/` -- `PlatformCRT`, the new WebKit CRT port (not a WPEPlatform backend),
  backed by `crtgfx`, `crtui` external surfaces, `crtmedia` and the CRT PAL. WPEPlatform
  appears only in the Linux first-green prototype (Tranche 3A) and is not the
  cross-platform architecture; see `docs/porting/crtweb_porting.md`.
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
