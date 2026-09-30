# LVGL provenance

`crtui` renders through LVGL as a **private** dependency. This directory holds
the pin, not the source: `tools/fetch_lvgl.py` (CMake target `crtui-lvgl-fetch`)
fetches, verifies and extracts it at build time, and `-DCRTUI_ENABLE_LVGL=ON`
compiles it into `libcrtui` (Tranche 1 of
[`docs/crtui_acceptance.md`](../../../docs/crtui_acceptance.md)).

| Field | Value |
| --- | --- |
| Version | 9.6.0 (released 2026-09-16) |
| Tag / commit | `v9.6.0` / `80ca777e37a2b176770726a02e07a6fb79ef0b39` |
| Archive | `https://github.com/lvgl/lvgl/archive/refs/tags/v9.6.0.tar.gz` |
| Archive SHA-256 | `b20ee3acc1bba13c62d854f9ebd62e4c51e0b443b1e0225892e86442defa84df` (111,467,067 bytes) |
| License | MIT (`LICENCE.txt`, Copyright (c) 2025 LVGL Kft) |
| Verified | 2026-09-30, from the GitHub API and the downloaded archive; see `recipe.json` |

The tag, commit, version header, license and archive hash were all checked
against the real release rather than taken from a planning document.
`recipe.json` is the machine-readable record, including the rule that
`expected_commit` is authoritative if GitHub ever regenerates the tarball.

Policy: never patch LVGL's source; keep LVGL headers out of every installed SDK
(enforced by `tools/verify_dist.py`); no LVGL GPU backend.
