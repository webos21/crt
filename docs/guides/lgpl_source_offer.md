# LGPL Source Offer

**Status: implemented; owner review pending. Not legal advice.** The facts below come from the SDK inventories and
the build recipes (checked against the macOS/arm64 `06-web` development SDK of 2026-10-20). The offer text itself is
no longer written by hand: `tools/crt_source_offer.py` generates `share/licenses/SOURCE-OFFER.md` in every SDK that
ships FFmpeg (and WebKit in `06-web`), and `tools/prepare_release_assets.py` adds the corresponding-source bundle to
the release. `public_release_allowed` stays `false` until the Tranche 10 release conditions are met.

## 1. Which parts are LGPL

Only two components of the SDK stages carry LGPL code that is compiled into the redistributed binaries. Everything
else is permissive or project-owned (see `share/licenses/*` and `share/crt/web-sbom.spdx.json` in the SDK).

| Component | Version | License evidence | In the SDK as | Linked how |
| --- | --- | --- | --- | --- |
| WPE WebKit (WebCore, JavaScriptCore, WTF, WebKit; the Skia and bmalloc inside are BSD) | 2.54.1 | The license scan of the pinned `Source/` tree (69,575 files) finds `LGPL-2+` 3,040, `LGPL-2.1+` 77, `LGPL-2` 14, `LGPL` 6 and `LGPL-2.1` 1 files next to BSD-2/3-Clause (44,577) and Apache-2.0 files. The counts are of the **source tree**, not of the objects in the binary. | `06-web` only: `libexec/crtweb/lib/libWebKit.dylib.SOVERSION` (`.so` on Linux, a DLL on Windows) and the helper processes `WebProcess`, `NetworkProcess`, `crtweb_host` | **Private shared library** beside its helpers. Applications link only `libcrtweb` (CRT's own API, BSD-style); they never link WebKit. |
| FFmpeg (libavcodec, libavformat, libavutil, libswresample) | 8.1.2 | Built with the recipe's narrow configuration, **never** `--enable-gpl`/`--enable-nonfree`; `configure` reports "License: LGPL version 2.1 or later". Text: `share/licenses/ffmpeg/COPYING.LGPLv2.1`. | `04-gfx-media` and later: static archives `lib/libavcodec.a`, `libavformat.a`, `libavutil.a`, `libswresample.a`, and the same code built into the shared `libcrtmedia` (`.dylib`/`.so`/`.dll`) | Static archives, separate from `libcrtmedia.a`; inside the shared `libcrtmedia` (private, not exported: only `crtmedia_*`). |

Not LGPL and therefore outside this offer, but with their own notices in the SDK: OpenSSL (Apache-2.0), curl,
mbedTLS (used under Apache-2.0 of its Apache-2.0-or-GPL-2.0+ dual license), FreeType (used under the FreeType
License), fontconfig, HarfBuzz, ICU (Unicode), libxml2, expat, libpsl, libwebp, libjpeg-turbo, libpng, zlib, SQLite,
libwpe, Skia, LVGL, the LLVM C++ runtime (Apache-2.0 with LLVM exception), and the C library/shell components under
`share/licenses/crt-libc`, `mksh`, `toybox`, `onetrue-awk`.

## 2. What the corresponding source is

| Component | Corresponding source |
| --- | --- |
| WebKit 2.54.1 | (a) the upstream release archive `https://wpewebkit.org/releases/wpewebkit-2.54.1.tar.xz` (46,206,356 bytes, SHA-256 `0de8bdfba011f9cd63ba9a9171fa6038dd2c08237894f4082abbb821f29d3fe9`; the upstream commit is `537776966344c4982751692dd475c94124504271`); (b) the **23 carried patches** and their manifest (`libcrtweb/patches/`, shipped in the SDK as `share/crt/dependencies/webkit/patches/`) with the per-file SHA-256 before/after; (c) the CRT-owned port overlay `libcrtweb/port/` (shipped as `share/crt/dependencies/webkit/port/`); (d) the build recipes and drivers (`tools/crt-stage-build.py`, `tools/build_stage_06_web.py`, `porting/recipes/*` of the private dependency ports, listed in `share/crt/web-source-correspondence.json`) and the exact `06-web` source asset named there by SHA-256. |
| FFmpeg 8.1.2 | (a) the unmodified upstream archive `https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz` (SHA-256 `464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c`); (b) the build recipe `porting/recipes/ffmpeg.json` (configure options, no patches). |

The CRT repository itself (`https://github.com/webos21/crt`) holds (b)-(d). Binary releases must name the repository
commit and the stage-source asset hashes that built them (`manifest.json` and `web-source-correspondence.json` already
do).

## 3. How a recipient replaces or relinks the LGPL code

* **WebKit (private shared library).** Rebuild the private closure with `tools/crt-stage-build.py` using the
  source recipe and predecessor SDK named in `web-source-correspondence.json` (the reproduction command is in
  `docs/acceptance/crtweb_acceptance.md`, "isolated build" sections), after changing the WebKit tree or a patch, then
  replace `libexec/crtweb/` (library **and** the matching helper processes) together: the SDK is built so that
  nothing but `libcrtweb` depends on them (`web-source-correspondence.json`: "Replace the private WebKit/runtime
  libraries and matching helper processes together; never link them into the application").
* **FFmpeg, static link.** An application that links `libcrtmedia.a` links `libavcodec.a`, `libavformat.a`,
  `libavutil.a` and `libswresample.a` as separate archives: build a modified FFmpeg with the same recipe and relink
  the application with your archives in place of the SDK's.
* **FFmpeg inside the shared `libcrtmedia`.** Rebuild `libcrtmedia` from the CRT repository with your FFmpeg install
  (`tools/crt-port-build.py` with `porting/recipes/ffmpeg.json`, then the stage build) and substitute it.

## 4. How the offer is produced

| Input | Source | Set by |
| --- | --- | --- |
| Version / tag, stage | the SDK's `VERSION` and manifest | the release (`--release-version`) |
| Release page, bundle name | `repository_url` + `/releases/tag/<tag>`; `crt-<tag>-corresponding-source.tar.xz` | derived |
| Upstream archive URL, SHA-256, size; number of carried patches; commit | `porting/recipes/ffmpeg.json`, `libcrtweb/third_party/webkit/recipe.json`, `libcrtweb/patches/manifest.json` (read from the installed SDK in `06-web`) | the pins |
| Where to ask, how long | `third_party/crt-runtime/release.json`: `issue_tracker_url` (the project's GitHub issue tracker), `offer_years` (3, the LGPL minimum; lower values are rejected) | the owner, once |

Where the text lands: `create_dist.py` writes it into stages that redistribute FFmpeg (04-gfx-media, 05-ui);
`build_stage_06_web.py` renders it again for `06-web` from the SDK's own recipes, adding WebKit; `verify_dist.py`
fails an SDK whose offer is missing, still has `{{...}}`/placeholder text, names another version or stage, or lacks
the SHA-256 of a component's archive. A development build points at the repository, not at a release page.

**The bundle** `crt-<tag>-corresponding-source.tar.xz` is the tracked tree of the release commit (CRT sources, the 23
carried patches, the PlatformCRT overlay, every recipe and build tool) plus `CORRESPONDING-SOURCE.json` (commit, tag,
and each upstream archive with address and SHA-256). It is built from a clean tree only. Decision of 2026-10-20: the
**upstream archives are not copied into the bundle**; the generated "Written offer" section therefore promises a copy
on request through the issue tracker, for the offer period, if an upstream address stops working. Keep the SHA-256
values: they are what lets a recipient (or the project) fetch the same bytes later.

## 5. Decisions

Settled 2026-10-20: contact channel = the project's issue tracker; period = 3 years, fixed; upstream archives are not
included in the bundle.

Still open:

1. **FFmpeg inside the shared `libcrtmedia`:** whether the combined library satisfies LGPL section 6 with the rebuild
   instructions above (permissive CRT code combined with LGPL code in one library), or the shared library must also ship
   relinkable objects. Optional external review.
2. **Dual-licensed components:** record the license chosen for mbedTLS (Apache-2.0) and FreeType (FreeType License).
3. **Source-tree files that are not shipped:** the GPL-with-exception bison skeletons under
   `Source/ThirdParty/ANGLE/tools`, the MPL files (121) and any other flagged class in `license-scan.json` are in the
   upstream archive. This guide assumes none is compiled into a shipped binary; that was checked for the build inputs
   through the SBOM, not object by object.
4. **Windows and Linux/aarch64 SDKs:** the same generator applies, but those SDKs have not been regenerated with it, so
   the lists above were verified on macOS/arm64 only.
5. **Release gate:** `prepare_release_assets.py` still publishes only `01-c`..`04-gfx-media`; offering `05-ui` and
   `06-web` and the switch that lets `public_release_allowed` become true are separate Tranche 10 work. The
   release owner reviews this guide and `share/licenses/crt-libc/NOTICE` (including the four attributed header-less files in
   `third_party/bionic/header_decisions.json`); an external legal review is optional.
