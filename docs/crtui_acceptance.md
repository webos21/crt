# crtui Acceptance (Stage `05-ui`)

**Status: contract frozen (Tranche 0, all three hosts); LVGL first pixels
(Tranche 1), input/focus/resize (Tranche 2) and the private-LVGL wrapper
(Tranche 3) closed on all three hosts; layout, styling and the v1 widget set
(Tranche 4) accepted on Windows/x64, with macOS/arm64 and Linux replay open.**
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
| Surfaces | `crtui_surface_view_create()` reserves the API and returns `CRTUI_ERROR_UNSUPPORTED` until Tranche 5. Frozen ownership rule: the view owns only position, size, clip, opacity, visibility, z-order, damage and hit-testing, never the producer's frame |
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
  tree's `dist/04-gfx-media`. Logged in `TODO.md`.
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
are complete; macOS/arm64 and Linux replay remain before this tranche closes.

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

### 5. External Surface (the WebKit prerequisite)

`crtui_surface_view` displays a producer-owned `crtgfx` external surface and
owns only position, size, clip, opacity, visibility, z-order, damage, and
input hit-testing. It does not know the producer. This is what lets a later
WebView avoid being copied into an LVGL CPU framebuffer.

### 6. MediaView

Connect the accepted zero-copy path (`crtmedia` GPU frame -> Skia image ->
`crtgfx` surface) to a `VideoView` widget without passing pixels through an
LVGL image buffer. UI and video are composed in the final present, not merged
earlier.

### 7. Cross-host and isolated-package closure

Windows/x64, then macOS/arm64, then Linux. Stage acceptance builds `05-ui` in
isolation from the installed `04-gfx-media` SDK (no repository headers), with
`tools/create_stage_source.py` and `tools/crt_dist_prerequisites.py`
registries updated in the same change -- both were silently stale in earlier
tranches; do not repeat that. Include an installed sample, `verify_dist.py`,
and the per-OS dependency/RPATH checks.

## Host order

Windows/x64 first (interactive iteration), macOS/arm64 second (input/event
semantics), Linux last (product/embedded acceptance).
