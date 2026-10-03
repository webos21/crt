# libcrtweb

The CRT-owned Web Runtime API (stage `06-web`): a WebKit CRT Port (`PlatformCRT`)
plus the `crtweb` runtime/view API and the `crtui` WebView on top of it. Contract,
tranche order and evidence: [`docs/crtweb_acceptance.md`](../docs/crtweb_acceptance.md);
upstream mapping: [`docs/crtweb_porting.md`](../docs/crtweb_porting.md).

**Status: only the upstream pin exists.** Nothing here is built, imported or
exposed yet, and `libcrtweb` is not part of the root CMake build. Web Tranche 0
(scope, version and license freeze) is closed; the JavaScriptCore bring-up is next.

## Layout

- `third_party/webkit/` -- the pinned WPE WebKit 2.54.0 reference: `recipe.json`
  (URL, size, SHA-256, signed tag and commit, license and security policy),
  `license-inventory.json` (the archive's 64 license/notice files and bundled
  third-party trees) and `README.md` (provenance). The source itself is never
  vendored; `tools/fetch_webkit.py` downloads and verifies it.

Planned, following the tranche order (none of it exists):

- `include/crtweb/` -- the public API. No WebKit, WPE or GLib type may appear here;
  `tools/verify_dist.py` will enforce it for `06-web` as it does for LVGL in `05-ui`.
- `src/` -- the runtime/view implementation over the CRT-owned platform.
- `platform/` -- `PlatformCRT`, the WPEPlatform implementation backed by `crtgfx`,
  `crtui` external surfaces, `crtmedia` and the CRT PAL.
- `tests/` and `tools/` -- per-tranche acceptance tests and demos, as in the other
  libraries.

## Tools

- `tools/fetch_webkit.py` -- download and verify the pinned archive
  (`tools/test_fetch_webkit.py`).
- `tools/scan_webkit_licenses.py` -- regenerate the license inventory from the
  verified archive.
