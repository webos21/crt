# WebKit CRT Port: Upstream Mapping

**Status: placeholder for Web Tranche 0.** This file will map upstream WebKit
and WPE components to their `PlatformCRT`/CRT counterparts, and record every
place the CRT port deliberately differs from upstream and why. Acceptance and
tranche order are in [`crtweb_acceptance.md`](crtweb_acceptance.md). Until
Tranche 0 pins a WebKit revision nothing below is a commitment.

## Mapping table (to be filled from the pinned revision)

| Upstream area | WPE reference | CRT target | Decision / notes |
| --- | --- | --- | --- |
| Platform windowing/input | WPEPlatform | `crtui` external surface + CRT input | |
| Graphics output | WPE buffer/Skia compositor | `crtgfx` external surface (SHM first, then GPU buffer) | |
| Process launch / IPC | GLib-based launcher | CRT PAL process/pipe/shared-memory | |
| Network | libsoup | CRT networking (later substitution, Tranche 8) | keep upstream first |
| Media | GStreamer | `crtmedia` (later substitution, Tranche 8) | keep upstream first |
| Fonts / text | fontconfig/FreeType | CRT-controlled FreeType/font layer | |
| JavaScriptCore | JSCOnly/WPE build | CRT libc/libc++ build, JIT-off then on | |
| Event loop | GLib main loop | to be decided | |

## Rules

- Upstream source is not patched to hide a CRT/PAL deficiency; a missing
  Bionic-compatible surface is fixed in CRT (`../AGENTS.md`).
- Each deviation from upstream is a row here with a removal condition, the same
  way port-specific workarounds are documented for `porting/recipes/`.
- No WebKit/WPE/GLib type crosses the `libcrtweb` public API.
