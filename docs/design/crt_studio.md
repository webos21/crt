# CRT Studio: CLI-backed Native IDE

Design baseline: 2026-10-20. `webos21/crt-studio` is the target repository for
a later product. This document does not claim that the IDE exists or that all
Desktop UI contracts are ready. The
[SDK Development Contract](sdk_development_contract.md) owns producer
boundaries; [CRT-CLI](crt_cli.md) owns prerequisite development/package
management and the joint development sequence.

## 1. Product Purpose and Development Order

Studio's primary purpose is not to provide another editor. It lets developers
build applications without manually managing CRT-specific startup, sysroot,
compile/link, and runtime contracts. Deliver that experience through the CLI
first; the IDE provides a visual frontend to the same capabilities.

The final goal is a Studio built only against an installed SDK that can create,
edit, build, run, debug, and package new CRT applications, eventually including
Studio's own sources. Self-hosting does not mean a circular build entangled
with SDK source/build trees. It means building its sources in an independent
workspace through the CLI and a pinned SDK.

Do not move the CLI back into Studio or duplicate its SDK manager/recipe engine.
The CLI owns SDK downloads, port builds, locks, and project build/run/package
policy. It remains an independent product for CI, AI, and remote automation
without a GUI.

## 2. Architecture and Ownership

```text
Studio (CRT-native C++17 application)
  workbench / editor / project UX / LSP / DAP
             |
  versioned CLI commands, JSON results and job events
             |
  crt-cli -> selected SDK / source-first ports / native build tools

Studio rendering/input -> public crtui / crtgfx
optional documentation/preview -> public crtweb/WebView
```

Studio consumes CRT UI through public APIs. LVGL is a private CRT implementation
dependency; Studio must not include LVGL types/private headers directly.
Do not expose internal Skia/WebKit objects, allocators, or ownership through
IPC or product APIs.

| Capability | CRT/SDK | CLI | Studio |
| --- | --- | --- | --- |
| Startup, runtime/C++ ABI, OS boundary | Owns | Delegates to selected contract | Consumes |
| SDK/host-tool production | Owns | Installs/selects/diagnoses | Status/selection UI |
| Port recipe technical policy | Owns | Fetch/resolve/build/store/lock | Search/add UI |
| Project configure/build/run/test/package | Toolchain/deployment contract | Execution engine | Command/diagnostic UI |
| Window/input/IME/clipboard, drawing/widgets | Generic public APIs | Environment support as needed | Application consumer |
| Dock layout and editor document model | Reusable primitives only | Not involved | Owns |
| Syntax, completion, project browsing, unsaved buffers | Not involved | Project metadata | Owns |
| LSP/DAP | Process/IPC and runtime support | Launch/tool identity contract | Protocol client and UX |
| AI/external IDE integration | Public SDK | Same machine-readable workflow | Later frontend |

Before placing functionality in CRT, ask whether a second external consumer
would use the API. Keep IDE-specific docking policy, editor buffers, and
breakpoint models out of CRT. Feed Studio-discovered Bionic/PAL gaps back into
CRT as small reproducers and regression tests.

## 3. CLI Integration Contract

Studio pins compatible CLI/SDK identities and checks schemas/capabilities at
startup. Do not accidentally select another installed `crt` through global
PATH. Expose CLI location/version, project lock, SDK/profile, and job state
in the UI.

- Use CLI transactions for SDK, host-tool, and port installation/updates.
- Obtain project metadata and actual build plans from CLI responses rather
  than reconstructing flags.
- Send child logs to the UI console and associate structured diagnostics with
  files, lines, columns, and tools. Separate JSON from application stdout/stderr.
- Long-running jobs expose progress, cancellation, timeouts, and termination
  reasons. On GUI exit, clean up only owned jobs; never kill other users'
  processes by global executable name.
- Do not expose tokens, passwords, certificate private keys, or other secrets
  through UI logs/reports.
- Opening an arbitrary workspace/recipe must not automatically build, install,
  or execute it. Show commands and trust origins, and apply workspace trust
  and user consent.

Use CLI commands plus JSON as the first boundary. Do not introduce a daemon,
RPC service, or shared internal C++ ABI before a demonstrated need.
An interactive terminal/PTY is distinct from a non-interactive build-log panel;
add it only after actual CRT/PAL capabilities are validated.

Initially, approved **native host tools** such as clangd, LLDB/DAP, and
CMake/Ninja may execute as external processes. Studio being CRT-native does
not require porting all of these tools to CRT. Target programs, however, use
CRT SDK/toolchain contracts. Validate whether the LLVM debugger actually
handles CRT Windows DWARF exceptions, emutls, and startup through separate
debug acceptance; tool names alone do not prove support.

## 4. Desktop UI and Editor Investigation

Audit gaps against the current public `crtui` API and external-surface
contract. Do not start by exposing the full LVGL widget set or designing a
monolithic desktop API.

| Priority | CRT boundary to inspect | Studio consumer |
| --- | --- | --- |
| Prerequisite | Focus/key/shortcut, IME composition/commit/cancel, clipboard | Real Korean input, selection, and editing PoC |
| Prerequisite | Custom drawing/text shaping, scale/clipping/coordinates, lifecycle | Professional editor-surface PoC |
| Basic workbench | Splitter/tab/tree/context menu/dialog, cursor/High-DPI | Explorer/editor/build-panel assembly |
| Later | File watch/process, PTY/ConPTY, accessibility, multi-window | External changes, terminal, product UX |

This is an inspection list, not a claim that every listed capability is absent.
IME requires more than UTF-8 commit events: connect composition ranges, caret
and candidate-window positions, focus, commit, and cancellation, and verify
them on all three native OSes.

An editor engine such as Scintilla is an **evaluation candidate**.
Select an engine only after measuring license fit, CRT-only builds, and
public drawing/input adapter changes through a PoC. This document does not
assume adoption or a completed port. Studio owns document buffers, undo/redo,
save, and recovery; editing-data integrity takes priority over UI features.

Use public `crtweb`/WebView APIs when documentation or preview requires them.
Do not require the entire WebKit runtime just to display an empty workbench.
Determine the minimum stage from actual imports/deployment closure; evaluate
UI-only configurations against `05-ui`.

## 5. Staged Goals

Use stable consumer contracts from CLI stages D0-D6 as prerequisites.
High-risk UI/IME/editor PoCs may proceed in parallel, but must not bypass
SDK/CLI boundaries through private includes.

| Studio stage | Scope | Acceptance goal |
| --- | --- | --- |
| S0 External consumer | Pinned SDK/CLI, minimal native window, input, shutdown | External build without CRT source/build trees; three OSes; no private APIs |
| S1 Workbench | Explorer/editor/build placeholders, menus/settings/layout | UI model/toolkit lifecycle, focus/resize/High-DPI, IME PoC |
| S2 Editor | Documents/editing/undo/save/recovery, syntax, LSP | Real files and Korean composition; crash recovery; editor surface |
| S3 Build/run | CLI SDK/port/project UI, diagnostics, cancellation | Same locks/plans/results as CLI; no unnecessary SDK rebuilds |
| S4 Debug | Native debugger/DAP, breakpoints/step/threads/stack | Actual CRT application debugging, teardown/restart, failure recovery |
| S5 Preview | Packaging, optional preview/AI integration, self-development | Studio runs without an SDK and builds/debugs itself in an independent workspace |

Do not reimplement installation, source-first ports, locks, or host-tool
acceptance inside IDE tranches. CRT provides/validates the required API first;
Studio then updates its immutable SDK lock. Distinguish native GUI/IME/debug
acceptance from headless CI success. Do not extend support to Windows/arm64 or
other architectures without validating the actual SDK/CLI/host-tool/debugger
combination.

## 6. Target Repository and Documentation Layout

Start Studio with areas such as `src/app`, `workbench`, `editor`,
`language/lsp`, `debug/dap`, `cli_adapter`, `resources`, `tests`, and
`packaging`. Do not copy CLI SDK/port engine sources. Finalize directory
names during implementation; do not add a top-level `tests/desktop_ui` to
CRT that violates its library-owned test policy.

Studio maintains its own AGENTS/TODO/HISTORY and architecture/acceptance
documents. Joint UX reports link original CLI/SDK test results and artifact
identities. Keep CRT API/ABI measurements in CRT acceptance, CLI
environment/port/project measurements in CLI acceptance, and avoid copying
completion status across all three repositories.

Studio versions are independent of SDK/CLI versions. Public previews pin
combinations that have passed their respective deployment, security, license,
and source conditions. A successful PoC using an SDK development snapshot does
not authorize public release of Studio or the CRT SDK.
