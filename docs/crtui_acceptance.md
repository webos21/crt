# crtui Acceptance (Stage `05-ui`)

**Status: contract frozen (Tranche 0, all three hosts); LVGL first pixels
(Tranche 1) done on Windows/x64 2026-09-30.** The public contract in `libcrtui/include/crtui/ui.h` and the
rules in the table below are frozen and covered by a resource-free test.
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
| Events | Input enters through `crtui_context_send_input()`. The event goes to the target's callback, then bubbles to each ancestor up to the window; a callback returning `CRTUI_EVENT_HANDLED` stops the bubble; `event->target` stays the original target. `FOCUS_IN`/`FOCUS_OUT`/`RESIZED` do not bubble. Default actions (Tab/Shift+Tab focus, Enter/Space activate a button, arrows step a slider) run only if `KEY_DOWN` was not handled. Programmatic changes emit no event. Input sent from inside a callback is queued and delivered after the current event finishes (FIFO), never nested; no callback fires for a destroyed widget |
| Focus and input routing | One focus per context. Pointer/wheel goes to the topmost visible widget under the point (later siblings on top, children clipped to every ancestor); a press on a focusable widget focuses it, and releasing over the same button activates it. Key input goes to the focused widget of the addressed window, else the window. Traversal is tree pre-order over widgets that are focusable, enabled and visible (all ancestors too), Tab wraps. When the focused widget is destroyed, hidden or disabled, focus moves to the next eligible widget after it (wrapping), else clears; `FOCUS_OUT` is never sent to a destroyed widget. A disabled widget (or one under a disabled ancestor) receives nothing: the input is dropped, not passed through. A modal window (the most recently made modal wins) drops all input for every other window and refuses focus into them; making a window modal moves focus into it |
| Geometry | Bounds are integer logical pixels relative to the parent. A window's size and DPI scale (physical = logical x scale, scale > 0) come from `crtui_window_set_size()`, which emits `RESIZED` (x,y = size, value = scale in percent) to the window. Hit-testing uses the clipped, visible area. Automatic layout arrives in Tranche 4; until then bounds are application-set and authoritative |
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

**Windows/x64 done 2026-09-30.** (details below)

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

### 2. Input, focus, and resize

Map CRT keyboard/pointer/wheel (and touch where the host has it) to LVGL's
input model. Tests: pointer hit-test, Tab focus traversal, Enter/Space
activation, arrow navigation, slider keyboard adjustment, resize/re-layout,
focus loss and recovery.

### 3. `crtui` wrapper (first green)

The sample application calls no `lv_*` symbol. LVGL becomes a private
dependency: not in installed headers, not in the public link line of the API.

### 4. Layout, styling, and the v1 widget set

Window/Screen, Container, Row, Column, Stack, Text, Button, Image, Slider,
Progress, Switch, Checkbox, List/ScrollView, TextInput. A CRT-neutral property
set only: size, margin, padding, alignment, background, foreground, font,
border, opacity. Do not re-export LVGL's style API.

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
