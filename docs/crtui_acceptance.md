# crtui Acceptance (Stage `05-ui`)

**Status: planned, contract not yet frozen.** This document records the
intended shape and tranche order so Tranche 0 can freeze it. Nothing here is
implemented; update the tranche sections with evidence as each closes, in the
same way `crtmedia_encode_capture_acceptance.md` and
`crtmedia_networking_acceptance.md` do. Completed work is recorded in
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

## Frozen common contract (to be frozen in Tranche 0)

The public API exposes no LVGL type. Opaque handles: `crtui_context`,
`crtui_window`, `crtui_widget`. Constructors follow the media/gfx idiom
(`crtui_window_create`, `crtui_container_create`, `crtui_text_create`,
`crtui_button_create`, `crtui_image_create`, `crtui_slider_create`, ...).
Tranche 0 must pin, in prose and in a resource-free test:

| Topic | Rule to freeze |
| --- | --- |
| Thread ownership | UI tree mutation happens only on the UI/event thread; calls from other threads are rejected or marshalled by an explicit, documented entry point |
| Lifetime | parent/child ownership, when a widget handle becomes invalid, release-while-event-dispatching |
| Events | dispatch order, re-entrancy, callback ownership |
| Focus and input routing | who receives keyboard/pointer input, focus loss/recovery, hit-testing, modal behavior |
| Geometry | layout ownership, resize, visibility, clipping, DPI |
| Surfaces | external-surface attach/detach, damage, z-order, ownership of the producer frame |
| Errors | reuse `crtmedia_result`-style codes; no silent failure |

STB/HMI use means remote-style keyboard navigation (Tab/arrow/Enter/Space) is a
first-class input mode from Tranche 2, not a later add-on.

## Tranches and gates

### 0. Contract freeze and stage creation

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

### 1. LVGL import and first pixels (Windows/x64 first)

LVGL software draw buffer -> a CRT display adapter -> the existing
`crtgfx`/window present path. No LVGL GPU backend. Acceptance: a
`Window > Column > Label/Button/Slider/Progress` demo produces expected pixels
and shuts down cleanly (no leaked handles/threads).

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
