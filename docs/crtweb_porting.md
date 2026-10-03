# WebKit CRT Port: Upstream Mapping

**Status: Web Tranche 0 closed (2026-10-03); the table is revised as later tranches read the source.** This file maps upstream WebKit and WPE
components to their `PlatformCRT`/CRT counterparts, and records every place the
CRT port deliberately differs from upstream and why. Acceptance and tranche order
are in [`crtweb_acceptance.md`](crtweb_acceptance.md); the pin and its provenance
are in [`../libcrtweb/third_party/webkit/README.md`](../libcrtweb/third_party/webkit/README.md).

The reference is **WPE WebKit 2.54.0**. What is below comes from its release
notes and announcement plus an inventory of the verified archive (file names and
license files; the source was not built or read in depth yet). A row marked
*verify* is a hypothesis to confirm against the extracted source in the tranche
named, not a commitment.

## Mapping table

| Upstream area | WPE 2.54 reference | CRT target | Decision / notes |
| --- | --- | --- | --- |
| Embedding API | WPEPlatform: stable, on by default; WebKit selects a platform itself, with no separate backend library (WPEBackend-fdo is not needed). The libwpe-based API is deprecated | `PlatformCRT` as a WPEPlatform platform implementation | **Decided:** target WPEPlatform only, build with `ENABLE_WPE_LEGACY_API=OFF`. *Verify* in Tranche 3 that a platform can live outside the WebKit tree as `wpe-android` does (the release notes describe it as a platform outside the tree) |
| Platform windowing / input | WPEPlatform views and events | `crtui` external surface plus CRT input | The WebView is a `crtui` external-surface producer (accepted in `05-ui`) |
| Graphics output | Skia compositor in the web process (deferred display lists, damage-aware); Cairo removed; threaded tile painting mandatory | `crtgfx` external surface: shared memory first, then a GPU buffer | Upstream 2.54 notes DMA-BUF buffers selected by `WPE_BUFFER_FORMAT`. Sharing one Skia context with CRT is not an initial goal. **Confirmed:** the archive bundles its own Skia, milestone **154**, and ANGLE under `Source/ThirdParty`; CRT's Skia is m148. They stay separate copies (the web process owns WebKit's); *verify* in Tranche 3/9 that no symbol of either leaks into the other's process |
| Process launch | `WPEProcessManager` and `WPEProcessLaunchOptions` let the embedder control how auxiliary processes are launched and terminated | CRT PAL process, pipe and shared-memory primitives | Key hook for Tranche 5 and for Windows (Tranche 6); *verify* the exact surface when extracted |
| IPC | WebKit's own IPC between UI, web, network and GPU processes | CRT PAL handles and shared memory | Substrate unchanged; the CRT work is the platform primitives (Tranche 5/6) |
| Build | CMake with Ninja only (the Makefile generator is unsupported) | CRT sysroot toolchain driven by Ninja | Matches the CRT default generator |
| Network | libsoup | CRT networking | Keep upstream first; substitute in Tranche 8 |
| Media | GStreamer | `crtmedia` | Keep upstream first; substitute in Tranche 8 |
| Fonts / text | fontconfig / FreeType | CRT-controlled FreeType/font layer | *Verify* dependency set at extraction |
| JavaScriptCore | JSCOnly / WPE build | CRT libc/libc++ build, JIT off first, then on | Tranche 1; surfaces threads, TLS, executable memory and signal gaps in the lower runtime |
| Event loop | GLib main loop | not decided | Open question; depends on how deeply GLib stays in the port |

## Rules

- Upstream source is not patched to hide a CRT/PAL deficiency; a missing
  Bionic-compatible surface is fixed in CRT (`../AGENTS.md`).
- Each deviation from upstream is a row here with a removal condition, the same
  way port-specific workarounds are documented for `porting/recipes/`.
- No WebKit/WPE/GLib type crosses the `libcrtweb` public API.
- A pin bump (including security updates) changes `libcrtweb/third_party/webkit/recipe.json`
  and re-runs the stage gates; see Tranche 0 in `crtweb_acceptance.md`.
