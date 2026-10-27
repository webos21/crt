# CRT Build Stages And Distributions

## Purpose

CRT is a Bionic-compatible cross-platform C runtime and PAL for rebuilding the
same application source as native Linux, Windows, and macOS executables. Its
primary deployment target is a Linux-kernel embedded device such as an HMI,
set-top box, game console, or IVI system; desktop builds are also the native
development and debugging environment.

CRT distributions never bundle LLVM, Clang, LLD, GCC, or a vendor compiler.
An embedded SDK normally supplies its own compiler, binutils, target triple,
and device libraries. A CRT distribution supplies the sysroot, startup
objects, runtimes, wrappers, CMake integration, and a manifest describing the
external toolchain contract.

## Cumulative Stages

| Stage | Content | Intended use |
| --- | --- | --- |
| `01-c` | libc, libm, libdl, startup objects, shell, mksh, toybox, awk, make | C applications and C libraries |
| `02-cxx` | `01-c` plus libc++, libc++abi, and libunwind | C++ applications and libraries |
| `03-gfx-simple` | `02-cxx` plus one window, keyboard/mouse input, and a CPU-writable software framebuffer | Industrial HMI and simple native UI |
| `04-gfx-media` | `03-gfx-simple` plus the GPU API, Skia CPU/GPU rendering, Vulkan/D3D12/Metal presentation, and FFmpeg media | accelerated UI and playback |
| `05-ui` | 04-gfx-media + crtui + private LVGL 9.6, CRT-owned layout/style/v1 widgets, input/focus/resize, external-surface composition and `MediaView`, the optional Skia/media companions | native application UI |
| `06-web` (source stage; scope and release checks in [Web acceptance](../acceptance/crtweb_acceptance.md#10-distribution-and-security-closure)) | `05-ui` plus JavaScriptCore/WebKit, `libcrtweb`, and the WebView | web runtime |

### CMake stage command names

Each public prefix below has `-config`, `-build`, `-test` and `-dist` targets:

| SDK stage ID | Public CMake prefix | Previous public prefix |
| --- | --- | --- |
| `01-c` | `crt-c` | unchanged |
| `02-cxx` | `crt-cxx` | `crt-libcxx` |
| `03-gfx-simple` | `crt-gfx` | `crt-gfx-simple` |
| `04-gfx-media` | `crt-media` | `crt-gfx-media` |
| `05-ui` | `crt-ui` | unchanged |
| `06-web` | `crt-web` | unchanged |

For an existing build directory, rerun `cmake --preset <preset>` to regenerate
the target graph, then use the new names. Retired public prefixes have no aliases.
The former internal `crt-gfx-build/test` and `crt-media-build/test` were partial
stage-04 aggregates; their names are now `crt-internal-media-gfx-build/test` and
`crt-internal-media-runtime-build/test`. Public `crt-gfx-*` always selects stage
03, while `crt-media-*` selects the complete stage 04.

This changes in-tree CMake target names only. SDK stage IDs, install components,
archive/directory names, source recipes, cache step IDs and the standalone
`tools/crt-libcxx-build.py` driver retain their names. Preflight reports still use
the internal stage IDs (`stage-config/libcxx.json`, `gfx-simple.json` and
`gfx-media.json`). Older acceptance logs retain the commands used at their
recorded source commits; apply this mapping when reproducing them in a newer tree.

### Development SDK producer and external validation

The reduced Development SDK is produced from plain `02-cxx` and the media
stage's shared zlib/Mbed TLS/curl build implementation. It retains the
`02-cxx` manifest stage and adds the `dev` profile sidecar; no graphics/media/UI
or Web payload is installed. The ordinary entry point builds its predecessor,
verifies the profile and runs the static/shared HTTPS+inflate reference consumer:

```sh
cmake --build --preset <preset> --target crt-development-sdk-test
```

For producer-tree-hidden acceptance, additionally run on native Linux with
bubblewrap, or native macOS with the OS-provided `sandbox-exec`. Both need
OpenSSL as a build-host test tool:

```sh
python3 tools/test_dev_sdk_external.py \
  --sdk-root out/<preset>/dist/dev --output out/dev-sdk-external
```

This copies the SDK to a fresh external directory with spaces and makes the
original SDK and producer checkout (including build outputs) inaccessible.
Linux masks them with empty mounts; macOS denies their file reads and writes
using an inherited OS sandbox policy, without renaming or changing permissions
on the checkout. A child visibility probe must pass before validating the
relocated profile and building/running both linkage modes and certificate
negatives. The macOS consumer receipt also records each executable's Mach-O
binding audit against the SDK-owned symbols, including the mixed-runtime check.
OS/compiler/Python tools remain available. Use repeated
`--hide-root <path>` arguments for producer build roots outside the checkout.
Optional `--work-root <fresh-path>` must be outside all hidden roots; the work
directory is retained and recorded in `result.json`. Missing isolation tools,
namespace/sandbox setup or consumer failures produce no success receipt; there
is no unisolated fallback. This driver rejects foreign-target execution.
On native Windows, the same driver uses a read-and-write restricted token and
an inherited job for bounded child cleanup. Set `CRT_CC`, `CRT_CXX`, `CRT_AR`,
`CRT_RANLIB` and `CRT_WINDOWS_SDK_LIBPATH` to external host tools and the matching
Windows SDK import-library directory, then run with native host Python:

```powershell
python tools/test_dev_sdk_external.py --sdk-root 'out/<preset>/dist/dev' --output out/dev-sdk-external
```

Only a new private work directory receives a per-run synthetic-SID ACL; no
existing source, SDK, tool installation or user ACL/account/policy is changed.
The build-host Python runtime is copied there too, without user site packages;
the SDK and fixture are relocated under paths with spaces. The restricted
token retains public OS/tool and logon access but not private producer-file
access, following Windows' [additional restricted-SID access check](https://learn.microsoft.com/en-us/windows/win32/secauthz/restricted-tokens).
The directory/file-denial probe and a descendant's direct-file-denial probe must
pass before profile/consumer testing. A publicly readable producer checkout may
therefore fail this gate: do not weaken the probe or silently retry unrestricted.
This is an acceptance boundary for trusted build/test tools, not a general
hostile-code sandbox. No producer handles are inherited and child processes run
hidden; shared consumers use app-local DLLs with System32-only runtime PATH.
Foreign-target execution is rejected; run Windows/aarch64 with native arm64
Python on that host, not as an x64 cross-compile claim. See the
[split matrix](../acceptance/sdk_split_acceptance.md) for actually replayed pairs.
For Windows ZIP replay, record source links and extracted object types separately:
the existing archive helper materializes internal file aliases as target bytes.
Require equal payload bytes at every path and explain each declared alias, while
checking each source/extracted/relocated inventory remains unchanged by testing.
This does not replace the real-filesystem symlink requirement for Windows builds.

These targets produce local SDK candidates. A `development` VERSION, equal
runtime hashes or an older predecessor's known source commit do not qualify a
matching clean/tagged release pair. The strict release collectors and their
independent positive rehearsal remain the release gate.

#### S5 package, negative and identity replay

After the native producer is accepted, use a fresh evidence directory and an
identified plain Full SDK of the same native target with matching inherited
runtime bytes. A plain `02-cxx` predecessor is a valid Full reference for this
producer gate; using it does not claim `06-web` release qualification.

```sh
python3 tools/test_sdk_split_s5.py \
  --sdk-root out/<preset>/dist/dev \
  --full-sdk out/<preset>/dist/02-cxx --output out/dev-sdk-s5
```

The driver verifies inputs, packages/extracts the candidate, runs the same
producer-denying consumer and requires expected rejection diagnostics for seven
Dev mutations plus a stale Full source identity. Mutations affect private
copies only. The original payloads must remain unchanged. It uses the existing
Linux/macOS/Windows native isolation prerequisites above and rejects foreign
target execution. Choose a new `--output` path for each replay; existing evidence
is not replaced.

The resulting `handoff.json` retains artifact/profile/manifest hashes, producer
identity, actual native import closure and case results for CLI review. It is
local test evidence, not an artifact promotion, CLI lock/adoption record or
release approval. See [S5 scope and remaining hosts](../acceptance/sdk_split_acceptance.md#s5-native-negative-and-candidate-handoff-gate).

### Stage production

`05-ui` is accepted: the repository builds it in-tree (`crt-ui-dist`) and as an
isolated `04-gfx-media -> 05-ui` stage on Windows/x64, macOS/arm64 and
Linux/x86_64. `tools/prepare_release_assets.py` supports `05-ui` and `06-web`,
subject to the [release checks](release_preview.md#05-ui-and-06-web); support
in the tool is not evidence of an uploaded release. The former `05-js` skeleton (`libcrtjs`, no engine)
and its `crt-js-*` targets were deleted on 2026-09-29; the QuickJS plan behind
it is retired ([`runtime_roadmap.md`](../design/runtime_roadmap.md)). `05-ui` was created
fresh by `crtui` Tranche 0 (2026-09-30) with `crt-ui-build/test/dist` targets.
The presets enable LVGL (and Skia, FFmpeg, curl and the imported libc++), and `crt-ui-build`/`crt-ui-dist` fetch what
they need first, so no separate fetch step or configure option is required
([build targets](build_targets.json), [README](../../README.md#build)).

The internal `crtgfx-wayland-*` and `crtgfx-xkbcommon-*` port targets are
registered by default for Linux targets, including Linux/x86_64 host to
Linux/aarch64 target builds. Registration follows `CRT_TARGET_OS`, not the host
OS. For standalone port experiments targeting macOS or Windows, configure with
`-DCRTGFX_ENABLE_WAYLAND_PORT_TARGETS=ON`. This does not select a Wayland window
backend on those systems. Linux cross builds still require an independently
built native scanner through `CRTGFX_WAYLAND_HOST_SCANNER`; target binaries must
not be executed as build-host generators.

Each binary stage is cumulative and independently consumable. The repository
build currently creates each later directory by copying the preceding
installed stage and overlaying the new component. That proves artifact
inheritance, but it is not by itself proof that the new component can be
rebuilt without repository headers or libraries. The separate source-stage
chain below supplies that stronger boundary.

Each stage is also a self-contained development sysroot for its declared
capabilities. If a stage exposes an external library through its public API,
link interface, or runtime dependency, the distribution must install that
library's required public headers under `include/` and its redistributable
link/runtime artifacts under `lib/` and, on Windows where appropriate,
`bin/`. This includes static archives and import libraries needed for linking
as well as the corresponding `.so`, `.dylib`, or `.dll` files needed to run.
The rule applies transitively: an application must not need an undeclared
developer package merely because CRT linked one of its stage libraries to an
external dependency.

OS-owned libraries and frameworks that are part of the documented minimum
target OS, and device-specific GPU/video drivers such as Vulkan ICDs or VA-API
drivers, remain target prerequisites rather than copied CRT payloads. Every
such exception must be explicit in `manifest.json`; source/version/license
metadata must accompany every third-party artifact that CRT does redistribute.
Package construction and acceptance must fail when a manifest-declared header,
link artifact, runtime library, or notice is absent. `verify_dist.py` also
parses every packaged Linux ELF `DT_NEEDED` entry, Windows PE normal/delay
import, and macOS Mach-O `LC_LOAD_DYLIB`-family dependency without an
external inspection tool. Each dependency leaf name (a Mach-O dependency
normalizes to its own basename, or to its `Name.framework` bundle name for a
real framework) must match a real target binary in the cumulative SDK or be
exactly declared as an `external_prerequisites` component; embedded paths,
undeclared host libraries, and a packaged macOS dylib whose own `LC_ID_DYLIB`
is not a portable `@rpath/...` spelling all fail acceptance. Packaged Mach-O
files must not retain an absolute `LC_RPATH`; an executable that loads an
`@rpath/...` dependency must instead carry a portable `@loader_path`- or
`@executable_path`-relative RPATH.

Simple Graphics intentionally excludes Skia CPU raster and text, Skia GPU,
and the public `crtgfx/gpu.h` surface. Its drawing contract is the mapped
software framebuffer in `crtgfx/window.h`. The Windows implementation may use
the native compositor internally to present that CPU buffer; this does not
make Skia or the advanced GPU API part of the Simple Graphics contract.

## Build Targets

Configure once with a host preset. The default workflow only builds and tests
the C stage:

```sh
cmake --workflow --preset <os>-host-ninja-debug
```

Build, test, and package a specific layer with:

```sh
cmake --build --preset <preset> --target crt-c-build
cmake --build --preset <preset> --target crt-c-test
cmake --build --preset <preset> --target crt-c-dist

cmake --build --preset <preset> --target crt-cxx-build
cmake --build --preset <preset> --target crt-cxx-test
cmake --build --preset <preset> --target crt-cxx-dist

cmake --build --preset <preset> --target crt-gfx-build
cmake --build --preset <preset> --target crt-gfx-test
cmake --build --preset <preset> --target crt-gfx-dist

cmake --build --preset <preset> --target crt-media-build
cmake --build --preset <preset> --target crt-media-test
cmake --build --preset <preset> --target crt-media-dist

cmake --build --preset <preset> --target crt-ui-build
cmake --build --preset <preset> --target crt-ui-test
cmake --build --preset <preset> --target crt-ui-dist
```

The more focused `crt-internal-media-gfx-build/test` and `crt-internal-media-runtime-build/test` targets are
also available. A `*-dist` target builds all preceding stages automatically.
Advanced graphics packaging reflects the configured capabilities; enable and
build the imported libc++, Skia, Vulkan/Metal/D3D12, and FFmpeg paths before
claiming those optional entries in a release manifest.

The official policy deliberately separates the fast cumulative developer lane
from release-grade upper-stage acceptance:

- ordinary presets and cumulative `*-dist` targets keep expensive optional
  Skia and FFmpeg integration disabled unless the caller explicitly enables
  it, so routine CRT/PAL work does not rebuild those dependencies;
- release acceptance for `04-gfx-media` uses the option-ON, predecessor-only
  `03-gfx-simple -> 04-gfx-media` stage entry point, which builds, tests, and
  packages the real Skia/FreeType/FFmpeg payload from the extracted SDK; and
- no second option-ON cumulative target is required. It would duplicate the
  weaker in-tree build without replacing the isolated boundary, and would make
  routine build behavior and caching harder to reason about.

Changing this policy requires evidence that the isolated lane cannot exercise
a release requirement. Build speed alone is handled by its warm dependency
cache and explicit job controls, not by adding the heavyweight path to the
default workflow.

Outputs are placed under:

```text
out/<preset>/dist/01-c/
out/<preset>/dist/02-cxx/
out/<preset>/dist/03-gfx-simple/
out/<preset>/dist/04-gfx-media/
out/<preset>/dist/05-ui/
```

Windows packages are emitted as `.zip`; Linux and macOS packages as
`.tar.xz`. Every directory contains `manifest.json`, `VERSION`, activation
scripts, wrappers, and `crt-toolchain.cmake` in addition to the cumulative
CRT and required redistributable dependency headers and libraries.

Windows build and source-stage hosts must have Developer Mode enabled. The
supported Windows contract uses real filesystem symbolic links for the
Bionic/POSIX `symlink()` surface, upstream source-archive links, and GNU-style
install aliases. Cygwin/MSYS marker or shortcut links and MSYS2's default
deep-copy behavior are deliberately not alternative backends: they are not
transparent to the native Python, CMake, LLVM, and Windows-loader processes
that consume a CRT SDK. Disabling Developer Mode therefore makes the host
unsupported for producing or validating Windows distributions.

Each directory also carries the CRT mksh/toybox shell environment, optional
Python porting drivers, recipes/tests/shims, and the examples appropriate to
that stage. Python is an orchestration convenience and is declared in the
manifest when required; configure and make commands themselves run through
the packaged CRT mksh. MSYS and Git Bash are not distribution prerequisites.

Archive names carry version, OS, architecture, and stage, for example
`crt-development-windows-x86_64-01-c.zip` and
`crt-development-linux-aarch64-04-gfx-media.tar.xz`. A release replaces the
`development` token with the project release tag: configuring with
`-DCRT_RELEASE_TAG=<tag>` sets the SDK `VERSION` files, the archive names, and
the download URLs in the packaged stage recipes together, and
`tools/prepare_release_assets.py` then collects and checks the result. See
[`release_preview.md`](release_preview.md).

## Source-stage bootstrap chain

The CRT GitHub repository is the meta-toolchain: it produces both the binary
SDK archives above and immutable source assets for the individual upper
stages. A released binary SDK does not clone the live repository or build
against an arbitrary branch. Instead, it contains a catalog of stage recipes
that records, at minimum:

- the input and output stage IDs;
- the target OS, which must match the preceding SDK;
- an immutable CRT GitHub Release asset URL;
- the source commit represented by that asset;
- the archive byte size and SHA-256 digest;
- the required preceding binary stage and external tools;
- configure/build/install/test entry points and expected installed artifacts.

All paths recorded in `redistributed_dependencies` are normalized portable
relative paths. Verification rejects absolute, drive-relative, parent-
traversing, non-normalized, or symlink-escaping entries before checking that
the declared header, link, runtime, notice, and provenance payload exists.

`external_prerequisites` is the complementary, non-payload contract. Each
entry has a stable `id`, a `kind` (`os-runtime`, `os-framework`,
`host-library`, `runtime-service`, or `device-driver`), non-empty
`required_for` capabilities and `components`, and `bundled: false`. The list
is cumulative by stage and canonical for the manifest's target OS. The
verifier rejects missing, duplicate, unexpected, malformed, or target/stage-
contradictory entries; adding or removing a prerequisite therefore requires a
reviewed change to the common producer/verifier definition rather than an
ad-hoc manifest edit.

On Linux, `02-cxx` and every later stage, including the Development profile,
declare the architecture's OS ELF loader as `linux-elf-loader` for shared C++
runtime TLS. It is not copied into the SDK. This is the same loader boundary
inherited by Web; it does not permit a host libc or C++ runtime dependency.
Older SDKs with missing or Web-only loader metadata must be regenerated with the
matching producer/verifier; do not edit an installed manifest to bypass an
import-closure failure. Manifest format 1 and recipe versions are unchanged.

| Target/stage | External runtime contract added at that stage |
| --- | --- |
| Windows `01-c` | Windows kernel/API-set runtime |
| Windows `03-gfx-simple` | USER32, D3D11, and DXGI desktop presentation runtime |
| Windows `04-gfx-media` | D3D12/DXGI/shader compiler, COM, and a compatible display driver |
| macOS `01-c` | libSystem |
| macOS `03-gfx-simple` | Foundation, AppKit, QuartzCore, CoreGraphics, CoreFoundation, and Objective-C runtime |
| macOS `04-gfx-media` | Metal, AudioToolbox, VideoToolbox, CoreVideo, and CoreMedia |
| Linux `01-c` | Linux kernel syscall ABI |
| Linux `03-gfx-simple` | reachable Wayland compositor with xdg-shell, plus the host `libwayland-client.so.0` |
| Linux `04-gfx-media` | host Vulkan loader library plus a target-GPU Vulkan ICD |

`02-cxx` adds no OS prerequisite beyond `01-c` on any host. (It used to list
`libatomic.so.1` for Linux, a `DT_NEEDED` of `libc++.so` found 2026-09-14; the libc++ build
linked the host library, which also pulled glibc's `libc.so.6` in. The recipe now sets
`LIBCXX_HAS_ATOMIC_LIB=OFF`, a fresh SDK has no such entry on Linux/x86_64 and aarch64, and the
JSC host-ABI audit would flag it again.) Linux `03-gfx-simple`'s own `libwayland-client.so.0` line
moved here from `04-gfx-media` (2026-09-15, real Linux/aarch64 binary-
dependency-gate acceptance): `libcrtgfx.so`/`crtgfx_window_demo` already
carry a genuine `DT_NEEDED` on it at this stage, not just from
`04-gfx-media` onward -- found running the cumulative dependency gate for
the first time on a native Linux host. (The Networking and streaming curl/mbedTLS/zlib
layer is part of `04-gfx-media`, redistributed as static libraries plus the
`curl` tool.)
Compiler/toolchain inputs remain in the separate external-toolchain fields
and redistributable third-party ports remain in `redistributed_dependencies`.
The runtime a stage itself redistributes is declared there too, with `kind` `crt-runtime`:
`crt-libc`, `mksh`, `toybox` and `onetrue-awk` from `01-c`, and `llvm-runtimes` from `02-cxx`
(registry `third_party/crt-runtime/components.json`, installer `tools/crt_runtime_notices.py`). Each carries
its notices under `share/licenses/<name>/` and a `provenance.json`; `verify_dist.py` requires them for the
stage and every earlier stage. These notices are an inventory, not a legal review
(`docs/acceptance/crtweb_acceptance.md`, "Inherited runtime notices").

The first supported transitions are `01-c -> 02-cxx`, then
`02-cxx -> 03-gfx-simple -> 04-gfx-media`. Thus `01-c` can fetch the pinned
libc++/libc++abi/libunwind source package and build `02-cxx`; `02-cxx` can
fetch the Simple Graphics and advanced Graphics/Media packages and build them
in dependency order. The `04-gfx-media -> 05-ui` transition follows the same model and is
implemented. `05-ui -> 06-web` has native Linux/x86_64, macOS/aarch64 and
Windows/x64 development paths plus a Linux/x86_64 -> Linux/aarch64 cross path,
`tools/build_stage_06_web.py`. It requires the option-ON predecessor (including
the shared-image headers, Skia/media archives and dependency provenance), not a
default-OFF SDK. The ordinary option-ON `crt-ui-dist` now packages that complete
development link interface too. An old `05-ui` SDK must be rebuilt, not supplemented
with headers/libraries from a build tree.

For a local Web development candidate:

```sh
python3 tools/crt-stage-build.py --sdk-root /absolute/path/to/05-ui \
  --recipe /absolute/path/to/06-web.json \
  --asset /absolute/path/to/06-web-source.tar.xz \
  --work-root /absolute/path/to/web-work \
  --output /absolute/path/to/06-web --dependency-jobs 4 --reuse-work-root
```

`--source-cache /absolute/path/to/webkit-product` optionally supplies the existing
verified upstream Git/source cache; no WebKit object or library is imported from
it. The entrypoint checks the product commit/tree/listing and patch hashes again.
The cache must contain a complete self-contained, pinned Git checkout and its
bare repository; incomplete metadata, tracked modifications, untracked or ignored
generated files fail before the dependency build. Omit `--source-cache` to fetch
the pinned source afresh. It builds host gperf and ICU generators outside the SDK, then target dependency
ports and WebKit with the predecessor's CRT wrappers. Reuse requires identical
source-asset and predecessor-content identities. For a packaging-only repair,
`--previous-source-asset` can prove the previous archive's identity and unchanged
compiled sources/configuration before reusing the heavy build. Adding explicit
dependency roots is allowed; changing/removing compiled inputs is not. The SDK
staging tree is copied afresh from the unchanged predecessor on every retry.
Binding FindCURL's pkg-config discovery alias to the same already-selected
static archive is also a permitted link-only repair. The native `make` and
`openssl` commands are build/verification tools; `openssl` creates fresh
loopback certificates and is not bundled in the SDK.
The native Windows development path has passed installed/space-path and
product-hidden consumer replay (see [Web acceptance](../acceptance/crtweb_acceptance.md#fresh-windows-full-stage-replay-after-pal-fixes));
this is not public-release authorization.
Its build-host tools additionally require Git Bash, native MinGW gcc/g++/make,
and a compatible GNU patch (the replay uses Git's 2.7.6, not Strawberry's
2.5.9, which asserts on a carried patch). Select host patch explicitly via
PATH while retaining native Windows Perl precedence, for example prepend
`C:/Strawberry/perl/bin;C:/Program Files/Git/usr/bin;` to the existing host
PATH. This does not change the target port builder's CRT-only shell PATH.
The serializer's long input list is dispatched by the CRT-owned Windows
CMake host-generator adapter through an argument file, without patching the
upstream generator or shortening the SDK path.
WebKit notices are shipped with their exact inventory-pinned SHA-256 bytes.
A Windows CRLF checkout is accepted only when CRLF-to-LF conversion matches
that pinned hash; other content differences remain fatal and the checkout
itself is never modified.
The host tools also require a POSIX-path Perl for OpenSSL's Unix CRT target;
WebKit's own generators
still use native Windows Perl. A trimmed Git Perl can use a prepared host-only
module root through `CRT_WEB_HOST_PERL_LIB`; the entrypoint probes
`IPC::Cmd`/`ExtUtils::MakeMaker` and records its files' hashes. No global module
installation is changed and neither these modules nor host compiler runtime
DLLs are shipped in the target SDK. OpenSSL's generated `configdata.pm` also
requires `Pod::Usage` and its dependencies (`Pod::Text`/`Pod::Simple`); prepare
them in that same host-only module root before a clean Web build
([native replay](../acceptance/sdk_split_acceptance.md#windowsx64-clean-production-and-s5-replay-2026-10-27)).
Git shell/Perl commands need working 8.3
aliases because their generated Make command lines do not quote tool paths;
this does not substitute a short alias for the SDK path under replay.
Host/cross-target configurations
not implemented by these native paths fail explicitly instead of running a
target generator as a host tool or selecting host libraries.

For the Linux/aarch64 cross path, the predecessor itself must be an installed
**aarch64** option-ON `05-ui` SDK, not a relabeled x86_64 SDK. Add
`--linux-system-lib-dir /absolute/path/to/aarch64-os-libraries` to the command
above when the target's OS graphics imports are not installed in the normal
target multiarch directory. That directory supplies only the existing PAL
boundaries `libvulkan.so.1`, `libwayland-client.so.0`, `libva.so.2` and
`libva-drm.so.2`: their architecture and hashes are checked, they are not
bundled, and it is not a glibc sysroot. Changing these inputs invalidates work
reuse. Host shell/make/gperf/interpreters and ICU generators stay outside the
target SDK. Configure declares both Autoconf build/host triples; WebKit binds
the host generator paths explicitly. Cross acceptance audits ELF files and
every static-archive member without running target programs. Native browser,
HTTP/TLS, GPU and device replays are separate from that compile/package proof.

When preparing that predecessor from source, use
`cmake/toolchains/linux-aarch64.cmake` with explicit `CRT_TARGET_OS=linux` and
`CRT_TARGET_ARCH=aarch64`. Supply an AArch64 compiler-rt archive through
`CRT_COMPILER_RT_BUILTINS_OVERRIDE` if the compiler installation lacks one;
never rename an x86_64 archive or use a host GCC runtime as compiler-rt.
`build_wayland.py --host-scanner` separates the pinned native 1.26.0 scanner
from the target client/protocol objects. The standalone `03-gfx-simple` CMake
project can opt into that native window backend with `CRT_STAGE_WAYLAND_PREFIX`
and an explicit target `CRTGFX_LINUX_WAYLAND_CLIENT_LIB`; its ordinary source
stage remains window-only by default. For narrowly scoped Linux VA/V4L2
translation units, `CRT_LINUX_SDK_ROOT` selects the target OS headers behind
the existing `-fcrt-real-linux-sdk` adapter, without exposing them in CRT's
public sysroot. The normal lower-stage drivers still execute their native
tests: a cross bootstrap uses their compile/install projects and does not
claim those native tests passed.

The public C library is `lib/libcrtweb.so` (Linux), `lib/libcrtweb.dylib`
(macOS), or `bin/libcrtweb.dll` with `lib/libcrtweb.dll.a` (Windows).
Browser helpers live in
`libexec/crtweb/{crtweb_host,WebProcess,NetworkProcess,crtweb_gpu_worker}`
(with `.exe` suffixes on Windows);
WebKit and its private dependencies live in `libexec/crtweb/lib/`.
The CPU browser's media closure (including VA stubs on Linux) is confined there, not placed in the
SDK's common library directory. Ordinary media applications retain their real
hardware backend. Versioned/unversioned LLVM runtime names share one library
image through relative symlinks, preserved when copying/publishing the SDK.
On macOS, versioned images such as `libWebKit.dylib.SOVERSION` and their aliases
are preserved together. New Web images have package-local install names and
ordered `@loader_path` RPATHs and are ad-hoc signed after editing; inherited
runtime images are not rewritten. WebKit's static OpenSSL curl exports are
hidden, leaving the predecessor's media curl unchanged. The native observer
holds the painted sample at an explicit checkpoint while `ps`/`vmmap` samples
the three browser roles. OS/framework shared-cache mappings are recorded
separately from CRT libraries; the strict direct Mach-O dependency audit still
applies. Only PID-scoped, unlinked, non-executable CRT IPC data receives the
existing anonymous-memory classification.
The Linux manifest explicitly declares the target architecture's existing ELF
interpreter for shared-runtime TLS imports, not a dependency on host libc/libstdc++.
The public application sample is `examples/web-browser`; it uses a separate
WebView process and never links WebKit's private Skia. HTTPS needs an explicit
application PEM trust bundle, and the example has no sandbox. This is not a
security-hardened browser or an authorization to publish a release. Native
development-package acceptance does not close the cross-target or public-release
gates recorded in Web acceptance.

The SHA-256 is over the exact release asset bytes, not merely a Git ref. Git
commit IDs remain provenance metadata, while the digest is the download
integrity and reproducibility boundary. A development build may generate a
local source asset and matching recipe, but a published recipe must never use
a mutable branch URL or an unfilled digest.

`tools/create_stage_source.py` produces the `02-cxx` and `03-gfx-simple`
assets. The former packages the already-fetched pinned
libc++/libc++abi/libunwind trees; the latter packages the selected OS window
backend and, on Linux, the pinned libxkbcommon source port. Both carry only the
CRT recipes, standalone build files, wrappers, tests, and source files needed
for that target. Asset names include the target OS, and schema-v2 recipes bind
that OS to both the predecessor SDK and the archive metadata. Archive entry
ordering, timestamps, owners, and modes are normalized before SHA-256
calculation. The producer refuses a dirty meta-toolchain tree for release
output unless local testing explicitly opts in.

Every SDK carries `tools/crt-stage-build.py`. Given an input SDK and a
published recipe, it checks the input stage, byte size, SHA-256, embedded CRT
commit, archive path safety, and build entry point before executing anything.
The `02-cxx` entry point copies `01-c` to a new output, builds the imported C++
runtime against that copy, overlays the installed headers/libraries, runs the
static/shared imported-libc++ smoke, and verifies the resulting distribution.
The `03-gfx-simple` entry point similarly builds the window-only standalone
CMake project against `02-cxx`; it deliberately excludes GPU/native-Wayland
Vulkan coupling, runs window and synthetic-input tests, installs and rebuilds
the external example, and verifies the cumulative SDK. On Linux it first
builds libxkbcommon from the pinned source asset and carries its headers,
static library, license, and recipe provenance in the result. These files are
listed under `redistributed_dependencies` in `manifest.json`; distribution
verification rejects a Linux `03-gfx-simple` SDK whose xkbcommon declaration
or declared payload is incomplete. This is the first implemented port-library
instance of the transitive dependency rule, not an exemption for future
graphics/media ports.
Release packaging may add generated recipes under `stages/recipes/` with
`create_dist.py --stage-recipe <recipe>`; development packages do not claim a
downloadable transition until such a fully pinned recipe exists.

## External Toolchain Contract

The activation scripts and wrappers accept these externally supplied values:

- `CRT_CC`, `CRT_CXX`: C and C++ compiler executables;
- `CRT_AR`, `CRT_RANLIB`: archive tools;
- `CRT_WINDOWS_SDK_LIBPATH`: Windows SDK `um/x64` or `um/arm64` import-library
  directory needed by the freestanding Windows link.

For a packaged distribution, source `activate.sh` or call `activate.cmd`, then
build the application normally. The wrappers select the installed CRT sysroot
and its freestanding/default-runtime policy. `crt-toolchain.cmake` is provided
for CMake consumers and selects those wrappers. An embedded vendor toolchain
that cannot use the Clang-compatible wrappers remains authoritative and must
consume the packaged headers, startup objects, and libraries directly.
New SDKs also persist their OS/architecture in `tools/crt-target.sh`, so direct
unactivated wrapper calls do not silently infer a cross SDK's target from
`uname`. Explicit `CRT_TARGET_OS`/`CRT_TARGET_ARCH` overrides remain available.
Cross-Linux activation exposes the compiler scripts, but does not prepend
the SDK's target shell/make/toybox directory to the build-host PATH.
On Windows, `activate.cmd` derives `CRT_WINDOWS_SDK_LIBPATH` from
`WindowsSdkDir` and `WindowsSDKLibVersion` when run from a Developer Command
Prompt; otherwise set it explicitly.

Clang configuration files can provide GCC-spec-like defaults for a particular
driver name, but they are a convenience layer rather than the distribution
boundary: their discovery rules depend on the compiler installation and the
selected target triple. CRT therefore keeps explicit wrappers/toolchain files
and records the chosen compiler/triple/options in the manifest.

## Distribution Acceptance

A release stage is complete only when acceptance tests run outside the source
and build trees using the packaged directory. At minimum they must verify:

1. compile, link, and run a C or C++ sample as appropriate;
2. static and shared runtime linkage;
3. a path containing spaces;
4. one CMake consumer and one configure/make consumer where applicable;
5. the next stage fetched from its pinned source asset, SHA-256 verified,
   built and tested using only the preceding extracted stage;
6. no access to repository headers, libraries, or build outputs during that
   isolated stage build;
7. packaged examples rebuilt and run at the appropriate stage;
8. no accidental absolute source/build paths in installed files (binaries via
    the RPATH/install-name gates, text files via `verify_dist.py`'s `.la`/`.pc`/
    `bin/*-config` check; `tools/crt_text_relocate.py` makes the isolated
    stages' port text files relocatable);
9. no compiler or linker executable in the archive;
10. every declared external dependency's headers, link artifacts, and runtime
    libraries are present, and its provenance/license metadata is recorded;
11. the built-in ELF/PE/Mach-O dependency inventory finds no embedded
    dependency path or undeclared `.so`/`.dll`/`.dylib` import, no absolute
    macOS `LC_ID_DYLIB` or absolute `LC_RPATH`, and no macOS executable with
    an `@rpath` dependency but no portable `@loader_path`/`@executable_path`
    RPATH entry;
12. OS-owned or device-driver prerequisites excluded from the archive are
    explicitly named in the manifest;
13. the manifest's OS, architecture, stage, compiler inputs, and option set.

## Linux Host And Device Capabilities

Package names below are examples, not CRT dependencies baked into an embedded
image.

For a Debian/Ubuntu development host, the common build tools are `git`,
`cmake`, `ninja-build`, `python3`, `clang`, `lld`, and `compiler-rt`. The
compiler packages are host prerequisites and are never copied into CRT.

Simple Graphics needs a reachable Wayland compositor and the Wayland runtime;
keyboard mapping also needs xkbcommon data. The software surface is `wl_shm`
based and does not require Vulkan, Mesa, or libva.

Advanced Graphics on a Debian/Ubuntu host normally uses:

- build: `libvulkan-dev` and Wayland development files;
- runtime loader: `libvulkan1`;
- one Vulkan ICD: for example `mesa-vulkan-drivers`, or the GPU vendor's ICD;
- diagnostics: optional `vulkan-tools`.

Fedora uses `vulkan-loader-devel`; Arch uses `vulkan-icd-loader` plus a
separate device driver package. Mesa is one implementation, not an ABI
requirement. An embedded product should use the board vendor's loader/ICD and
Wayland WSI, or a future CRT DRM/KMS backend when that backend exists.

The Linux FFmpeg recipe enables VA-API hardware H.264 decode unconditionally
(verified 2026-09-22, [`archived HISTORY.md`](https://github.com/webos21/crt/blob/4e5eead68048723c37e46c22d80bca43915ac093/HISTORY.md)), so `libva`/`libva-drm` are a real,
declared `external_prerequisites` dependency (`linux-vaapi-runtime`/
`linux-vaapi-driver`, `tools/crt_dist_prerequisites.py`) of `04-gfx-media`, not
a hypothetical future one. A typical Debian/Ubuntu split is `libva-dev` and
`pkg-config` for builds, `libva2` and `libva-drm2` at runtime, `vainfo` for
diagnostics, and a GPU-specific VA driver (e.g. `intel-media-va-driver-non-free`
on Intel). Fedora uses `libva-devel`; Arch uses `libva` and `libva-utils`. A
successful `vainfo` is a device-image acceptance check, not evidence that CRT
should bundle the driver -- the driver stays a manifest-declared host
prerequisite, never copied into the SDK.

## Linker Scope

The `linker/` directory remains reserved for the long-term CRT-owned dynamic
loader. Implementing or distributing that loader is outside the current
roadmap. The active goal is source-rebuild portability and native executables,
not unmodified glibc binaries, APKs, containers, or a bundled compiler suite.
