# crtui Acceptance (Stage `05-ui`)

**Status: contract frozen (Tranche 0, all three hosts); LVGL first pixels
(Tranche 1), input/focus/resize (Tranche 2) and the private-LVGL wrapper
(Tranche 3) closed on all three hosts; layout, styling and the v1 widget set
(Tranche 4) closed on Windows/x64, macOS/arm64 and Linux/x86_64; External
Surface scene metadata, real final composition and host replay (Tranche 5)
closed on all three hosts; MediaView (Tranche 6) closed on all three
hosts (zero-copy VA-API/Vulkan on Linux); isolated-package closure
(Tranche 7) closed on all three hosts.**
The public contract in `libcrtui/include/crtui/ui.h` and the rules in the table
below are frozen and covered by resource-free tests.
Tranches 1-7 add rendering, input mapping, the LVGL-backed widgets, external
surfaces and packaging behind that contract. Update the tranche sections with
evidence as each closes, in the same way `crtmedia_encode_capture_acceptance.md`
and `crtmedia_networking_acceptance.md` do. Completed work is recorded in
`../HISTORY.md`; the current tranche state is in `../TODO.md`.

## Purpose

> Give CRT applications a stable, application-facing UI layer so they never
> call the OS or LVGL directly.

`crtui` is not "LVGL ported to CRT". LVGL is a private implementation
dependency behind CRT-owned headers, exactly as libcurl/mbedTLS sit behind
`crtmedia`'s transport and FFmpeg sits behind `crtmedia`'s codec API.

```text
CRT application
       |
     crtui
       |
 +-----+------------------------+
 |                              |
LVGL widgets            External surfaces
 |                     +--------+--------+
 |                     |        |        |
 |                 VideoView CameraView WebView (06-web)
 +----------+-------------------+
            |
          crtgfx
            |
          Skia
            |
 Vulkan / D3D12 / Metal
```

## Scope and non-goals

In scope: a small set of widgets, layout, CRT-neutral styling, keyboard/
pointer/(touch) input with focus navigation, and an external-surface view that
composes producer-owned GPU surfaces (video first) with the UI.

Out of scope for v1: LVGL's GPU backend, a full style API, tree/table/chart/
on-screen-keyboard widgets until a real consumer needs them, accessibility,
multi-window desktop behavior, and anything WebKit-specific (that arrives in
`06-web` through the External Surface contract below).

## Frozen common contract (Tranche 0)

The public API exposes no LVGL type. `crtui_context` is an opaque pointer;
widgets are opaque 64-bit ids (`crtui_widget`, with `crtui_window` an alias)
that carry a generation, so a stale id is *detected* rather than dereferenced.
Constructors follow the media/gfx idiom (`crtui_window_create`,
`crtui_container_create`, `crtui_text_create`, `crtui_button_create`,
`crtui_slider_create`, `crtui_surface_view_create`, ...). The rules below are
the frozen contract; each is asserted by `crtui_contract_test`.

| Topic | Frozen rule |
| --- | --- |
| Thread ownership | A context belongs to the thread that created it. Every call except `crtui_context_post()` must come from that thread, otherwise `CRTUI_ERROR_WRONG_THREAD` and nothing changes (this includes `crtui_context_destroy()`). `crtui_context_post()` is the single thread-safe entry: it queues a function that `crtui_context_pump()` runs on the UI thread in FIFO order, never inline |
| Lifetime | A parent owns its children; destroying a widget destroys its whole subtree at once and every id in it is invalid from that moment (`CRTUI_ERROR_INVALID_HANDLE`), including from inside a callback. A reused slot never yields an equal id. No reparenting in v1. `crtui_context_destroy()` destroys all widgets and discards posted functions that never ran. Callback `user` pointers are borrowed |
| Events | Input enters through `crtui_context_send_input()`. The event goes to the target's callback, then bubbles to each ancestor up to the window; a callback returning `CRTUI_EVENT_HANDLED` stops the bubble; `event->target` stays the original target. `FOCUS_IN`/`FOCUS_OUT`/`RESIZED` do not bubble. Default actions run only if the event was not handled: Tab/Shift+Tab move focus, arrows move focus spatially (a focused slider consumes Left/Right to step; Up/Down still navigate -- revised in Tranche 2, see below), Enter/Space activate a button, wheel steps a slider, a press on a slider sets it and captures the pointer until the release. Programmatic changes emit no event. Input sent from inside a callback is queued and delivered after the current event finishes (FIFO), never nested; no callback fires for a destroyed widget |
| Focus and input routing | One focus per context. Pointer/wheel goes to the topmost visible widget under the point (later siblings on top, children clipped to every ancestor); a press on a focusable widget focuses it, and releasing over the same button activates it. Key input goes to the focused widget of the addressed window, else the window. Traversal is tree pre-order over widgets that are focusable, enabled and visible (all ancestors too), Tab wraps. When the focused widget is destroyed, hidden or disabled, focus moves to the next eligible widget after it (wrapping), else clears; `FOCUS_OUT` is never sent to a destroyed widget. A disabled widget (or one under a disabled ancestor) receives nothing: the input is dropped, not passed through. A modal window (the most recently made modal wins) drops all input for every other window and refuses focus into them; making a window modal moves focus into it |
| Geometry | Bounds are integer logical pixels relative to the parent. A window's size and DPI scale (physical = logical x scale, scale > 0) come from `crtui_window_set_size()`, which emits `RESIZED` (x,y = size, value = scale in percent). Hit-testing uses the clipped, visible area. Free Window/Container children use application-set bounds; Tranche 4 adds CRT-owned Row/Column/Stack layout, fill/grow/alignment/spacing, hidden-child reflow, and ScrollView/List offsets |
| Binary surface | Only functions declared `CRTUI_API` are exported from the shared library (hidden visibility everywhere else): LVGL symbols are never visible to consumers -- see Tranche 3 |
| Surfaces | `crtui_surface_view_create()` creates the producer-neutral scene placeholder. The view owns only position, size, clip, opacity, visibility, scene z-order, damage and hit-testing, never the producer's frame. `crtui_window_get_surface_layers()` exposes those properties. The optional C++ `crtui_skia` companion maps each stable view id to a producer-owned `SkImage`, renders LVGL UI planes around it, and balances the producer's acquire/release callbacks; core `libcrtui` remains GPU- and producer-neutral |
| Errors | `crtui_result`: `0..-7` numerically identical to `crtmedia_result` (`OK`, `INVALID_ARGUMENT`, `UNSUPPORTED`, `WOULD_BLOCK`, `IO`, `TIMEOUT`, `CANCELLED`, `PROTOCOL`), plus `WRONG_THREAD = -8`, `INVALID_HANDLE = -9`, `STATE = -10`. A failed create always clears its output. No silent failure |

STB/HMI use means remote-style keyboard navigation (Tab/arrow/Enter/Space) is a
first-class input mode from Tranche 2, not a later add-on.

## Tranches and gates

### 0. Contract freeze and stage creation

**Closed 2026-09-30 on Windows/x64, macOS/arm64, and Linux/x86_64.** (details after the gate text below)

Freeze the table above in this document, add the resource-free contract test,
and create the `05-ui` stage. The superseded skeleton was already deleted
(2026-09-29: `libcrtjs/`, `05-js`, `crt-js-*`), so this is a fresh addition,
not a rename: new `libcrtui`, `crt-ui-build/test/dist` targets, `05-ui` in
`tools/create_dist.py`, `tools/verify_dist.py`,
`tools/crt_dist_prerequisites.py`, `tools/prepare_release_assets.py`,
`distribution.md`, root `CMakeLists.txt`, and tests. Pin the LVGL release and
record its license/provenance under `third_party/`; the candidate is LVGL
v9.6.x (public-API separation and system-library-style install are the reason)
-- **verify the exact tag and hash at import time, do not trust this line.**

**Tranche 0 result (Windows/x64, 2026-09-30).** The contract is host-neutral and
resource-free; the macOS/arm64 and Linux replays needed no code change.

- `libcrtui/include/crtui/ui.h` is the frozen public header (no LVGL type);
  `libcrtui/src/core.c` is a headless model of the tree, event routing, focus
  and geometry -- the state LVGL will later render, not a stand-in for it.
- `crtui_contract_test` covers every table row: errors, lifetime (stale ids,
  subtree invalidation, slot reuse), thread ownership (rejection from another
  thread, FIFO `post`/`pump` on the UI thread), event order/bubbling/handled,
  keyboard defaults, focus traversal and recovery, re-entrancy (self-destroy in
  a callback, queued nested input), modal routing, hit-testing/clipping/
  z-order, and resize/DPI. Real result:

  ```text
  crtui_contract_test: ok errors=pass lifetime=pass threads=pass events=pass
      keyboard_focus=pass focus_recovery=pass reentrancy=pass modal=pass
      hit_test=pass geometry=pass
  ```

  Mutation-checked: removing the "handled stops the bubble" rule makes it fail.
- The `05-ui` stage exists: `libcrtui` static and shared libraries, the
  `crt-ui-build`/`crt-ui-test`/`crt-ui-dist` targets, `05-ui` in
  `tools/verify_dist.py` (public header, both libraries, and a check that no
  LVGL header leaks into the SDK) and `tools/crt_dist_prerequisites.py`.
  `crt-ui-dist` builds and verifies `out/<preset>/dist/05-ui` on Windows
  (3 min on top of the existing `04-gfx-media` package). `crtui_*` tests are
  excluded from the default C-stage `ctest` filters like `crtgfx_*`/`crtmedia_*`.
  `05-ui` is not yet offered by `tools/prepare_release_assets.py`.
- LVGL is pinned, not imported: `libcrtui/third_party/lvgl/{recipe.json,
  README.md}` record v9.6.0 (released 2026-09-16, commit
  `80ca777e37a2b176770726a02e07a6fb79ef0b39`, MIT), verified against the GitHub
  API and the downloaded archive (SHA-256 `b20ee3ac...a84df`, 111,467,067
  bytes; `lv_version.h` reads 9.6.0). The archive is ~111 MB because it carries
  tests/docs/examples, so Tranche 1 should import only `src/`, `include/`,
  the licence and the `lv_conf` template.

**Tranche 0 replay (macOS/arm64, 2026-09-30).** No code change was needed; the
contract is host-neutral as intended.

- `crtui_contract_test` prints the identical line as on Windows (all nine
  groups `pass`), with byte-identical output over three runs.
- Mutation check reproduced: dropping the "handled stops the bubble" rule fails
  the test at `contract_test.c:320` and `:491`; reverted afterward.
- Full in-tree `ctest` 153/153 (the camera-authorization test is the expected
  skip in a non-interactive run); tooling tests 79/79.
- `crt-ui-dist` builds and `verify_dist.py --stage 05-ui` passes (about 3 min on
  top of `04-gfx-media`): `libcrtui.dylib` has id `@rpath/libcrtui.dylib`, only
  `@rpath` CRT deps plus `libSystem.B.dylib`, and no LVGL header in the SDK.
- Not covered here by design: the isolated stage build and
  `create_stage_source.py` registration belong to Tranche 7.

**Tranche 0 replay (Linux/x86_64, native Intel host, 2026-09-30).** No code
change was needed.

- `crtui_contract_test` prints the identical all-`pass` line as Windows and
  macOS, byte-identical over five runs (this host exercises the real-pthread
  thread-ownership rows with the CRT's own pthreads).
- Mutation check reproduced: dropping the "handled stops the bubble" rule
  (`libcrtui/src/core.c`, `deliver()` loop condition) fails the test at
  `contract_test.c:320` and `:491`, the same lines as macOS; reverted and
  re-confirmed green (`git diff` clean).
- The LVGL pin was re-checked without downloading anything: `git ls-remote`
  resolves tag `v9.6.0` to commit `80ca777e37a2b176770726a02e07a6fb79ef0b39`,
  matching `recipe.json`. The 111 MB archive and its SHA-256 were *not*
  re-downloaded here; that part rests on the Windows verification.
- `crt-ui-dist` was built in a **fresh, options-default build directory**
  (`cmake --preset linux-host-ninja-debug -B ...`, FFmpeg/curl/Skia OFF, all of
  `01-c` .. `05-ui` from scratch) and every stage `verified`. `verify_dist.py
  --stage 05-ui` passes on the directory and again on the *extracted*
  `crt-development-linux-x86_64-05-ui.tar.xz` (9.2 MB). `libcrtui.so`:
  `NEEDED` only `libm/libdl/libc++/libc`, `RUNPATH $ORIGIN`, SONAME
  `libcrtui.so`, and its undefined symbols are only CRT-versioned libc/pthread
  functions (`@CRT_1.0`) -- no host library. No LVGL header or file is in the
  SDK.
- Full dev-tree `ctest` 146/147 (the one failure is the pre-existing
  no-sound-card `crtmedia_playback_pipeline_test_runs` gap); tooling tests 79/79.
- Finding, *not* a `crtui` defect: running `crt-ui-dist` in the long-lived dev
  tree (`CRTMEDIA_ENABLE_CURL=ON` for the networking work) fails inside the
  predecessor `crt-gfx-media-dist` with `lib/libcrtmedia.so -> undeclared
  libcurl.so.4`. On Linux the in-tree `libcrtmedia.so` deliberately links the
  shared `libcurl.so` (the static zlib is not PIC; `libcrtmedia/CMakeLists.txt`),
  and `verify_dist.py` has no entry for it. The docs already call the ordinary
  `crt-gfx-media-dist` the options-OFF path, so this is an unsupported
  combination rather than a regression, and the isolated stage links curl
  statically and is unaffected. Recorded in `TODO.md` as a follow-up, not fixed
  here.

### Linux partial-option media packaging follow-up (PAL-04, 2026-11-23)

Source review at `0686ca5` confirms the path behind the earlier curl-only
failure. With curl enabled, Linux `libcrtmedia.so` links the shared curl
library. The ordinary stage-04 target supplies the dependency-copy arguments
only when Skia, FFmpeg and curl are all enabled. Stage 05 inherits the assembled
04 tree. A cross-linked ELF fixture with a real `DT_NEEDED` entry reproduces
`lib/libcrtmedia.so -> undeclared libcurl.so.4` in the binary audit when that
library is missing. The audit accepts the fixture after its matching SONAME
file is added. The retained fixture in `out/pal-followup/pal04-elf/` exercises
the audit only. The packaging Python suite ran 31 tests, with six host-specific
skips.

Native follow-up uses fresh Linux build directories with `CRTMEDIA_ENABLE_CURL=ON`
and these option sets. A clean option-off tree and the existing fully enabled
path are controls if the eventual change affects their inputs.

| Skia | FFmpeg | LVGL | Ordinary target |
| --- | --- | --- | --- |
| OFF | OFF | OFF | `crt-media-dist` |
| OFF | OFF | ON | `crt-ui-dist` |
| OFF | ON | ON | `crt-ui-dist` |
| ON | OFF | ON | `crt-ui-dist` |

Configure each row with `cmake --preset linux-host-ninja-debug -B <fresh>`,
setting `CRTGFX_ENABLE_SKIA`, `CRTMEDIA_ENABLE_FFMPEG` and `CRTUI_ENABLE_LVGL`
to the row values. Build the indicated target and retain both the first error
and the installed manifest. Verify stage 04 and, where applicable, stage 05.
Check the transitive shared-object closure, dependency records, applicable
license and relink payloads, and rebuild each shipped example from a relocated
package via its `crt-toolchain.cmake`.

#### Native result and decision (2026-11-23, CRT `312fec9` plus the uncommitted fix)

The Linux/x86_64 replay at `312fec9` (Ubuntu 26.04, Clang/LLD 21.1.8) reproduced
two packaging gaps, both rooted in the stage-04 dependency publication being
all-or-nothing: `create_dist.py` receives the Skia/FFmpeg/curl closure
(`--media-dependency-prefix`, `--skia-*`) only when all three options are ON.

| Skia | FFmpeg | curl | Before | Now |
| --- | --- | --- | --- | --- |
| OFF | OFF | ON | passes (`libcrtmedia.so` has no curl dependency: the transport is compiled only with FFmpeg) | passes, unchanged |
| OFF | ON | ON | stage 04 fails: `lib/libcrtmedia.so -> undeclared libcurl.so.4` after a full build | `crt-media-dist`/`crt-ui-dist` refused in under a second with a diagnostic |
| ON | OFF | ON | built and verified, but shipped `libcrtgfx_skia.*`, `libcrtui_skia*.a` and `examples/gfx-skia` without `lib/libskia.a`; the relocated example failed at configure | refused in under a second with the same diagnostic |
| ON | ON | ON | passes | passes, generated command lines byte-identical |

Decision: the ordinary `crt-media-dist`/`crt-ui-dist` package supports **Skia,
FFmpeg and curl all OFF, or all ON**, on Linux targets. Evidence: a package
that ships the FFmpeg-backed `libcrtmedia` also needs the FFmpeg link/runtime
artifacts, the LGPL relink kit and source offer, FreeType and the curl, Mbed TLS
and zlib closure; a package that ships the Skia companions needs `libskia.a`,
its headers and its record. `copy_media_development_dependencies()` and
`add_redistributed_dependencies()` publish those as one unit and neither takes a
Skia-less or FFmpeg-less shape. No isolated stage, consumer or release asset
needs such a package. Making them partial-capable would be a larger change with
its own manifest, verification and relink-kit variants, so it is not done here
(see the TODO follow-up).

Enforcement:

- `cmake/crt_media_dist_options.cmake` holds the one predicate. On Linux targets
  `CMakeLists.txt` replaces `crt-media-dist` and `crt-ui-dist` with a failing
  target carrying the diagnostic for an unsupported combination, so the
  refusal happens before the build of the stage prerequisites. Other targets of
  that tree still build. The first draft refused only `crt-media-dist`, which let `crt-ui-dist`
  queue its other prerequisites for 4.5 to 8 minutes before the build stopped.
- `tools/verify_dist.py` (`validate_optional_payload_closure`) rejects any
  04/05 package that ships Skia payload (`include/crtgfx/skia.h`,
  `examples/gfx-skia`, `libcrtgfx_skia*`, `libcrtui_skia*`) without
  `lib/libskia.a` and a `skia` record, or the FFmpeg-backed media-player sample
  without the `ffmpeg`, `curl`, `mbedtls` and `zlib` records. The old
  `out/pal04-r4` package passed `verify_dist` before and is rejected now.
- Tests: `tools/test_media_dist_options.py` runs the exact predicate over all
  eight combinations and checks the Linux wiring; `OptionalPayloadClosureTests`
  in `tools/test_verify_dist.py` covers each marker and the passing shapes.

Fresh Release trees under `out/` (all with `CRTMEDIA_ENABLE_CURL=ON`, LVGL ON):

| Tree | Skia/FFmpeg | Target | Result |
| --- | --- | --- | --- |
| `pal04-g3` | OFF/ON | `crt-ui-dist`, `crt-media-dist` | refused in 0.6 s with the diagnostic |
| `pal04-g4` | ON/OFF | `crt-ui-dist`, `crt-media-dist` | refused in 0.6 s with the diagnostic |
| `pal04-c1` | OFF/OFF | `crt-media-dist` | builds in 6 min 7 s; stage-04 `verify_dist` passes; `gfx-simple`, `gfx-gpu` rebuild from a copy under a space/Hangul path |
| `pal04-c2` | OFF/OFF | `crt-ui-dist` | builds in 7 min 40 s; 04 and 05 `verify_dist` pass; `gfx-simple`, `gfx-gpu`, `ui-basic` rebuild from the relocated package; `ui-basic` with 30 frames reports `presented=30 pixel_check=pass input_check=pass` |
| `pal04-cf` | ON/ON | `crt-ui-dist` | builds in 12 min 16 s; 04 and 05 `verify_dist` pass; `gfx-simple`, `gfx-gpu`, `gfx-skia`, `ui-basic`, `media-player` rebuild from the relocated package; `ui-basic` passes as above |

The first `g3`/`g4` runs (before `crt-ui-dist` was also replaced) already
printed the diagnostic but only after 4 min 33 s and 8 min 10 s.
`crt-ui-dist`/`crt-media-dist` command lines in the full tree's `build.ninja`
equal those of a tree configured from the unmodified `312fec9` sources once the
tree names are normalized. Limits: Linux/x86_64 only; macOS and Windows
packaging was neither changed nor replayed, so a partial combination there is
neither refused nor verified; `media-stream` exists only in the isolated stage;
the `cf` build time was not analyzed for cache effects; packages were not run
against a display beyond the `ui-basic` 30-frame check.

#### macOS/arm64 packaging replay (2026-11-24)

Inputs: CRT `cc8638348636b2a8a2b3903e3a1b145fec760970`, macOS 27.2.1
(`26B131`), Xcode 27.2 (`27B52`), Apple Clang 21. Release trees and retained
logs are under `out/pal04-macos-20261124/`. This is the ordinary cumulative
package path, not isolated-stage or release qualification. Native Windows
replay is assigned to a separate physical host; no Windows Actions run was
requested or dispatched.

Use the macOS preset with a new `-B` directory for each partial option row:

```sh
cmake --preset macos-host-ninja-debug -B out/pal04-macos-20261124/<row> \
  -DCMAKE_BUILD_TYPE=Release -DCRT_USE_IMPORTED_LIBCXX=ON \
  -DCRTGFX_ENABLE_SKIA=<ON-or-OFF> -DCRTMEDIA_ENABLE_FFMPEG=<ON-or-OFF> \
  -DCRTMEDIA_ENABLE_CURL=ON -DCRTUI_ENABLE_LVGL=ON
cmake --build out/pal04-macos-20261124/<row> --target crt-ui-dist -j6
```

The new trees reuse verified raw download/clone caches, not previously built
runtime or Skia objects. `tools.test_media_dist_options`, `test_verify_dist`,
`test_create_dist` and `test_sdk_workflows` run 61 tests: 60 pass, one
Linux-only test skips (`packaging-unit.log`).

- **Minimum control (`off`, Skia OFF/FFmpeg OFF/curl ON):** the fresh build
  completes `crt-ui-dist`, including verified stage 04 and 05 packages.
  Curl has no effect without FFmpeg. The stage-05 dependency records are
  `crt-libc`, `mksh`, `toybox`, `onetrue-awk`, `llvm-runtimes` and `lvgl`;
  no optional Skia/FFmpeg payload is advertised. The macOS binding audit
  passes for 102 images (9 dylibs and 93 executables).
- **Relocated minimum control:** copy stage 05 with symlinks preserved to
  `relocated off 한글/05-ui`. Its own toolchain rebuilds `gfx-simple`,
  `gfx-gpu` and `ui-basic`. The UI executable runs 30 frames with
  `presented=30 pixel_check=pass input_check=pass`; the relocated package
  also passes `verify_dist`. An additional rebuild with both
  `CRT_RUNTIME_LINKAGE=shared` and `CRT_CXX_RUNTIME_LINKAGE=shared` passes
  all three consumer binding audits and repeats the UI 30-frame check.
  Retain `off-shared-examples.json`, `off-shared-consumer-bindings.json`
  and `off-shared-ui-run.json` separately from the default-linkage runs.
- **Skia-only (`skia-only`, initially Skia ON/FFmpeg OFF/curl ON):** the
  native build reaches stage-04 publication, but the shared verifier rejects
  it: `package ships Skia payload ... without its Skia dependency:
  lib/libskia.a and the skia dependency record are required`. Preserve
  `skia-only-build.log`, `skia-only-configure-before-full.txt` and
  `skia-only-rejected-package/` as the failed inputs/output. This is an
  actual package-build failure, not just a synthetic verifier fixture.
- **FFmpeg-only (`ffmpeg-only`, Skia OFF/FFmpeg ON/curl ON):** the fresh
  native build also reaches stage-04 publication, then fails verification:
  `package ships FFmpeg-backed payload ... without its dependency record(s):
  ffmpeg, curl, mbedtls, zlib`. Retain `ffmpeg-only-build.log` and the
  rejected `ffmpeg-only/dist/04-gfx-media/`; stage 05 is not accepted.
- **Current-source full control:** after preserving the Skia-only failure,
  use `cmake --fresh` on that tree with all three options ON, retaining
  `full-configure.log` and `full-build.log`. This deliberately reuses its
  current-source runtime/Skia outputs; it is not a second cold build.
  Both stage 04 and 05 build and verify. The full dependency set adds
  `freetype`, `ffmpeg`, `skia`, `curl`, `mbedtls` and `zlib` to the minimum
  package, including the FFmpeg relink kit at `share/crt/relink/libcrtmedia`
  and its license/source offer. The package binding audit passes for
  110 images (15 dylibs and 95 executables), retaining the auditor's
  existing pure-math note for `crtgfx_skia_gpu_window_demo` rather than
  silently treating it as a new all-symbol binding guarantee.
- **Relocated current full control:** `relocated current full 한글/05-ui`
  verifies and rebuilds all five shipped examples with shared CRT linkage
  explicitly enabled as above. All five external-consumer binding audits
  pass; `ui-basic` runs 30 frames with both pixel/input checks passing.
  Evidence: `current-full-shared-{examples,consumer-bindings,ui-run}.json`.
- **Archive controls:** extract each minimum/current-full stage-05 `.tar.xz`
  into a new directory and run `verify_dist.py --stage 05-ui` again; both
  pass (`archive-verify.json` and `*-extracted-verify.log`).
- **Existing full-package control:** the previously built stage 04/05 SDK
  at source commit `a79a06b585a7da2322c6acadb3da963c9208d128` passes the
  current verifier and macOS binding audit. Its relocated stage 05 rebuilds
  `gfx-simple`, `gfx-gpu`, `gfx-skia`, `ui-basic` and `media-player`; UI
  runs the same 30-frame check. This is an older-artifact control, not a
  current-source rebuild claim.
- **Negative verifier control:** remove both the Skia manifest record and
  `lib/libskia.a` from a disposable relocated full SDK. The new closure
  check rejects it with the same dependency diagnostic. Removing only the
  archive is already caught by the older inventory check. Restore the
  original manifest bytes and archive; the restored SDK verifies again.

At this baseline, early CMake refusal is still **Linux-only** (the later [macOS early refusal](#macosarm64-early-refusal-2026-11-24) closes that for macOS). macOS builds
both unsupported partial shapes before rejecting their packages. Do not report
this as successful partial-package support or promote either rejected SDK.
The relocation checks rebuild examples from packaged inputs but do not hide
the producer/source tree; GPU and media examples are compile/link checks,
not additional GPU presentation or media-playback evidence. No runtime,
ABI, dependency pin or release asset was changed; documentation was being
recorded during the full-control build. This replay does not apply a macOS
packaging policy change.

#### Windows/x64 packaging replay (2026-11-24)

Inputs: CRT `cc8638348636b2a8a2b3903e3a1b145fec760970` plus the working-tree change recorded below, Windows 11 Pro 10.0.26100 x64 (physical),
LLVM 22.1.8, CLI `1ab78e1` and crt-vscode `e418379` as the synchronized neighbors (they are not inputs of this packaging path). Release trees and retained logs are
under `out/p4w-*` and `out/pal04-windows-x64-20261124/` (`inputs.json`, `setup-notes.md`). This is the ordinary cumulative package path, not isolated-stage or release
qualification; no Windows Actions run was requested. Every row used a new Release tree (`windows-host-ninja-debug`, imported libc++ ON, curl ON, LVGL ON, `-DCMAKE_BUILD_TYPE=Release`).
The Skia source checkout is the pinned `13ffba25...` with only the two controlled build patches (`.gn`, `gn/toolchain/BUILD.gn`, hashes in `inputs.json`).

- **Setup failure, kept.** The pre-existing `out/windows-host-ninja-debug/external/llvm-runtimes` source cache failed the integrity check (`libcxxabi/src/cxa_exception_storage.cpp`). It was preserved and a pristine
  cache `out/p4w-deps/llvm` was used. A first combined unit run also errored in the workflow Git query on Windows (`missing config value GIT_CONFIG_VALUE_0`: the environment-clearing mocks delete an injected empty value);
  with `GIT_CONFIG_COUNT=0` only in the test process, the unchanged suites pass. Neither is a PAL-04 result, and no Git configuration was changed.
- **Baseline before any change: both partial shapes build, then fail late.** Skia OFF / FFmpeg ON / curl ON (`g3`) builds through stage 04 and stops in the verifier: `package ships FFmpeg-backed payload (examples/media-player,
  examples/bin/crtmedia_player_demo.exe) without its dependency record(s): ffmpeg, curl, mbedtls, zlib` (the manifest lists only `crt-libc`, `mksh`, `toybox`, `onetrue-awk`, `llvm-runtimes`; `libavformat.a`, `libavcodec.a`, `libcurl.a`,
  `libmbedtls.a`, `libz.a` are absent). Skia ON / FFmpeg OFF / curl ON (`g4`) ships the Skia header and example but no `libskia.a` and no dependency record. Evidence: `g3-payload-gap.json`, `g4-payload-gap.json`, `g3-crt-ui-dist.log`, `g4-crt-ui-dist.log`.
- **Minimum control (`off`, Skia OFF / FFmpeg OFF / curl ON)** passes stage 04 and 05 verification. Dependency records are `crt-libc`, `mksh`, `toybox`, `onetrue-awk`, `llvm-runtimes` and `lvgl`. The cached LVGL source/header set (1,364 compiled files) equals a freshly
  fetched pinned copy (`lvgl-source-audit.json`).
- **Full control (Skia, FFmpeg and curl ON)** passes stage 04 and 05 and adds `freetype`, `ffmpeg`, `skia`, `curl`, `mbedtls` and `zlib`.
- **A real gap the replay found in the full package.** Verified and relocated to `SDK space 한글`, the full package's `gfx-skia` example failed to configure: `Could not find CRT_WIN32_SHIM_DIR using ... mingw_w64_compat.h`
  (`full-baseline-consumer-commands.json`). The isolated 04 stage already ships those private headers; the ordinary cumulative package did not. The verifier had passed this package, so the gap was a verifier hole as well.

**Change (code).** (1) `CMakeLists.txt` applies `cmake/crt_media_dist_options.cmake` to Windows as well as Linux, so the two partial shapes are refused when the dist targets are configured, before any prerequisite builds.
(2) For a Windows Skia package, `libcrtgfx/CMakeLists.txt` installs the private `include/crt-win32-shim` and `include/crt-mingw-w64-headers` trees with the mingw-w64 licenses and fetch provenance, and `tools/create_dist.py` records them as the
`mingw-w64-headers` private-SDK-headers dependency. (3) `tools/verify_dist.py` (`validate_optional_payload_closure(..., target_os)`) rejects a Windows Skia package that lacks these headers
(`Windows Skia consumer headers missing: ...`). Tests: `tools.test_media_dist_options`, `test_verify_dist` (new Windows header-closure test, including each header missing in turn, and no effect on Linux/macOS), `test_create_dist` and
`test_sdk_workflows` pass 62 tests with 6 host skips (`unit-final.log`). The macOS early-refusal decision is unchanged.

**After the change.** Fresh trees for the two partial shapes (`ng3`, `ng4`) refuse `crt-media-dist` and `crt-ui-dist` in 0.17-0.19 s with the option diagnostic and run no SDK prerequisite build (`rule-result.json`). The supported controls keep an identical
packaging command line. Re-packaged `off` and `full` pass `verify_dist` at stage 04 and 05, relocated to `SDK space 한글` they rebuild every shipped example (`off`: `gfx-simple`, `gfx-gpu`, `ui-basic`; `full`: those plus `gfx-skia` and `media-player`), and
`ui-basic` runs `crtui_window_demo: presented=30 pixel_check=pass input_check=pass` with only the SDK `bin` on `PATH`. The `.zip` of stage 05, extracted into a new directory, verifies and runs the same 30-frame check
(`off`: 66,259,514 bytes, SHA-256 `60fc6c4728381c64f9e3ff80cb20fcaa2e9ec6f228a88eba271e3dd6eb8c451d`; `full`: 102,609,332 bytes, `acdd415d50fc8f970e2b4e2db5e83fe7cefff8910618281fd0fdf8b2955048d6`; manifests
`02a3cc10...7383` and `b17447b2...0972`). Negative verifier control: the changed verifier rejects the retained pre-change full package (`full-before-fix.zip`) with `Windows Skia consumer headers missing: ...` (`full-baseline-verifier-negative.log`).

Not covered: Windows/arm64 (replayed later: [Windows/arm64 record](#windowsarm64-packaging-and-closure-replay-2026-11-24)); a published release asset or signature; GPU presentation or media playback beyond compile/link and the 30-frame software UI check; a partial package with a complete dependency closure (a Skia-less or FFmpeg-less closure
remains the optional follow-up). No runtime, ABI, dependency pin or release asset changed. The macOS early-refusal decision stays open.

#### macOS/arm64 early refusal (2026-11-24)

**Decision: refuse the partial Skia/FFmpeg/curl combinations before building on macOS too.** Rationale: the [macOS replay](#macosarm64-packaging-replay-2026-11-24) reproduced
the same late failure as Windows/x64 for both partial shapes (Skia ON/FFmpeg OFF and Skia OFF/FFmpeg ON each build to stage-04 publication and are then rejected by the shared
verifier), so a macOS user pays a long build before learning that the combination was never packageable. The verifier stays the second line of defense on every host. This is
not support for partial packages; a partial package with a complete dependency closure remains the optional follow-up.

**Change.** `CMakeLists.txt` now applies `cmake/crt_media_dist_options.cmake` to every target OS (the Linux/Windows OS guard is gone), so `crt-media-dist` and `crt-ui-dist`
(which depends on it) become the single refusal command on macOS for any combination other than Skia, FFmpeg and curl all OFF (curl has no effect without FFmpeg) or all ON. Other
targets of the same tree still build. `tools/test_media_dist_options.py` now asserts the unguarded application (and that no OS guard returns). No runtime, public header,
ABI, dependency pin or release asset changes.

**Native check** (CRT `ffa8251` plus the change; macOS 27.2.1 `26B131`, Xcode 27.2 `27B52`, Apple clang 21.0.0; Release, `CRT_USE_IMPORTED_LIBCXX=ON`, `CRTMEDIA_ENABLE_CURL=ON`,
`CRTUI_ENABLE_LVGL=ON`; fresh trees under `out/pal04-macos-early-20261124/`, configure plus build invocations only):

| Row (Skia / FFmpeg) | `cmake --build --target crt-media-dist` and `crt-ui-dist` | Evidence |
| --- | --- | --- |
| ON / OFF | both exit 1 in 0 s, **0** compile steps, the full option diagnostic (names the three options, the supported combinations and the isolated-stage alternative) | `skia-only-*.log`; `ninja -t commands`: one command, the refusal script |
| OFF / ON | the same | `ffmpeg-only-*.log` |
| OFF / OFF | not refused: `ninja -t commands` lists 903 (`crt-media-dist`) and 1881 (`crt-ui-dist`) commands, including `create_dist.py` and `crt_stage_cache.py`, and no refusal script | `off-*.commands` |
| ON / ON | not refused: 930 and 1912 commands, the same packaging and cache commands, no refusal script | `full-*.commands` |

The two supported rows were deliberately **not rebuilt**: the minimum and full macOS packages, their relocated rebuilds and the UI 30-frame runs are the unchanged results of the
[macOS replay](#macosarm64-packaging-replay-2026-11-24), and nothing they depend on changed (the command lines are the ordinary ones). Unit tests: `test_media_dist_options`,
`test_verify_dist`, `test_create_dist` and `test_sdk_workflows` pass 62 tests with one Linux-only skip.

A first attempt of this check invoked the configure loop from `zsh`, whose unquoted `$row` is not word-split, so the rows were configured with the preset's defaults and real
builds started; the trees were removed and the check was redone with explicit variables. Nothing from that attempt is recorded as a result.

Not covered: Windows/arm64 (replayed later: [Windows/arm64 record](#windowsarm64-packaging-and-closure-replay-2026-11-24)); building the supported rows again; a published asset or signature; a partial package with a complete closure.

#### Windows/x64 Skia-less and FFmpeg-less closure packages (2026-11-24)

The optional follow-up of PAL-04 (a partial package with a complete dependency closure) on the physical Windows/x64 host, with the same toolchain, Skia pin and
controlled patches as the [packaging replay](#windowsx64-packaging-replay-2026-11-24). Evidence: `out/pal04-windows-x64-20261124/` (`g3-*`, `g4-*`, `closure-rule*.json`, `rejected-partial-before-closure/`).

**Closures.** *Skia* is FreeType and Skia (`libfreetype.a`, `libskia.a`, the skcms headers, the Skia/FreeType records and, on Windows, the private consumer headers). *FFmpeg* is FFmpeg with curl, Mbed TLS and
zlib. The Skia companions (`libcrtgfx_skia`, `libcrtui_skia`, `libcrtui_skia_media`) belong to the Skia closure: `crtui_skia_media` is built whenever Skia is on, because `crtmedia` has a backend-less core, so an
FFmpeg-less package ships it.

| Skia | FFmpeg | curl | Windows/x64 result |
| --- | --- | --- | --- |
| OFF | OFF | any | nothing optional (unchanged) |
| ON | ON | ON | both closures (unchanged) |
| OFF | ON | ON | **Skia-less:** the FFmpeg closure only |
| ON | OFF | any | **FFmpeg-less:** the Skia closure only (curl has no effect without FFmpeg) |
| OFF/ON | ON | OFF | refused before building: `FFmpeg media needs curl, Mbed TLS and zlib in its dependency closure` |

**Change.** `cmake/crt_media_dist_options.cmake` takes a fifth `partial` argument; `CMakeLists.txt` passes it ON only for `windows` and passes `--media-components skia,ffmpeg` (any subset) to `tools/create_dist.py`, which copies exactly those files
(`copy_media_development_dependencies`) and records exactly those dependencies (`add_redistributed_dependencies(..., components)`; the FreeType and Mbed TLS license choices are written only for a shipped dependency). The isolated stage and the full
package default to both closures. `tools/verify_dist.py` now also requires the FreeType record and `lib/libfreetype.a` with any Skia payload. Linux and macOS keep refusing the two partial shapes until they have their own replay.
Tests: the option matrix with and without `partial`, the verifier (the Skia closure with FreeType, one complete closure without the other, the media companion), the per-closure copy and the per-closure records pass 105 tests with 6 host skips.

**Results (fresh evidence on the retained Release trees).** The rejected baseline packages (`g3`, `g4`: late verifier failure) are preserved. After the change, `crt-ui-dist` packages both rows (Skia-less 207 s, FFmpeg-less 230 s) and `verify_dist` passes at stage 04 and 05 and on the copy relocated to `SDK space 한글`:

- **Skia-less (`g3`):** records `ffmpeg`, `curl`, `mbedtls`, `zlib` and no `skia`/`freetype`; the FFmpeg, curl, Mbed TLS and zlib archives are present and `libskia.a`, `libfreetype.a` and `examples/gfx-skia` are absent. The relocated SDK rebuilds `gfx-simple`, `gfx-gpu`, `ui-basic` and `media-player`
  and `ui-basic` runs `presented=30 pixel_check=pass input_check=pass` with only the SDK `bin` on `PATH`. The stage-05 `.zip` (76,219,342 bytes, SHA-256 `7a444548f629c8167e615cf6338d365e7a2ba8296376fc4ef99c7031f8edf7a3`) extracts, verifies and runs the same 30-frame check.
- **FFmpeg-less (`g4`):** records `freetype`, `skia` and `mingw-w64-headers` and no `ffmpeg`/`curl`/`mbedtls`/`zlib`; `libskia.a`, `libfreetype.a` and the skcms headers are present and the FFmpeg/curl archives and `examples/media-player` are absent. The relocated SDK rebuilds `gfx-simple`, `gfx-gpu`,
  `ui-basic` and `gfx-skia` (including its private headers) and the 30-frame check passes. Its `.zip` (92,649,517 bytes, SHA-256 `0703ee695db1e0466beff12cda916eb91ec6e990629d1bbd7bae50778c62b950`) extracts, verifies and runs it.
- **Refusal.** Fresh trees for FFmpeg ON / curl OFF with Skia OFF and ON refuse `crt-media-dist` and `crt-ui-dist` in 0.19-0.20 s with no SDK prerequisite build (`closure-rule-result.json`).
- **Controls.** The all-OFF and all-ON rows keep their packages: the only packaging-command difference is the added `--media-components skia,ffmpeg` on the all-ON row, and their dependency records equal the earlier candidates. Their manifest hashes change only through the recorded build
  identity (`created_utc`, `source_commit`). The re-packaged controls were relocated, rebuilt and archive-checked again (`off-final-*`, `full-final-*`).

**Attempt kept.** The first FFmpeg-less package failed in `verify_dist` because a check this change had added wrongly required the FFmpeg records with `libcrtui_skia_media`; the library does not need FFmpeg, so the check was removed and its test now pins the correct rule (`g4-crt-ui-dist-2.log`).

Not covered: Windows/arm64 (replayed later: [Windows/arm64 record](#windowsarm64-packaging-and-closure-replay-2026-11-24)), Linux and macOS (they still refused both shapes then), shared-runtime linkage of the new packages, GPU presentation or media playback beyond compile/link and the 30-frame software UI check, a published release or signature, and CI.
No runtime, ABI, dependency pin or release asset changed.

#### macOS/arm64 Skia-less and FFmpeg-less closure packages (2026-11-24)

Replay of the Windows/x64 [closure packages](#windowsx64-skia-less-and-ffmpeg-less-closure-packages-2026-11-24) on macOS/arm64. CRT `85a2b6d` plus the change below; macOS 27.2.1 (`26B131`), Xcode 27.2 (`27B52`), Apple clang 21.0.0;
Release, `CRT_USE_IMPORTED_LIBCXX=ON`, curl ON, LVGL ON. The retained trees of the [macOS replay](#macosarm64-packaging-replay-2026-11-24) are reconfigured and re-packaged (their compiled Skia, FFmpeg and runtime
outputs are reused, not a cold build). Evidence: `out/pal04-macos-20261124/` (`closure-*`, `ffmpeg-less-package/`, `r1-closure-*` for the failed first attempt).

**Change.** `CMakeLists.txt` passes `CRT_MEDIA_DIST_PARTIAL=ON` for `macos` as well as `windows`, so the option rule accepts the two complete single closures; `tools/test_media_dist_options.py` pins that. The packaging itself (`--media-components`,
per-closure copy and records, the FreeType requirement in `verify_dist`) is the shared code the Windows replay added; nothing in it needed a macOS change. Linux still refuses both shapes.

| Skia | FFmpeg | curl | macOS/arm64 result |
| --- | --- | --- | --- |
| OFF | OFF | any | nothing optional (control, unchanged records) |
| ON | ON | ON | both closures (control) |
| OFF | ON | ON | **Skia-less:** FFmpeg, curl, Mbed TLS and zlib; no `libskia.a`, `libfreetype.a` or `gfx-skia` |
| ON | OFF | any | **FFmpeg-less:** FreeType and Skia; no FFmpeg libraries or `media-player` |
| OFF/ON | ON | OFF | refused before building (rule unchanged) |

**Results.** `verify_dist` passes at stage 04 and 05 for all four packages (the macOS binding audit is part of it). Each stage-05 package is copied with symlinks preserved to `SDK space 한글/05-ui`, verifies again there, and its own toolchain
rebuilds every shipped example: Skia-less `gfx-simple`, `gfx-gpu`, `ui-basic`, `media-player`; FFmpeg-less `gfx-simple`, `gfx-gpu`, `ui-basic`, `gfx-skia`; controls as before. `ui-basic` runs
`crtui_window_demo: presented=30 pixel_check=pass input_check=pass` with only the SDK `bin` on `PATH` for all four. The stage-05 `.tar.xz` of each extracts into a new directory and verifies
(Skia-less 10,845,480 bytes `19bc716f5e84...`, FFmpeg-less 13,022,096 bytes `a82d4a5506ae...`, full 18,737,228 bytes `a4e9cc15557f...`, off 5,425,060 bytes `3d971b404ad4...`).
Beyond the Windows record, every package's examples were also rebuilt with `CRT_RUNTIME_LINKAGE` and `CRT_CXX_RUNTIME_LINKAGE` both `shared`: all consumer binding audits report no problem and the UI runs the same 30 frames (`closure-archive-shared-results.json`).

Package contents (`manifest.json`): Skia-less records `ffmpeg`, `curl`, `mbedtls`, `zlib` and no `skia`/`freetype`; FFmpeg-less records `freetype`, `skia` and no `ffmpeg`/`curl`/`mbedtls`/`zlib`.

**Controls.** The re-packaged all-ON and all-OFF packages keep the earlier candidates' dependency records and license files byte for byte; the only difference is the build identity in `share/licenses/SOURCE-OFFER.md` (source commit `cc86383` to `85a2b6d`).
**Negative controls** on disposable relocated copies: removing `lib/libfreetype.a` or `lib/libskia.a` from the FFmpeg-less package or `lib/libavcodec.a` from the Skia-less package makes `verify_dist` fail ("distribution is missing"), and restoring the file makes it pass again; dropping the
`curl` record from the Skia-less package fails with `package ships FFmpeg-backed payload ... without its dependency record(s): curl`. Unit tests (`test_media_dist_options`, `test_verify_dist`, `test_create_dist`, `test_sdk_workflows`, `test_dev_sdk`) pass 105 tests with 3 host skips.

**Attempts kept.** The first run of the build script (`r1-*`) omitted the source directory from `cmake -B`, so every configure failed and the builds ran with each tree's previous options; its FFmpeg-less row was really the full package and is not counted. The second run rebuilt the FFmpeg-less package
in the tree that the full control then overwrote, so that package was regenerated once more and copied to `ffmpeg-less-package/` before the tree was restored to all-ON.

Not covered: Linux and Windows/arm64 (the latter replayed later: [Windows/arm64 record](#windowsarm64-packaging-and-closure-replay-2026-11-24)); GPU presentation or media playback beyond compile/link and the 30-frame software UI check; a published release or signature; CI (the change touches only CMake option selection). No runtime, ABI, dependency pin or release asset changed.

#### Windows/arm64 packaging and closure replay (2026-11-24)

Windows 11 Pro 10.0.26300 arm64 (physical), LLVM 22.1.8 (`woa64`), Windows SDK `10.0.28000.0` `um/arm64`; the CMake-driven builds ran Python 3.14 arm64, the direct `verify_dist`, unit-test and archive-extraction calls Python 3.11.9 arm64. Every row is a Release tree (`windows-host-ninja-debug`, imported libc++ ON, curl ON, LVGL ON, `-DCMAKE_BUILD_TYPE=Release`)
configured fresh or, from the second phase on, **reconfigured and re-packaged from the compiled trees** as the macOS closure replay did. It is the ordinary cumulative package path run by hand, not isolated-stage or release qualification, and no Actions run was requested.
Skia is the pinned `13ffba25...`. Retained logs, per-row results and the preserved archives are under `out/pal04-windows-arm64-20261124/` (ignored; `closure-rows-result.json`, `packages/<row>/`, `*-verify-*.log`, `gn-amd64/gn-fetch.json`).

**Phase 1, CRT `2fc6eaf` (the refusal policy of that time).** Fresh trees for Skia ON / FFmpeg OFF and Skia OFF / FFmpeg ON refused `crt-media-dist` and `crt-ui-dist` in 0.29-0.36 s, one command (the refusal script) and no prerequisite build, with the option diagnostic naming the
supported combinations. The minimum control (Skia and FFmpeg OFF) built `crt-ui-dist` in 28 min 18 s and `crt-media-dist` in 21 s; `verify_dist` passed at stage 04 and 05 (records `crt-libc`, `mksh`, `toybox`, `onetrue-awk`, `llvm-runtimes`, plus `lvgl` at 05). Copied to `SDK space 한글`
the package rebuilt `gfx-simple`, `gfx-gpu` and `ui-basic`, and `ui-basic` printed `crtui_window_demo: presented=30 pixel_check=pass input_check=pass` with only the SDK `bin` and System32 on `PATH`; the stage-05 `.zip` (64,397,970 bytes) extracted into a new directory, verified and ran the same check.
The full control (Skia, FFmpeg, curl ON) first **failed after 52 min 10 s at `crtgfx-skia-build`**: Skia's `bin/fetch-gn` got `HTTP Error 404` because the pinned GN revision has no `windows-arm64` package (the documented
[build-host input](crtweb_acceptance.md): the x64 `gn.exe`). That attempt is kept (`full-crt-ui-dist.log`). The x64 `gn.exe` of the same pinned revision `b2afae12...` (`https://chrome-infra-packages.appspot.com/dl/gn/gn/windows-amd64/+/git_revision:b2afae122eeb6ce09c52d63f67dc53fc517dbdc8`; zip 1,120,394 bytes SHA-256 `7249f55d6f22b55affa68aaf19d2f597c39946299216e12d78eac6cc1469f8a1`, `gn.exe` 2,371,072 bytes SHA-256 `cf28c1ea5c5c6c2ec1c2e4e12b360f276595ceea8de6a35368dfd1f9b39b527d`, PE machine `0x8664`) runs under x64 emulation (`2175 (b2afae122eeb)`)
and was supplied with `-DCRTGFX_SKIA_GN`; the same tree resumed and built in 13 min 26 s (media 31 s). The full package passed `verify_dist` at stage 04 and 05 (records add `freetype`, `ffmpeg`, `skia`, `curl`, `mbedtls`, `zlib` and `mingw-w64-headers`; `include/crt-win32-shim`, `include/crt-mingw-w64-headers` and `libskia.a` present),
and the relocated copy rebuilt all five shipped examples, including `gfx-skia` (the one whose private headers the x64 replay found missing) and `media-player`, and ran the 30-frame check (archive 100,406,500 bytes, extracted package verified and ran it).
Negative control: in the extracted copy, renaming `include/crt-win32-shim` makes `verify_dist` exit 1 with `Windows Skia consumer headers missing: include/crt-win32-shim/mingw_w64_compat.h`; restoring it exits 0.

**Phase 2, CRT `1ae70fa` (complete closure packages on Windows and macOS).** `59ab344` and `1ae70fa` replaced the refusal of the two single-closure shapes by packaging them (see the [Windows/x64 record](#windowsx64-skia-less-and-ffmpeg-less-closure-packages-2026-11-24)), so Phase 1's refusals are history, not evidence for the current policy.
The four rows below were re-packaged at `1ae70fa`; each passes `verify_dist` at stage 04 and 05, the record and file expectations, the relocated rebuild of every shipped example with the 30-frame check, and the extracted `.zip` verified and run.

| Row (Skia / FFmpeg / curl) | `crt-ui-dist` / `crt-media-dist` | Dependency records at stage 05 (besides `crt-libc`, `mksh`, `toybox`, `onetrue-awk`, `llvm-runtimes`, `lvgl`) | Examples rebuilt | Stage-05 `.zip` |
| --- | ---: | --- | --- | --- |
| OFF / OFF / ON (control) | 246 s / 16 s | none | `gfx-simple`, `gfx-gpu`, `ui-basic` | 64,397,983 bytes `ccd6a4e8baab...` |
| ON / ON / ON (control) | 280 s / 21 s | `freetype`, `skia`, `ffmpeg`, `curl`, `mbedtls`, `zlib`, `mingw-w64-headers` | those plus `gfx-skia`, `media-player` | 100,406,517 bytes `74f96c998a7e...` |
| ON / OFF / ON (**FFmpeg-less**) | 219 s / 16 s | `freetype`, `skia`, `mingw-w64-headers`; no `ffmpeg`, `curl`, `mbedtls`, `zlib` | `gfx-simple`, `gfx-gpu`, `ui-basic`, `gfx-skia` | 90,276,169 bytes `fed59dd55d5b...` |
| OFF / ON / ON (**Skia-less**) | 206 s / 13 s | `ffmpeg`, `curl`, `mbedtls`, `zlib`; no `skia`, `freetype`, `mingw-w64-headers` | `gfx-simple`, `gfx-gpu`, `ui-basic`, `media-player` | 74,528,710 bytes `30d126e00bbc...` |

Manifest SHA-256 (stage 04 / 05): off `68942ca6fe4c...` / `21d04e9c6dd3...`; full `dc39d6d23d49...` / `0c092dd30cf1...`; FFmpeg-less `f51c1c11d06e...` / `370c8398d92e...`; Skia-less `759066926309...` / `1fa56fee3b68...`. The FFmpeg-less package has `libskia.a`, `libfreetype.a` and the Win32 consumer headers and no
FFmpeg or curl archives or `media-player`; the Skia-less package has the FFmpeg, curl, Mbed TLS and zlib archives and no Skia, FreeType or `gfx-skia`. The only combination still refused is FFmpeg ON with curl OFF: with Skia OFF and with Skia ON both dist targets exit 1 in 0.28-0.36 s with one command and the
diagnostic `FFmpeg media needs curl, Mbed TLS and zlib in its dependency closure`. Unit tests at `1ae70fa`: `test_media_dist_options` 4, `test_verify_dist` 25, `test_create_dist` 15 (6 host skips), `test_dev_sdk` and `test_sdk_workflows` 22 pass.

**Findings and attempts, kept.** (1) The Skia `gn` input above is a host input, not a CRT defect; it is the same input the documents already list for x64, but the arm64 package for it is not published, so an arm64 builder must supply the x64 `gn.exe` or fail with the 404. (2) The Phase 2 script's SHA-256 lines failed with
`Get-FileHash` not found inside the hidden PowerShell and left those fields out of its result file; the identities above were recomputed from the preserved archives and nothing else depended on them.

Not covered: Linux (it still refuses both single-closure shapes until its own replay); shared-runtime linkage of the new packages (the macOS replay covers it); GPU presentation or media playback beyond compile/link and the 30-frame software UI check; the late verifier failure of the partial shapes before the closure
change was not rebuilt on arm64 (the x64 and macOS records keep it); a published release or signature; CI. No runtime, ABI, dependency pin or release asset changed.

#### Native Windows replay handoff

Use a fresh Windows Release tree per row of the Skia/FFmpeg matrix above,
with `windows-host-ninja-debug`, imported libc++ ON, curl ON and LVGL ON.
Run `crt-ui-dist` (which includes stage 04) and retain the first failure,
the resolved options and any published manifest. On the two supported
controls, also run `crt-media-dist` and verify both packages:

```powershell
python tools/verify_dist.py --dist <build>/dist/04-gfx-media --stage 04-gfx-media
python tools/verify_dist.py --dist <build>/dist/05-ui --stage 05-ui
```

Rebuild every shipped example through a relocated SDK's toolchain, retaining
the Windows import-library/DLL load result and the UI 30-frame check. Check
dependency records, notices and the full package's relink/source payload.
Partial combinations must either be deliberately refused before expensive
prerequisites, or provide a complete, verifiable closure; a late verifier
failure is a finding, not a passing package. Linux early refusal is not
evidence of Windows behavior. This gate is closed on Windows/x64 by the
[replay above](#windowsx64-packaging-replay-2026-11-24) and on Windows/arm64 by the
[arm64 replay](#windowsarm64-packaging-and-closure-replay-2026-11-24).

### 1. LVGL import and first pixels (Windows/x64 first)

LVGL software draw buffer -> a CRT display adapter -> the existing
`crtgfx`/window present path. No LVGL GPU backend. Acceptance: a
`Window > Column > Label/Button/Slider/Progress` demo produces expected pixels
and shuts down cleanly (no leaked handles/threads).

**Closed 2026-09-30 on Windows/x64, macOS/arm64, and Linux/x86_64.** (details below)

LVGL v9.6.0 is imported as a private dependency and renders through a CRT
display adapter into a caller-supplied buffer; crtgfx presents that buffer.

- **Import.** `tools/fetch_lvgl.py` downloads the pinned archive (or reuses a
  cached one), verifies its size and SHA-256, verifies that the commit recorded
  in the archive's pax header equals `expected_commit`, and extracts only `src/`,
  `include/`, `LICENCE.txt`, `lv_conf_template.h` and the three root headers
  LVGL's own sources include (`lvgl.h`, `lvgl_private.h`, `lv_version.h`) into
  `<build>/lvgl/lvgl-9.6.0` (28 MB of the 111 MB archive). Nothing in it is
  patched. The `crtui-lvgl-fetch` CMake target runs it; `-DCRTUI_ENABLE_LVGL=ON`
  (default OFF, like `CRTMEDIA_ENABLE_CURL`) then compiles all 483 LVGL sources
  plus `src/lvgl_backend.c` into `libcrtui` (static and shared), with
  `-DCRTUI_HAVE_LVGL`. A build without it keeps the headless model and returns
  `CRTUI_ERROR_UNSUPPORTED` from `crtui_window_render()`.
- **Configuration.** `libcrtui/src/lvgl_conf/lv_conf.h` is the only LVGL "patch"
  (a configuration file): software draw unit only, no OS/thread integration, no
  drivers/GPU backends, CRT `malloc` with LVGL's builtin string/sprintf, the
  default theme disabled, assertions trap instead of hanging. The 483 sources
  compile cleanly on the CRT toolchain (no CRT/PAL gap surfaced; the only
  external symbols are `malloc`/`memcpy`-class libc calls).
- **Display adapter.** `crtui_window_render(context, window, pixels, stride,
  width, height)` (public, additive) renders the window's tree as BGRA8888 --
  bytes B,G,R,A, exactly `CRTGFX_PIXEL_FORMAT_BGRA8888_PREMULTIPLIED`; the
  window is opaque so premultiplication is a no-op. The model (core.c) flattens
  the tree to a private render list (`render_tree.h`, no LVGL type) and
  `lvgl_backend.c` mirrors it into LVGL objects, rebuilding only when the model
  changed, drawing FULL-frame into an internal buffer and copying rows out with
  the caller's stride. Every widget is styled explicitly (fixed default look:
  window `#F0F0F0`, text `#202020`, button `#2D6CDF` with white text, disabled
  `#B0B0B0`, slider/progress track `#C8C8C8`, slider fill `#2D6CDF`, knob
  `#1E4FA8`, progress fill `#2ECC71`), so the look is CRT code, not an LVGL theme.
  Also new in the contract: `CRTUI_WIDGET_PROGRESS` (`crtui_progress_create/
  set_value/get_value`, range 0..100, not focusable). "Column" layout is
  Tranche 4; the demo positions children by hand.
- **`crtui_render_test`** renders `Window > Container > Label/Button/Slider/
  Progress` at 320x200 and checks real pixels (never just "it ran"): background
  and full-buffer overwrite, glyph pixels for the label, button fill and white
  label text, slider fill vs. track around the knob, progress fill vs. track at
  50% and 100%, disabled and hidden re-renders, a padded stride (same image,
  padding untouched), resize, argument/thread errors. Then 40
  create/render/destroy cycles with a native handle audit. Real result:

  ```text
  crtui_render_test: lifecycle cycles=40 handles=77->77 threads=-1->-1
  crtui_render_test: ok pixels=pass background=pass label=pass button=pass
      slider=pass progress=pass rerender=pass stride=pass resize=pass
      lifecycle=pass
  ```

  Mutation-checked (changing the progress fill color fails 3 checks); the first
  frame was also looked at (`CRTUI_RENDER_DUMP=<path>` writes a BMP).
- **Present path (`crtui_window_demo <frames>`).** The same scene rendered into
  a real crtgfx window's software framebuffer and presented with
  `crtgfx_window_end_frame()`; on frame 0 it reads the framebuffer back and
  checks the background and button pixels. Real result on Windows:
  `crtui_window_demo: presented=30 pixel_check=pass`, exit 0.
- Full in-tree `ctest`: 164/164 (one expected no-webcam skip); the default
  C-stage preset excludes `crtui_*` and stays 127/127. `crt-ui-dist` still
  verifies with LVGL compiled in (no LVGL header in the SDK).
- Not yet covered (by design): input/focus/resize mapping (Tranche 2), the
  wrapper-only sample and system-library-style install of LVGL (Tranche 3),
  automatic layout and the v1 widget set (Tranche 4), isolated-stage packaging
  of the fetched LVGL source (Tranche 7).

**Tranche 1 replay (macOS/arm64, 2026-09-30).** No code change was needed.

- `tools/fetch_lvgl.py` downloaded the 111 MB archive, passed the size,
  SHA-256 and pax-commit checks, and extracted 1445 files; all 483 LVGL sources
  plus `lvgl_backend.c` compiled on the CRT toolchain (Apple clang/arm64) with no
  source or conf change and no new CRT/PAL gap. Incremental build of
  `crtui`/`crtui_shared`/tests: ~11 s.
- `crtui_render_test` passes unmodified: `lifecycle cycles=40 handles=4->4
  threads=-1->-1` and all ten groups `pass` (handle counts are per-host: 77 on
  Windows, 4 here; the requirement is equality). Output is byte-identical over
  three runs. The dumped first frame was inspected (`Hello crtui`, blue OK
  button, slider with knob, green progress) and matches the Windows look.
- Mutation check reproduced: changing `CRTUI_COLOR_PROGRESS_FILL` fails the
  same three checks (`render_test.c:212/213/220`); reverted and re-confirmed
  green (`git diff` clean).
- Present path: `crtui_window_demo 30` opened a real Cocoa window through
  crtgfx and reports `presented=30 pixel_check=pass`, exit 0.
- Full in-tree `ctest` 154/154 (camera-authorization test is the expected
  skip); the `macos-host-ninja-debug` test preset (which excludes `crtui_*`)
  110/110; tooling tests 79/79.
- `crt-ui-dist` (LVGL compiled in) builds and `verify_dist.py --stage 05-ui`
  passes in 3 min: `libcrtui.dylib` id `@rpath/libcrtui.dylib`, dependencies
  only `@rpath` CRT libraries and `libSystem.B.dylib`, undefined symbols are
  only libc/pthread calls, and no LVGL file or header is in the SDK.
- Not covered here by design: isolated-stage packaging of the fetched LVGL
  source (Tranche 7).

**Tranche 1 replay (Linux/x86_64, native Intel host, 2026-09-30).** The
library and both tests needed no change; one *build-wiring* fix was needed for
the demo.

- `tools/fetch_lvgl.py` (run through `crtui-lvgl-fetch`; no cached archive
  existed on this host, so it downloaded `v9.6.0.tar.gz` from GitHub) passed the
  size, SHA-256 and pax-commit checks and extracted the same 1445 files. All 483
  LVGL sources plus `lvgl_backend.c` compiled on the CRT toolchain with no source
  or `lv_conf.h` change and no new CRT/PAL gap. A full first build of
  `crtui`/`crtui_shared`/tests took ~9 s of wall time on 12 cores.
- `crtui_render_test` passes unmodified: `lifecycle cycles=40 handles=4->4
  threads=1->1` and all ten groups `pass`, byte-identical over three runs
  (handle counts are per-host; the requirement is equality). The dumped first
  frame was inspected and matches the Windows/macOS look (`Hello crtui`, blue OK
  button, slider with knob, green progress).
- Mutation check reproduced: changing `CRTUI_COLOR_PROGRESS_FILL` fails the same
  three checks (`render_test.c:212/213/220`); reverted, `git diff` clean, green.
- Present path: `crtui_window_demo 30` opened a real Wayland window through
  crtgfx and reports `presented=30 pixel_check=pass`, exit 0, on three runs.
  **Linux-only fix:** the demo first failed to *link* (`undefined reference to
  strndup@@GLIBC_2.2.5`, "DSO missing"). The CRT `libc.so` does export
  `strndup`; the cause was static-archive order. `crtgfx_window` pulls
  `libxkbcommon.a`, which needs libc's `strndup`, but with `crtui` listed first
  CMake hoisted `crtui`'s `c/m/dl/cxx` ahead of `libxkbcommon.a`, so the linker
  fell through to the host libc. Listing `crtgfx_window` before `crtui` (plus a
  trailing `c`) in `libcrtui/CMakeLists.txt` puts the final `libc.a` after it,
  as `crtgfx_window_demo`'s own link does; the comment there records why.
- `crt-ui-dist` with LVGL compiled in (fresh options-default tree,
  `-DCRTUI_ENABLE_LVGL=ON`, reusing the fetched source) builds all of
  `01-c`..`05-ui` and every stage verifies. `libcrtui.so` (3.4 MB): `NEEDED` only
  `libm/libdl/libc++/libc`, `RUNPATH $ORIGIN`, every undefined symbol
  `@CRT_1.0`-versioned, and no LVGL file or header in the SDK.
- Full dev-tree `ctest` 147/148 (the one failure is the pre-existing
  no-sound-card `crtmedia_playback_pipeline_test_runs` gap); tooling 79/79.
  One earlier full run also reported `crtgfx_synthetic_event_runs` failing while
  the `crt-ui-dist` build was running in parallel; it was not reproduced (5/5
  alone, 72 concurrent runs on 12 cores, and a second full run all passed) and
  its output was not captured, so it is recorded here as an unexplained one-off
  (that test needs a live Wayland connection), not as a cause found.
- Not covered here by design: isolated-stage packaging of the fetched LVGL
  source (Tranche 7).

### 2. Input, focus, and resize

Map CRT keyboard/pointer/wheel (and touch where the host has it) to LVGL's
input model. Tests: pointer hit-test, Tab focus traversal, Enter/Space
activation, arrow navigation, slider keyboard adjustment, resize/re-layout,
focus loss and recovery.

**Closed 2026-09-30 on Windows/x64, macOS/arm64, and Linux/x86_64.**

*Design.* crtui's CRT-owned model already owned hit-testing, focus and event
routing (Tranche 0); "mapping to LVGL's input model" therefore means the model
drives what LVGL draws and crtgfx events drive the model, rather than a second
copy of the state living in LVGL indevs. Three additions:

- **crtgfx adapter (`crtui/crtgfx.h`, `crtui_window_handle_crtgfx_event()`).**
  Reads `crtgfx_event` (no link dependency on crtgfx): evdev keycodes for Tab,
  Enter/KP Enter, Space, the arrows and Escape (Shift carried), rounded pointer
  motion, LEFT-button press/release, scroll as a wheel at the last pointer
  position (dy rounded away from zero so a small scroll is never lost; sign as
  crtgfx reports it, host-native and unverified per crtgfx's own note),
  `RESIZE`/`DPI_SCALE_CHANGED` into `crtui_window_set_size()`, `FOCUS_OUT` into
  the new `CRTUI_INPUT_POINTER_CANCEL` (an in-progress press or slider drag ends
  without activating or changing anything). `TEXT` and unknown keys are ignored
  until text input exists. There is no touch input in crtgfx on any host, so
  "touch where available" is vacuous today; a touch source would map to the same
  pointer inputs.
- **Contract revision (arrows).** Tranche 0 froze "arrows step a focused
  slider". For remote/STB-style navigation the arrows now move focus spatially
  to the nearest eligible widget in that direction (score = distance along the
  axis + 2 x offset across it; no wrap; with nothing focused the first eligible
  widget), except that a focused slider consumes Left/Right to step and Up/Down
  still leave it. `crtui_contract_test`'s slider test moved from Up to Right.
- **Slider pointer and wheel.** A press sets the value from the pointer's x and
  captures the pointer: moves (even far outside the slider) update it and reach
  the slider, the release applies the final position and ends the capture; a
  capture also ends if the slider is disabled, hidden or destroyed. The wheel
  over a slider steps it by one. Focus and pressed state are drawn: a 2 px
  `#FF9800` ring just outside the focused widget (clipped by a parent it touches,
  per the clipping rule), and a pressed button in `#1E4FA8`.
- **Resize / re-layout.** Automatic layout is Tranche 4, so re-layout is the
  application's: the adapter turns a native resize into `crtui_window_set_size()`,
  which emits `RESIZED` (new size, DPI scale in percent) and the application
  repositions widgets in that callback; hit-testing, clipping and the renderer
  follow immediately.

*Evidence.*

- `crtui_input_test` (headless, LVGL-free, runs on every host; synthesized
  `crtgfx_event` values): `ok keyboard=pass pointer=pass slider_drag=pass
  wheel=pass resize=pass focus_loss=pass recovery=pass errors=pass` -- evdev
  mapping and Shift, Tab/Shift+Tab, Enter/Space/KP Enter, arrow navigation and
  slider Left/Right, unknown keys and TEXT ignored, left/right/middle buttons,
  press-and-release-elsewhere never activating, slider drag with capture past
  both ends, wheel rounding, resize with application re-layout (old position no
  longer hits, new one does; DPI change; focus survives a resize), focus loss
  cancelling a press and a drag, focus recovery when the focused widget is
  destroyed, and errors for NULL arguments and destroyed windows.
- `crtui_contract_test` gained `arrow_nav=pass slider_pointer=pass`: spatial
  navigation (best-aligned candidate, no wrap, disabled skipped, a handled arrow
  suppresses it), drag capture and its end conditions, wheel.
- `crtui_render_test` gained `focus_ring=pass pressed=pass slider_drag=pass
  relayout=pass`: real pixels for the ring, the pressed fill, a dragged slider's
  fill, and a re-laid-out progress bar after a window resize.
- `crtui_window_demo 30` (real crtgfx window; a scripted sequence injected with
  crtgfx's test hook onto the same queue real events use): Tab focuses the
  button, Enter activates it, a click and a wheel step move the slider by the
  expected amounts -- `presented=30 pixel_check=pass input_check=pass`, exit 0.
  Run without a frame limit it is interactive (real keyboard, mouse and
  resizing).
- Mutation-checked: flipping the slider step's sign fails `crtui_input_test`
  (1 failure) and `crtui_contract_test` (4 failures).
- Full in-tree `ctest` passes (one expected no-webcam skip); the default C-stage
  preset stays 127/127; `crt-ui-dist` verifies.
- Not covered here: text entry, IME, touch (no crtgfx source), automatic layout
  (Tranche 4), hover states.

**Tranche 2 replay (macOS/arm64, 2026-09-30).** No code change was needed.

- `crtui_input_test` passes unmodified with all eight groups `pass`,
  byte-identical over three runs. `crtui_contract_test` (now thirteen groups,
  including `arrow_nav`, `slider_pointer`) and `crtui_render_test` (fourteen
  groups, including `focus_ring`, `pressed`, `slider_drag`, `relayout`;
  `handles=4->4`) pass unmodified too.
- Mutation check reproduced with the same counts as Windows: flipping the
  slider's Left/Right step sign fails `crtui_input_test` (1 failure, line 202)
  and `crtui_contract_test` (4 failures, lines 394/397/403/678); reverted and
  re-confirmed green (`git diff` clean).
- `crtui_window_demo 30` drove a real Cocoa crtgfx window with the scripted
  input sequence: `presented=30 pixel_check=pass input_check=pass`, exit 0.
- Full in-tree `ctest` 155/155 (expected camera skip); the `crtui_*`-excluding
  preset 110/110; tooling 79/79; `crt-ui-dist` and `verify_dist.py --stage 05-ui`
  pass (`@rpath/libcrtui.dylib`, only `@rpath` CRT deps plus `libSystem`, both
  `crtui/ui.h` and `crtui/crtgfx.h` installed, no LVGL file in the SDK).
- Not verified here: *interactive* real keyboard/mouse/trackpad input and live
  window resizing. The scripted run injects events through crtgfx's test hook
  onto the queue real events use, so it covers crtui's mapping but not AppKit's
  own delivery. The macOS wheel sign is host-native (crtgfx reads AppKit's
  `deltaY`, already adjusted for the user's natural-scrolling setting) and
  remains unverified, as the Windows entry also noted.

**Tranche 2 replay (Linux/x86_64, native Intel host, Wayland, 2026-09-30).**
No code, test, or build-wiring change was needed (Tranche 1's link-order fix
already covers the demo).

- `crtui_input_test` (all eight groups), `crtui_contract_test` (thirteen
  groups, including `arrow_nav` and `slider_pointer`) and `crtui_render_test`
  (fourteen groups, including `focus_ring`, `pressed`, `slider_drag`,
  `relayout`; `handles=4->4 threads=1->1`) pass unmodified, each byte-identical
  over three runs.
- Mutation check reproduced with the same counts and lines as Windows/macOS:
  flipping the slider's Left/Right step sign (`libcrtui/src/core.c`) fails
  `crtui_input_test` (1 failure, `input_test.c:202`) and `crtui_contract_test`
  (4 failures, `contract_test.c:394/397/403/678`); reverted, `git diff` clean,
  re-confirmed green.
- `crtui_window_demo 30` drove a real Wayland window through crtgfx with the
  scripted input sequence: `presented=30 pixel_check=pass input_check=pass`,
  exit 0, three runs.
- Full dev-tree `ctest` 148/149 run on an otherwise idle machine (the one
  failure is the pre-existing no-sound-card `crtmedia_playback_pipeline_test_
  runs` gap; `crtgfx_synthetic_event_runs`, which failed once in the Tranche 1
  replay under parallel build load, passed); tooling tests 79/79.
- `crt-ui-dist` with LVGL on (fresh options-default tree, ~3.5 min incremental)
  builds and `verify_dist.py --stage 05-ui` passes: both `crtui/ui.h` and
  `crtui/crtgfx.h` are installed, `libcrtui.so` has `NEEDED` only
  `libm/libdl/libc++/libc`, `RUNPATH $ORIGIN`, every undefined symbol
  `@CRT_1.0`-versioned and none from crtgfx (the adapter has no link
  dependency, as designed), and no LVGL file or header is in the SDK.
- Not verified here: *interactive* real keyboard/mouse input, real scroll-wheel
  direction, and live window resizing on Wayland. This host has no input
  injection tool (`wtype`, `ydotool` and `dotool` are all absent), and the
  scripted run injects through crtgfx's test hook, so it covers crtui's mapping
  but not the Wayland seat/pointer/keyboard delivery. The Wayland scroll sign is
  host-native and unverified, as on the other hosts. Running `crtui_window_demo`
  with no frame limit on a desktop session is the manual check.

### 3. `crtui` wrapper (first green)

**Closed 2026-09-30 on Windows/x64, macOS/arm64, and Linux/x86_64.**

The sample application calls no `lv_*` symbol. LVGL becomes a private
dependency: not in installed headers, not in the public link line of the API.

The application-facing surface was already LVGL-free at the header level; this
tranche made the *binary* surface match and made both provable.

- **A real finding: the shared library published LVGL.** `libcrtui.dll` was
  exporting 2,286 `lv_*` functions plus LVGL's bundled helpers (`frogfs_decomp_raw`,
  `load_kern`) next to its 35 `crtui_*` functions, because
  `WINDOWS_EXPORT_ALL_SYMBOLS` exports everything (an ELF/Mach-O `.so`/`.dylib`
  would have done the same by default visibility). So a consumer could have
  called LVGL through the DLL even though no header was installed. Fixed with
  explicit export control: `include/crtui/api.h` defines `CRTUI_API`
  (`dllexport` / default visibility, only while building libcrtui; empty for
  consumers), every public function in `ui.h`/`crtgfx.h` carries it, crtui and
  LVGL are compiled `-fvisibility=hidden`, and `WINDOWS_EXPORT_ALL_SYMBOLS` is
  OFF. `libcrtui.dll` now exports exactly the 35 declared `crtui_*` functions and
  nothing else. (The `dllexport` definition is limited to the shared target on
  Windows so it never lands in a consumer executable; the ELF/Mach-O visibility
  attributes are reasoned, not yet run -- the Linux and macOS replays verify
  them.)
- **The sample application.** `examples/ui-basic` is the installed application
  view: `main.c` (the same source as `crtui_window_demo`: a
  Window/Container/Label/Button/Slider/Progress scene, crtgfx input through the
  adapter, present through crtgfx), a standalone `CMakeLists.txt` that finds
  only `crtui` and `crtgfx` (import libraries `crtui_dll`/`crtgfx_dll` on Windows,
  static archives elsewhere) and names no LVGL header or library, and the
  prebuilt `examples/bin/crtui_window_demo`. `verify_dist.py` requires all three
  and rejects an `lv_*`/`LV_*`/`lvgl` include in the installed sample sources.
- **`tools/check_crtui_privacy.py` (CTest `crtui_privacy_test_runs`).** With
  llvm-nm/llvm-readobj: (1) no crtui header includes LVGL or uses an `lv_`
  identifier (comments may say that LVGL is private); (2) the shared library
  exports only `crtui_*` symbols, none `lv_*`, and every `CRTUI_API` function
  the headers declare; (3) the sample application's object files reference no
  `lv_*` symbol; (4) the sample source names no LVGL. Real result:
  `crtui_privacy: ok declared=35 exports=35 lvgl_exports=0 objects=1 headers=3`.
  Mutation-checked: a sample that calls `lv_obj_create` fails both the source
  and the object check.
- **Rebuilt externally from the packaged SDK.** The installed
  `examples/ui-basic` was configured and built with the packaged
  `crt-toolchain.cmake` against `dist/05-ui` alone (no repository headers or
  libraries), linking only `libcrtui_dll` and `libcrtgfx_dll`; the resulting
  `crtui_basic_example.exe` imports `libcrtui.dll` and `libcrtgfx.dll` (plus
  KERNEL32/synch), passes the privacy tool against the packaged DLL and headers,
  and `crtui_basic_example 30` reports `presented=30 pixel_check=pass
  input_check=pass`.
- Full in-tree `ctest` passes (one expected no-webcam skip), the default C-stage
  preset stays 127/127, and `crt-ui-dist` verifies with the new checks.
- Not yet covered: the Linux/macOS variants of the export control and of
  `examples/ui-basic/CMakeLists.txt` (written, unverified), and the isolated
  stage build that fetches LVGL and rebuilds this sample (Tranche 7).

**Tranche 3 replay (macOS/arm64, 2026-09-30).** Two real findings; both fixed.

- **Finding 1 -- privacy tool could not run on macOS.** `crtui_privacy_test_runs`
  failed with `llvm-nm not found in /usr/bin`: on macOS `CMAKE_C_COMPILER` is
  Apple's `/usr/bin/clang` shim and the LLVM binutils live in the Xcode
  toolchain. `tools/check_crtui_privacy.py` now falls back to `PATH`, then
  `xcrun --find`. Result: `crtui_privacy: ok declared=35 exports=35
  lvgl_exports=0 objects=1 headers=3`.
- **Visibility control verified on Mach-O.** `libcrtui.dylib` exports exactly
  the 35 `crtui_*` functions. Mutation: dropping `-fvisibility=hidden` makes the
  dylib export 2,286 `lv_*` symbols plus `frogfs_decomp_raw`/`load_kern`, the same
  count as the Windows DLL, and the privacy test fails; restored and green. A
  sample that calls `lv_obj_create` fails both the object and source checks.
- **Finding 2 -- the shared library corrupted the heap.** The installed
  `examples/ui-basic`, rebuilt externally from the packaged SDK with the packaged
  `crt-toolchain.cmake` (links `libcrtui.dylib`/`libcrtgfx.dylib`), ran its 30
  frames and then aborted in `crtui_context_destroy()` with `pointer being freed
  was not allocated` (exit 134). The prebuilt `examples/bin/crtui_window_demo` and
  every in-tree test passed because they link the static library. Cause:
  `libcrtui.dylib` was linked `... -lSystem libc.dylib`; two-level namespace
  binding takes the first dylib that defines a symbol, so `pthread_mutex_init`,
  `malloc`, etc. bound to Apple's `libSystem`, while the structs were laid out
  with the CRT header's 40-byte `pthread_mutex_t` (Apple's is 64). Initializing
  the `post_lock` embedded in the context overwrote the following fields. Fix
  (`libcrtui/CMakeLists.txt`, macOS): name `libc.dylib` first on the crtui link
  line; the dylib now shows `_pthread_mutex_init (from libc)`. Guard:
  `crtui_contract_shared_test_runs` (macOS) runs the whole contract test linked
  against `libcrtui.dylib`; with the fix removed it aborts (exit 134), with it
  it passes.
- **Broader exposure, not fixed here.** The other CRT dylibs are linked the same
  way (`libcrtgfx`, `libcrtmedia`), so they bind libc to `libSystem` with the same
  struct-size mismatch. Recorded in `TODO.md` as a CRT/PAL follow-up rather than
  changed under a crtui tranche.
- After the fix: the externally built sample runs 3/3 (`presented=30
  pixel_check=pass input_check=pass`) with only `@rpath` crtui/crtgfx plus
  `libSystem` in its load commands; the privacy tool passes against the packaged
  dylib, headers, sample object and source. Full in-tree `ctest` 157/157
  (expected camera skip); `crtui_*`-excluding preset 110/110; tooling 79/79;
  `crt-ui-dist` verified.
- Not covered here: the isolated stage build that rebuilds the sample
  (Tranche 7).

**Tranche 3 replay (Linux/x86_64, native Intel host, 2026-09-30).** ELF export
control worked as reasoned; two build/tooling gaps found and fixed.

- **Visibility verified on ELF.** `libcrtui.so` exports exactly the 35 `crtui_*`
  functions and nothing else. Mutation: removing `-fvisibility=hidden` from the
  shared target and LVGL flags makes it export 2,286 `lv_*` symbols plus
  `frogfs_decomp_raw`/`load_kern` (same count as the Windows DLL and Mach-O
  dylib) and the privacy test fails; restored and green.
- **Finding 1 -- privacy tool found no LLVM binutils.** Debian installs only
  `llvm-nm-21`/`llvm-readobj-21` in `/usr/bin`, so `crtui_privacy_test_runs`
  failed `llvm-nm not found in /usr/bin`. `tools/check_crtui_privacy.py` now
  also tries versioned `/usr/bin/<tool>-N` and `/usr/lib/llvm-*/bin/<tool>`.
  Result: `crtui_privacy: ok declared=35 exports=35 lvgl_exports=0 objects=1
  headers=3`.
- **Finding 2 -- `crt-ui-dist` did not build what it installs.** The `crt-ui`
  install component ships `crtui_window_demo`, but test/demo executables are not
  part of `all` and `crt-ui-build` depended only on `crtui crtui_shared`, so
  `cmake --install --component crt-ui` failed with `cannot find
  .../libcrtui/crtui_window_demo` in a tree where the demo was not already
  built (the other hosts happened to have it). `crt-ui-build` now depends on
  `crtui_window_demo`; `crt-ui-dist` then builds, packages and verifies
  (`CRT distribution verified`).
- **Sample rebuilt externally from the packaged SDK.** `examples/ui-basic`
  configured with the packaged `crt-toolchain.cmake` against `dist/05-ui` alone,
  linking the static `libcrtui.a`/`libcrtgfx.a` (Linux's Wayland link list
  copied from gfx-simple worked unchanged); NEEDED is only the host
  `libwayland-client.so.0`. `crtui_basic_example 30` ran 3/3: `presented=30
  pixel_check=pass input_check=pass`. The privacy tool passes against the
  packaged headers, `libcrtui.so`, the sample object and source; a sample calling
  `lv_obj_create` fails both the object and source checks.
- **Caveat.** This options-default `out/linux-ui-check` dist contained no
  `libxdg-shell-protocol.a` (that tree never built Wayland), so the external
  rebuild used a scratch copy of the SDK with the archive taken from the dev
  tree's `dist/04-gfx-media`. This historical scratch-copy result remains limited;
  the ordinary producer repair and unmodified packaged example rebuilds are in
  the [2026-10-26 build-system acceptance](build_system_acceptance.md#final-linuxamd64-entry-point-acceptance).
- Full in-tree `ctest` 149/150 (only `crtmedia_playback_pipeline_test_runs`:
  no sound card, pre-existing); `crtui_window_demo 30`: `presented=30
  pixel_check=pass input_check=pass`; tooling 79/79.
- Not covered here: the isolated stage build that rebuilds the sample
  (Tranche 7).

### 4. Layout, styling, and the v1 widget set

Window/Screen, Container, Row, Column, Stack, Text, Button, Image, Slider,
Progress, Switch, Checkbox, List/ScrollView, TextInput. A CRT-neutral property
set only: size, margin, padding, alignment, background, foreground, font,
border, opacity. Do not re-export LVGL's style API.

**Windows/x64 result (2026-10-01).** Implementation and first-host acceptance
are complete; macOS/arm64 and Linux replay remained before this tranche could
close globally.

- `crtui/ui.h` adds only CRT-owned enums, structs and opaque widget ids: free,
  Row, Column and Stack layout; `CRTUI_SIZE_FILL`, grow, margin, padding, gap,
  main/cross-axis alignment; and a style mask for RGB background/foreground/
  border, border width, radius, opacity, three font sizes and text alignment.
  No LVGL type or constant crosses the public boundary. Image copies caller
  BGRA pixels; Switch/Checkbox share a checked contract; ScrollView/List own a
  clamped vertical offset; TextInput owns UTF-8 text, a byte-boundary caret,
  placeholder, edit keys and committed-text input.
- Layout is lazy but authoritative before bounds reads, hit-testing, input and
  rendering. Hidden children release their space, resizing reflows the tree,
  focus scrolls a child into view, and children remain clipped to ancestors.
  The flex implementation deliberately uses allocation-free multi-pass
  traversal: an allocator failure can no longer silently skip layout, preserving
  the frozen no-silent-failure rule.
- `lvgl_backend.c` mirrors the private render list into LVGL objects and maps
  the CRT style/widget state to real pixels. LVGL's 12/14/20 px fonts are
  private build inputs. `crtui_privacy_test_runs` reports
  `declared=63 exports=63 lvgl_exports=0`: the larger public surface is the
  additive Tranche 4 contract, while LVGL remains absent from headers, exports,
  sample source and sample objects.
- `crtui_layout_test` covers free/Row/Column/Stack placement, grow/alignment/
  spacing, hidden-child reflow, resize, ScrollView/List clipping and focus,
  style validation, toggles, UTF-8 TextInput editing and copied Image data. It
  passes both the LVGL-enabled build and a fresh `CRTUI_ENABLE_LVGL=OFF` build:
  `column=pass row=pass main_align=pass stack=pass free=pass scroll=pass
  list=pass style=pass toggles=pass text_input=pass image=pass`.
- The existing input and render gates were extended rather than replaced.
  `crtui_input_test` adds TextInput, toggle and scroll behavior;
  `crtui_render_test` adds real-pixel checks for layout, style and every v1
  widget and retains 40 create/render/destroy cycles. The official
  `crt-ui-test` target now depends on `crtui_layout_test`, `crtui_input_test`,
  the conditional renderer test and the macOS shared-contract test before it
  runs CTest, fixing the same clean-tree missing-executable class encountered
  in earlier gfx/media targets. Result: crtui 5/5.
- `crtui_window_demo` and installed `examples/ui-basic` now contain no manual
  child coordinates: a Column/Row tree lays out Button/Switch/Checkbox, Slider,
  Progress and TextInput, follows resize automatically, and accepts committed
  text through the crtgfx adapter. The real Win32 run reports
  `presented=30 pixel_check=pass input_check=pass`.
- Full Windows CTest is 167/167 with the expected no-camera skip. `crt-ui-dist`
  rebuilt and verified `dist/05-ui`; its installed sample was then configured
  and built in a fresh directory using only the packaged toolchain/SDK and ran
  against the packaged DLLs with the same 30-frame result. This host required
  the manifest-declared external `CRT_WINDOWS_SDK_LIBPATH` because it was not a
  Visual Studio Developer shell; that is a build-host prerequisite, not a
  bundled SDK dependency.

**macOS/arm64 replay (2026-10-01).** The implementation replayed without a
functional UI change; one cumulative-distribution build race was exposed and
fixed.

- The official LVGL-enabled `crt-ui-test` gate is 6/6, including the macOS
  shared-contract guard. Direct output covers every Tranche 4 group:
  `column/row/main_align/stack/free/scroll/list/style/toggles/text_input/image`
  all pass, the extended input groups pass, and render lifecycle remains
  `cycles=40 handles=4->4 threads=-1->-1`.
- A fresh `CRTUI_ENABLE_LVGL=OFF` tree built and passed both
  `crtui_layout_test_runs` and `crtui_input_test_runs` (2/2), proving that the
  authoritative model/layout/input path remains independent of LVGL.
- Both the in-tree demo and an externally configured sample built only from
  packaged `dist/05-ui` ran in real Cocoa windows for 30 frames and reported
  `presented=30 pixel_check=pass input_check=pass`. The packaged privacy check
  reports `declared=63 exports=63 lvgl_exports=0 objects=1 headers=3`.
- The first `crt-ui-dist` attempt exposed a real dependency race: the stage-04
  graphics/media build and stage-03 cumulative predecessor were sibling jobs,
  so `crt-libcxx-sysroot` and `crt-libcxx-dist` could refresh `dist/02-cxx`
  while `crtgfx_skia_gpu_window_demo` linked, temporarily removing
  `libc++.a`/`libc++abi.a`. The two `dist/02-cxx` publishers are now serialized
  (`crt-libcxx-sysroot`, then the release-grade `crt-libcxx-dist`), and the
  stage-04 aggregate plus its Skia demo wait for the complete, verified
  `crt-gfx-simple-dist` predecessor. Repeated full distribution runs then
  generated and verified every cumulative stage through `dist/05-ui` without
  the race.
- Tooling is 79/79. Full in-tree CTest is 157/158: all six crtui tests and all
  other tests pass, while the known independent
  `crtmedia_capture_encode_lifecycle_test_runs` repeatedly fails its capture
  lifecycle cycle on this camera environment instead of taking its documented
  skip path. This is recorded as a host/media residual, not hidden as a UI
  pass.

**Linux/x86_64 replay (2026-10-01).** The implementation replayed without a
functional source change and closes Tranche 4 on all three hosts.

- The official LVGL-enabled `crt-ui-test` gate is 5/5. It was repeated three
  consecutive times; contract, layout, input, render and privacy all remained
  green. Direct layout output reports
  `column/row/main_align/stack/free/scroll/list/style/toggles/text_input/image`
  all pass, and the extended input groups pass. Render covers all 17 groups
  and its 40-cycle lifecycle remains `handles=5->5 threads=1->1`.
- A fresh `CRTUI_ENABLE_LVGL=OFF` tree passed
  `crtui_layout_test_runs` and `crtui_input_test_runs` (2/2), proving the
  authoritative model/layout/input path remains independent of LVGL.
- The in-tree Wayland demo ran three times for 30 frames with
  `presented=30 pixel_check=pass input_check=pass`. After the documented clean
  then-documented Linux sequence (`crtgfx-wayland-build`, reconfigure, then distribution),
  `crt-ui-dist` generated and verified every cumulative stage through
  `dist/05-ui`; the manifest and package contain the generated
  `libxdg-shell-protocol.a` dependency.
- The installed `examples/ui-basic` was configured and built in a fresh
  directory using only packaged `dist/05-ui`, then ran 3/3 in real Wayland
  windows with the same 30-frame result. Its only dynamic host dependency is
  `libwayland-client.so.0`. Packaged privacy reports
  `declared=63 exports=63 lvgl_exports=0 objects=1 headers=3`.
- Tooling is 79/79. Full in-tree CTest is 150/151: all five crtui tests and all
  other tests pass except the previously recorded independent
  `crtmedia_playback_pipeline_test_runs` real-wall-time pacing check. A direct
  rerun reproduced it and `aplay -l` still reports `no soundcards found`, so
  this remains the known host/media residual rather than a UI failure.

### 5. External Surface (the WebKit prerequisite)

`crtui_surface_view` represents where the final compositor displays a
producer-owned `crtgfx` external surface and owns only position, size, clip,
opacity, visibility, z-order, damage, and input hit-testing. It does not know
the producer. This is what lets a later WebView avoid being copied into an LVGL
CPU framebuffer.

Work is split at the actual ownership boundary:

1. **5A, producer-neutral scene snapshot (Windows/x64 accepted 2026-10-01).**
   `crtui_surface_view_create()` creates a real widget. The allocation-free,
   two-call `crtui_window_get_surface_layers()` snapshot returns only visible,
   non-empty layers, in stable scene order, with a stable SurfaceView id,
   window-space bounds, the intersection of every ancestor clip, effective
   opacity multiplied through the ancestor chain, and pending damage. The
   application/final compositor maps that id to its producer-owned surface;
   `crtui` deliberately stores no producer pointer. Scene mutations
   conservatively mark the whole view dirty; producer damage rectangles are
   unioned, clipped at snapshot time and carry a monotonic serial until
   `crtui_surface_view_clear_damage()` acknowledges them. The snapshot does not
   allocate, acquire a frame, or call producer code. Insufficient caller
   capacity returns `CRTUI_WOULD_BLOCK`, reports the required count and writes
   no partial result.
2. **5B, real final composition (Windows/x64 accepted 2026-10-01).** The
   optional static `crtui_skia` companion is the final-compositor boundary;
   plain `libcrtui` retains no Skia/GPU dependency. It snapshots SurfaceViews,
   asks the application provider for one retained `SkImage` per visible layer,
   draws that image directly on the GPU-backed target, and invokes the matching
   release callback exactly once after the synchronous draw. LVGL renders only
   the UI preorder ranges below and above each SurfaceView into transparent UI
   planes. The producer image is never copied into LVGL or read back to the CPU.
   `crtui_window_render_plane()` retains geometry-only placeholders outside its
   range so descendant coordinates and ancestor clipping remain identical to
   the complete scene.
3. **5C, host replay (macOS/arm64 accepted 2026-10-01; Linux/x86_64 accepted
   2026-10-02).** The unchanged contract/compositor is accepted on both
   remaining hosts, closing Tranche 5 globally. MediaView remains Tranche 6.

**5A Windows evidence.** `crtui_surface_test` reports
`create=pass geometry=pass clip=pass opacity=pass z=pass damage=pass
hit_test=pass visibility=pass thread=pass`. The complete LVGL-enabled
`crt-ui-test` is 6/6;
the LVGL-OFF tree is 4/4. Export/privacy remains exact after adding three API
functions: `declared=66 exports=66 lvgl_exports=0 objects=1 headers=3`. Full
Windows CTest is 168/168 (one expected no-camera skip), and `crt-ui-dist`
rebuilds/verifies the cumulative packaged `05-ui`; the package itself passes
the same 66/66 privacy check.

**5B Windows evidence.** `crtui_surface_compositor_test` creates a synthetic
green GPU surface and verifies that its snapshot is texture-backed. Exact pixel
readback proves red UI below the SurfaceView, the ancestor-clipped hole, 50%
producer opacity over that UI, and blue UI above it. The same test exercises
damage metadata and a 160x100 to 200x120 resize, with exactly one producer
acquire and release per composition; a no-frame acquire returns
`CRTUI_WOULD_BLOCK` without a release callback. Its interactive acceptance mode
wrapped a real D3D12 swapchain as a Skia surface and presented 5/5 frames with
balanced ownership. The Skia-enabled official `crt-ui-test` gate passes 7/7,
including the new compositor test and all prior UI tests; shared-library privacy is
`declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`. This closed 5B on
Windows; the same sources were then replayed unchanged on macOS/arm64.

**5A/5B macOS/arm64 replay evidence.** The official Skia-enabled
`crt-ui-test` gate passes 8/8, including the macOS shared-contract guard and
the compositor test. Direct `crtui_surface_test` output reports
`create/geometry/clip/opacity/z/damage/hit_test/visibility/thread=pass`. A
fresh `CRTUI_ENABLE_LVGL=OFF` build passes contract, layout, surface and input
4/4, preserving the producer-neutral 5A boundary.

The real Metal-window compositor mode presents 5/5 frames and reports
`gpu/ordering/clip/opacity/damage/resize/ownership=pass`; producer ownership is
balanced. Core `libcrtui.dylib` remains free of Skia, Metal and `crtgfx`
dynamic dependencies, while privacy is exact at
`declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`. `crt-ui-dist`
rebuilds and verifies cumulative `dist/05-ui`, installing the optional static
`libcrtui_skia.a` companion and `crtui/skia.h` separately from core
`libcrtui`.

Tooling is 79/79. Every `crtui` test passed in the initial 159/160 full run;
the sole failure was the independent AVFoundation lifecycle residual. The
follow-up fixed that backend's queue ownership and dimension negotiation, and
full in-tree CTest is now 160/160. No functional UI source change was required
for the macOS replay.

**5A/5B Linux/x86_64 replay evidence.** The unchanged Skia-enabled official
`crt-ui-test` gate passes 7/7 and remained green for three consecutive runs. A
fresh LVGL-OFF tree passes contract, layout, surface and input 4/4. Direct
surface output reports
`create/geometry/clip/opacity/z/damage/hit_test/visibility/thread=pass`.

The real Wayland/Vulkan compositor presented 5/5 frames in three consecutive
runs and reported `gpu/ordering/clip/opacity/damage/resize/ownership=pass`,
with balanced producer ownership. Core `libcrtui.so` has only the CRT
`libm.so`, `libdl.so`, `libc++.so` and `libc.so` dependencies; it has no Skia,
Vulkan or `crtgfx` dynamic dependency. Privacy remains exact at
`declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`.

The first clean cumulative distribution replay exposed a real build-graph
defect: the `crt-gfx` install component shipped `crtgfx_skia_shared`, while no
stage-04 aggregate built it because the Skia window demo links only the static
bridge. `crt-gfx-media-build` now explicitly builds the shared bridge after
`crt-gfx-simple-dist`. From a new build directory, the documented libc++ and
Wayland setup followed by `crt-ui-dist` builds `libcrtgfx_skia.so` without a
manual target invocation and verifies every cumulative stage through
`dist/05-ui`. The package installs the optional static `libcrtui_skia.a` and
`crtui/skia.h` separately from core `libcrtui`. Tooling is 79/79. The full
feature-enabled in-tree CTest replay is 158/159: all seven `crtui` tests and
all other tests pass except the previously recorded, independent playback
wall-time residual on this host with no sound device. Tranche 5 is therefore
closed on Windows/x64, macOS/arm64 and Linux/x86_64.

### 6. MediaView

Connect the accepted zero-copy path (`crtmedia` GPU frame -> Skia image ->
`crtgfx` surface) to a `MediaView` widget without passing pixels through an
LVGL image buffer. UI and video are composed in the final present, not merged
earlier. Three slices: 6A contract, 6B Windows real-media acceptance, 6C replay
on macOS/arm64 and Linux/x86_64.

**6A, frozen contract (Windows/x64 accepted 2026-10-02).**

1. `crtui_media_view_create()` makes a distinct `CRTUI_WIDGET_MEDIA_VIEW`. It
   follows the SurfaceView scene rules exactly (bounds, clip, ancestor opacity,
   z-order, visibility, damage, hit-testing) and appears in
   `crtui_window_get_surface_layers()`. Core `libcrtui` stores no decoder, frame,
   GPU or Skia object, so it stays producer-neutral.
2. The optional static C++ companion `crtui_skia_media` (`crtui/skia_media.h`)
   owns one current imported image per MediaView. `submit()` hands a
   `crtmedia_gpu_frame` to the existing `crtgfx_skia_import_media_frame()`:
   success moves frame ownership into the image and clears the frame; failure
   leaves the frame untouched. A non-GPU (CPU) frame is rejected with
   `CRTUI_ERROR_INVALID_ARGUMENT` and is never labelled zero-copy.
3. Replacing or clearing a view releases the previous image only after any
   in-flight compositor acquire has released its temporary reference. `clear()`
   damages the view; composing a MediaView with no frame returns
   `CRTUI_WOULD_BLOCK` without a release callback. `crtui_skia_media` is
   installed separately; core and `crtui_skia` do not link `crtmedia`.

**6B Windows evidence.** `crtui_media_view_test` decodes the real H.264 fixture
through the hardware-preferring decoder and submits each GPU frame to a
MediaView. The offscreen run reports `backend=d3d12 interop=gpu-copy
gpu_frames=5 cpu_frames=0 releases=5` and
`frames/texture/composition/damage/ownership=pass`, covering UI planes above and
below the video, a 320x180 to 360x200 resize, replacement, clear and destroy
with every decoder frame released exactly once. `interop=gpu-copy` is the
honest Windows classification (GPU-resident copy, not shared-handle
zero-copy). Passing a frame count presents the same path in a real D3D12 window:
5/5 frames, `gpu_frames=5 cpu_frames=0 releases=5`.

A defect found while accepting: the test's cleared-view check built its empty
target at the pre-resize size, so `crtui_skia_compose()` correctly rejected the
size mismatch. The test now reads the current window size.

**6C macOS/arm64 replay (2026-10-02).** Accepted unchanged: no source,
CMake or test change was needed.

- `crtui_media_view_test` decodes the real H.264 fixture through
  VideoToolbox and submits each GPU frame to a MediaView. Offscreen:
  `backend=metal interop=zero-copy gpu_frames=5 cpu_frames=0 releases=5` and
  `frames/texture/composition/damage/ownership=pass`. Output is byte-identical
  over five runs. Unlike Windows (`gpu-copy`), the macOS path is classified
  `zero-copy`, as classified by the existing `crtgfx_skia_import_media_frame()`
  Metal import of the VideoToolbox frame (the classification is that function's
  and the test's own; this replay did not independently audit the GPU path), and
  no CPU frame was used (`cpu_frames=0`).
- Passing a frame count presents the same path in a real Cocoa/Metal window:
  `crtui_media_view_test 5` reports `window backend=metal interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, exit 0.
- Mutation check: skipping the release of the replaced image in
  `crtui_skia_media_provider_submit()` fails the ownership check (`releases=1`
  instead of 5, exit 1); reverted and re-confirmed green (`git diff` clean).
  (That mutation exposed a misleading summary: a failing run still printed the
  fixed `frames=pass ...` groups after `FAILED`. `crtui_media_view_test` and
  `crtui_surface_compositor_test` now print `FAILED (see the failed checks
  above)` and the group list only on success; re-mutated and confirmed.)
- Full in-tree `ctest` 162/162 (expected camera-authorization skip); the nine
  `crtui_*` tests pass (also through `crt-ui-test`); the `crtui_*`-excluding
  preset 110/110; tooling 80/80. `crt-ui-dist` verifies and installs
  `libcrtui_skia_media.a` and `crtui/skia_media.h` with no LVGL file in the SDK.
- Not covered here: Linux/x86_64 (Vulkan/VA-API) replay, which closes 6C, and
  the isolated-stage packaging of `crtui_skia_media` (Tranche 7).

**6C Linux/x86_64 replay (native Intel host, 2026-10-02).** Accepted unchanged:
no source, CMake or test change was needed (the dev tree only needed
`CRTMEDIA_ENABLE_FFMPEG=ON`, without which `crtui_media_view_test` is not
registered).

- `crtui_media_view_test` decodes the real H.264 fixture through VA-API and
  submits each GPU frame to a MediaView. Offscreen: `backend=vulkan
  interop=zero-copy gpu_frames=5 cpu_frames=0 releases=5` and
  `frames/texture/composition/damage/ownership=pass`, identical over three runs.
  Note the `interop=` word is derived from the backend by the test
  (`gpu-copy` only for D3D12); the evidence that this is the dma-buf path is
  that the Linux `crtgfx_skia_import_media_frame()` Vulkan branch accepts only
  `crtmedia_vaapi_gpu_frame_handle` dma-buf frames (the Tranche 3 zero-copy
  work) and `cpu_frames=0`; no CPU fallback was used or labelled zero-copy.
- Passing a frame count presents the same path in a real Wayland/Vulkan window:
  `crtui_media_view_test 5` reports `window backend=vulkan interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, exit 0.
- Mutation check: replacing the release of the replaced image in
  `crtui_skia_media_provider_submit()` with `(void)previous;` fails the ownership
  check (`releases=1` instead of 5, exit 1, `FAILED (see the failed checks
  above)` and no pass summary); reverted, rebuilt and green (`git diff` clean).
  (A first attempt that merely commented the line out did not compile -- unused
  variable -- so the run that followed was a stale binary; discarded.)
- Full in-tree `ctest` 154/155 (only `crtmedia_playback_pipeline_test_runs`: no
  sound card, pre-existing); all eight `crtui_*` tests pass; tooling 80/80.
  `crt-ui-dist` (run in the Skia/FFmpeg dev tree, curl OFF) verifies and
  installs `libcrtui_skia.a`, `libcrtui_skia_media.a` and `crtui/skia.h`,
  `crtui/skia_media.h`; `libcrtui.so` exports only `crtui_*`, NEEDED
  libm/libdl/libc++/libc, RUNPATH `$ORIGIN`, no LVGL file in the SDK. (The
  options-default `out/linux-ui-check` tree has Skia OFF and so cannot package the
  Skia companions.)
- Not covered here: interactive/visual inspection of the window output (the
  test checks pixels offscreen), and the isolated-stage packaging of
  `crtui_skia_media` (Tranche 7).

### 7. Cross-host and isolated-package closure

Windows/x64, then macOS/arm64, then Linux. Stage acceptance builds `05-ui` in
isolation from the installed `04-gfx-media` SDK (no repository headers), with
`tools/create_stage_source.py` and `tools/crt_dist_prerequisites.py`
registries updated in the same change -- both were silently stale in earlier
tranches; do not repeat that. Include an installed sample, `verify_dist.py`,
and the per-OS dependency/RPATH checks.

**Frozen design (Windows/x64 accepted 2026-10-03).**

1. *What the stage consumes.* The isolated, option-ON `04-gfx-media` SDK
   (`manifest.built_from.stage == "03-gfx-simple"`): the entrypoint refuses the
   ordinary cumulative 04 package up front, because that one has no Skia, FFmpeg
   or curl. crtgfx/crtmedia headers, `libskia.a`, FreeType, FFmpeg, the curl chain,
   the mingw/win32-shim headers and the libc-owned emulated-TLS runtime all come
   from the installed SDK. The asset (`distribution/stages/05-ui/CMakeLists.txt`,
   `tools/build_stage_05_ui.py`, `libcrtui/{cmake,include,src,tests,tools}`, the
   LVGL recipe and fetcher, the test clip, the sample project) is ~118 KB.
2. *LVGL.* Fetched live and pinned by `libcrtui/third_party/lvgl/recipe.json`
   (size and SHA-256 re-verified every run, archive cached in the work root),
   compiled privately into `libcrtui`. It is declared in the manifest as a
   `private-static-port` with no headers and no link artifacts, and its MIT notice
   (`share/licenses/lvgl/LICENCE.txt`) and pinned recipe
   (`share/crt/dependencies/lvgl/recipe.json`) are packaged -- the in-tree package
   never shipped that notice.
3. *One source list.* `libcrtui/cmake/crtui_sources.cmake` is the only place the
   core, LVGL-backend, Skia and MediaView source files are named; the in-tree
   `libcrtui/CMakeLists.txt` and the stage project both include it, and
   `tools/test_stage_source_closure.py` fails if a file it names is missing from
   the asset. The closure test also checks that stage 04 really installs the
   crtgfx/crtmedia header trees 05-ui compiles against.
4. *Installed result.* `libcrtui.a` + `libcrtui_dll.dll.a`/`libcrtui.dll`,
   `libcrtui_skia.a`, `libcrtui_skia_media.a`, `include/crtui/*`,
   `examples/bin/crtui_window_demo`, `examples/ui-basic`. `verify_dist.py
   --stage 05-ui` additionally requires, for an isolated build, both companion
   libraries and headers, the LVGL record (with no header/link artifact) and the
   notice/provenance files, and that every 04 dependency record survived. It does
   not require the companion libraries of the ordinary in-tree package, which
   installs `crtui/skia*.h` unconditionally but builds the libraries only with
   Skia/FFmpeg enabled.
5. *Prerequisites registry.* `tools/crt_dist_prerequisites.py` needed no new
   entry: crtui adds no external runtime, so `05-ui` equals `04-gfx-media` on every
   OS (checked), and the PE dependency scan of `libcrtui.dll` is clean.
6. *Recipe chain.* `STAGE_SUCCESSORS` gained `04-gfx-media -> 05-ui`;
   `crt-stage-05-source` generates the asset and recipe; `crt-gfx-media-dist`
   packages `05-ui.json` into the in-tree 04 package, and the 04 source asset now
   embeds it (as 02-cxx embeds 03's) so an isolated 04 SDK can bootstrap 05-ui from
   a downloaded asset alone.

**Windows evidence.** From the isolated 04 SDK (built from the real packaged 03
SDK with `crt-stage-build.py`), the isolated 05-ui stage passes in 151 s: ctest
8/8 (contract, layout, surface, input, render, privacy, surface compositor, media
view), where the compositor test reports `gpu/ordering/clip/opacity/damage/resize/
ownership=pass` and the media view test decodes the real clip
(`backend=d3d12 interop=gpu-copy gpu_frames=5 cpu_frames=0 releases=5`); the
installed sample rebuilt externally from the SDK and the prebuilt
`examples/bin/crtui_window_demo` both report `presented=30 pixel_check=pass
input_check=pass`; `verify_dist.py` and the atomic publish pass. A run from a work
and output path containing a space (129 s) passes too. Removing the LVGL notice
makes `verify_dist.py` fail, so the new check is live.

**Clean-build verification (Windows/x64, 2026-10-03).** `out/` and every scratch
directory were deleted and the whole chain rebuilt: `cmake --fresh`, default
preset CTest 127/127, `crt-gfx-simple-dist` (01-c..03 packaged and verified, ~10
min), an isolated 04 built with the 03 SDK's *own* `crt-stage-build.py` and recipe
and no cache (4556 s, 17/17 stage tests, verified), then the isolated 05-ui built
with the 04 SDK's *own* `crt-stage-build.py` and its embedded `05-ui.json`:
ctest 8/8, sample and packaged demo `presented=30 pixel_check=pass
input_check=pass`, `verify_dist.py` and publish pass in 132 s, from a plain path
and from work/output paths containing a space (132 s). An isolated 04 SDK copies
`tools/` from its 03 predecessor rather than from its own asset, so it only
accepts the 05-ui recipe when the 03 SDK was packaged from this tree -- true here
(`crt_stage_recipe.py` is byte-identical in the 03 and 04 SDKs and the repo),
which resolves the earlier finding that an older 04 SDK rejects the recipe. The
in-tree `crt-ui-dist` (LVGL on, Skia off) verifies 01..05, and its 05-ui package
has `crtui/skia*.h` without the companion libraries, which `verify_dist.py` must
and now does accept (`tools/test_verify_dist.py` covers both packages on a fake
tree, and reverting to the first draft's check makes that test fail). `crt-ui-dist`
needs `crtui-lvgl-fetch` and `-DCRTUI_ENABLE_LVGL=ON`: the sample it must package
is built only then.

The clean build found two real defects that the previous `out/` had hidden, both
fixed: libc++'s sparse checkout fails on Windows with "Filename too long" when
neither Windows long paths nor git `core.longpaths` is enabled
(`tools/crt-libcxx-build.py` now passes `core.longpaths=true` to its own git
commands on Windows), and `capture_mf.c` could compile before the mingw-w64
headers existed because `crtmedia_backend_objects` did not depend on
`crtgfx-mingw-w64-headers-fetch` (`libcrtmedia/CMakeLists.txt` now orders them).

**Other hosts.** The stage project's macOS and Linux branches were written from
the 04 stage's link interfaces and first ran in the replays below, each of which
needed one small builder fix and nothing in the stage CMake.

**macOS/arm64 replay (2026-10-03).** The stage project's macOS branches ran
for the first time and needed one fix; nothing else changed.

- **Finding (fixed).** `tools/build_stage_05_ui.py` aborted before configuring:
  `llvm-nm/llvm-readobj were not found next to CRT_CC or on PATH`. On macOS
  `CRT_CC` is Apple's `/usr/bin/clang` shim and the LLVM binutils live in the
  Xcode toolchain, the same cause as the privacy-tool failure in the Tranche 3
  replay. `llvm_bin_directory()` now falls back to `xcrun --find llvm-nm`.
  Because the builder runs from the source asset, the fix required regenerating
  the asset, the embedded recipe chain and the 03/04 packages; the chain below
  was rebuilt after it.
- **Clean isolated chain on this host.** `crt-gfx-simple-dist` (01-c..03 packaged
  and verified, 2.5 min) regenerated the stage assets and recipes; the isolated
  `04-gfx-media` was built with the 03 SDK's *own* `crt-stage-build.py` and its
  recipe (615 s: FreeType/FFmpeg, Skia, curl chain; 17/17 stage tests; verified);
  the isolated `05-ui` was then built with the 04 SDK's *own*
  `crt-stage-build.py` and its embedded `05-ui.json`, in 48.8 s: LVGL fetched and
  SHA-256 verified, ctest 8/8 (contract, layout, surface, input, render,
  privacy, surface compositor, media view), the installed `examples/ui-basic`
  rebuilt externally from the SDK and the prebuilt `examples/bin/crtui_window_demo`
  both `presented=30 pixel_check=pass input_check=pass`, `verify_dist.py` and the
  atomic publish pass. A run from a work/output path containing spaces
  (`/tmp/t7 sp/work dir/...`, 49.3 s) passes identically.
- **Shared-library binding holds in the stage.** The isolated `libcrtui.dylib`
  binds `_pthread_mutex_init`/`_malloc` to `libc` (not `libSystem`), id
  `@rpath/libcrtui.dylib`, loads `@rpath` CRT libraries plus `libSystem` only,
  exports no non-`crtui_*` symbol, and the externally built sample (which links
  that dylib) exits cleanly -- the failure mode of the Tranche 3 replay does not
  recur. The packaged demo carries `@loader_path/../../lib`. The package has the
  LVGL notice and recipe (`share/licenses/lvgl`, `share/crt/dependencies/lvgl`)
  and no LVGL header or link artifact.
- In-tree after the fix: full `ctest` 162/162 (expected camera skip), the
  `crtui_*`-excluding preset 110/110, tooling 94/94 after the relocation fix, `crt-ui-dist` verified.
- **Inherited finding, fixed.** The isolated 04 layer installs its ports with
  `--install-prefix <temporary staged dir>`, which autotools/pkg-config bake into
  text files: `lib/pkgconfig/*.pc` (8 files: zlib, freetype2, libcurl,
  libav*/libsw*), `lib/libcurl.la`, `lib/libfreetype.la` and `bin/curl-config`.
  After the move to the output directory they named a path that no longer exists.
  Binaries had been made relocatable (Mach-O/ELF RPATHs) but text files had no
  such step, and `verify_dist.py` only inspected binaries, although
  `docs/guides/distribution.md` criterion 8 forbids build paths in installed files.
  Fix: `tools/crt_text_relocate.py` (run by `build_stage_04_gfx_media.py` before
  the distribution gate; shipped in the SDK tools and the stage source asset)
  drops `*.la`, rewrites each `.pc` to `prefix=${pcfiledir}/../..` with other
  occurrences as `${prefix}`, and makes `bin/*-config` compute its prefix from its
  own location, handling single-/double-quoted occurrences; anything else that
  still contains the staging path is reported, not guessed. New
  `verify_dist.validate_text_paths()` rejects any `.la`, any absolute `.pc`
  variable or non-system `-I/-L`, and any absolute `prefix=` in `bin/` scripts.
  `tools/test_text_relocate.py` (5 tests) covers an unrelocated tree failing,
  relocation, the real `curl-config` run after the tree is moved, an unknown file
  type being reported, and a symlinked spelling of the root.
  Evidence on the clean chain (03 -> isolated 04 in 607 s -> isolated 05-ui in
  47 s, stage 8/8, sample and demo `presented=30 pixel_check=pass
  input_check=pass`, verify and publish pass): no `.la` remains, no file in the
  05-ui SDK names the work/staging path (the manifest's `source_recipe` is the
  caller-supplied recipe location by design), `pkg-config --cflags --libs freetype2
  libcurl` resolves to the SDK, and a copy of the SDK moved to a path with a space
  gives `curl-config --prefix` and `pkg-config` results under the new location.
  Windows and Linux are unverified (tracked in `TODO.md`).
- Not covered here: the stage does not run the macOS-only
  `crtui_contract_shared_test` (the in-tree guard); the installed sample linking
  the dylib is the stage's coverage of that failure.

**Linux/x86_64 replay (native Intel host, 2026-10-03).** The stage project's
Linux branch ran for the first time and needed no change; one builder fix.

- **Finding (fixed).** `tools/build_stage_05_ui.py` aborted before configuring:
  `llvm-nm/llvm-readobj were not found next to CRT_CC or on PATH`. Debian ships
  only `llvm-nm-21` in `/usr/bin` (unversioned ones live in `/usr/lib/llvm-21/bin`),
  the same cause as the Tranche 3 privacy-tool failure. `llvm_bin_directory()` now
  also tries `/usr/bin/llvm-nm-N` and `/usr/lib/llvm-*/bin/llvm-nm`. The fix
  changes the stage asset, so `crt-stage-05-source` was rerun.
- **Clean-ish isolated chain.** `crt-gfx-simple-dist` (01-c..03, verified, 2 min)
  regenerated the assets and recipes; the isolated `04-gfx-media` was built with
  the 03 SDK's *own* `crt-stage-build.py` and recipe from empty work/cache
  directories (1102 s; verified and published); the isolated `05-ui` was then
  built with the 04 SDK's own `crt-stage-build.py` in 23.5 s: LVGL fetched and
  SHA-256 verified, ctest 8/8 (contract, layout, surface, input, render, privacy,
  surface compositor, media view), the installed `examples/ui-basic` rebuilt
  externally from the SDK and the prebuilt `examples/bin/crtui_window_demo` both
  `presented=30 pixel_check=pass input_check=pass`, `verify_dist.py` and the
  atomic publish pass. A run from work/output paths containing spaces (22.7 s)
  passes identically.
- **Shortcut, stated.** Because of the fix above, the 04 SDK's embedded
  `05-ui.json` no longer matched the regenerated asset, so the 05-ui run used the
  freshly generated `stage-sources/05-ui.json` and asset (`--recipe`/`--asset`)
  instead of the recipe embedded in the 04 SDK; the 04 chain was not rebuilt a
  second time. The embedded-recipe path was exercised on Windows and macOS.
- **Package content.** `libcrtui.a/.so`, `libcrtui_skia.a`, `libcrtui_skia_media.a`,
  all five `crtui/*.h`, no LVGL header or file, LVGL notice and recipe under
  `share/licenses/lvgl` and `share/crt/dependencies/lvgl`. `libcrtui.so` exports
  only `crtui_*` (0 others); NEEDED libc/libm/libdl (the Skia/media companions are
  static archives); RUNPATH is `$ORIGIN:$ORIGIN` (duplicate, harmless; the in-tree
  package has a single `$ORIGIN`). Removing the LVGL notice makes `verify_dist.py`
  fail.
- **Text-file relocation verified on Linux.** No `*.la` remains in the 04 or 05
  SDK, no installed text file names the work/staging path, `pkg-config` resolves
  `freetype2`/`libcurl` into the SDK, and a copy of the SDK moved to a path with a
  space gives `curl-config --prefix`/`--libs` and `pkg-config` results under the
  new location.
- In-tree after the fix: full `ctest` 154/155 (only the sound-card test),
  tooling 94/94.
- Not covered: running the stage as a release asset downloaded from GitHub (the
  development assets are local files via `--asset`).

**Windows/x64 replay of the text-file relocation (2026-10-03).** The macOS fix that
makes installed `.pc`, `.la` and `*-config` files independent of the staging path had
never run on Windows, and its first Windows run failed. After the isolated 04 stage
had built FreeType, FFmpeg, Skia and the curl chain (4575 s), installed and run every
example, `verify_dist.py` rejected the SDK: `freetype2.pc`, `libav*.pc`,
`libcurl.pc`, `libswresample.pc`, `libswscale.pc` and `bin/curl-config` still named the
staging directory. On Windows the CRT shell (mksh) that runs configure passes the
prefix as `/c/crtw/...`, so that is the spelling baked into the files, while
`tools/crt_text_relocate.py` only knew `C:\...` and `C:/...`; this could not show on
macOS or Linux.

- **Fix.** `root_spellings()` now also produces the MSYS drive spelling (`/c/...` and
  `/C/...`). `tools/test_text_relocate.py` gained two tests (the spelling list, and a
  tree baked with `/c/...` being relocated and passing `validate_text_paths()`); with
  that branch removed both fail. Tooling is 7 tests there, one skipped on Windows
  (it needs a POSIX `sh`).
- **Clean chain after the fix.** `crt-gfx-simple-dist` regenerated the 01-c..03 SDKs
  and assets; the isolated `04-gfx-media` was built with the 03 SDK's own
  `crt-stage-build.py` and recipe with `--reuse-work-root` (so its dependency layers
  are now cached; 4560 s, 17/17 stage tests, relocation 11.7 s, verify and publish
  pass); the isolated `05-ui` was built with the 04 SDK's own tool and embedded
  `05-ui.json` (165.7 s: ctest 8/8, the rebuilt sample and the packaged
  `crtui_window_demo` both `presented=30 pixel_check=pass input_check=pass`, verify and
  publish pass).
- **Evidence on the published SDKs.** No `.la` remains; the 8 `.pc` files start with
  `prefix=${pcfiledir}/../..`; no `.pc` or `bin/curl-config` names the staging path;
  and a copy of `bin/curl-config` moved to `C:/crtw sp reloc/final location/sdk/bin`
  and run by the SDK's own mksh reports `--prefix` as that new location and its
  `--static-libs` under it. (The shell needs the CRT's `/system/bin` on its PATH.)
- Not covered: Windows ARM64, and `pkg-config` itself (the SDK ships none; the `.pc`
  files are checked textually and through `curl-config`).

## Host order

Windows/x64 first (interactive iteration), macOS/arm64 second (input/event
semantics), Linux last (product/embedded acceptance).
