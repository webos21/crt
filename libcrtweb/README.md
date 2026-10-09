# libcrtweb

The CRT-owned Web Runtime API (stage `06-web`): a WebKit CRT Port (`PlatformCRT`)
plus the `crtweb` runtime/view API and the `crtui` WebView on top of it. Contract,
tranche order and evidence: [`docs/crtweb_acceptance.md`](../docs/crtweb_acceptance.md);
upstream mapping: [`docs/crtweb_porting.md`](../docs/crtweb_porting.md).

**Status: bring-up.** Web Tranche 0 (scope, version and license freeze) is closed.
Tranche 1 (JavaScriptCore) is green through 1C (interpreter and Baseline JIT) on
Linux/x86_64, Linux/aarch64, macOS/arm64 and Windows/x64. 1D-A (DFG/concurrent JIT) is green
on all four hosts; 1D-B (FTL/B3), 1D-C (WebAssembly) and 1D-D (sampling profiler) are green
on Linux/x86_64, macOS/arm64 and Windows/x64. Linux/aarch64 is the remaining 1D-B/C/D replay.
All are built by `tools/build_webkit_jsc.py` from the verified pin against an installed SDK.
W^X remains separate hardening. No WebCore, WebKit, `PlatformCRT` or `crtweb` API code exists yet, and
`libcrtweb` is not part of the root CMake build.

## Layout

- `tests/jsc/` -- the JavaScriptCore acceptance: `jsc_acceptance.js` and
  `jsc_jit_acceptance.js` (run through the `jsc` shell), `jsc_context_cycle.cpp` (VM
  create/use/release on several threads) and `jsc_watchdog_test.cpp` (terminating compiled
  code with a signal-based trap).
- `third_party/webkit/` -- the pinned WPE WebKit 2.54.0 reference: `recipe.json`
  (URL, size, SHA-256, signed tag and commit, license and security policy),
  `license-inventory.json` (the archive's 64 license/notice files and bundled
  third-party trees) and `README.md` (provenance). The source itself is never
  vendored; `tools/fetch_webkit.py` downloads and verifies it.

Planned, following the tranche order (none of it exists):

- `include/crtweb/` -- the public API. No WebKit, WPE or GLib type may appear here;
  `tools/verify_dist.py` will enforce it for `06-web` as it does for LVGL in `05-ui`.
- `src/` -- the runtime/view implementation over the CRT-owned platform.
- `platform/` -- `PlatformCRT`, the new WebKit CRT port (not a WPEPlatform backend),
  backed by `crtgfx`, `crtui` external surfaces, `crtmedia` and the CRT PAL. WPEPlatform
  appears only in the Linux first-green prototype (Tranche 3A) and is not the
  cross-platform architecture; see `docs/crtweb_porting.md`.
- `tests/` and `tools/` -- per-tranche acceptance tests and demos, as in the other
  libraries.

## Tools

- `tools/build_webkit_jsc.py` -- fetch, extract, configure, build and accept JavaScriptCore
  (JSCOnly) with the CRT toolchain; writes `webkit-jsc-config.json` (the inputs of a run) and
  a host-ABI audit of what the binaries load.
- `tools/fetch_webkit.py` -- download and verify the pinned archive
  (`tools/test_fetch_webkit.py`).
- `tools/scan_webkit_licenses.py` -- regenerate the license inventory from the
  verified archive.
