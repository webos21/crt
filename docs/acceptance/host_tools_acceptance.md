# Independent Host-tools Acceptance

Producer contract: [experimental host tools](../design/host_tools_contract.md).
Work queue: [remaining host qualification](../../TODO.md#remaining-host-qualification).
The CLI owns commands, adoption and native execution in its
[compatibility ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#independent-host-tools-and-d1-linux-adoption-2026-11-02)
and [D1 evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md).
This document owns assembly, provenance, prerequisites and producer regressions.

## Local Linux/x86_64 candidate (2026-11-02)

Reviewed CRT `8616fd64b13fa41925c1fce2b08f4937b7e8dcf1` and CLI
`adc03b04ed124c71f0726a895da2cc2cde98b78d` before this uncommitted increment.
CRT owns the new assembler/CMake entry point and host-tools format; CLI owns
the environment, scoped diagnosis and process adapter. SDK/provider/project
formats and production locks do not change. This is local technical evidence,
not a clean committed assembler, release, promotion or three-host acceptance.

| Input / output | Exact identity |
| --- | --- |
| Native source SDK | `out/tranche17-20261102/build5/dist/01-c`, clean CRT `49879bf82e2742baeb1878e492d42c7d561b6f7a`, `v0.4.0-sourcefix.20261102.5` |
| Source SDK manifest SHA-256 | `5b0914ceefd5f4ef79343fb58fa2ac7fd84ef649d6866c37b1b93afb8e1c3576` |
| Selected output | `out/tranche2-20261102/host-tools-r4`, `v0.1.0-hosttools.20261102.4`, Linux/x86_64 |
| Host-tools manifest SHA-256 | `59bdf8f83ce7817175010953abe652fe6f52852f8aff9a5528f2ce8b64812652` |
| Payload inventory SHA-256 | `3b75fb9bbab8c9e9a1c7a542f5ced227f1c66bb92ed875e87d78f86e1aba11bf` |
| Assembler script SHA-256 | `9599fad6b3749d034f0034eabc6cd20eb8c867d05281e0e04d53a291d6d06e0f` |
| ELF inspection module SHA-256 | `c878c5c449b3074e8acf635e7810bcc6cf6ea1fc05b84f666c7359fde66e6fa0` |

The manifest truthfully records assembler baseline `8616fd6` with
`working_tree_dirty: true`. Its source SDK remains the separate clean candidate
from [Tranche 1.7](release_qualification.md#common-source-candidate-selection-2026-11-02).
The pending tracked patch, new files and their hashes are retained in
`out/tranche2-20261102/review-snapshot/` for the baseline above.
Assembly needs only native `01-c` public artifacts, CRT-owned notice/recipe inputs
and producer Python. It does not build or require C++/GFX/Media/UI/Web. Runtime
consumption uses the standalone copied bundle, without source SDK or Python.

The bundle has 120 inventory entries, 90 executable paths and 86 declared
aliases; physical copied aliases are retained as supplied by this SDK. Four
primary images are CRT-built ELF x86_64 and have no DT_NEEDED. mksh, toybox and
awk have no runtime search path; make has an **empty RUNPATH tag**, which is
allowed rather than described as an absent tag. All four name the real OS
interpreter `/lib64/ld-linux-x86-64.so.2`. `interpreters` and explicit external
prerequisites record it alongside the Linux kernel ABI; the OS loader is not
bundled, and no host libc or host C++ runtime is imported.

An audit caught that copying `01-c` prerequisites alone omits this PT_INTERP
boundary. Candidate r4 adds actual interpreter inspection and rejects foreign
loader paths; earlier r1-r3 manifests stay retained and are not the final
dependency declaration. The executable payload bytes did not change.

GNU make's missing SDK notice is supplied from
`third_party/crt-runtime/make.COPYING`, verified against the exact official
AOSP `toolchain/make` `44fc4fe66a484b91844c302f03eaa8438e065d17` COPYING blob,
SHA-256 `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903`.
The companion source receipt, adopted make recipe and actual source-SDK recipe
must agree. A changed cached COPYING fails before publication. Notices and
provenance for CRT libc/compiler-rt, mksh, toybox and separate awk are retained;
the compiler-rt notice can be installed from CRT's pinned LLVM notice registry
when a C-only SDK omits the combined LLVM-runtime notice directory.

## Reproduction and checks

From CRT, with a previously verified native public C SDK:

```sh
python3 -m unittest tools/test_host_tools.py tools/test_crt_elf.py
python3 tools/create_host_tools.py \
  --sdk-root out/tranche17-20261102/build5/dist/01-c \
  --output out/new-host-tools-candidate --version v0.1.0-hosttools.20261102.4
```

For the CMake entry point, configure a fresh native preset output with
`CRT_HOST_TOOLS_PREDECESSOR=<public-01-c-SDK>`,
`CRT_HOST_TOOLS_VERSION=<independent-version>` and optionally
`CRT_HOST_TOOLS_OUTPUT=<new-path>`, then:

```sh
cmake --build out/tranche2-20261102/producer-final --target crt-host-tools-test
```

The ordinary default predecessor is `crt-c-dist`; an explicit predecessor
avoids rebuilding it. Select a **new output path/version** when inputs change;
the assembler refuses to replace an existing candidate. Repeated unchanged
builds use the shared content-aware cache, followed by inventory/native-image
verification. Logs are retained under `out/tranche2-20261102/` as
`producer-final-configure.log`, `producer-final-reconfigure.log`,
`producer-final-r4-build.log` and `producer-final-r4-cache.log`.
The CMake output `host-tools-r4-cmake` matches r4's manifest and payload bytes.

21 focused host-tools/ELF parser tests pass. Fixtures cover SDK independence,
native host/image rejection, dispatcher/notice corruption, immutable outputs,
escaped links, symlinked output parents, permission removal and changed cached
make notices. ELF fixtures cover real PT_INTERP without DT_NEEDED, ELF32 big
endian/ELF64 little endian, malformed strings/truncated headers, absent
interpreters and foreign-loader rejection. Fixture passes are not native tool
execution proof; that belongs to the linked CLI replay.

## macOS/arm64 independent bundle replay (2026-11-03)

Reviewed clean producer `518953f4d9ecc48c1d082644414e0a409a5a49ad` and
consumer `095afbcde4b0501267520381b24ba8114315d89a`. Assembled a **new**
independent bundle using the already verified public macOS C SDK; this is not
a rebuild of the SDK at `518953f` or adoption of Linux's `49879bf` artifacts.
The SDK input and its historical validation keep their original identities.

| Input / output | Identity |
| --- | --- |
| Public native C SDK | `out/cli-macos-revalidation-20261101/dist/01-c`, clean `8db69f404dcfa70b5aa0569cae51a962fd0615d2`, `v0.4.0-cli.20261101` |
| C SDK manifest SHA-256 | `63f04e670e97e167b955f645a8a1b9235f25026b0a6a9198bf770d258059930c` |
| Selected host tools | `out/tranche2-macos-20261103/host-tools-r1`, `v0.1.0-hosttools.20261103.macos.1` |
| Manifest SHA-256 | `0a81db3eacb89b54c106d8f57bfd5a0e6bb57a24977e124f583e7c81751f1160` |
| Payload SHA-256 | `6e6984bb1edd573132ad2bbff01ea8828485be4a7fb239c029f686f53b58608e` |
| Assembler SHA-256 | `9599fad6b3749d034f0034eabc6cd20eb8c867d05281e0e04d53a291d6d06e0f` |

The selected bundle was assembled before test/document edits, so its assembler
records clean `518953f`. It has 120 inventory entries, 90 executable paths and
86 aliases, with the original relative links/copies preserved. License/recipe
checks, including the pinned GNU make COPYING, pass. All four primary tools
are native arm64 Mach-O, own their CRT allocator, have **no LC_RPATH**, and
import only `/usr/lib/libSystem.B.dylib`. The binding audit reports no CRT-owned
undefined symbols bound to a foreign runtime. There are **no private dylibs**
in this selected closure: no private-dylib load/collision success is inferred
from this static-tool case. The OS runtime boundary remains declared.

Producer checks:

```sh
python3 -m unittest tools.test_host_tools tools.test_crt_elf
python3 tools/create_host_tools.py \
  --sdk-root out/cli-macos-revalidation-20261101/dist/01-c \
  --output out/new-macos-host-tools --version <new-local-version>
cmake --build out/tranche2-macos-20261103/producer --target crt-host-tools-test
```

21 focused tests pass; the expanded producer/cache/binding selection passes
56 tests. The fresh producer configuration explicitly selects
the public C SDK as `CRT_HOST_TOOLS_PREDECESSOR`; it builds no target runtime
or upper stage. First and repeated `crt-host-tools-test` pass, respectively
reporting `running (first run)` and `up to date`. The separate CMake candidate
`host-tools-cmake` has the **same payload hash**, but a different manifest
(`4010094bbe0bea0f06845a37fbdda33cf52786674ebccd90d45269e0b243b02d`)
because its version and truthful assembler dirty state differ; it is not
relabeled as the selected clean r1 manifest.

Evidence is under `out/tranche2-macos-20261103/`: `producer-{configure,build,cached}.log`,
`host-tools-bindings.json`, `selected-inputs-final.json`, bundle manifests and CLI
replay logs. The CLI-owned
[macOS D1 replay](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#macosarm64-independent-host-environment-replay-2026-11-03)
owns actual Seatbelt-denied SDK/source/toolchain access, shell/make/awk/alias
execution, paths, cleanup, resource/signature and packaged-CA HTTPS results.
Python/Clang/OpenSSL remain test-host tools outside that product boundary.
No runtime, upstream source, SDK payload, production pin, promotion or STATUS
change was required.

## Windows/x64 independent bundle replay (2026-11-03)

Reviewed producer `9d8be12e27802b87e0bca61fa562a281b1c7319b` and consumer
`0b40a7087e567e8b7bfae2aa880d9f4f2e1abdb3`. A fresh producer directory at
`out/tranche2-windows-20261103/build` builds C/C++, the Development ports and
the SDK. Later PAL/shell corrections produce the separate C and Development
candidates below. Unchanged pinned imported C++ artifacts are reused through
the official cumulative publisher and rechecked with static/shared C++ smokes;
their shared images import the new `libc.dll`, rather than embedding old libc.
This does not claim another C++ compilation. Upper graphics/media/UI/Web
features are disabled; no old SDK is relabeled as a new build. The local edits
are truthfully recorded as `working_tree_dirty: true`, version `development`,
with the separate original compile/base provenance retained.

| Input / output | Identity |
| --- | --- |
| Public native C SDK | `c-sdk-rootfs-fix`, manifest `3390ca1f9dc1bf881b92032ca629c2b982484fff1cbe8cb5973bbbbd4908abd5` |
| Development SDK | `dev-rootfs-fix`, manifest `31bcc5d0b2874bda33c17198f0c391c588ad9be32b9a59bbd57cf6f6a194b421` |
| Development profile | `aaf7fdaa079ad7f8a8de2d324a0cbbd530001bb83a0450289076467a085b53bd` |
| Selected host tools | `host-tools-r5`, `v0.1.0-hosttools.20261103.windows.5` |
| Host-tools manifest | `513eab805ed0753b67d00939fd8b00e03a577f6f14cbe74b2ad4e3862691b1cf` |
| Payload inventory | `69a725081e93134cdd2b16d7a9a6a08f14d5a4d061d883fc430f7609a87f8b16` |
| Assembler script | `9599fad6b3749d034f0034eabc6cd20eb8c867d05281e0e04d53a291d6d06e0f` |

The bundle has **209 inventory entries, 179 executable paths and 175 aliases**.
All four primary images are native x86_64 PE and import only `KERNEL32.dll`
and `api-ms-win-core-synch-l1-2-0.dll`. Alias copies retain their primary
dispatcher's bytes. There are **no private runtime DLLs** in this selected
closure; private-DLL initial-load/collision acceptance cannot be inferred from
it. The explicit Windows OS boundary remains declared. Runtime use requires
neither a target SDK nor the Python producer. Real job-directory links require
Developer Mode; the UTF-8 executable policy requires Windows 10 1903 or later.

Fresh CMake `host-producer-r5-exact` explicitly selects this C SDK as
`CRT_HOST_TOOLS_PREDECESSOR`. First and repeated `crt-host-tools-test` pass,
reporting first-run assembly and `up to date` respectively. Its entire manifest
and payload bytes match r5. Earlier `host-producer-r5` has the same payload but
version `v0`. That candidate is retained; the exact replay uses a fully quoted
PowerShell version argument.
The C-stage suite passes **168 tests with one existing
conditional fork skip (169 selected)**, including the native UTF-8 narrow-string
test. Both Development candidates pass static/shared HTTPS+inflate and trust negatives.
The focused producer/fetch/cache/provider/replay selection passes **71 of 73,
two host-conditional skips**. The exact updated existing CI tooling selection
passes **174 of 178, four conditional skips**; the two new adapter modules join
that same step, with no second CI build or skip-CI change.

### Preserved failures and LF recovery

The first CLI replay rejects the retained clean `d054a66` SDK before seed
compilation because it lacks the adopted `crt-narrow-encoding-1` capability.
Its independent-tool candidate `host-tools-r1` stays historical. Initial new
C/Development candidates and `host-tools-r2` also retain their pre-correction
bytes; they are not the final selected r5 combination.

The fresh producer then exposes system Git `core.autocrlf=true` rewriting
LLVM worktree bytes: `cmake/README.rst` becomes 1,923 bytes/59 CRLFs instead
of the pinned 1,864 LF bytes. Strict source-stage validation correctly rejects
it. `crt-libcxx-build.py` now scopes `core.autocrlf=false` and `core.eol=lf`
to its Git commands, retaining command-scoped Windows long-path handling.
No global Git setting, upstream patch or validation exception is introduced.
The regression uses actual Git repositories with an ambient CRLF policy and
compares both primary and sibling source bytes to pinned blobs; all 16 fetch
tests pass.

`failed-crlf-sources` and `failed-crlf-recovery` retain the failed copies and
Git stores. Reusing existing index timestamps did not rewrite every CRLF
file; cloning a partial repository through the local upload protocol also
failed. New empty worktrees from copies of the retained `.git` stores recover
**12,855 canonical files** without network ref changes. Whole-cache/pin/recipe
checks pass before production resumes. The original non-development tag
attempt also correctly refuses later dirty inputs; the successful local retry
uses `development`, never a falsely clean release. The frozen 02 source archive
SHA-256 is `9710b089ac5872b759b73e859379d91e913e690c80e5913bc07fd6ab9dbe66a3`.

### Native shell/PAL boundaries found by the replay

The original fixture compared Windows' mixed native-root/manifest `system/bin`
spelling to a POSIX-only PATH expectation. The driver now checks the declared
PATH resolves to the **one selected directory**, then compares the child's
exact spelling with that declaration. ASCII grep is exercised separately;
Unicode remains in tool/script/cwd/job paths. Product environment filtering
and the no-host-shell-fallback policy are unchanged.

Three producer defects have actual fail-before/pass-after C regressions:

- The Windows-only mksh adapter retained `-1` (select real fork) as an async
  launch status. `set -e; (exit 7) & ...` exited 255 before waiting. Reset the
  sentinel to upstream's initial success; the regression also requires `wait`
  to report the child's actual exit 7. The existing adapter's
  [patch record](../../shell/toybox/PATCHES.md#mksh-windows-asynchronous-launch-status)
  explains ownership/removal conditions. Generic upstream logic is unchanged.
- Windows `access(directory, X_OK)` incorrectly required a PE image, so mksh
  discarded its physical TMPDIR and failed a heredoc. Recognize an existing
  directory before executable-only suffix fallback. Retain ordinary-data
  rejection and reject missing `name` when only a `name.exe` directory exists.
  `file_path_test` covers access/faccessat search and those boundaries. The
  [Bionic public modes](https://raw.githubusercontent.com/aosp-mirror/platform_bionic/master/libc/include/unistd.h)
  and [kernel-delegating faccessat](https://raw.githubusercontent.com/aosp-mirror/platform_bionic/master/libc/bionic/faccessat.cpp)
  are the interface reference; this is not a full Windows ACL model.
- Private Windows spawn bootstrap used the parent's `CRT_ROOTFS` even when
  envp selected or omitted it. A hostile ambient root overrode the CLI job
  root, so make could not find `/system/bin/sh.exe`. Select the transport value
  from the effective child environment, following
  [Bionic's envp-or-environ rule](https://raw.githubusercontent.com/aosp-mirror/platform_bionic/master/libc/bionic/spawn.cpp).
  `rootfs_process_test` proves explicit selection and no parent/private-marker
  leakage when omitted. Existing bootstrap-variable filtering stays intact.

The initial make also lacked its UTF-8 manifest: lightweight sysroot installed
headers/libraries, while only later `create_dist.py` installed the manifest.
Install the same file with Windows' `crt-c` component before the first make
link. Fresh `manifest-preflight` CMake builds actual sysroot/port-build-make:
installed manifest bytes match the source, and the first make PE contains the
UTF-8 resource. Final r5 explicitly recompiles make with the existing official
recipe engine/new public C SDK. General packaged-port mode intentionally skips
SDK-owned make; the initial empty-prefix attempt stays failed evidence, not a
claimed rebuild. No target recipe/upstream source patch is introduced.
All four final primary tools carry the UTF-8 manifest.

Receipts include `core-ctest-rootfs-final.log`, each C regression's
`*-before.log`/`*-after.log`, and `rootfs-producer-status.json` (successful
C/tool/C++ gates, then a missing-tool-environment preflight failure).
`rootfs-producer-dev-status.json` passes after declaring Clang/Clang++/AR/RANLIB.
`rootfs-producer*.log`, `dev-rootfs-*-r2.log`, `manifest-preflight-*.log` and
`host-producer-r5-exact-*.log` retain commands. Old CLI/Development artifacts and
failed tools stay historical. Final CLI replay must use the new Development
SDK so its statically linked runtime also includes the bootstrap correction.

Evidence is retained under `out/tranche2-windows-20261103/`: producer logs
`producer-build.log`, `source-recovery{,-r2,-r3}.log`, `producer-build-r{2,3}.log`,
the failed directories, `host-tools-r{1,2}`, `host-producer-{configure,build,cached}.log`,
`core-ctest.log`, `utf8-runtime.log`, `focused-tests-final.log`,
`ci-tooling-final.log` and the CLI replay receipts. Original candidate identities
and failed states are preserved. Consumer execution and final D1 closure are
recorded separately in the CLI-owned acceptance ledger.

### Final combined replay (2026-11-03)

The selected combination is the dirty-local Development SDK `dev-rootfs-fix`, tools `host-tools-r5` and the new CLI
(`a9d922ef71fb042a8b2ed7419db7da3d5b078700937b661f14dd63d4c1bb223c`, 24,328,192 bytes), all rechecked by
`out/tranche2-windows-20261103/final_review.py` (`final-result.json`, `passed: true`). CLI product inputs are unchanged
(21 pre-configure inputs, `2aa32f07...0155`), so the Linux/macOS increment and this Windows build compile the same
product sources; only two portable Python fixtures changed in the CLI repository.

| Receipt | Result |
| --- | --- |
| `cli-replay-r3` (restricted token, job accounting, producer trees hidden) | seed + self-build **13 executed/0 reused**, cached **0/13**, **857** native checks, bare help/version/doctor with source/SDK/provider denied; imports `kernel32` and one API-set DLL; 472 process records |
| `host-environment-r4` (independent r5 tools, copied package/CLI) | **32** cases: self/environment doctor, tools workflow, normal and timeout cleanup, child exit/limit, wrong pin/host, tamper and signature rejections |
| `https-r2` (selected package CA, empty PATH, hidden trees) | **32** D1-07 cases, 32 CLI calls, 64 process records (copied CLI and OS conhost only) |
| `selected-project-r4` (explicit copied SDK) | project doctor/environment/shell pass; SDK inventory and project pins unchanged |
| `bootstrap-loop-r2` (previous committed Windows CLI to stage 1 to stage 2) | passes; stage binaries are **not** byte-identical, no reproducibility claim |

Receipt SHA-256: `cli-replay-r3/result.json` `b2cf8577...b7bf`, `host-environment-r4/result.json` `3c17daaa...d5f9`,
`https-r2/cases/result.json` `8d47dd21...c4c`, `selected-project-r4/result.json` `eae3fc38...1e7a0`,
`bootstrap-loop-r2/result.json` `a26c49c2...3918`, `final-result.json` `3135b478...ec2e`. The consumer-owned record is the
[CLI Windows ledger](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#windowsx64-independent-host-environment-replay-2026-11-03). Isolation is Windows accounting (restricted token plus job process records), not
per-executable launch denial.

## Native scope and remaining gates

Linux consumes the independently copied artifact with source/SDK/provider/LLVM
paths hidden and host shells/Python/build drivers masked. Exact native tool,
make/awk/alias, process cleanup, bootstrap-resource and HTTPS results are
[CLI-owned](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md).
No global PATH/security setting, upstream source, runtime ABI or target SDK was
changed. PAL rootfs mapping and environment filtering are not a security sandbox.

Linux, the [macOS replay](#macosarm64-independent-bundle-replay-2026-11-03) and the
[Windows/x64 replay](#windowsx64-independent-bundle-replay-2026-11-03) have their own exact
combinations. The Windows closure covers aliases, Unicode paths, real directory links and owned-child cleanup; the
selected closure has **no private runtime DLLs**, so private-DLL initial-load and collision acceptance, Windows/arm64 and
any production adoption stay open. Earlier SDK tool probes remain historical and do not qualify this independent
artifact. Interactive terminal/job control, source-port/compiler adapters,
cross-target execution, official trust/metadata policy and public release are
separate work; no new production consumer lock is adopted here.

## macOS/arm64 private-dylib closure replay (2026-11-06)

**Pass for the previously unexercised private-dylib scenario, at this exact
local fixture.** CRT baseline `82466be` plus the recorded assembler/test delta;
CLI baseline `1dc3c03` plus the earlier macOS workflow correction, executable
`94ddda0ee179b3e47bf4dbfc91502415ef33209dfa9945706f7cb1116fd9ea8d`.
Native macOS 27.2/arm64, explicit Xcode Clang/ld. Reused clean CRT `784389b`
Dev (`v0.0.1-rehearsal.6`), content
`72d1b37593abb2f5dd9d58a8338f44191b0b2023d6cbf2621cf7cd688ce80b4c`.
No fresh runtime or original SDK change is involved.

### Fixture and producer change

`tools/replay_host_private_macos.py` copies the existing onetrue-awk import,
without source edits, compiles its nine normal C sources and links using that
SDK's pinned native `executable-shared` C policy (`6ffbf556...41d2e7`). Only
the separate test view's AWK and copied aliases change. Packaging normalizes
its RPATH to `@loader_path` and ad-hoc signs that fixture image; upstream code
and the actual SDK/contract pins are untouched. The view's manifest records
its parent SDK and AWK source hashes, and VERSION labels it as a test fixture.
It is not a new Development SDK or an official all-shared host-tools release.

The ordinary assembler copies the actual dependency closure:
`system/bin/libc.dylib`, `libm.dylib` and `libdl.dylib`, with the existing OS
libSystem boundary. It now rejects foreign native images and absolute/unsafe
Mach-O private load names before publication. Copied AWK aliases in `bin` and
`usr/bin` retain their executable bytes and gain relative links to the same
canonical three dylibs. Those six links are covered by inventory format 1.

| Fixture identity | SHA-256 |
| --- | --- |
| Host-tools manifest | `1e71a84593d220cbe09d62932c7d446073a5dc8f5bc7313128142aaa9c2301e8` |
| Payload | `f9db98a574d3eb5ec99092dff3bbe9a6609c5553e837cfbfafb6cd88371213cf` |
| Shared AWK | `7034e5fd9b2ef20199c11fcc2b9398fb51428fdb4e0d7c12d7bc160fd9b7db7c` |
| Accepted 27-case result | `d068c05a558688f64ff82ec19ae46b693a642ba91bbba6e49e9935f637d5fad3` |

### Native evidence

**27/27 pass** in `replay-r3`. Original producer/SDK/contract/source/compiler
paths are unreadable under Seatbelt; native tool execution is explicitly
allowed, host shell/awk/make/Python/compiler fallbacks and network are denied.
The selected bundle is read-only to consumers. The test orchestrator alone
mutates disposable copies for negative cases.

- Two independent space/Hangul relocations preserve the same inventory/pin
  and run actual shared AWK through CLI shell jobs and its physical bundle alias.
- `DYLD_PRINT_LIBRARIES` proves all three CRT dylibs load from the relocated
  bundle. The shared AWK and closure pass the existing binding/allocator audit.
- A valid, host-only poisoned `libdl.dylib` exits 97 and writes a marker when
  deliberately selected by an unfiltered direct launch. With the same hostile
  cwd and DYLD paths supplied to the CLI, its children use the verified bundle,
  print the correct AWK/math results and leave no marker. The trap is never an
  accepted product dependency or host-libc fallback.
- Removing copied-alias runtime links produces an actual dyld missing-library
  failure; restoring the canonical links makes that same alias run again.
- A missing canonical dylib produces a direct dyld failure with original SDK
  access denied. CLI doctor and shell reject missing/changed inventory before
  job creation; restoration verifies and runs. Expected dyld abort diagnostics
  are negative controls, not accepted runtime crashes.
- Original SDK, bundle and imported AWK bytes remain unchanged after restoration.
  The previous static bundle is freshly reassembled from its original C SDK:
  complete inventory/payload remains `6e6984bb...58608e`. Its new manifest differs
  truthfully in assembler identity. **51 focused producer tests pass** (host
  tools, binary dependencies, ELF parsing and runtime notices).

### Retention and limits

Evidence is in ignored `out/host-private-macos-20261106/`: exact invocation,
`replay-r3/{result,fixture,commands,bindings}.json`, per-call output, producer
tests and `static-regression.json`. Work is
`/private/tmp/crt-host-private-20261106/private dylib 한글 wygec906`.
`replay-r1` (18/25) retains unsynchronized fixture alias bytes, ineffective
pre-sandbox DYLD injection and an absolute `/usr/bin/awk` attempt; `replay-r2`
passes 25/25 before the two copied-alias controls are added. No failed result
is relabeled as the final candidate.

Apple's protected sandbox-exec startup strips DYLD variables. A native
**test-host-only** bridge sets those keys after that boundary, then execs the
selected image; the poison constructor proves the control actually operates.
It is not a shipped launcher. The earlier CLI selection/first-build adapters
are strengthened the same way and pass 66/66 and 86/86 again; their old results
remain separate. Existing absolute host paths may pass through PAL rootfs
translation: `/usr/bin/awk` is intentionally denied here, and alias tests use
the selected bundle's physical paths. This is not chroot or a new PAL namespace
policy. Direct manual launches with arbitrary DYLD overrides are not hardened.

The [CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#macosarm64-private-dylib-host-tools-replay-2026-11-06)
owns consumer adoption. This closes the macOS/arm64 private-closure test slice,
not every future bundle/profile, macOS/x86_64, Windows private DLLs, Windows/arm64
or production trust/promotion. No default, root lock, SDK release or STATUS change.
