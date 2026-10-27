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
  as premultiplied BGRA (the AR24 layout of the web surface protocol). Tranche 9A's application-side
  GPU compositor still uploads those CPU pixels. A future GPU-buffer transport adds a distinct
  contract; it does not change this function or reinterpret v1 shared memory as a GPU handle.

## Rules

1. **Thread.** Every runtime/view call except `crtweb_api_version`/`crtweb_result_string` must come from the
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

## Network trust (Tranche 8A)

The product uses WebKit's Curl request scheduler over the CRT-owned OpenSSL/libcurl ports and
CRT PAL sockets, not WPE's libsoup/GLib backend. Keep browser semantics (redirects, cache, cookies,
authentication challenges, same-origin/CORS, downloads and cancellation) inside WebKit; the media
HTTP range transport is not a general browser request API. OpenSSL remains WebKit's private
certificate-verifier/WebCrypto dependency. This is not a replacement of every Curl implementation
file with a new CRT HTTP client.

`crtweb_network_configure()` takes a copied PEM bundle and an explicitly named development-only
verification opt-out, matching `crtmedia`'s trust policy without exporting its or Curl's types.
No CA is the default: HTTPS fails closed, HTTP is unaffected. Neither host root stores nor ambient
`CURL_CA_BUNDLE`/`SSL_CERT_FILE` grant trust. An invalid bundle, untrusted chain, expired certificate
or hostname mismatch fails the asynchronous page load. The insecure opt-out disables both chain
and hostname verification; it must never be the production default.

Trust is **process-wide, immutable once runtime creation initializes WebKit** (even if a later
creation step fails), including after all runtimes are destroyed. WebKit shares a NetworkProcess across isolated data stores; pretending
that a per-runtime trust option is isolated would be misleading. Configure on the future owner's
thread and externally serialize it with runtime creation. NULL resets defaults before creation.
The copied bundle is limited to 16 KiB to bound the portable child-process environment. Internal
launch variables are implementation details; applications must not mutate them. Runtime creation
clears ambient internal overrides if the configure API was not used.

The separate `crtui` WebView has its own host process and therefore its own trust configuration.
`WebView::LaunchOptions::ca_pem_file` supplies a target-side CRT path; the host reads and copies
the bundle before runtime creation, failing startup on an unreadable, empty or oversized file.
`insecure_skip_verify_for_local_development` is false by default. Direct host equivalents are
`--ca-pem-file=FILE` and `--insecure-skip-verify-for-local-development`.

## Scope of v1, and deliberate limits

Part of v1 (Tranche 4, milestones C4-C8; the acceptance document owns the evidence): several runtimes
per process, each with its own website data store; cookie and website-data control (a data directory,
ephemeral or persistent; accept policy; clearing); downloads (the application decides the
destination and sees progress and the result); popups (`window.open`, `window.close`) and
JavaScript dialogs (alert, confirm, prompt) answered by the application; and IME (composition and
commit of arbitrary UTF-8).

Video camera capture and the application-owned camera permission contract below are implemented
as the Tranche 8C extension approved 2026-10-14; host acceptance belongs to the acceptance document. Microphone capture remains
deferred; enabling video must not imply support for `audio: true`.

Not in v1, recorded so they are decisions, not omissions: device scale other than 100 %;
authentication challenges, other permission prompts (geolocation, notifications), file pickers and
context menus; a JavaScript message channel (only `crtweb_view_evaluate_script`); accessibility.
Each is a later addition to this header, not a reason to expose a WebKit object.

## Camera permissions (Tranche 8C)

Video camera capture is an explicitly approved extension of the browser target, not of WebRTC,
microphone or display capture. Use WebKit's secure-context and frame/Permissions Policy checks;
do not install a script-level `getUserMedia` substitute or treat an application grant as a bypass
of those browser checks. Requests requiring unsupported microphone/display capture must fail
honestly, never return an empty or fake audio track.

The application owns camera permission decisions through a plain-C callback/completion boundary
and the separate `crtui` WebView control channel. Callback delivery follows the existing pump/owner
thread rules. Expose a request token and trustworthy requesting/top-level origins, not WebKit
objects or host camera handles. No callback, no explicit grant, or a cancelled request means deny.
A decision resolves once; late/duplicate completions are rejected. No blocking synchronous prompt
on a capture worker and no automatic grant because playback was allowed.
The runtime owns token storage, including inert tokens of destroyed views, until
runtime destruction. Applications discard held tokens when destroying that runtime.
Completion validates ownership before reading token state; addresses are not reused
for a different request within that runtime, so a stale decision cannot grant a new
request. The retained bookkeeping contains no WebKit request/device reference after resolution.

The grant is scoped to the requesting frame/document: no implicit persistence in cookies, website data or a shared
NetworkProcess, and no reuse by another view/origin. Within that scope WebKit may reuse the grant,
as described below; `track.stop()` alone is not permission revocation. Pending requests are cancelled on document
replacement, frame removal, view/runtime destruction or process loss. The application must also be
able to revoke active camera access; revocation ends affected tracks and releases their capture
lease without disturbing another view's separately authorized work. `track.stop()` and the last
track release must also close the source. Callbacks stop after view/host destruction.

Device metadata discovery must not start capture; browser device IDs/labels retain WebKit's
permission privacy filtering. `crtmedia_capture_open/start` and frame production follow an
explicit grant. Negotiated dimensions/rate and unsupported constraints must be reported honestly;
missing, busy, denied-by-host or unplugged devices are real failures, not synthetic success. The
worker owns the CRT capture handle and transfers owned CPU frames, with finite dequeue waits and
generation checks so stop/navigation cannot publish stale frames. No FFmpeg, V4L2, AVFoundation,
Media Foundation or private Skia type may cross the public API.

Implementation order was permission lifecycle -> CRT video source/device factory -> MediaStream
video display over the CPU playback path -> physical-host replay. Synthetic/test sources must be
test-only and must never be reported as physical camera evidence. Automated default gates prove
denial and lifecycle without activating a camera; physical-camera validation is explicitly opt-in.
The public surface is `on_media_permission` / `crtweb_media_permission_complete` / `crtweb_view_revoke_media`
(and their `crtweb::WebView` forms). Within one document WebKit keeps a granted frame's access after
`track.stop()` (what getUserMedia's specification allows); only a revocation, navigation, document
replacement or view destruction resets it. Acceptance requirements live in [Tranche 8C](../acceptance/crtweb_acceptance.md#8c-capture-separate-from-playback).

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

## GPU final composition (Tranche 9A)

The optional C++17 `crtweb/skia.h` companion provides
`crtweb::compose_gpu(view, ui, window, context, target)` for an application-owned
Ganesh context and GPU `SkSurface` (offscreen or an acquired `crtgfx` swapchain).
These are the application's SDK Skia objects, never WebKit's private Skia.
The application still links no WebKit. Core `libcrtui` and the public `crtweb.h`
C ABI gain no GPU, Skia or native-handle dependency.

The helper validates context/target identity, rejects a raster target and
reports an abandoned context instead of silently falling back. A missing web
frame returns `CRTUI_WOULD_BLOCK`. It composes the existing External Surface
provider between the UI planes, then flushes/submits with a CPU completion
wait, including after a partially composed scene, and checks submission success.
On GPU submission failure it returns `CRTUI_ERROR_IO`; the caller must recover/
tear down that GPU path before source retirement, not treat failure as completion.
After successful submission the application
can poll, resize or shut down the producer without an outstanding GPU read
of the source memory from that call. UI/context-owner thread only. The caller
owns native presentation and sends `frame_presented()` only after successful
composition/presentation; the helper never acknowledges failure.

This path is explicitly **CPU shared memory -> GPU upload -> GPU composition**,
not GPU WebCore rendering, zero-copy transport or a GPUProcess implementation.
The production compositor does not read GPU pixels back; the acceptance test
does so only to compare exact output. No application/producer `SkImage`,
`GrDirectContext` or private Skia object crosses IPC. Native GPU loaders stay in
the application's existing graphics PAL, outside the shared-runtime browser
processes. The CRT-owned no-context EGL/GLES link stubs remain distinct from
those native loaders.

### GPU buffers and synchronization (Tranche 9B)

The Linux, Windows and macOS resource implementations use a **separate v2 channel**
(`platform/crtweb_gpu_wire.h`); the frozen v1 CPU memfd packets and public C
`crtweb.h` API are unchanged. `crtweb::GpuSurfaceClient` imports resources for
the application's SDK Ganesh context. `crtweb_gpu_worker` is a separate,
statically CRT-linked producer using the SDK graphics PAL; it owns an ordinary
`crtweb::WebView` and its separate browser processes. It links no WebKit library.
The browser remains CPU-rendered: browser CPU BGRA -> worker upload -> shared
GPU image -> app GPU composition. Only **worker -> application** is shared GPU
memory, with no consumer CPU upload or worker GPU readback. This is not
accelerated WebCore or upstream WebKit's GPUProcess.

The initial native resource is an optimal-tiled, dedicated Vulkan BGRA8 image.
Its opaque layout has no public CPU plane stride/modifier; v1 cannot pretend
to consume it. HELLO/CONFIG negotiate capabilities and matching device **and
driver** UUIDs. BUFFER_ADD carries fixed-width metadata and exactly five owned
SCM_RIGHTS descriptors: memory, ready/release binary semaphores, ready/release
fences. The Vulkan adapter verifies backend, format, extent, native limits,
allocation size and memory type before import. Import duplicates borrowed FDs;
the receiver closes transfer FDs on success or failure. Private Vulkan handles,
pointers and Skia ABI objects never cross IPC. This is a trusted application /
owned-worker resource seam, not validation of arbitrary hostile GPU allocations.

Native synchronization uses OPAQUE_FD **reference imports**, not snapshot
SYNC_FD or `crtgfx_gpu_fence` (the latter is a CPU pthread fence). Each epoch
waits at most two seconds for a native ready/release fence before enqueueing
its GPU semaphore wait. A timeout grants no ownership and enqueues no unsignaled
wait. The acquirer resets the consumed shared fence, so a stale signal cannot
authorize a second read/write. Ganesh acquires EXTERNAL queue ownership and
returns the image in GENERAL/EXTERNAL state before the matching native signal.
The worker waits for upload completion before releasing its browser CPU source.
Vulkan rules: [external resource overview](https://docs.vulkan.org/guide/latest/extensions/external.html),
[opaque-memory allocation requirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryAllocateInfo.html),
[reference fence handle semantics](https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalFenceHandleTypeFlagBits.html).

**D3D12 (Windows).** The same v2 channel and strict rules, with a D3D12 adapter instead of Vulkan: the descriptor
names backend D3D12 and handle kind NT_HANDLE, and BUFFER_ADD carries exactly **three** owned descriptors
(`crtgfx_shared_image_handle_count()`): the shared texture and two shared `ID3D12Fence` objects (ready,
release). A D3D12 fence is both the GPU wait (Ganesh queue wait) and the CPU wait (`SetEventOnCompletion`)
and only counts up, so an epoch is a number: the consumer waits for ready >= its read count + 1, the
producer for release >= its write count, and a stale signal cannot satisfy a later epoch (nothing is "reset").
The texture is a committed shared-heap resource with `ALLOW_RENDER_TARGET | ALLOW_SIMULTANEOUS_ACCESS`; before
ownership moves, each side records an explicit barrier from the state Ganesh reports to COMMON on its own
direct queue and then signals. NT handles cross processes as CRT descriptors (`__crt_windows_fd_adopt_handle`,
duplicated into the peer by the CRT's `SCM_RIGHTS`). Identity is the adapter LUID + vendor + device id and the
user-mode driver version + subsystem + revision; a different adapter or driver build negotiates CPU mode.
Neither process shares a D3D12 device with the other; `OpenSharedHandle` is the only crossing.

**Metal (macOS).** BGRA8 is backed by one IOSurface, wrapped independently as a
Metal texture on each process's own device. Two `MTLSharedEvent` objects carry
monotonic ready/release epochs. A bounded native CPU event wait must succeed
before a GPU wait is enqueued; after Ganesh flush/submit, a command buffer on
that same queue signals the next epoch. Neither a CPU ACK nor socket traffic
is GPU completion. Device identity combines `MTLDevice.registryID` with a
format tag; driver identity hashes `kern.osversion`. Metadata, allocation size,
row size, pixel format and native IOSurface extent must match before import.

The `MACH_CAPABILITY` handle kind carries **two** owned CRT descriptors, not
task-local Mach port numbers: repeatable metadata and a lease socket. A random,
ephemeral bootstrap capability redeems three real Mach send rights (IOSurface,
ready event, release event). No launchd agent is installed. The native adapter
uses Metal's public `MTLSharedEventHandle` coding hook with an `NSXPCCoder`
subclass; it records the opaque coding keys supplied by Metal instead of
depending on private fields or hardcoded keys. Received rights are released
after independent IOSurface/event references have been acquired. This is a
trusted owned-worker boundary, not an authenticated sandbox service.

A private C99 transport TU uses CRT descriptors/pthreads and Bionic constants;
the Objective-C++ TU alone uses the real Apple SDK. The shared bridge binds CRT
libc before frameworks re-exporting libSystem, including the isolated stage.
Exported leases retain the native resource even after its producer wrapper is
destroyed; all lease copies closing revokes the offer. **The exporting process
must remain alive until import completes.** Once imported, independent kernel
references survive closure of every transfer descriptor. Pre-import exporter
death fails closed; recovery from producer death or hung GPUs belongs to 9C.
Missing native events or bootstrap capability access returns unsupported,
never an image upload or CPU-fence substitute.
Native API references: [IOSurface Mach ports](https://developer.apple.com/documentation/iosurface/iosurfacecreatemachport(_:)),
[MTLSharedEvent](https://developer.apple.com/documentation/metal/mtlsharedevent),
[NSXPCCoder](https://developer.apple.com/documentation/foundation/nsxpccoder).

ACK is pacing only. The consumer retains its current image until a replacement,
flushes all reads, signals native release and sends RELEASE. The producer accepts
only the exact live slot/generation/serial and reuses it only after native GPU
completion. Pools have three buffers per extent, at most six live slots during
resize; old generations retire only after release. Client provider references
must be returned before polling/resize/destruction and cannot schedule later
draws. Devices and same-device contexts outlive all imported objects/Skia refs.
Each allocation has one producer/consumer pair starting at epoch one; reconnect
uses a new allocation rather than joining a previously consumed epoch stream.

Unsupported sharing negotiates CPU mode with zero GPU resources; the caller
must use the existing v1 WebView explicitly. It is never GPU acceptance.
The strict resource gate is separate from 9A. Vulkan, D3D12 and Metal have native
resource implementations; each must pass its own native replay, not CPU fallback.
### Owned GPU process lifecycle (Tranche 9C)

The product GPU-process boundary is the **CRT-owned GPU worker**, confirmed
2026-10-17. Upstream WebKit GPUProcess activation is not a prerequisite for this
boundary: `ENABLE_GPU_PROCESS` remains off, WebCore painting remains CPU, and
neither private Skia objects nor native GPU clients enter the browser process.
This is an explicit architecture decision, not a claim that upstream GPUProcess
or accelerated WebCore has been implemented.

An EOF/error closes a resource session. On a healthy application context the
last already-imported frame remains independently owned and drawable after
producer death. No old allocation or epoch joins a replacement connection:
the process owner waits/reaps the old worker and constructs a new client/pool.
The worker's CPU-browser socket closes on process death, letting the separate
browser stop through its existing EOF/IPC shutdown. The process owner must
observe/reap its worker and account for the browser descendants, not just close
an application socket. Worker cleanup shuts down the browser before native GPU
drains. A consumer EOF retires the entire session without authorizing reuse.

The worker reloads a crashed Web process, keeping the previous GPU image until
a replacement paint arrives; at most three automatic reloads are attempted,
with a ten-second replacement-paint deadline. Host/worker loss is handled by
the owning application restarting a fresh worker session, not by adopting old
GPU handles. Failure remains observable as a closed client.

`GpuSurfaceClient` checks Ganesh abandonment (which also detects native device
loss) before exposing an image, polling or acknowledging work. A lost context
offers no usable image/ACK. The owner drops all imported images and targets,
then the context, then the native device, and creates a fresh pair/session.
`shutdown(timeout_ms)` sends BYE even on a lost context, drains received transfer
descriptors without importing new resources, and clears all slots. Draining is
load-bearing for Windows's receiver-side duplicated handles. The default drain
budget is six seconds; false is a failed shutdown, not GPU completion. The
process owner still waits/reaps the child separately. Native ready/release
completion keeps its two-second per-operation wait budget.

Acceptance distinguishes controlled context abandonment plus backend-owned
device teardown/recreation from a physical GPU reset. It does not promise that
arbitrary calls into a hung OS driver can be interrupted. Native queue drains
remain driver calls. Host replay and actual measurements belong to
[Web acceptance](../acceptance/crtweb_acceptance.md#9-gpu-integration).

## WebKit support policy (Tranche 10)

Status: adopted 2026-10-21 by the owner with `G = 60` and `S = 30`. The upstream facts it rests on (only the
latest stable branch gets security fixes, no published end of life, a series' last micro precedes its successor
by 1-28 days) are recorded in
[Web acceptance](../acceptance/crtweb_acceptance.md#upstream-support-window-of-the-wpe-webkit-stable-series-checked-2026-10-20).

**Terms.** The *pinned series* is the even-minor series of `libcrtweb/third_party/webkit/recipe.json`.
It is *superseded* from the day the first release of the next even-minor series (`X.Y+2.0`) is published
upstream; the number of days since then is its *superseded age*.

**Rules.**

1. A public release must pin the newest micro of the newest stable series (enforced as `pin_is_latest_micro`).
2. While the pinned series is superseded for no more than `G = 60` days, a public release may still ship it,
   because upstream series end shortly before or at the successor's first release and a re-pin to a new series
   is a tranche-sized change (carried patches, three-host replay). Release notes must name the successor and
   the planned re-pin.
3. Beyond `G` days a public release on the superseded series is refused. Development candidates are not
   affected.
4. Re-pin to a new series starts within `S = 30` days of its first release and ends when the Linux/x86_64
   gates are green; macOS and Windows replays follow in the usual host order. A micro update inside the
   pinned series follows the existing advisory gate and needs no policy decision.
5. An advisory that affects the pin blocks release until fixed, regardless of series age (existing gate).
6. Already published releases are not patched when their series loses upstream support. The next release
   states the unsupported state in its notes; withdrawing a release is an owner decision per case.

**Check.** `tools/check_webkit_security.py` derives the successor's first-release date from the upstream release
index it already reads (a later even-minor series; odd minors are development series) and records
`series_superseded_since`, `series_superseded_age_days`, `series_grace_days` and `series_within_grace` in
`share/crt/web-security.json`. Past `G` it adds the release blocker "pinned series superseded N days ago
(limit 60): re-pin required", and `tools/crt_web_release.py` refuses a public release (and rejects a recorded
public one) unless `series_within_grace` is true; this reason is not one the owner's `accepted_blockers` can
waive. Rules 4 and 6 are procedures for the owner and are not machine-checked.

**Still open.** Whether rule 6 should promise a fixed deprecation notice period.

**Current application (2026-10-27).** The pin is 2.54.2 (released 2026-10-23).
Its fresh advisory/support-window snapshot passes; native replay and release
assembly remain separate gates in [release qualification](../acceptance/release_qualification.md).
