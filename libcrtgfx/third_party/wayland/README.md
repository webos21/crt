# Wayland External Build Metadata

This directory reserves the project-owned metadata/build-glue location for a
future Wayland or Weston-derived import. It is intentionally not a checkout.

The current Linux adapter is a narrow, hand-written client of the real core
Wayland and stable xdg-shell wire protocols. It does not link host
`libwayland-client`; see `docs/libcrtgfx_wayland_plan.md`.

When an upstream protocol parser, generated bindings, or compositor component
is imported, record its upstream revision, license/provenance, and local build
policy here. Keep the actual fetched source and all build outputs under
`out/<preset>/external/wayland/`, and apply the same no-upstream-patch /
CRT-first porting discipline used for Skia.

`recipe.json` in this directory (added 2026-08-22, matching
`libcrtgfx/third_party/skia/recipe.json`'s own schema) is a deliberate
placeholder: `source.repository`/`source.ref`/`source.expected_commit` are
`null` because nothing is pinned or fetched today. See its own `notes` array
for likely future candidates and what to fill in when this stops being a
placeholder.
