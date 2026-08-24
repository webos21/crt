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
round trip, not just a version-string check) and, longer term, by `libcrtgfx`'s own Skia
`SkFontMgr_custom` integration once that lands -- see
`docs/runtime_roadmap.md`/`docs/libcrtgfx_wayland_plan.md` for the broader
text-rendering plan this is part of.
