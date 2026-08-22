# Skia External Build Metadata

This directory is project-owned metadata, not an upstream Skia checkout. The
source declaration itself lives in `recipe.json` (schema modeled after
`libstdc++/third_party/{libcxx,libcxxabi,libunwind}/recipe.json`'s own
"declarative source description + a dedicated engine" shape, since Skia's own
build is GN/Ninja-based, not CMake -- see `recipe.json`'s own `engine` key for
why reusing `tools/crt-libcxx-build.py`'s CMake-specific engine, or inventing
a parallel schema for a genuinely different build shape, was rejected the same
way that file's own module docstring already rejects reusing `tools/crt-port-
build.py`'s generic porting-recipe engine for it).

`libcrtgfx/CMakeLists.txt` reads `recipe.json`'s own `source.*` fields at
configure time (via CMake's `string(JSON ...)`) to populate the
`CRTGFX_SKIA_VERSION`/`CRTGFX_SKIA_REF`/`CRTGFX_SKIA_REPOSITORY`/
`CRTGFX_SKIA_EXPECTED_COMMIT`/`CRTGFX_SKIA_SPARSE_PATHS`/`CRTGFX_SKIA_SYNC_DEPS`
CACHE variable *defaults* -- `recipe.json` is the single source of truth for
what gets fetched; those CACHE variables remain the override mechanism (pass
`-D` on the command line to fetch something else without editing the recipe).

- **Pinned, not floating** (since 2026-08-21, recorded in `recipe.json` since
  2026-08-22): `source.ref`/`source.expected_commit` are a real commit SHA,
  not a floating branch. See `recipe.json`'s own `notes` array for the full
  pinning/re-pinning/sparse-checkout/sync-deps rationale -- this file no
  longer duplicates it.
- `crtgfx-skia-fetch` clones the selected upstream source into
  `out/<preset>/external/skia/src/` and records the resolved commit (and the
  sparse-checkout paths) in `.crt-skia-fetch.json` there.
- `crtgfx-skia-configure` and `crtgfx-skia-build` generate GN output below
  that same checkout and install `libskia.a` into
  `out/<preset>/external/skia/install/`. Both auto-bootstrap a pinned `gn`
  binary via Skia's own `bin/fetch-gn` (itself pinned to a specific
  `git_revision` hardcoded in that upstream script) when `bin/gn`/`bin/
  gn.exe` is not already present and no `--gn`/`CRTGFX_SKIA_GN` override was
  given.
- The build uses `tools/crt-cc`, `tools/crt-c++`, and `tools/crt-ar` against
  the active CRT sysroot, routed through the project-owned imported libc++
  (see `TODO.md`'s dated 2026-08-22 sub-bullet and `HISTORY.md`'s matching
  entry for the toolchain-wiring work this took). `crt-ar` expands GN
  response files before calling a host archiver because Apple `ar` does not
  support them natively. Windows GN invokes the matching `tools/crt-ar.cmd`
  entry point via `tools/crt-cc.cmd`/`tools/crt-c++.cmd`.
- **Do not patch the fetched upstream Skia *source* (.cc/.cpp/.h) to make a
  compile error go away.** A failure first identifies a missing CRT/PAL/C++
  runtime surface and follows the project porting loop in `AGENTS.md` --
  `recipe.json`'s own `patches` array is reserved for build-*configuration*-
  file edits only (currently one: the fetched `.gn` dotfile's own
  `script_executable` line), a materially different thing from patching
  Skia's own C++ implementation. See `recipe.json`'s own notes for the full
  distinction.

Skia's public headers are exposed from the fetched checkout after a successful
build; this metadata directory never pretends to provide substitute headers.
