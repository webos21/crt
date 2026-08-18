# Skia External Build Metadata

This directory is project-owned metadata, not an upstream Skia checkout.

- The default source track is Skia `m148` (`refs/heads/chrome/m148`). CMake
  exposes `CRTGFX_SKIA_VERSION`, `CRTGFX_SKIA_REF`, and
  `CRTGFX_SKIA_EXPECTED_COMMIT` for an explicit reproducible pin.
- `crtgfx-skia-fetch` clones the selected upstream source into
  `out/<preset>/external/skia/src/` and records the resolved commit in
  `.crt-skia-fetch.json` there.
- `crtgfx-skia-configure` and `crtgfx-skia-build` generate GN output below
  that same checkout and install `libskia.a` into
  `out/<preset>/external/skia/install/`.
- The build uses `tools/crt-cc`, `tools/crt-c++`, and `tools/crt-ar` against
  the active CRT sysroot. `crt-ar` expands GN response files before calling a
  host archiver because Apple `ar` does not support them natively. Windows GN
  invokes the matching `tools/crt-ar.cmd` entry point.
- Do not patch the fetched upstream source to make it build. A failure first
  identifies a missing CRT/PAL/C++ runtime surface and follows the project
  porting loop in `AGENTS.md`.

Skia's public headers are exposed from the fetched checkout after a successful
build; this metadata directory never pretends to provide substitute headers.
