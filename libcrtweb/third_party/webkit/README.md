# WPE WebKit provenance

The CRT Web Runtime (`06-web`, [`docs/acceptance/crtweb_acceptance.md`](../../../docs/acceptance/crtweb_acceptance.md))
is a **WebKit CRT Port** (`PlatformCRT`). WPE WebKit is the reference it is built
from and compared against. This directory holds the **pin**, not the source:
nothing here is vendored, fetched into a build, or compiled yet (Web Tranche 0).

| Field | Value |
| --- | --- |
| Version | WPE WebKit 2.54.0, first release of the 2.54 stable series (2026-09-16) |
| Tag / commit | `wpewebkit-2.54.0` (annotated, PGP-signed, object `1616e9ae260d8d0601da601de24cd8d96adc5e0f`) / `73f39d84ea9d4071994214373efbde3665402b05` |
| Archive | `https://wpewebkit.org/releases/wpewebkit-2.54.0.tar.xz` |
| Archive size | 46,202,080 bytes (confirmed by HTTP `HEAD`) |
| Archive SHA-256 | `efa9bcc3cb891c2d88f50eec710d9ccee71cbdf1040420361eb98c17355eb452` -- recomputed from the downloaded archive and equal to the release page |
| Tag signature | valid; signer Adrian Perez de Castro, key `5AA3BC334FD7E3369E7C77B291C559DBE4C9123B` (DSA-1024, expires 2027-03-16; not web-of-trust certified) |
| License | mixed per-file LGPL and BSD plus bundled components; 64 license/notice files inventoried in `license-inventory.json`; per-file headers not scanned |
| Bundled | its own Skia (milestone **154**; CRT uses m148), ANGLE, pdf.js, gtest, libsysprof-capture, xdgmime, unifdef |

`recipe.json` is the machine-readable record, including what is still unverified.
`tools/fetch_webkit.py` downloads the archive and verifies size and SHA-256
(`tools/test_fetch_webkit.py` covers it); `tools/scan_webkit_licenses.py` regenerates
`license-inventory.json` from the verified archive. Six sampled source files were
compared with the tagged commit and are byte-identical; the tarball is not otherwise
proven equal to the commit tree.

## Why 2.54

It is the current stable series (the development series is 2.53.92 at the time
of writing). Its release notes make WPEPlatform the stable, default embedding API,
deprecate the libwpe-based API, move the web-process compositor from TextureMapper
to Skia (Cairo is removed), require Ninja, and add `WPEProcessManager` /
`WPEProcessLaunchOptions` for embedder control of auxiliary processes. The latest
security advisory at the pin date, WSA-2026-0006, lists versions before 2.54.0 as
affected. CRT therefore targets WPEPlatform only and does not use the legacy API.

## Policy

- **Never patch upstream.** A missing CRT/PAL surface is fixed in CRT
  (`AGENTS.md`). A carried patch needs a row in
  [`docs/porting/crtweb_porting.md`](../../../docs/porting/crtweb_porting.md) with its upstream
  reference and removal condition.
- **No WebKit/WPE/GLib type in a public CRT header.** `tools/verify_dist.py` will
  enforce this for the `06-web` stage as it does for LVGL in `05-ui`.
- **Security and bumps.** See Tranche 0 in `docs/acceptance/crtweb_acceptance.md`.
