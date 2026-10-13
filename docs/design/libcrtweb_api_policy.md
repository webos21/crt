# libcrtweb API Policy

The public boundary of `libcrtweb` (Web Tranche 4): what an application sees of the
`PlatformCRT` WebKit port, and what it must never see. The header is
[`libcrtweb/include/crtweb/crtweb.h`](../../libcrtweb/include/crtweb/crtweb.h); this document
owns the *why* and the rules, the acceptance document owns the evidence.

## Decision

`crtweb` is a **project-owned, plain-C, opaque-handle API** with two objects: a *runtime* and its
*views*. It is shaped like the other CRT libraries (`crtmedia_*`, `crtui_*`): `struct_size` first
in every struct, result codes, UTF-8 strings, no allocation exposed. WebKit's own C API
(`WKView`, `WKPage`, ...) is the implementation language, not the contract: it is a
PlayStation-shaped, partly deprecated surface that changes with WebKit and needs CRT extensions
(`WKViewCRT*`) to be usable at all. Neither WebKit, WPE, GLib nor JavaScriptCore types appear in a
public header, an exported symbol or a dependency of the SDK's consumer-facing link line.

## Model

- **Runtime.** Several per process in v1, each with its own website data store and process pool.
  All belong to the first runtime's creating thread and share the web event loop. The engine can
  share a Network process across stores; destroying one runtime preserves another's live launches.
  Web processes start lazily with the first view.
- **View.** One page: navigation history, size, focus, painted picture. Any number per runtime.
- **Pumping.** WebKit's work happens on the thread that created the runtime and only while the
  application calls `crtweb_runtime_pump()`. Every callback is delivered from inside a pump, on that
  thread: never from another thread, never re-entrantly from another `crtweb_*` call. There is no
  hidden thread an application must synchronise with, and nothing happens in an application that does
  not pump (it is the application's loop that owns time: `crtui`'s frame loop, a test, a CLI).
- **Pixels.** v1 delivers CPU pixels: `crtweb_view_copy_frame()` copies the newest complete picture
  as premultiplied BGRA (the AR24 layout of the web surface protocol). A GPU path (Tranche 9) adds
  a new function; it does not change this one.

## Rules

1. **Thread.** Every call except `crtweb_api_version`/`crtweb_result_string` must come from the
   runtime's creating thread; others return `CRTWEB_ERROR_STATE` and do nothing.
2. **Callbacks.** Delivered by `crtweb_runtime_pump`. An application may call any view function
   from a callback, including `crtweb_view_destroy` (the memory is retired when the pump ends).
   After `crtweb_view_destroy` returns no callback for that view is delivered.
3. **Ownership.** All strings passed in are copied. Strings passed to callbacks are valid for the
   duration of the callback only. A view must not outlive its runtime; `crtweb_runtime_destroy`
   destroys the remaining views first.
4. **Errors.** Functions return `crtweb_result` (negative = error, `CRTWEB_WOULD_BLOCK` = nothing
   yet, not a failure). A failed *page load* is not an error of the call that started it: it is a
   `CRTWEB_LOAD_FAILED` event.
5. **Versioning.** `CRTWEB_API_VERSION` changes only with an incompatible change. Structs grow by
   appending; a function reads only the fields inside the caller's `struct_size`.
6. **Navigation policy.** Only `http:`, `https:`, `file:` and `about:` URLs are accepted by
   `crtweb_view_load_url`. `on_navigation_request` may veto any main-frame navigation, including
   those the page starts itself. Sub-frame navigations are not reported in v1.

7. **Paths.** On Windows a path or `file:` URL may be spelled with a host drive (`C:\dir`, `file:///C:/dir`). The
   runtime hands the engine the CRT's POSIX spelling (`/c/dir`) for `file:` URLs and for `data_directory`, because
   libcurl and SQLite inside the engine take only `/`-rooted absolute paths. Other path arguments (a download
   destination) are passed through as given.

8. **Termination and recovery.** `on_process_terminated` reports a WebProcess death once. The view
   handle and navigation history survive; the application chooses `reload`, `load_url` or
   `load_html`, including from the callback. Recovery creates a replacement WebProcess and paints
   through the existing frame/input contract. Frame copy returns `CRTWEB_WOULD_BLOCK` after death
   until a fresh picture is available; a separate-process WebView may keep its last composed image.
   Reloading an inline `load_html` document's `about:blank` URL does not recreate its original HTML;
   the application resubmits that HTML with `load_html`.
9. **Shutdown.** View destruction detaches callbacks, cancels pending dialogs/downloads and closes
   the page, including pending navigation/launch work. Runtime destruction tracks all owned launch
   tokens, including cached/prewarmed WebProcesses, gives cooperative exit two seconds, then kills
   unresponsive owned children and waits up to two more seconds for their reaper. Tokens identify a
   launch rather than a recyclable OS PID. A shared NetworkProcess belongs to the surviving
   runtime. `crtui` WebView shutdown also closes its owned surface-protocol socket and reaps its host.

## Scope of v1, and deliberate limits

Part of v1 (Tranche 4, milestones C4-C8; the acceptance document owns the evidence): several runtimes
per process, each with its own website data store; cookie and website-data control (a data directory,
ephemeral or persistent; accept policy; clearing); downloads (the application decides the
destination and sees progress and the result); popups (`window.open`, `window.close`) and
JavaScript dialogs (alert, confirm, prompt) answered by the application; and IME (composition and
commit of arbitrary UTF-8).

Not in v1, recorded so they are decisions, not omissions: device scale other than 100 %;
authentication challenges, permission prompts (geolocation, camera, notifications), file pickers and
context menus; a JavaScript message channel (only `crtweb_view_evaluate_script`); accessibility.
Each is a later addition to this header, not a reason to expose a WebKit object.

## The crtui WebView, and why it is a separate process

`crtweb` is toolkit-neutral; the `crtui` WebView is `crtweb::WebView`
(`libcrtweb/include/crtweb/webview.h`, header-only, in the style of `crtui/skia_media.h`). It does
not call `crtweb` in-process. It launches `crtweb_host` (`libcrtweb/tools/crtweb_host.cpp`, built from
the public `crtweb` API alone), talks to it over the web surface protocol, and exposes what `crtui`
composes: a Skia image behind a `crtui_surface_view` through `crtui_skia_surface_provider`, input
forwarding, resize and focus, plus page control (`load_url`, `load_html`, `go_back`, `reload`,
`evaluate`, load/title/URL/history events). `libcrtui` itself keeps no WebKit dependency, and an
application that shows a web page links no WebKit library.

The reason is a measured constraint, not taste. A `crtui`/`crtgfx` application is linked against the
static CRT archives (`libc.a`, `libc++.a`, Skia, FreeType) with `--exclude-libs,ALL`, because a shared
CRT `libc.so` interposes the host's glibc symbols inside `libvulkan.so`/`libwayland-client.so`
(the SDK examples document the collision). `libWebKit.so` needs the opposite: shared CRT libc and
libc++ (several libraries and three processes must agree on one allocator) and its own bundled Skia,
which is a different revision from the SDK's. One address space cannot hold both; two C runtimes and
two Skias interpose each other's symbols. The protocol boundary is already a process boundary, so the
WebView uses it. Applications that do not link the static window/GPU stack (a command-line tool, a
test, `crtweb_host` itself) can use `crtweb` directly.

Consequences of the protocol path, kept visible: `on_navigation_request` (a synchronous veto) exists
only in-process; the control channel is request/event, and the host waits for the application only
where the engine's own callback is synchronous (a download's destination, 60 s). Popups are reported,
not created: the host shows one view, and the application opens another `WebView` (its own host and
runtime) if it wants the window. Text titles are truncated to the frozen 95 bytes of `TITLE`. A larger
argument than 1 MiB is refused by the client.

## Relationship to the 3A wire protocol

The web surface protocol (`libcrtweb/platform/crtweb_surface_wire.h`) remains the contract for a
*separate-process* producer. `crtweb` is the in-process API; it uses the same pixel layout and the
same input vocabulary (keysym + evdev keycode, modifier bits, logical pixels) so an application can
switch between them without changing its input mapping.
