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

The later [public Windows/aarch64 replay](#windowsaarch64-host-tools-public-signature-and-native-replay-2026-11-19)
adds native producer and D3 evidence for that release; D1's large-output case remains open. It does not qualify Windows private DLLs.

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


## Linux/x86_64 Preview bundle rehearsal from the public Dev SDK (2026-11-16)

Scope: the producer tooling for an independent **public** host-tools Preview and a native Linux/x86_64 rehearsal. It is a rehearsal: the
assembler tree was dirty (the new tooling was uncommitted), so the archive is not a release candidate and nothing is published or pinned.
The contract is [Public Preview releases](../design/host_tools_contract.md#public-preview-releases-adopted-2026-11-16-linuxx86_64-first).

**Input SDK.** The published Dev SDK Preview `v0.0.1-preview.20261110.4` (stage `02-cxx`, content `22107183...ca81`) was downloaded from
GitHub, verified against its release manifest and installed with the published CLI. The unchanged `create_host_tools.py` assembled the bundle from it
(`--promotion preview` is the only change): a separate `01-c` SDK is not required, answering the first publication question.

**Tooling.** `create_host_tools.py` gains a closed `--promotion {local-only,preview}` label; `create_host_tools_release.py pack|verify` builds
the deterministic archive, checksum file and release record and re-verifies an extraction (`test_host_tools.py` and `test_host_tools_release.py`:
21 tests, including determinism, dirty/tag/local-only refusals, a changed archive or record, foreign/escaping/extra members and an existing
extraction directory). The rehearsal archive is 626468 bytes; packing twice gives identical bytes.

**Native replay** (`tools/replay_host_tools_linux.py`, 40/40): the verified bundle copied to a path with spaces and Hangul runs in an empty
bubblewrap root exposing only `/usr` (the OS loader) and the copy, with an empty environment: `sh`, `awk`, `make`, twelve applet smokes and every
declared alias starts. The same smoke passes in an Ubuntu 24.04.5 base (glibc 2.39) root, where a CRT shared-runtime executable failed earlier
(the bundle's tools are static-PIE with the host ELF loader as their only dependency). The CLI accepts the bundle and runs shell/awk/make, and
rejects, before any job, a changed alias, an extra file, a missing alias, an escaping symlink, a lost execute bit, a wrong pin, an unknown
promotion and a wrong host (the last two with their own consistent pin).

**Source port.** With the preview bundle, the CLI's five port suites pass on the retained rehearsal SDK/project: `port-build` 35, `port-store` 90,
`port-oracle` 23, `port-command` 33 and `port-project` 81 checks, none skipped. That SDK is not the public `.4`; it shows the bundle's tools
drive the reviewed zlib recipe, not the public-SDK-to-hosted-CI combination. The CLI needed a one-line compatibility change (accept `preview`,
report the manifest's own label); it is in the CLI repository's working tree, not yet committed or released.

**Linux/aarch64 path.** This machine cannot assemble an aarch64 bundle. `.github/workflows/host-tools.yml` (manual, publishes nothing) runs
`tools/ci_host_tools_release.py` on `ubuntu-26.04` and `ubuntu-26.04-arm`: it downloads the pinned public Dev SDK Preview (x86_64
`v0.0.1-preview.20261110.4`, aarch64 `v0.0.1-preview.20261110.linux.arm64.2`; release-manifest SHA-256 pins in `.github/ci/host-tools-sdk-pins.json`),
checks the archive against its manifest, assembles with `--promotion preview`, packs, verifies the extraction and replays it, then uploads the
candidate and evidence. The driver passed locally on x86_64 against the public `.4` SDK (16/16 replay cases there; the glibc 2.39 and CLI cases above
need `--glibc-root`/`--cli`). Its hosted aarch64 result is recorded separately once run.

**Hosted clean-tree candidates (run [43818243485](https://github.com/webos21/crt/actions/runs/43818243485), commit `5fc1a7d`, 2026-11-16).** Both
legs pass: unit tests, bubblewrap setup, assemble/pack/verify and the 16-case native replay (empty-environment sandbox over `/usr` and a space/Hangul
bundle copy: shell, awk, make, twelve applet smokes, every declared alias). Records say `promotion: preview`, assembler and packer trees clean, and
`publisher_authentication: not_verified`.

| Host | Runner | Source Dev SDK | Archive (size, SHA-256) | Bundle manifest SHA-256 |
| --- | --- | --- | --- | --- |
| Linux/x86_64 | `ubuntu-26.04` | `v0.0.1-preview.20261110.4` (content `22107183e512...`) | 626632, `81174849...2155` | `9bd3ddf1...f192a` |
| Linux/aarch64 | `ubuntu-26.04-arm` | `v0.0.1-preview.20261110.linux.arm64.2` (content `cd2ab657615c...`) | 589644, `7a4c26ad...c75f` | `2ea7bff5...b3c2` |

Versions are `v0.1.0-hosttools.20261116.linux-x86_64.1` and `...linux-aarch64.1`. These are workflow artifacts retained under
`/home/syszone/GitLocal/crt-hosttools-20261116/hosted-r1/` for the maintainer's review; they are candidates, not pins, until a release uploads
exactly these bytes (the workflow publishes nothing, and a re-run at another commit changes the manifest digest). The aarch64 tools ran only on the
hosted runner's own replay; the glibc-2.39 root, the CLI acceptance/negative controls and the five port suites were run for x86_64 only (above).

## Linux host-tools Preview publication (2026-11-16)

Both candidates above were published as prereleases of `webos21/crt`, tag target `5fc1a7d639f27a5b8b38f81035b571a675c1e51a`
(the candidate commit; later documentation commits do not move the tags): draft upload, byte-for-byte comparison of the re-downloaded draft assets
with the workflow artifacts (all six identical), then publication at 2026-11-16T06:43Z.

| Release | Archive size | Archive SHA-256 | Bundle manifest SHA-256 |
| --- | --- | --- | --- |
| [`v0.1.0-hosttools.20261116.linux-x86_64.1`](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261116.linux-x86_64.1) | 626632 | `811748496ed027339916609fceaee1a78c806d9dbdbb3aaa8b3a035095972155` | `9bd3ddf18176c5a875e137846edc4f7e62dc2f178e1f1afcc97533a4cc5f192a` |
| [`v0.1.0-hosttools.20261116.linux-aarch64.1`](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261116.linux-aarch64.1) | 589644 | `7a4c26ada280e8a877d05d3adab9c76df2b6dc181a96cac42e8977030019c75f` | `2ea7bff56cd784dd2dd5b6d5a858b59bf41a1585bf3f77e8b09e79c8b4b7b3c2` |

Each release carries the archive, `SHA256SUMS-host-tools-linux-<arch>` and `host-tools-release-record-linux-<arch>.json`; the record's
`consumer_pin` holds the six values a consumer pins (`url`, `name`, `size`, `sha256`, `manifest_sha256`, `version`).

**Public download check (unauthenticated, redirect followed).** Both archives match the checksum file, the record's size/digest and the workflow
candidate. The published **x86_64** archive was then verified with `create_host_tools_release.py verify` and replayed natively: **40/40**, covering
the empty-environment sandbox, an Ubuntu 24.04.5 (glibc 2.39) root and the CLI's acceptance plus its eight fail-closed controls (changed alias, extra
file, missing alias, escaping symlink, lost execute bit, wrong pin, unknown promotion, wrong host). The published **aarch64** archive was checked for
digest/size and was executed only by the hosted runner's replay (16/16 on the workflow artifact); no aarch64 glibc-root, CLI-control or port-suite run
exists yet. Publisher authentication stays `not_verified`: trust is the pinned digests.

**Not done.** The CLI pins and hosted integration (five port suites on both Linux runners), aarch64 CLI/port-suite evidence, and macOS/Windows bundles.

## macOS/Windows host-tools publication, signatures and macOS replay (2026-11-19)

Three independent host-tools Previews are public. The Windows candidates came from the successful Windows jobs in
[run 44427905842](https://github.com/webos21/crt/actions/runs/44427905842) at `56fc914ff4029a5ef0db8d7050418725a9dafc4b`;
that run's macOS job failed on an unbalanced Seatbelt profile. The corrected macOS candidate came from
[run 44428128691](https://github.com/webos21/crt/actions/runs/44428128691) at `19b72f340c8160342d793a65aeca7af5e3fefb8d`.
The release records retain each candidate's clean assembler/packer identity, source SDK, manifest pin and notices.

| Public release | Archive bytes | Archive SHA-256 | Detached signature SHA-256 |
| --- | ---: | --- | --- |
| [macOS/aarch64](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261119.macos-aarch64.1) | 583176 | `90c7585a92fc020e05ea69e7bcd752a4df67f060706c87f81822cb11b0c676a1` | `15c2af47b2de52ccf876a0d1c335ba187a92ff92de8b14c2fb52521b0ac3d451` |
| [Windows/x86_64](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261119.windows-x86_64.1) | 784724 | `bd65a2b8d1a5b7865110233bb47a1d13ea56724ad76593333cc349bb0980a210` | `72f522291148d0e5fae696d2783eb32153b4513c8c45498d557ed60326896695` |
| [Windows/aarch64](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261119.windows-aarch64.1) | 697220 | `c74ad50ea5f5008d515e1aac3581fa5d73d7acba276ae990a9d3df84599706ee` | `2956f38c0621feae16a0c6f02ca8346cc77423c33f8adcc108d92f7d8d46443f` |

### Hosted producer publication

Issue [#9](https://github.com/webos21/crt/issues/9) requested macOS/arm64 and
Windows/x64 bundles; the producer also supplied the first independent
Windows/arm64 bundle. [PR #10](https://github.com/webos21/crt/pull/10), merged as
`26c9579`, added the workflow's `hosts` selector and `macos-15`, `windows-2022`
and `windows-11-arm` runners. Each uses its pinned public Dev SDK from
`.github/ci/host-tools-sdk-pins.json`; Windows inputs are ZIP archives, macOS
inputs are tar.xz. The driver checks archive members before extraction.

The successful jobs in the runs above assemble with `--promotion preview`,
pack deterministically, safely extract and verify the bundle, then pass native
producer replay **18/18** on each of the three hosts. Windows execution uses
a relocated copy and clean environment; it does not claim network or filesystem
denial. Neither Windows bundle contains private runtime DLLs; both import only
`KERNEL32.dll` and `api-ms-win-core-synch-l1-2-0.dll`.

After comparison of draft uploads with the workflow artifacts, all three were
published as prereleases, not latest releases. Unauthenticated public downloads
match the workflow artifacts, checksum files and release records. Each record's
`consumer_pin` retains the six archive/manifest identity fields. In particular,
the Windows/aarch64 bundle manifest SHA-256 is
`6f7225ac4ffafe4c5651bf025b47d7b58df0939c520dbf40b7a029aa2892b6f1`, from
Dev SDK `v0.0.1-preview.20261118.windows.arm64.1`. This is hosted producer
evidence; Windows/arm64 CLI controls, port suites and D3 zlib qualification
were not exercised by that producer run. Subsequent archive signing and native consumer replays are
recorded separately below.

### Publisher authentication from public bytes

The owner supplied three `<archive>.sig` files (256 bytes each). Each signature verified against the corresponding public archive with the
[pinned publisher key](../security/publisher_signing.md), SPKI SHA-256
`59218576956e493476bc36f7385d8e01a03ff9b92946a531b013ad3937d43fa2`, RSA-PSS SHA-256/MGF1 SHA-256/salt 32.
Archive size/SHA-256 also match each published release record. The signatures were attached to their respective releases as new assets;
all three were downloaded back, compared byte-for-byte with the owner-supplied files and verified again (`Verified OK` for each).
No existing archive, checksum file or release record was replaced. The release notes were updated to remove the now-obsolete unsigned-archive claim.

The packaging record's original `trust.publisher_authentication: not_verified` is preserved: it records the pre-signing producer step.
Only consumers that verify the detached signature may report `signature_verified`. The JSON records and checksum files have no detached
signature of their own; the archive signature does not authenticate every asset in the release.

### Native macOS/arm64 public-bundle replay

Host: macOS 27.2.1 (26B131), arm64; verifier checkout `26c9579`. Input: the downloaded public macOS archive above, not a rebuilt candidate.
Source SDK provenance is `v0.0.1-preview.20261110.macos.1`; bundle manifest SHA-256
`27fac5efbb6f846041b471d8b0be8e8004608ef5839ce4a2ef23c62aae8e07b8`, content SHA-256
`d692809a7b1c244de2b42bd6756f5f4a2c3a3f035373cf82874a0faffb2cc589` (120 inventory files, 90 executable paths/aliases, 77 applets).

`create_host_tools_release.py verify` passes the archive/checksum/record agreement, safe extraction, complete inventory, content and manifest
identity, and native executable checks. `replay_host_tools_macos.py` passes **18/18** on a separate space/Hangul-named copy under Seatbelt:
network denied, writes confined to scratch and `/dev`, `PATH=/nonexistent`, with no inherited environment apart from explicit PATH/HOME/TMPDIR.
The cases exercise shell output/exit status, awk, GNU make, printf/echo/expr/basename/uname, redirection/cat/sort/wc/ls and every declared alias.
`otool -L` on the four canonical tools (`mksh`, `toybox`, `awk`, `make`) shows only `/usr/lib/libSystem.B.dylib`; this closure has no private dylibs.
The four focused producer modules pass **35/35** tests.

Reproduce with new extraction/output directories (the tools intentionally reject existing destinations):

```sh
python3 tools/create_host_tools_release.py verify \
  --archive <public-archive.tar.xz> --record <public-release-record.json> --extract-to <new-extraction>
python3 tools/replay_host_tools_macos.py \
  --bundle <new-extraction>/crt-host-tools-macos-aarch64 --output <new-evidence>
python3 -m unittest tools.test_host_tools tools.test_host_tools_release tools.test_ci_host_tools_release tools.test_host_tools_replay_common
```

Evidence is retained in ignored `out/host-tools-public-replay-20261119/`: per-host public downloads and signature-verification output,
`macos-aarch64/replay-r2/{host-tools-replay.json,policy.sb}`, the replay log and `native-dependencies.log`.
`replay-r1` is retained as a failed environment attempt: nested `sandbox-exec` returned `sandbox_apply: Operation not permitted` inside the
agent's sandbox (5/18, not accepted). The identical public bundle passes in `replay-r2`, run on the host with Seatbelt permitted; neither
the payload nor the replay policy was weakened. Two Windows extraction attempts stopped at the verifier's intentional native-host check
(`unsupported host-tools manifest/host`); this macOS run claims Windows archive size/digest and signature verification only, not Windows execution.

This producer replay does not hide source/SDK paths from reads, exercise CLI selection or its fail-closed controls, or run the CLI's five port
suites. Those remain consumer-owned acceptance. Hosted Windows replay is prior producer evidence, not a new Windows replay on this Mac.
No private-DLL search-path qualification, Developer ID/notarization, consumer pin promotion or STATUS update is claimed.

### CLI D3 consumer follow-up (2026-11-19)

After the producer replay above, current CLI source at `c79d57c4c049ec651076fd648ad0e7b3630a638f` was freshly built and tested with this
signed public macOS bundle, public SDK `v0.0.1-preview.20261110.macos.1` and signed bootstrap CLI `v0.1.0-preview.20261117.macos-arm64.1`.
The six input-enabled D3 suites, static/shared consumers and oracle comparison pass, including actual port publication/reuse/verification
and source-hidden execution. [CLI acceptance](https://github.com/webos21/crt-cli/blob/144581752cc492f0906fb9e422d6e91663bccb0a/docs/acceptance/sdk_install_acceptance.md#signed-public-host-tools-d3-replay-macosarm64-2026-11-19)
owns the exact inputs, 442 checks, 27/27 replay gates, nine-image binding audit and receipt hashes; the CLI compatibility ledger owns adoption
scope. Raw evidence is retained in ignored `out/d3-hosttools-macos-20261119/`. No SDK, host-tools or product bytes were changed, and the original
producer-only result above is not retroactively expanded. Hosted integration and CI pin adoption remain open; this follow-up claims no new Windows run.

## Windows/x86_64 host-tools public signature and native replay (2026-11-19)

Native Windows 11 Pro (10.0.26100) x64 verification of the public
[Windows/x86_64 host-tools Preview](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261119.windows-x86_64.1) (published
`2026-11-19T00:35:11Z`). Inputs are the unauthenticated public downloads, not a rebuilt candidate: Python `urllib`, HTTPS only, TLS >= 1.2 with peer and hostname
verification against the Windows certificate store, proxies disabled, no credentials, exactly one redirect per file and only to `release-assets.githubusercontent.com`
(temporary signed URLs are not recorded). Verifier checkouts: CRT `84eec172`, `crt-cli` `c79d57c`.

| Public asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.1.0-hosttools.20261119.windows-x86_64.1.tar.xz` | 784724 | `bd65a2b8d1a5b7865110233bb47a1d13ea56724ad76593333cc349bb0980a210` |
| `crt-v0.1.0-hosttools.20261119.windows-x86_64.1.tar.xz.sig` | 256 | `72f522291148d0e5fae696d2783eb32153b4513c8c45498d557ed60326896695` |
| `host-tools-release-record-windows-x86_64.json` | 3446 | `5f7b05861e9d0c2e2d7808a955224317d6a5c054f3123f7005546609bd6b4841` |
| `SHA256SUMS-host-tools-windows-x86_64` | 121 | `f7aaceb3305b2d2d4c32b240178957ba4029ae54328fdd2d16ad36316c24cf9b` |

All four equal the sizes and digests on the release page; the archive equals the `SHA256SUMS` line and the record's `consumer_pin`.

### Publisher signature (two independent verifiers)

1. **openssl 3 (Strawberry Perl build), 9/9.** The repository key file has SPKI SHA-256 `59218576956e493476bc36f7385d8e01a03ff9b92946a531b013ad3937d43fa2` (RSA 2048, the
   [published fingerprint](../security/publisher_signing.md)); the signed fingerprint notice `fingerprint-notice-2026-11-16.txt` verifies with that key and names the same fingerprint;
   the detached signature (256 bytes) verifies over the public archive (`Verified OK`, RSA-PSS SHA-256, MGF1 SHA-256, salt 32). Negative controls refuse, each for its own reason: the same
   signature over a one-bit-changed archive, a one-bit-changed signature, a different temporary RSA key, and a PKCS#1 v1.5 signature by another key.
2. **The published `crt-cli` itself, 8/8.** The executable of the public
   [`crt-cli` Preview 20261117 (Windows x86_64)](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261117.windows-x86_64.1) (archive
   `02396e04cd4775cb7cfd65bd5c0e2c8975c4fa5b4ad18cfa039f65bf5cdd706f`, itself signature-verified with the same key; executable `feb9779109892cf73235307617398e8dfe70bdadcc86d3dff22ba04642c84df1`) runs
   `fetch --url <public URL> --ca-file <57 roots exported from the Windows store> --sha256 <pin> --size 784724 --redirect-policy github-release --publisher-key <pem> --signature <sig>`:
   the real public URL is fetched, the integrity is `pinned-digest`, the reply reports the publisher algorithm `rsa-pss-sha256` and `signature_verified`, and the output equals the separately
   downloaded archive. A swapped signature (the Windows/aarch64 archive's) and a one-bit-changed signature fail as `publisher_signature_invalid` (stage `authenticate`, `published: false`);
   a wrong archive digest fails as `digest_mismatch`; none of the three leaves an output file.

Limit unchanged from the [signing policy](../security/publisher_signing.md): the key file, the notice and the releases share one GitHub account, so the check is as strong as the fingerprint copies
recorded independently (the project blog post), which this run did not re-fetch. The record and checksum file are unsigned. Keep the digest pins. This is not Authenticode.

### Native behavior on this host

`create_host_tools_release.py verify` (archive/record agreement, safe extraction, complete inventory, content and manifest identity, native image checks) passes; manifest SHA-256
`cd6cb75e91b8f9202cda82876de9898e19fb4aeb01be981ebcfde9eabb7b7e00`, content `dc106db47ed8bcbb56bdd6da0415dce527653fcb42b0c54bdc1402dca74c35fe` (209 inventory files, 179 executables and aliases, 77 applets), source SDK
`v0.0.1-preview.20261110.windows.1` (content `5bb55372ec10e3cfa24c84de981a7130f56aeb57ef0d43d5da22a65036dec7ef`). `replay_host_tools_windows.py` passes **18/18** from a space/Hangul-named copy with no `PATH` and only the system root
(shell status, awk, GNU make, printf/echo/expr/basename/uname, redirection/cat/sort/wc/ls, every declared executable and alias started without a loader failure, signal or fatal status). All **90** PE images import only `KERNEL32.dll` and
`api-ms-win-core-synch-l1-2-0.dll` (no private runtime DLL) and none has a certificate table (no Authenticode). The four producer modules pass 35 tests (1 skipped).

```text
python tools/create_host_tools_release.py verify --archive <archive> --record <record> --extract-to <new-dir>
python tools/replay_host_tools_windows.py --bundle <new-dir>/crt-host-tools-windows-x86_64 --output <new-evidence-dir>
python -m unittest tools.test_host_tools tools.test_host_tools_release tools.test_ci_host_tools_release tools.test_host_tools_replay_common
```

The consumer-owned replays (fail-closed controls and the five port suites on this exact artifact) are recorded by the
[CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#windowsx64-published-host-tools-consumer-replay-2026-11-19).
Evidence: `C:\Users\Lee\s12\` (downloads and receipts, `signature-openssl.json`, `cli-fetch-signature.json`, `ht-replay/`), outside both repositories.

Not claimed: Windows/arm64 (a separate release), network or file-system denial (Windows has no unprivileged sandbox here; isolation is the empty environment, the relocated copy and, for the consumer replays, a
restricted token with process accounting), a job rootfs without Developer Mode, private-DLL search-path qualification (this bundle ships none), consumer pin promotion or STATUS.

## Windows/aarch64 host-tools public signature and native replay (2026-11-19)

Host: native Windows/ARM64, OS build `10.0.26300`; verifier CRT `0d9d573b93a7c9edb00af18f2d1351d9e774b6a8`, CLI
`2460919048c5f95211721c8d4ea4454098ffd54f`. Input is the public
[Windows/aarch64 Preview](https://github.com/webos21/crt/releases/tag/v0.1.0-hosttools.20261119.windows-aarch64.1), not a rebuilt candidate.
The retained unauthenticated HTTPS downloads record one GitHub asset redirect per file and these identities:

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.1.0-hosttools.20261119.windows-aarch64.1.tar.xz` | 697220 | `c74ad50ea5f5008d515e1aac3581fa5d73d7acba276ae990a9d3df84599706ee` |
| Archive `.sig` | 256 | `2956f38c0621feae16a0c6f02ca8346cc77423c33f8adcc108d92f7d8d46443f` |
| `host-tools-release-record-windows-aarch64.json` | 3468 | `1a4b179ca771d0c21057545cc04d99f482ee4dd9290f7bbd0a3bb97bcbbf90c8` |
| `SHA256SUMS-host-tools-windows-aarch64` | 122 | `40b91be426fd1e8759a4775fad721181fddeabdbfb7eb23cf87bd696049faa05` |

### Publisher authentication

The retained OpenSSL transcript passes **9/9**: the public key's SPKI SHA-256 equals
`59218576956e493476bc36f7385d8e01a03ff9b92946a531b013ad3937d43fa2`, the signed fingerprint notice and public archive verify,
and the changed archive/signature and other-key PSS/PKCS#1 v1.5 signatures are refused, with positive controls for the temporary key.
Parameters are RSA-PSS/SHA-256, MGF1 SHA-256, salt 32; the archive signature is 256 bytes.

The published native [ARM64 CLI Preview](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261118.windows-arm64.1) independently
passes **4/4 fetch cases** against the real public archive URL. `fetch --publisher-key --signature --redirect-policy github-release`
reports `signature_verified`; its output matches the archive pin. The x64 archive's signature and a one-bit-changed ARM64 signature fail with
`publisher_signature_invalid`; a wrong digest fails with `digest_mismatch`. All three refusals leave no output file.
CLI archive `c5ff710532c0ff9191ae373b48a8f8fe5ceb04bea70647989f9146e255e46e11` and signature
`b72eb649d476673d784a6f4f84c090f73da9df505372cfc9005d19fa2a2da5e6` match its published pins; OpenSSL verifies that signature with the same key.
CLI executable SHA-256 is `132c8b5ae15f66ed9dd82fcb5a425160e4d682c008ab218353b7eed30f6d3db8`.
The explicit Windows-store CA file is `3cb88031f066192761b84355b7a921d18592c1874146aab63ea355ca8fffd0a8`.

The [key-independence limit](../security/publisher_signing.md) remains: key, signed notice and release share a GitHub account;
the blog fingerprint copy was not fetched again. The release record and checksum file remain unsigned; this is not Authenticode.

### Producer behavior and consumer boundary

`create_host_tools_release.py verify` passes archive/record/inventory/content/native-image validation. Manifest pin:
`6f7225ac4ffafe4c5651bf025b47d7b58df0939c520dbf40b7a029aa2892b6f1`; content:
`ffc6c139cae055422113235ac0f3c3189007046bd03cb782a597399dd3368bad`. The source SDK is
`v0.0.1-preview.20261118.windows.arm64.1`, clean CRT `e7c763094d96ccba1b524077b24b04b876a91d5b`.
The space/Hangul-path producer replay passes **18/18** with empty PATH. A separate PE scan covers **179 file paths including aliases**
(four distinct image byte hashes): every path is ARM64 (`0xaa64`), imports only `KERNEL32.dll` and
`api-ms-win-core-synch-l1-2-0.dll`, and has an empty certificate table. The four focused producer modules run **35 tests, 1 skipped**.

The D1 harness now checks the actual native host and chooses the other Windows architecture for its wrong-host case, replacing two x64 constants.
It preserves a timed-out case as failed and continues collecting the remaining cases; its final result still fails if any case fails.
**D1 is 31/32, not accepted as a full pass:** the 10 MiB `dd` stdout case exceeds the harness's 90-second outer limit. A focused run with
a 900-second outer budget completes in 91.172 seconds, but the unchanged 60-second inner limit returns 124/`timed_out: true`;
the captured output is correctly capped at 8 MiB with `output_truncated: true`. Direct `dd` (both alias spellings) and the CLI's small
65,536-byte case succeed. The CLI intentionally relays captured stdout to stderr; those NUL bytes are not evidence of misrouting.
No runtime fix, longer accepted inner timeout or complete D1 qualification is claimed.
Consumer combination results and the retained contract replay belong to
[CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#windowsarm64-published-host-tools-consumer-replay-2026-11-19).

Evidence: ignored `out/host-tools-public-replay-20261119-winarm64/` (downloads, `signature-openssl.log`, `verify.log`, `ht-replay/`,
`pe-audit.json`, `cli-fetch-signature.json`, replay scripts and consumer logs). Windows isolation is a restricted token, selected inaccessible
input paths, an empty environment and job-object process accounting; executable/network denial is not available here. This bundle has no private
DLL, so private-DLL search-path qualification remains open. No CI/default pin, project lock, SDK bytes, public asset or STATUS was changed.

## Producer request #9 closure and remaining handoff (2026-11-19)

Issue [#9](https://github.com/webos21/crt/issues/9) is **closed as completed** for its producer publication scope
([completion comment](https://github.com/webos21/crt/issues/9#issuecomment-6998966548)). The requested macOS/arm64 and Windows/x64 bundles
are public signed Previews; archive, detached signature, checksum and release record supply the six consumer-pin fields, clean provenance
and `promotion: preview`. Producer questions on closure/dependencies, runners, Windows symlinks, offline signing and independent per-host
adoption are answered in the issue. The Windows/arm64 publication is additional coverage, not a new completion condition for #9.

The dated records above retain the producer replays and subsequent native public-byte/consumer results. macOS D3, Windows/x64 D1/D3 and
Windows/arm64 D3 pass for their exact combinations; **Windows/arm64 D1 is still 31/32 and not fully qualified**. Closing #9 does not assert
otherwise. CLI owns macOS/Windows integration-pin adoption and hosted jobs (including the Windows job-rootfs symlink prerequisite);
the ARM64 D1 timeout and Windows private-DLL collision gate remain open in the [CRT queue](../../TODO.md#remaining-host-qualification)
and [CLI queue](https://github.com/webos21/crt-cli/blob/main/TODO.md#ci-platform-expansion).

This is a scope/status reconciliation of existing evidence, not a runtime change, new native replay, CI/default-pin promotion, SDK/lock
change or STATUS refresh. The [CLI request record](https://github.com/webos21/crt-cli/blob/2e4c4d1e8fa7df25bdebe1358587b469fce7752e/docs/crt_host_tools_publication_request.md)
owns the consumer-facing producer handoff; detailed consumer acceptance and compatibility remain CLI-owned.

## Consumer hosted integration handoff completed (2026-11-19)

The CLI-owned macOS/Windows integration handoff above is complete for the public signed bundles: integration pins and hosted Port/oracle
jobs are adopted on consumer main `f06a036c01c1e71d14df87c1f082b1df816c33c6`, with completion documented at
`858c04bda6f29968d91be98cc80c8ca6a9f55936`. The owning [CLI acceptance record](https://github.com/webos21/crt-cli/blob/858c04bda6f29968d91be98cc80c8ca6a9f55936/docs/acceptance/ci_acceptance.md#windowsx64-hosted-port-integration-2026-11-19)
records the exact public inputs, native boundaries, failed fixture attempt and successful artifact checks.

[Integration 44490048419](https://github.com/webos21/crt-cli/actions/runs/44490048419) passes all 11 jobs at consumer
`9a417fc52073b977dc8e36d7c8c978cd09dbb6f9`, including all five required port suites on Linux/x64, Linux/arm64, macOS/arm64 and Windows/x64.
Windows hosted qualification includes actual CRT symlink creation, private original inputs, separate-bare-SID execution, static/shared
consumers, PE imports and unchanged inventories. [Main CI 44493533992](https://github.com/webos21/crt-cli/actions/runs/44493533992)
passes all six required native rows and `CI gate`; the delta from the integration source to that main revision is documentation only.
Detailed consumer measurements remain CLI-owned rather than being duplicated here.

This closes the stale producer TODO handoff using observed hosted evidence, not merely the earlier native/D3 receipts. The original #9
publication record retains its producer-only scope. First scheduled-run observation and further redirect automation remain consumer-owned;
Windows/arm64 D1 (31/32), private-DLL qualification and Full SDKs stay separate. No producer code, runtime, SDK/host-tools bytes, release,
consumer pin, root/project lock or STATUS changes, and no runtime test is rerun for this documentation synchronization.
