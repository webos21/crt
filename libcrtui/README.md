# libcrtui

The CRT-owned application UI API (stage `05-ui`). Contract, tranche order and
evidence: [`docs/crtui_acceptance.md`](../docs/crtui_acceptance.md).

- `include/crtui/ui.h` -- the frozen public contract (no LVGL type).
- `src/core.c` -- headless model of the widget tree, event routing, focus and
  geometry that later tranches render through a private LVGL backend.
- `tests/contract_test.c` -- the resource-free acceptance test
  (`crtui_contract_test`).
- `third_party/lvgl/` -- the pinned LVGL release (provenance only; imported in
  Tranche 1).
