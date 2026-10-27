# WPE WebKit provenance

The CRT Web Runtime (`06-web`, [`docs/acceptance/crtweb_acceptance.md`](../../../docs/acceptance/crtweb_acceptance.md))
is a **WebKit CRT Port** (`PlatformCRT`). WPE WebKit is the reference it is built
from and compared against. This directory holds the **pin**, not the source:
no upstream source is vendored here. Native evidence for earlier pins remains
in Web acceptance; the 2.54.2 replay is tracked in
[release qualification](../../../../docs/acceptance/release_qualification.md).

| Field | Value |
| --- | --- |
| Version | WPE WebKit 2.54.2, stable micro release (2026-10-23); re-pinned on 2026-10-27 |
| Tag / commit | `wpewebkit-2.54.2` (signature-verified annotated tag `6c294e37d744618c3f9983b44c7759b8265ecf63`) / `32717fabcb9dcad9aa00a54ef1f23e41ee64e849` |
| Archive | `https://wpewebkit.org/releases/wpewebkit-2.54.2.tar.xz` |
| Archive size | 49,565,492 bytes |
| Archive SHA-256 | `f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56` -- recomputed from the downloaded archive and equal to the release page |
| Signatures | Both detached `.tar.xz.asc` and annotated Git tag verify under key `5AA3BC334FD7E3369E7C77B291C559DBE4C9123B` (DSA-1024, expires 2027-03-16; signer Adrian Perez de Castro, not web-of-trust certified). Complete tarball/commit correspondence is measured independently |
| License | mixed per-file LGPL and BSD plus bundled components; 64 license/notice files inventoried in `license-inventory.json`; per-file headers of the product source scanned in `license-scan.json` |
| Bundled | its own Skia (milestone **154**; CRT uses m148), ANGLE, pdf.js, gtest, libsysprof-capture, xdgmime, unifdef |

`recipe.json` is the machine-readable record, including what is still unverified.
`tools/fetch_webkit.py` downloads the archive and verifies size and SHA-256
(`tools/test_fetch_webkit.py` covers it); `tools/scan_webkit_licenses.py` regenerates
`license-inventory.json` from the verified archive. `tools/fetch_webkit_commit.py
--correspondence` hashes every tarball file against the commit tree: 34,445 files
are identical, 130 differ only by CRLF, zero differ in content. `license-scan.json`
classifies 69,600 files from the full product source.

## Why 2.54

It is the current stable series (the development series is 2.53.92 at the time
of writing). Its release notes make WPEPlatform the stable, default embedding API,
deprecate the libwpe-based API, move the web-process compositor from TextureMapper
to Skia (Cairo is removed), require Ninja, and add `WPEProcessManager` /
`WPEProcessLaunchOptions` for embedder control of auxiliary processes. The latest
security advisory at the pin date, WSA-2026-0006, lists versions before 2.54.0 as
affected (2.54.0 was the first pin; 2.54.2 is the current pin). WPEPlatform is
the reference/prototype API; the product uses PlatformCRT.

## Policy

- **Never patch upstream.** A missing CRT/PAL surface is fixed in CRT
  (`AGENTS.md`). A carried patch needs a row in
  [`docs/porting/crtweb_porting.md`](../../../docs/porting/crtweb_porting.md) with its upstream
  reference and removal condition.
- **No WebKit/WPE/GLib type in a public CRT header.** `tools/verify_dist.py` will
  enforce this for the `06-web` stage as it does for LVGL in `05-ui`.
- **Security and bumps.** See Tranche 0 in `docs/acceptance/crtweb_acceptance.md`.
