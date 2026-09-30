# libcrtui

The CRT-owned application UI API (stage `05-ui`). Contract, tranche order and
evidence: [`docs/crtui_acceptance.md`](../docs/crtui_acceptance.md).

- `include/crtui/ui.h` -- the frozen public contract (no LVGL type).
- `src/core.c` -- headless model of the widget tree, event routing, focus and
  geometry that later tranches render through a private LVGL backend.
- `tests/contract_test.c` -- the resource-free acceptance test
  (`crtui_contract_test`).
- `src/lvgl_backend.c`, `src/lvgl_conf/lv_conf.h`, `src/render_tree.h` -- the
  private LVGL software renderer (built with `-DCRTUI_ENABLE_LVGL=ON`).
- `tests/render_test.c` -- Tranche 1 pixel acceptance (`crtui_render_test`);
  `tools/window_demo.c` -- the crtgfx present demo (`crtui_window_demo`).
- `third_party/lvgl/` -- the pinned LVGL release (provenance; fetched at build
  time by `tools/fetch_lvgl.py`, never vendored).
