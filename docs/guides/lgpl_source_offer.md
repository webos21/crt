# LGPL Source Offer

**Status: implemented; owner review in progress (decision 1 recorded 2026-10-21, see section 5). Not legal advice.** The facts below come from the SDK inventories and
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
* **FFmpeg inside the shared `libcrtmedia`.** The isolated `04-gfx-media` stage (and so `05-ui` and `06-web`, which
  inherit its tree) installs a relink kit, `share/crt/relink/libcrtmedia/`: the CRT-side objects the shared library was
  linked from (`objects/`, with SHA-256 in `MANIFEST.json`), the export list the link needs, `relink.json` (the link
  command the build used, with `@SDK@`, `@OBJ@`, `@OUT@` placeholders) and `relink.py` (standard library only). In a
  shell where the SDK is activated, `python relink.py --ffmpeg-libdir <your FFmpeg libs> --out <dir>` links a new
  `libcrtmedia` against your `libavcodec.a`, `libavformat.a`, `libavutil.a` and `libswresample.a`; `--print` shows the
  command. The host libraries the link names (Windows SDK import libraries; `CRT_WINDOWS_SDK_LIBPATH` must be set) are
  not part of the kit. In `06-web` the private copy under `libexec/crtweb/lib/` is the same binary and is replaced with
  the relinked one. Rebuilding from the CRT repository (`tools/crt-port-build.py` with `porting/recipes/ffmpeg.json`,
  then the stage build) stays available.

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

Decided 2026-10-21 and implemented the same day: **FFmpeg inside the shared `libcrtmedia`** is handled by shipping the
relinkable objects (section 3), not by relying on the rebuild instructions alone. `tools/crt_relink_kit.py` takes the
link command from the stage build (`ninja -t commands`), rewrites it to placeholders, copies the objects and fails if a
build-tree path survives; `build_stage_04_gfx_media.py` writes the kit after install and declares it as `relink_kit` on
the `ffmpeg` dependency; `verify_dist.py` rejects an isolated-stage SDK that ships FFmpeg without a complete kit
(in-tree developer SDKs have none). Evidence on Windows/x64: from a real stage build (`gfx-media-build`), the kit
relinked `libcrtmedia.dll` (same size as the SDK's, exports identical except one symbol that comes from the newer
Windows SDK import library on this machine) and also with `--ffmpeg-libdir` pointing at a separate copy of the four
archives. Not verified: the Linux and macOS kits (same code path, output names `lib/libcrtmedia.so` and `.dylib`; no
host to run them) and an end-to-end stage rebuild that exercises the new step. Tests: `tools/test_crt_relink_kit.py`.

Decided 2026-10-21: **dual-licensed components.** FreeType is used under the FreeType License (FTL), not the GPLv2
alternative; Mbed TLS under Apache-2.0, not GPL-2.0-or-later. Both are permissive, neither makes the combined SDK
GPL, and the FTL needs only its text and a credit line. The SDK now says so itself: the `04-gfx-media` stage builder
(`record_license_choices`) installs `share/licenses/freetype/FTL.TXT` (previously not shipped, although FreeType's
`LICENSE.TXT` points at it), `share/licenses/freetype/CRT-LICENSE-CHOICE.txt` with the credit line the FTL requires
("Portions of this software are copyright (c) 2026 The FreeType Project (https://freetype.org). All rights
reserved.", year read from the pinned source) and `share/licenses/mbedtls/CRT-LICENSE-CHOICE.txt`, all listed in the
manifest notices; the `06-web` stage now also collects `FTL.TXT` from its private FreeType. Tests:
`tools/test_license_choices.py`. Takes effect in SDKs regenerated from this tree (not yet rebuilt). The SBOM still
declares `NOASSERTION` for ports; filling their license fields is a separate, optional improvement.

Decided 2026-10-21: **flagged source-tree files** (GPL, GPL-with-exception and MPL classes of `license-scan.json`). Checked
object by object, not through the SBOM, against the Windows/x64 WebKit 2.54.1 build tree (`build.ninja`, CMake cache,
unified sources) of the isolated `06-web` build:

* **Not built** (no compile or link input in the build): pdf.js (118 MPL locale and image files; `ENABLE_PDFJS=OFF`), the
  ANGLE `flex-bison` skeletons and m4 files (host tools, GPL) and the ANGLE bison outputs `glslang_tab_autogen.*`
  and `preprocessor_tab_autogen.cpp` (`ENABLE_WEBGL=OFF`, no `ANGLE/src/compiler` input), libwebrtc and its yasm files,
  capstone (`.clang-format` only) and the libsoup `directory.js` resource (CRT uses curl). They stay in the upstream
  archive the offer names and nowhere in the binaries.
* **Built, and acceptable:** `Source/WTF/wtf/text/StringBuilderJSON.cpp`, `StringBuilderJSON.h`, `EscapedFormsForJSON.h`
  (MPL-2.0, file-level copyleft; their source is in the WebKit archive of the offer, and the MPL allows them in a larger
  work), and `Source/WebCore/xml/XPathGrammar.cpp`/`.h` (Bison output compiled through `UnifiedSource-xml-2.cpp`; the
  GPL's Bison special exception lets the parser be part of a larger work under other terms). Both are now named in the
  generated `share/licenses/SOURCE-OFFER.md` of `06-web` ("Two other licenses apply to files compiled into the WebKit
  binaries"); test: `tools/test_crt_source_offer.py`.

Limits of the check: one host (Windows/x64) and one configuration; the macOS and Linux builds use the same CMake options
but were not re-inspected. A change of those options (for example WebGL or the PDF viewer) or of the WebKit pin repeats
this check. No copy of the MPL text is installed: the three file headers carry the Exhibit A notice and the URL, and MPL-2.0
Section 3.2 asks for a statement of how to get the source of the Executable Form, which the offer now gives. Say so if you
want the license text installed anyway.

Decided 2026-10-21: **notice and header decisions.** The four attributed header-less files of
`third_party/bionic/header_decisions.json` are approved as recorded (`gdtoa_fltrnds.h`, `s_llrint.c` and `s_llrintf.c`
are covered by the headed file they include; `s_signgam.c`, three lines, is attributed to FreeBSD msun without a license
text). Two further findings of the NOTICE review were fixed: `include/regex.h` is CRT-written and no longer carries the
Henry Spencer and UCB copyright lines (those apply to the implementation in `libc/src/regex`, whose BSD-3 text is in
the NOTICE); and `tools/crt_runtime_notices.py` no longer takes a descriptive comment that merely contains the word
"permissions" (`libc/src/arch/macos/common/jit_permissions.c`) for a license header. The NOTICE now has 34 groups for
137 headed files (was 36 and 139); test: `tools/test_crt_runtime_notices.py`.

Decided 2026-10-21: **already published binaries.** The release `v0.4.0-preview.1` (2026-09-21) carries `04-gfx-media` SDKs
with FFmpeg 8.1.2 and predates this offer. The distribution policy has changed completely since, so the owner will
delete that release from the distribution (done by the owner on GitHub, not by tooling) instead of adding the offer to
it; no release is offered without the generated `SOURCE-OFFER.md` from now on (`verify_dist.py` and the release tool
enforce that). The git tag is a source reference and is not an object-code distribution.

Still open:

1. **SDK coverage of the generator:** macOS/arm64, Windows/x64 and Linux/aarch64 SDKs were regenerated with it
   (2026-10-20/21); Windows/arm64 has not been (needs an arm64 host).
2. **Release gate:** `prepare_release_assets.py` exports `05-ui` and `06-web` behind the owner-controlled
   `web_public_release` switch in `third_party/crt-runtime/release.json` (a final tag refuses a development candidate);
   the switch stays off until the Tranche 10 conditions are met.
