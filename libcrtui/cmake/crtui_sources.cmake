# Pure source-list data shared by the in-tree libcrtui/CMakeLists.txt and the
# isolated 05-ui stage project (distribution/stages/05-ui/CMakeLists.txt), so
# the two cannot drift: the stage keeps its own copy of everything else, and a
# list that lives in two places is how earlier stages lost sources silently.
# Every path is relative to libcrtui/. No target, option or variable other
# than these lists is defined here.

# Always compiled: the CRT-owned model, layout, input and the crtgfx adapter.
set(CRTUI_CORE_SOURCES
  src/core.c
  src/crtgfx_adapter.c
)

# Compiled only with the private LVGL renderer (CRTUI_ENABLE_LVGL). LVGL's own
# sources are globbed from the fetched tree by the including project.
set(CRTUI_LVGL_BACKEND_SOURCES
  src/lvgl_backend.c
)

# Optional C++ companions that need Skia and crtgfx_skia (crtui/skia.h) and,
# for MediaView, crtmedia (crtui/skia_media.h). Never part of core libcrtui.
set(CRTUI_SKIA_SOURCES
  src/skia_compositor.cc
)
set(CRTUI_SKIA_MEDIA_SOURCES
  src/skia_media_provider.cc
)
