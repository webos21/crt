# Bundled fonts

This directory holds font files this project ships directly (never resolved
via host font discovery -- no fontconfig on Linux, no CoreText on macOS, no
GDI/DirectWrite on Windows -- matching this project's own "own the toolchain,
no host substitute" policy already applied everywhere else).

## DejaVuSansMono.ttf

- Source: [DejaVu Fonts](https://dejavu-fonts.github.io/) 2.37 (2016-07-30,
  the project's last release), downloaded from the official SourceForge
  release (`dejavu-fonts-ttf-2.37.zip`,
  sha256 `7576310b219e04159d35ff61dd4a4ec4cdba4f35c00e002a136f00e96a908b0a`),
  `ttf/DejaVuSansMono.ttf` extracted directly with no modification.
  `DejaVuSansMono.ttf` itself: sha256
  `b4a6c3e4faab8773f4ff761d56451646409f29abedd68f05d38c2df667d3c582`.
- Chosen over DejaVu Sans (the proportional variant) purely because a
  monospace test font makes glyph-advance/bitmap-dimension assertions in a
  smoke test simpler to reason about; no other reason.
- License: see `DejaVu-LICENSE.txt` (Bitstream Vera Fonts Copyright + Arev
  Fonts Copyright + public-domain contributions, a permissive license that
  does not require royalties and allows embedding/redistribution -- see that
  file's own text for the authoritative terms) and `DejaVu-AUTHORS.txt` for
  the upstream credits list, both copied unmodified from the same release
  archive alongside the font itself.

Used by `porting/recipes/freetype.json`'s own `glyph-rasterize-*` port tests
(a real `FT_New_Face()`/`FT_Set_Pixel_Sizes()`/`FT_Load_Char(..., FT_LOAD_RENDER)`
round trip, not just a version-string check) and by `libcrtgfx`'s own Skia
`SkFontMgr_New_Custom_Directory` integration -- see
`docs/runtime_roadmap.md`/`docs/libcrtgfx_wayland_plan.md` for the broader
text-rendering plan this is part of. Kept available as an explicit fallback
family (`crtgfx_skia_default_typeface()`, `crtgfx/skia.h`) once Pretendard
GOV became the project default below.

## PretendardGOV-*.otf

- Source: [Pretendard](https://github.com/orioncactus/pretendard) release
  `v1.3.9` (2023-11-05), the "Pretendard GOV" variant designed for the
  Republic of Korea public-service environment -- see that repo's
  `packages/pretendard-gov/README.md`. Downloaded from the release's own
  unpkg CDN mirror (`unpkg.com/pretendard-gov@1.3.9/dist/public/static/`,
  the same static desktop OTF files the GitHub release's
  `PretendardGOV-1.3.9.zip` asset contains), not a woff/woff2 webfont
  variant and not the variable-font build. All nine static weights are
  bundled (Thin through Black); `PretendardGOV-Regular.otf` is the one
  code currently draws with, the others are staged for when a consumer
  needs a specific weight (headings, emphasis) rather than added and left
  unused. Embedded family name confirmed directly from each file's own
  name table (`grep -a -o` on the binary): `Pretendard GOV`.
  sha256 of each file:

  ```text
  54108c754cfd9b24bae01243778829fc7db0244df983ce543b23ba34ed92c9e9  PretendardGOV-Black.otf
  deeff873beb2e32ccb3378d08f5d37687e0367df092e0be748f586634cfa4495  PretendardGOV-Bold.otf
  7744029c85962d9a17a6d69a8ceb9e42c4e2026a00af83d6b50eb164de4cfc74  PretendardGOV-ExtraBold.otf
  cc797f47222d8f4494edc841cfc6cd16bc3441f9179e7389f5923324358ee47d  PretendardGOV-ExtraLight.otf
  e5937926759ed4c072f3a732f700d9c4eedf136bde80379b442374a716c1c78c  PretendardGOV-Light.otf
  e170ae4b6d91f558b0d3692dbb27b30cb5c3b9c3f719b7670673a28b9552ac58  PretendardGOV-Medium.otf
  3a3f8ee05092d07b25c4c0b4801f78dbac8d1f9362e4610b97033d4128e48310  PretendardGOV-Regular.otf
  43dca6392644a8dd2b29251d04f23ff68af6dbe73b6820f3edb78a2cf8e19234  PretendardGOV-SemiBold.otf
  dc98e78c4e279f78ee3a4cad5e5a1f62de18e739c7331f6277979157e591bf60  PretendardGOV-Thin.otf
  ```
- License: see `PretendardGOV-LICENSE.txt` (SIL Open Font License, Version
  1.1 -- Copyright Kil Hyung-jin, Reserved Font Name "Pretendard"; a
  permissive license that allows embedding, bundling, and redistribution
  without royalties as long as the font itself is not sold standalone),
  copied unmodified from the same release.
- **This project's system default typeface as of 2026-08-29** (explicit
  user direction: adopt Pretendard GOV as the default while keeping
  DejaVu Sans Mono available, not replacing it). `crtgfx_skia_default_
  typeface()` (`crtgfx/skia.h`) resolves it by exact family-name match
  (`matchFamilyStyle("Pretendard GOV", style)`) rather than relying on
  `legacyMakeTypeface(nullptr, ...)`'s directory-scan-order default,
  which would otherwise keep resolving to DejaVu Sans Mono now that two
  real families share this directory (`D` sorts before `P`).
