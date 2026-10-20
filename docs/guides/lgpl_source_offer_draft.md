# LGPL Source Offer (DRAFT)

**Status: draft for the Tranche 10 publication gate. Not legal advice, not yet adopted.** It was written from the
SDK inventories and the build recipes (engineering facts, checked against the macOS/arm64 `06-web` development SDK of
2026-10-20); every `[PLACEHOLDER]` and every item in "Decisions needed" must be settled by the project owner and
legal review before any binary SDK is published. `public_release_allowed` stays `false` until then.

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

## 4. The offer (text for the SDK, to be placed in `share/licenses/SOURCE-OFFER.md` and the release notes)

> **Written offer for corresponding source.** This SDK contains software licensed under the GNU Lesser General
> Public License: WPE WebKit 2.54.1 (as modified by the CRT Project) and FFmpeg 8.1.2. You may obtain the complete
> corresponding source code of these components, the CRT Project's modifications, and the build recipes needed to
> rebuild and replace them, from **[PLACEHOLDER: permanent URL of the source bundle for this exact release]**, or
> by writing to **[PLACEHOLDER: postal/e-mail contact]** and asking for it on a physical medium, for which we may
> charge no more than the cost of physically performing the distribution. This offer is valid for **[PLACEHOLDER:
> at least three years]** from the date you received this SDK, and for as long as we offer updates or support for
> the release. The upstream archives and their SHA-256 values, the CRT patches, and the commit and source-asset
> hashes that built this SDK are listed in `share/crt/web-source-correspondence.json` and
> `share/crt/dependencies/*/recipe.json`. The full text of the LGPL is in `share/licenses/ffmpeg/COPYING.LGPLv2.1`
> and under `share/licenses/webkit/Source/`. You may modify these components and replace them in your copy of the
> SDK as described in `docs/guides/lgpl_source_offer_draft.md` section 3 [PLACEHOLDER: final location of that text].

## 5. Decisions needed (not engineering)

1. **Hosting and contact:** the permanent location of the source bundle per release, who answers physical-medium
   requests, and the retention period (the draft says at least three years from distribution).
2. **Bundle versus pointer:** whether the source bundle (the two upstream archives, the patch set, the overlay and the
   recipes, a few tens of MB) ships inside every binary distribution, which would make a written offer unnecessary, or
   is hosted.
3. **FFmpeg inside `libcrtmedia` shared libraries:** whether the combined library is acceptable under LGPL section 6
   with the rebuild instructions above (the permissive CRT code is combined with LGPL code in one library), or
   whether the shared `libcrtmedia` must also ship relinkable objects.
4. **Dual-licensed components:** record the license chosen for mbedTLS (Apache-2.0) and FreeType (FreeType License).
5. **Source-tree files that are not shipped:** the GPL-with-exception bison skeletons under
   `Source/ThirdParty/ANGLE/tools`, the MPL files (121) and any other flagged class in `license-scan.json` are in
   the source archive. This draft assumes none is compiled into a shipped binary; that has been checked for the
   build inputs only through the SBOM, not object by object.
6. **Windows and Linux/aarch64 SDKs:** the same offer applies, but their SDKs have not been regenerated with the
   inherited-runtime notices yet (Tranche 10 TODO), so the lists above were verified on macOS/arm64 only.
7. **Legal sign-off** of this text and of `share/licenses/crt-libc/NOTICE` (including the four attributed header-less
   files in `third_party/bionic/header_decisions.json`).
