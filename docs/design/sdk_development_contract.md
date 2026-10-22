# CRT SDK Development Contract

Design baseline: 2026-10-20. This document defines the **target contract**
between SDK producers and `crt-cli`/`crt-studio` consumers. It does not claim
that the proposed bundles, manifest extensions, profiles, or CLI commands
already exist or have been released. Current distribution procedures and
measured acceptance evidence belong to the
[distribution guide](../guides/distribution.md) and the relevant acceptance documents.

Related designs: [CRT-CLI](crt_cli.md), [CRT Studio](crt_studio.md).

## 1. Purpose and Product Boundaries

Developers should build CRT applications without copying startup sources or
manually assembling `-ffreestanding`, `-nostdlib`, sysroots, C++ exception
options, link order, or DLL/worker paths. The authoritative rules belong to
the **SDK wrappers, toolchain, and deployment contract**, not the CLI.

| Repository/product | Owns | Does not own |
| --- | --- | --- |
| `webos21/crt` / SDK | libc/PAL, C++ runtime, GFX/Media/UI/Web, startup, wrappers, host tools, official port recipes and technical validation | IDE policy, CLI project UX |
| `webos21/crt-cli` | SDK installation/selection, development environments, port resolution/local builds, project build/run/package | A separate implementation of CRT ABI/link rules |
| `webos21/crt-studio` | CRT-native IDE and editing/debugging UX | A duplicate SDK installer or recipe engine |

The separate CLI and Studio repositories now contain their own design and
bootstrap documentation, not product implementations. Their local designs own
subsequent product decisions; CRT's CLI/Studio documents retain the original
integration baseline. Initially, develop `crt` and `crt-cli` together; Studio
workflow integration follows available CLI capabilities.

Build dependencies flow from Studio to the public CLI contract/SDK and from
the CLI to the SDK. Consumer bug reports and requirements may flow back to the
producer, but CRT runtime builds must not require CLI or Studio sources.
Product assembly jobs that bundle an external CLI remain separate from
runtime/SDK production jobs.

## 2. SDK Stages and Port Packages Are Different Layers

Keep the existing cumulative stage IDs. User-facing descriptions such as
`gfx-ui` or `libc++` do not introduce new stage IDs. A profile is only a UX
name that selects an existing stage.

| Proposed profile | Existing stage selected | Cumulative content |
| --- | --- | --- |
| `c` | `01-c` | libc/libm/libdl, PAL, startup, shell and utilities |
| `cxx` | `02-cxx` | C plus imported libc++/libc++abi/libunwind |
| `gfx` | `03-gfx-simple` | C++ plus window/input/software framebuffer |
| `media` | `04-gfx-media` | Simple graphics plus Skia/GPU/FFmpeg/media |
| `ui` | `05-ui` | Media plus CRT-owned UI and private LVGL |
| `web`, `full` | `06-web` | UI plus WebKit/JSC/WebView and helper processes |

Initially, download **existing, validated cumulative archives**. A C++
developer must not have to download WebKit. Adding an upper stage means
installing/selecting a separate immutable SDK, not overwriting an existing
SDK. Check actual manifest options/capabilities even for the same profile;
a profile name alone does not prove that an optional feature is enabled.

Distinguish the following concepts:

- SDK: one validated runtime/ABI combination. Do not mix arbitrary `libc` and
  `libc++` versions.
- Port: an external library added above the SDK, such as the existing zlib,
  `sqlite-amalgamation`, and libpng recipes. Upstream versions and recipe
  revisions have separate identities.
- Host tools: mksh/toybox/awk/make and generators that **execute on the build
  machine**.
- External toolchain: Clang/LLD, CMake/Ninja, and required OS/vendor SDKs.
  Preserve the existing policy of not bundling compilers in CRT SDKs.

SDK production order, port dependencies, and deployment runtime closures are
separate graphs. A recipe's `make` dependency, for example, is a host
execution requirement, not an application link library. WebKit's private
Skia/OpenSSL/FFmpeg dependencies do not automatically become public port packages.

Future SDK delta-component archives require explicit file ownership,
duplication/conflict, alias, license, and runtime-closure rules. Components
assembled from the same approved build must have the same byte/alias inventory
as the cumulative archive. This optimization is not a prerequisite for the
initial CLI or source-first port engine.

## 3. Existing Foundations and Required Extensions

| Existing foundation | Initial consumer use | Additional contract needed |
| --- | --- | --- |
| `manifest.json`, `VERSION`, `crt-toolchain.cmake` | SDK-only CMake configuration after stage/target/capability checks | Machine-readable development contract and compatibility negotiation |
| `tools/crt-cc`, `crt-c++`, platform launchers | Delegate startup/sysroot/ABI/link policy | CLI diagnostics and tool-selection adapters |
| `tools/crt-env.*`, `tools/crt-target.sh` | Interpret existing environment/target information | Independent host/target selection and explicit environment profiles |
| CRT shell/tools in `system/bin`, `bin`, `usr/bin` | Preserve the existing rootfs namespace | Independent Host Tools Bundle and host execution manifest |
| `porting/recipes/*.json`, tests/shims, packaged port driver | Reuse recipe execution and validation | Versioned registry, dependency-role separation, immutable store |
| SDK examples and upper imports | Technical baseline for external project templates | Public target inventory and transitive dependency contract |
| `verify_dist.py`, ELF/Mach-O/PE and Host ABI checks | SDK/application/port validation foundation | Stable consumer interface for port/runtime closures |

Reusing existing APIs/wrappers does not make internal Python module APIs or
build trees public contracts. The initial CLI adapter invokes **version-pinned
entry points in the installed SDK**. Declare Python as a host requirement when
needed. A later fully CRT-native recipe executor must preserve compatibility
against the same fixtures and results.

## 4. Proposed SDK Build Contract v1 Contents

Do not silently change existing `manifest.json` semantics or the stage recipe
schema. Introduce the development contract through a versioned sidecar
(proposed path: `share/crt/contracts/development.json`) or an explicitly
versioned manifest extension. The location/schema require joint review during
the first implementation.

The producer must supply at least:

1. **Identity:** source commit, release/build ID, stage, target OS/architecture,
   archive digest/size, and SDK content fingerprint. Normalize input aliases
   such as `x64/amd64/arm64`, but record existing canonical architecture IDs
   in locks.
2. **Compatibility:** contract schema/version, capabilities and link modes,
   minimum OS, compiler/OS SDK requirements, C/C++ runtime and exception/TLS ABI
   conditions. Do not assume an as-yet undefined public `crt_abi` number
   exists. Conservatively bind v1 compatibility to exact SDK content identity.
3. **Build:** SDK-relative wrapper/toolchain paths, language standards,
   startup/link modes, public CMake targets or link interfaces, and
   compiler-rt requirements.
4. **Run/deploy:** runtime libraries, aliases, workers/resources by executable
   type, package-local search policy, and explicit OS/framework/driver
   prerequisites.
5. **Host execution:** host tuple, tool/applet names and actual paths,
   interpreters/runtime closures, CRT rootfs requirements, and native
   generator/external-tool roles.
6. **Provenance:** redistributed dependencies, source hashes, notices/SBOM,
   source offer and source correspondence, following existing production and
   verification policy.

Record normalized paths relative to installation roots. Keep actual host
absolute paths only in local execution records for selected external tools.
`latest` and Git branch names are not artifact identities; a checksum alone
does not authenticate the publisher. Do not claim signing/provenance
mechanisms are available before they are implemented.

SDK wrappers select `crt1.o`, Windows DLL startup, and other startup artifacts
by purpose. Do not require users to copy/compile `crt0.c`. Preserve the
Windows C++ DWARF/Itanium/libunwind contract; do not mix MSVC/GNU C++ runtimes.
Allow CRT-controlled OS boundaries, not convenient exposure of host standard
headers/libraries.

## 5. Host Tools Bundle

An independent bundle is a **new distribution design**. Do not arbitrarily
move existing tools out of SDKs or flatten everything into one `bin/`.
Preserve `/system/bin`, `/bin`, and `/usr/bin` as interpreted through
`CRT_ROOTFS`, together with wrapper/native-tool invocation relationships.

For each host OS/architecture, inventory:

- mksh and its `sh` aliases, toybox and supported applets, separate awk and GNU make.
- Each applet's alias type/target, executable permissions, file hashes, and
  runtime-library closure.
- Required native-tool bridges, wrappers/configuration, and each tool's
  source/license/provenance.
- POSIX/native path conversion rules and writable scratch/rootfs locations.

Bundling toybox does not provide every Unix command or make. Enumerate commands
and support according to the [applet status](../porting/toybox_applet_status.md).
Test alias/launcher behavior through actual native processes.

Windows build/source-stage hosts retain **Developer Mode as a mandatory
constraint**. Do not replace real symlinks with MSYS markers, shortcuts, or
deep copies, and do not let the CLI change this setting automatically.
Do not promise a new alias mechanism without separate validation.

Independent-bundle acceptance first requires mksh/awk/make and applet execution
and configure-like child-process creation without a target development SDK or
CRT source/build tree. The separate zlib static/shared build-and-run gate
requires a pinned target SDK, but no CRT source/build tree; host tools must
remain selected independently of that SDK. Also test paths containing spaces and
Korean characters, colliding DLL/tool names on PATH, and update failure/recovery.
Do not claim unfinished process-signal or interactive job-control capabilities.

Host tools must match the execution host. Do not execute make/scanners from an
aarch64 target SDK on Linux/x86_64. Run target-compiled test binaries only during
native validation or through an explicitly supported runner.

## 6. Bootstrapping a CRT-built CLI

Linking the CLI against CRT is different from requiring a development SDK at
runtime. Initially, build the independent `crt-cli` repository against a pinned
SDK and distribute a **bootstrap archive containing the CLI and its actual
minimal runtime closure**. Users must be able to extract it with OS facilities,
run the CLI, and install SDKs.

```text
CRT source -> approved SDK + Host Tools
                     |
pinned crt-cli source + pinned SDK -> CLI bootstrap archive
                                             |
                                 installation/management without an SDK
```

### Bootstrap SDK profile and artifacts (adopted direction, 2026-10-22)

Building the CLI needs a development SDK; running it must not. CRT therefore
produces three artifacts from the same sources, stage outputs and port recipes:

| Artifact | For | Content |
| --- | --- | --- |
| **Bootstrap SDK** | `crt-cli` developers and CI | The `02-cxx` stage plus the ports the CLI links: libcurl, its TLS backend, zlib, and any other library the real link result requires (headers, link libraries, startup, toolchain files, notices) |
| **CLI runtime bundle** | End users | The CLI executable and only the runtime files its import closure needs, placed app-local |
| **Full SDK** | CRT application developers | Unchanged, up to `06-web` |

The Bootstrap SDK is a reduced packaging of verified `01-c`/`02-cxx` and port
outputs, not a second build system, ABI or recipe set. Do not start it with a
separate compile path: extract it after the external CLI build against the
existing outputs succeeds. The ports are produced and validated by CRT;
`crt-cli` neither ports them nor vendors CRT headers or libraries, and consumes
only the pinned Bootstrap SDK identity. The runtime closure is computed from the
CLI's actual ELF/Mach-O/PE imports, never from a guessed list. Whether a static
CLI is viable is a separate technical check; the default is a minimal dynamic
bundle.

Do not introduce a cycle in which CRT runtime/SDK builds require the CLI.
If CRT product assembly includes the CLI, explicitly supply the external source
revision/hash and an already produced SDK. CLI source vendoring, unpinned
submodules, and sibling-checkout paths are unnecessary.

The initial implementation target is CRT-native C/C++ (C++17 baseline).
The earlier proposal requiring a host-native Go/Rust CLI is not adopted.
The C99 core policy is unchanged; build-host Python/CMake tools are not
categorically prohibited.

Explicitly validate the CRT-built ports needed for minimal CLI TLS/download
support. Do not make the whole UI/Web SDK a runtime prerequisite or silently
link host libcurl. TLS trust/download-stack selection belongs to the early
bootstrap review. Windows imported DLLs must resolve during initial loading,
using validated placement such as app-local deployment. Merely placing them
under `runtime/` does not guarantee PATH-based resolution.

Bootstrap acceptance must exercise an actual HTTPS metadata fetch and artifact
download on a clean host without the development SDK, producer build tree, or
globally installed CRT DLLs. Record the shipped runtime inventory, TLS stack,
trust inputs, publisher-authentication policy, and negative cases separately
from later SDK installation/transactions. CLI's D1-07 acceptance owns these
consumer measurements; writing this requirement is not execution evidence.

## 7. Change, Validation, and Release Principles

Keep early producer/consumer changes narrow and ordered:

1. Jointly review use cases, reproducer fixtures, public contracts, errors,
   and lifetimes.
2. Test the CRT change and produce a source/content-hash-pinned SDK.
3. Run consumer tests in a CLI candidate against that artifact alone.
4. Merge CRT first, verify/promote the SDK from the merged commit, then update
   the CLI lock. A successful PR-candidate artifact does not automatically
   validate the merge-commit artifact.
5. On failure, retain the previous lock/installation. Never replace an
   existing artifact under the same identity.

CRT acceptance owns ABI/tool validation; future `crt-cli` acceptance owns CLI
installation/port/project validation; future `crt-studio` acceptance owns
end-to-end Studio UX. Joint reports link both commits, artifact digests,
host/target identities, and results without duplicating the original evidence.
Follow the [CLI joint sequence](crt_cli.md#9-joint-development-sequence)
for the detailed development order.

SDK/CLI/Studio versions are independent. A consumer design or development
snapshot alone does not authorize publication. The `05-ui`/`06-web` release
tooling, source/notices/SBOM obligations and continuing security checks belong
to [Web acceptance](../acceptance/crtweb_acceptance.md) and the
[release guide](../guides/release_preview.md). Tranche 10's owner approval is
recorded there; this SDK/CLI design does not independently approve a consumer
product or publish artifacts. [TODO](../../TODO.md) owns the next producer work.

## 8. Cross-repository Contract Review

`crt-cli` owns the shared adoption record at `docs/compatibility.md` in the
[CLI repository](https://github.com/webos21/crt-cli). It links original evidence;
it does not replace producer SDK acceptance or Studio's native UX acceptance.
Keep the original design baseline separate from the exact SDK/tool artifacts,
CLI command/JSON version, and combinations actually exercised by Studio.

For changes to shared contracts, each repository's review must:

1. Identify the contract owner and the affected producer/consumer paths and
   immutable baseline commits. Record changed ownership explicitly.
2. Distinguish existing wire/schema fields from proposed versions and
   capabilities. Existing SDK manifest `format: 1`, port recipe
   `schema_version: 1`, and isolated-stage recipe `schema_version: 2` are
   separate formats, not a common SDK/CLI contract version.
3. Describe compatibility/errors/lifetimes, migration and fallback/rejection;
   link proposed commands to their actual implementation/capability status.
4. Link the owning TODO step, acceptance case/fixtures, and original native
   evidence (or **Not run**). A documentation baseline or fixture pass does
   not approve a runnable SDK/CLI/Studio combination.
5. Review affected repositories and update the adoption record through linked
   changes. Validate/promote the producer first, then update consumer locks;
   preserve previous working identities and failed results.

Apply this as a manual review checklist now. Lightweight future automation can
check pinned references, owner/version links, declared vs implemented commands,
and TODO-to-acceptance links once concrete metadata exists. Do not enforce
identical Markdown text across owners or add a second full build for doc edits.
