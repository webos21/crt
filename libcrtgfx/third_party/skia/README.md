# Skia External Build Metadata

This directory is project-owned metadata, not an upstream Skia checkout. The
source/build declaration itself lives in `libcrtgfx/CMakeLists.txt`'s own
`CRTGFX_SKIA_*` cache variables (repository, pinned ref, sparse-checkout
paths) plus `tools/fetch_skia.py`/`tools/build_skia.py` -- the same
"declarative source description + a dedicated engine" shape as
`libstdc++/third_party/{libcxx,libcxxabi,libunwind}/recipe.json`'s own
recipes, just expressed as CMake cache variables rather than a JSON file:
Skia's own build is GN/Ninja-based, not CMake, so reusing `tools/crt-
libcxx-build.py`'s CMake-specific engine (or inventing a parallel JSON schema
for a genuinely different build shape) was rejected the same way that file's
own module docstring already rejects reusing `tools/crt-port-build.py`'s
generic porting-recipe engine for it.

- **Pinned, not floating** (since 2026-08-21): `CRTGFX_SKIA_REF`/
  `CRTGFX_SKIA_EXPECTED_COMMIT` default to a real commit SHA
  (`13ffba253fc7854fd3b34f67c82dfb2418dc2944`), captured via `git ls-remote
  https://skia.googlesource.com/skia.git refs/heads/chrome/m148` that day --
  not "whatever `refs/heads/chrome/m148` currently resolves to," the
  previous default. That specific commit's own message ("Remove CQ for
  unsupported branch refs/heads/chrome/m148") confirms the branch was
  already frozen/unsupported by Skia's own infra at pin time, verified for
  real rather than assumed. `CRTGFX_SKIA_VERSION="m148"` is still the
  human-readable label; `CRTGFX_SKIA_REF` is what actually gets fetched.
  Re-pin by re-running the same `git ls-remote` command and updating both
  CMake cache values (and this file's own note), never by reverting either
  to empty/floating.
- **Sparse-checkout, not a full clone**: `CRTGFX_SKIA_SPARSE_PATHS` (cone
  mode: `bin`, `build_overrides`, `client_utils`, `gn`, `include`, `modules`,
  `specs`, `src`, `third_party`, `toolchain`) was derived empirically via
  `ninja -t inputs skia` against a real build of this project's own
  CPU-raster-only GN config, not guessed -- confirmed for real to drop the
  fetch from 260MB+ (an untrimmed, unshallowed working tree) to ~103MB, with
  a genuinely successful `libskia.a` (21MB) build against the trimmed
  checkout. `modules/` had to be kept whole rather than trimmed to just the
  one module actually linked (`modules/skcms`): `gn gen` needs to at least
  *load* every module's own `BUILD.gn` to evaluate its own `enabled =
  skia_enable_<x>` condition, even for a disabled module, confirmed for real
  via a failed `gn gen` ("Unable to load ... modules/skottie/BUILD.gn")
  before this was understood.
- **`git-sync-deps` is OFF by default** (`CRTGFX_SKIA_SYNC_DEPS`), and this
  project's own default GN config needs it not at all: confirmed for real
  that it unconditionally downloads Skia's *entire* third-party dependency
  set per its own `DEPS` file regardless of which GN features are actually
  enabled (8.6GB and climbing, including a full Emscripten/WASM toolchain,
  before being killed) -- and separately confirmed, via `ninja -t inputs
  skia` again, that the CPU-raster-only config in `tools/build_skia.py`'s
  own `default_gn_args()` needs zero `third_party/externals/` content at
  all once `skia_use_wuffs` is also explicitly disabled (Skia's own GIF
  decoder default, `skia_use_wuffs = true` in `gn/skia.gni`, is the one
  codec flag this project's config did not already turn off alongside every
  other optional codec -- fixed alongside this pin).
- `crtgfx-skia-fetch` clones the selected upstream source into
  `out/<preset>/external/skia/src/` and records the resolved commit (and now
  the sparse-checkout paths) in `.crt-skia-fetch.json` there.
- `crtgfx-skia-configure` and `crtgfx-skia-build` generate GN output below
  that same checkout and install `libskia.a` into
  `out/<preset>/external/skia/install/`. Both auto-bootstrap a pinned `gn`
  binary via Skia's own `bin/fetch-gn` (itself pinned to a specific
  `git_revision` hardcoded in that upstream script) when `bin/gn`/`bin/
  gn.exe` is not already present and no `--gn`/`CRTGFX_SKIA_GN` override was
  given -- confirmed necessary on Windows, where neither a `gn` binary nor a
  `python3`-named executable (which GN's own `.gn` dotfile hardcodes as
  `script_executable`) exists by default; `tools/build_skia.py` also
  provisions a throwaway `python3.bat` PATH shim for exactly that second gap
  on Windows, without touching any real system PATH.
- The build uses `tools/crt-cc`, `tools/crt-c++`, and `tools/crt-ar` against
  the active CRT sysroot. `crt-ar` expands GN response files before calling a
  host archiver because Apple `ar` does not support them natively. Windows GN
  invokes the matching `tools/crt-ar.cmd` entry point.
- Do not patch the fetched upstream source to make it build. A failure first
  identifies a missing CRT/PAL/C++ runtime surface and follows the project
  porting loop in `AGENTS.md`.

Skia's public headers are exposed from the fetched checkout after a successful
build; this metadata directory never pretends to provide substitute headers.
