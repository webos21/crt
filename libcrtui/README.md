# libcrtui

The CRT-owned application UI API (stage `05-ui`, accepted on Linux, Windows and
macOS). Contract, tranche order and evidence:
[`docs/crtui_acceptance.md`](../docs/crtui_acceptance.md).

## Layout

- `include/crtui/ui.h` -- the frozen public contract (no LVGL type);
  `api.h` -- export control (`CRTUI_API`); `crtgfx.h` -- the `crtgfx` input
  adapter; `skia.h` / `skia_media.h` -- the optional final-compositor and
  MediaView companions.
- `src/core.c` -- the CRT model: widget tree, layout, events, focus, hit-testing,
  scroll, and the external-surface scene snapshot. Headless; no LVGL, GPU or
  decoder.
- `src/lvgl_backend.c`, `src/lvgl_conf/lv_conf.h`, `src/render_tree.h` -- the
  private LVGL software renderer (built with `-DCRTUI_ENABLE_LVGL=ON`).
- `src/crtgfx_adapter.c` -- `crtgfx_event` to `crtui` input.
- `src/skia_compositor.cc`, `src/skia_media_provider.cc` -- the optional C++
  companions (`libcrtui_skia.a`, `libcrtui_skia_media.a`); they need Skia, and
  the MediaView one also `crtmedia`.
- `cmake/crtui_sources.cmake` -- the one source list shared by this directory's
  `CMakeLists.txt` and the isolated stage project
  (`distribution/stages/05-ui/CMakeLists.txt`).
- `third_party/lvgl/` -- the pinned LVGL release (provenance; fetched at build
  time by `tools/fetch_lvgl.py`, never vendored).

## Tests and tools

- `tests/contract_test.c`, `layout_test.c`, `surface_test.c`, `input_test.c` --
  headless, LVGL-free (`crtui_contract_test`, `crtui_layout_test`,
  `crtui_surface_test`, `crtui_input_test`).
- `tests/render_test.c` -- LVGL pixel acceptance (`crtui_render_test`).
- `tests/surface_compositor_test.cc`, `tests/media_view_test.cc` -- real GPU final
  composition and MediaView with a real H.264 clip; Skia/FFmpeg-enabled trees only.
- `tools/window_demo.c` -- the crtgfx present demo (`crtui_window_demo`), also the
  installed `examples/ui-basic` sample.
- `tools/check_crtui_privacy.py` (repository `tools/`) -- headers, exports and
  sample stay LVGL-free (`crtui_privacy_test`).

## Build

```sh
cmake --build --preset <preset> --target crtui-lvgl-fetch
cmake -S . -B out/<preset> -DCRTUI_ENABLE_LVGL=ON
cmake --build --preset <preset> --target crt-ui-test
cmake --build --preset <preset> --target crt-ui-dist
```

`crt-ui-dist` requires `CRTUI_ENABLE_LVGL=ON`: the sample it must package is only
built then. Without LVGL the headless model still builds and the renderer
returns `CRTUI_ERROR_UNSUPPORTED`.
