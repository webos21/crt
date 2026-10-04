# libcrtweb

The CRT-owned Web Runtime API (stage `06-web`): a WebKit CRT Port (`PlatformCRT`)
plus the `crtweb` runtime/view API and the `crtui` WebView on top of it. Contract,
tranche order and evidence: [`docs/crtweb_acceptance.md`](../docs/crtweb_acceptance.md);
upstream mapping: [`docs/crtweb_porting.md`](../docs/crtweb_porting.md).

**Status: bring-up.** Web Tranche 0 (scope, version and license freeze) is closed.
Tranche 1 (JavaScriptCore) is green on Linux/x86_64 for 1A (the ICU and gperf ports)
and 1B (the interpreter acceptance), built by `tools/build_webkit_jsc.py` from the
verified pin against an installed SDK; the JIT (1C) and the Windows and macOS replays
remain. No WebCore, WebKit, `PlatformCRT` or `crtweb` API code exists yet, and
`libcrtweb` is not part of the root CMake build.

## Layout

- `tests/jsc/` -- the JavaScriptCore acceptance: `jsc_acceptance.js` (run through the
  `jsc` shell) and `jsc_context_cycle.cpp` (VM create/use/release on several threads).
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
