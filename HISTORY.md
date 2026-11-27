# HISTORY: CRT Resolved Work Log

Reverse-chronological record of completed work on the CRT shell/rootfs/porting
loop and PAL, one dated entry per resolved item (grouped by the commit(s) that
landed it). Current/open work lives in `TODO.md`. This file retains November
2026; [October](docs/history/HISTORY-2026-10.md) and earlier months are in the
[monthly archive](docs/history/README.md), with pinned Git sources and recovery
instructions. Dates are the git author date of the commit that introduced or last
substantively updated each entry, so an entry whose investigation spanned
multiple days is dated by its span (`start..resolved`) or by its last
substantive update.

## 2026-11-27

### Deliver raise(), self-kill() and abort() as real signals

`raise()` emulated delivery: `SIGHUP`, `SIGQUIT` and even `SIGKILL` returned, and `SIGINT`/`SIGTERM`
exited 134. As in Bionic, it now sends a signal to the calling thread on Linux and macOS, so parents see
`WIFSIGNALED`; `abort()` follows Bionic's sequence; blocked signals stay pending; Windows ends the
process with the tagged exit code its `waitpid()` already decodes. macOS `kill()` and `waitpid()` now
translate signal numbers (`kill(pid, SIGUSR1)` had sent Darwin `SIGBUS`). `process_signal_test` had
expected a blocked signal to be dropped and was corrected. Design, host differences and verification
limits (Windows not run): [signal delivery](docs/design/signal_delivery.md#sending-a-signal-and-default-actions-2026-11-27).

This changes SDK runtime behaviour and takes effect in an SDK built from it, not in v0.5.0-rc.2.
`crt-cli` was not changed; its `_exit(128+signal)` workaround for canceled runs can be revisited
against such an SDK. A separate crash found here, `pthread_create()` in a forked macOS child, is the next
entry.

### macOS: threads in a forked child crashed with SIGSEGV

A child of `fork()` that did not exec died with SIGSEGV (wait status `0xb`) in
`pthread_create()`, with single- and multithreaded parents alike. macOS threads
are Apple pthreads, but `fork()` was a bare system call, so libSystem's atfork
child handlers never ran. The crash report showed `_pthread_create` ->
`mach_vm_allocate` followed by `mig_get_reply_port` and
`_kernelrpc_mach_port_construct` recursing on the parent's cached port state
into the stack guard.

Decision: `fork()` (and so `vfork()`) now calls `libsystem_c`'s own `fork()`,
found by Mach-O symbol lookup like the `pthread_*` entry points
(`libc/src/arch/macos/common/fork.c`). CRT `pthread_atfork()` handlers are a
separate list and still run once; handlers host libraries registered with
Apple's `pthread_atfork()` now run too. This follows Bionic, where `fork()` is
the atfork handlers around `_Fork()` and the child's libc stays usable.

Limit: `_Fork()` remains the raw system call (no public libSystem entry
reinitialises without running Apple's `pthread_atfork()` handlers), so unlike
Bionic its macOS child must not create threads. The internal `posix_spawn()`
child, which only execs, also keeps the raw call.

Evidence (macOS/arm64): `libc/tests/fork_thread_test.c` fails with status `0xb`
on the old `fork()`; with the fix it passed 200 `ctest --repeat` runs and 800
runs across eight parallel loops. macOS/x86_64 was not run.

## 2026-11-26

### Record the 0.5.0 Dev SDK candidate replay gates

Close D5-01 for the recorded Dev SDK RCs: relocated external C/C++
consumers with the producer hidden, the supported static/shared modes and
native dependency checks have evidence on all five host pairs. The same
records cover independent SDK package verification and the initial ABI
comparisons with the signed host Previews:
[Linux/x86_64](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26),
[Linux/aarch64](docs/acceptance/release_qualification.md#linuxaarch64-dev-sdk-050-rc1-2026-11-26),
[macOS/arm64 RC1](docs/acceptance/release_qualification.md#macosarm64-dev-sdk-050-rc1-2026-11-26),
[Windows/x64](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26) and
[Windows/arm64](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26).
[macOS/arm64 RC2](docs/acceptance/release_qualification.md#macos-150-deployment-target-2026-11-26)
repeats the SDK, consumer and Mach-O checks. Signed Host Tools and zlib
passed with all five RC1s; they were not replayed with macOS RC2. Final
artifact ABI and distribution verification, publication and exact pin
handoff remain open. This is a review of existing results, not a new test
run, Full SDK qualification or general application-packaging acceptance.

### Qualify the local Windows/arm64 Dev SDK 0.5.0 RC1

Produced and independently verified the native Release candidate from clean
CRT `e1e379c`. The PE comparison with the signed 2026-11-18 Preview finds six
additive static libc++ definitions and no removals. Core CTest has 169 passes,
one skip and zero failures. The relocated static and shared C++ consumer
passes with both source checkouts hidden. The retained signed Host Tools
bundle passes the native replay 18/18. The candidate CLI and bundle pass D1
32/32, all six D3 suites and a real zlib port build with static and shared
round trips. The native VS Code source-extension workflow passes with the
same candidate SDK and CLI.
[Candidate inputs, verification and limits](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26).

### Set macOS 15.0 as the supported minimum

SDK images and programs built with the SDK now declare macOS 15.0, the oldest
macOS the project's CI uses, instead of the build host's version. The release
collector enforces it and the SDK workflows run the assets on `macos-15`.
Against RC1 only the declared minimum changes. A hosted run built the SDK on
macOS 26.7.2 and, on macOS 15.8.2, verified the downloaded assets and passed
the source-hidden C++ consumer and the published CLI's C/C++ smoke. This
closes D7-03.
[Local replay](docs/acceptance/release_qualification.md#macos-150-deployment-target-2026-11-26) ·
[hosted macOS 15 execution and limits](docs/acceptance/release_qualification.md#hosted-macos-15-execution-2026-11-26).

### Prepare the macOS/arm64 Dev SDK 0.5.0 candidate

Built local `v0.5.0-rc.1` on macOS from the same clean CRT `dc092e49` as the
Linux candidates. Against the signed 2026-11-10 macOS Preview, 118 Mach-O
images and archives show one executable export added (`_memchr` in `awk`) and
no library change; a new Mach-O comparison tool captures that surface. The
hidden-producer consumer, C provider (76/76), editor compile policy, signed
Host Tools (18/18), zlib D3 (28/28), a CLI built from the candidate and its
local package pass. Both macOS builds declare `minos 27.2` because no deployment
target is set; no older host was exercised with this candidate. The VS Code Extension Development Host replay with these
candidates passes 54/54 on macOS. The editor policy tool gained a macOS path,
and test drivers and fixtures were corrected.
[Inputs, evidence and remaining release gates](docs/acceptance/release_qualification.md#macosarm64-dev-sdk-050-rc1-2026-11-26).

### Prepare the first Windows/x64 Dev SDK 0.5.0 candidate

Build local `v0.5.0-rc.1` from clean CRT `8689b4c8` (CTest 170/170, in-build C provider 4/4, independent verification OK). New `compare_dev_sdk_pe_abi.py` compares it with the signature-verified 20261110 Preview: 255 images and archives on both sides, nothing removed, only six static libc++ definitions added (the PAL-05 wide-path streams).
The restricted-token Dev consumer, C 36/36 and C++17 22/22 (direct `std::ofstream(path)`) pass; the editor-policy replay and its fixture now run natively on Windows (cleared environment, 8 compile commands). The SDK alone builds CLI `2644716` (cold 22/0, warm 0/22), its local package passes 12/12 clean-token checks, and the signed Windows Host Tools (18/18) pass the six D3 suites and a real zlib port build.
Signing, publication, pin adoption, R05-04 naming, Windows/arm64 and the VSIX stay open; the same candidates pass the [VS Code replay from the extension's source checkout](https://github.com/webos21/crt-vscode/blob/main/HISTORY.md#replay-the-editor-workflow-on-windowsx64-v1-01). [Evidence](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26).

### Prepare the first Linux/x86_64 Dev SDK 0.5.0 candidate

Build local `v0.5.0-rc.1` from clean CRT `dc092e49`. The extracted SDK
passes independent verification, producer-hidden C/C++ runs and the C
provider matrix. Compare 122 ELF files and static archives against the signed
November 25 SDK. Public symbol, import and runtime metadata and 17 aliases
match. Record 21 binary byte changes separately. The signed Host Tools
package passes native verification with the retained CLI and the
zlib source-port replay with CLI `8942a35` built against RC1.
[Inputs, evidence and remaining release gates](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26).

## 2026-11-25

### Validate the installed SDK compile policy for the first editor workflow (P1-01)

The signed November 25 Linux/x86_64 Dev SDK and CLI Preview pass the
source-hidden C/C++ fixture. The real CLI emits eight compile argv across
four modes, all matching the portable SDK policy and resolving the expected
installed headers. The two executables run and the warm build reuses every
step. An unsupported runtime mode and a stale contract lock fail without
changing project state. The CLI can build its editor APIs on these inputs
with the existing SDK format.
[Evidence and replay](docs/acceptance/sdk_split_acceptance.md#linuxx86_64-sdk-compile-policy-for-the-first-editor-workflow-2026-11-25).

### Qualify the retained ARM64 Dev SDK on a physical Raspberry Pi (P1-01)

The signed November 10 SDK and November 16 CLI pass native C/C++ workflows
on Raspberry Pi 3 B+, Debian 13.7, glibc 2.41 and LLVM 19.1.7. Fresh static
and shared C++ exception probes pass; ELF and mapped-image audits confirm
CRT libraries with only the declared OS loader. The CLI consumer owns the
raw replay. The bounded compatibility task leaves TODO; untested Pi/OS,
Full SDK and newer source combinations remain outside this qualification.
[Producer handoff and exact bounds](docs/acceptance/release_qualification.md#raspberry-pi-arm64-native-compatibility-2026-11-25).

### Close sign-before-publishing for the Dev SDK and CLI (D6)

Both repositories restrict the `release` environment to `main` with required
reviewers. A dry-run draft could not supply a finalization receipt in either
repository. A real but unsigned draft likewise failed after approval, leaving
it non-public. After the owner signed every file offline, the separately
approved finalizers published the Linux/x86_64 SDK and CLI as non-latest
prereleases. Fresh unauthenticated downloads matched the staged payloads and
all eight signatures verified. No consumer pins changed; manual Host Tools
publication is outside the automated gate.
[Run IDs, digests and scope](docs/acceptance/release_qualification.md#sign-before-publishing-qualification-2026-11-25).

## 2026-11-24

### Implement separate offline-signature finalization for SDK and CLI previews

Split the release path into protected draft staging and a separate approved
finalizer. A non-rehearsal stage run records release ID, tag/commit and exact
asset IDs and digests; rehearsal drafts get no receipt. The finalizer reuses
that receipt and the pinned RSA-PSS publisher key, requires a detached
signature for every payload, and checks the downloaded bytes before and
after the publication call. The archive-signature consumer format is unchanged.
The artifact-only host-tools workflow stays unchanged; its manual publication
uses the same offline policy. Local tests use fake GitHub state and temporary
keys; no hosted rehearsal, production signature or publication was performed.
[Owner procedure](docs/security/publisher_signing.md#signing-procedure-owner).

### Consume the Windows/arm64 closure packages with the shared CRT (PAL-04)

The minimum, FFmpeg-less and Skia-less stage-05 packages (and the full one as a control), relocated to a `SDK space 한글` path, build all their shipped examples with both `static` and `shared` CRT/C++ linkage (32 builds, no CRT change) and run them with only the package `bin` on `PATH`: every example exits 0 with its own check in both modes. The PE import tables show the shared builds importing `libc.dll` (`gfx-skia` also `libc++.dll`) and the static ones importing neither, with no import outside the package and System32.
A "no `bin` on `PATH`" negative control was wrong (the static UI example also needs the SDK DLLs) and a first module sample was empty; both are recorded as non-evidence. Loaded-module lists match the package byte for byte but cannot tell the modes apart, since the SDK's own DLLs import `libc.dll`. [Evidence](docs/acceptance/crtui_acceptance.md#windowsarm64-packaging-and-closure-replay-2026-11-24).

### Complete the Windows/x64 shared-runtime Media/UI replay (PAL-04)

At CRT `027b8810`, reconfigure the four retained Release trees and build the
ordinary Media/UI packages. All four stage-04/05 packages verify, and every
example builds and runs from the extracted ZIPs under a space/Hangul path.
PE imports confirm shared `libc.dll` for all 16 examples and `libc++.dll` for
both Skia examples. Each UI run passes 30 frames with pixel and input checks.
The shipped graphics and media examples present eight frames each. Two fresh
FFmpeg-without-curl trees refuse both distribution targets before their
prerequisites. The packaging unit run has 99 passes and six host skips.
[Acceptance](docs/acceptance/crtui_acceptance.md#windowsx64-shared-runtime-closure-replay-2026-11-24)
owns the exact inputs, outputs and measured results.

### Replay the Media/UI packaging and the closure packages on Windows/arm64 (PAL-04)

On the physical Windows/arm64 host the PAL-04 packaging matrix passes at CRT `1ae70fa`: the minimum and the full controls, the FFmpeg-less package (Skia, FreeType and the private Win32 consumer headers) and the Skia-less package (FFmpeg, curl, Mbed TLS and zlib) each verify at stage 04 and 05,
rebuild every shipped example from a relocated `SDK space 한글` copy, run the UI 30-frame check, and extract and verify from their stage-05 `.zip`; FFmpeg ON with curl OFF is refused in under 0.4 s. At `2fc6eaf` (before the closure packaging) both partial shapes were refused before building and the full control
carried the private Skia headers, with a negative verifier control. The first full build failed after 52 minutes because the pinned Skia GN revision has no `windows-arm64` package (HTTP 404 in `fetch-gn`); the documented x64 `gn.exe` of that revision, supplied with `-DCRTGFX_SKIA_GN` and run under emulation, resumed it in 13 minutes.
That input, the failed attempt and a result-file gap (the script's SHA-256 lines, recomputed from the preserved archives) are recorded, not hidden. With this the Windows/arm64 gap of the focused CRT/PAL queue is closed and the section leaves In Progress; the Linux closure packaging is a conditional Planned item
([evidence](docs/acceptance/crtui_acceptance.md#windowsarm64-packaging-and-closure-replay-2026-11-24)).

### Replay the Skia-less and FFmpeg-less closure packages on macOS (PAL-04)

On macOS/arm64 the Windows closure packaging works unchanged: the retained Release trees re-package a Skia-less (FFmpeg, curl, Mbed TLS, zlib) and an FFmpeg-less (FreeType, Skia) package that pass `verify_dist` at stages 04 and 05, rebuild
every shipped example from a relocated `SDK space 한글` copy (default and shared CRT linkage, binding audits clean), run the UI 30-frame check and verify as extracted archives. The all-ON and all-OFF controls keep their dependency records. macOS now passes
`CRT_MEDIA_DIST_PARTIAL=ON` like Windows; Linux still refuses both shapes. The first build script omitted `-S` and was rerun; negative verifier controls fail and recover as expected. [Evidence](docs/acceptance/crtui_acceptance.md#macosarm64-skia-less-and-ffmpeg-less-closure-packages-2026-11-24).

### Package complete Skia-only and FFmpeg-only closures on Windows/x64 (PAL-04)

`crt-media-dist`/`crt-ui-dist` on Windows/x64 now accept Skia ON/FFmpeg OFF (FreeType and Skia) and Skia OFF/FFmpeg+curl ON (FFmpeg, curl, Mbed TLS, zlib) and package exactly that closure; FFmpeg without curl is refused before building, and Linux and macOS
keep refusing both shapes. `create_dist.py` and `add_redistributed_dependencies` take the closures as components, `verify_dist` also requires FreeType with Skia, and 105 tests pass. On the existing Release trees both packages verify at stages 04 and 05, relocate to `SDK space 한글`,
rebuild their examples (media-player or gfx-skia), run the UI 30-frame check and verify as extracted `.zip` archives; the all-OFF and all-ON controls keep their dependency records. A first FFmpeg-less attempt failed on an over-strict check of mine
(the Skia media companion does not need FFmpeg) and is kept. Merged with the macOS early-refusal commit (`2fc6eafe`). Windows/arm64 (replayed later, see the 2026-11-24 Windows/arm64 entry above), Linux/macOS replay, shared-runtime linkage and CI remain open. [Evidence](docs/acceptance/crtui_acceptance.md#windowsx64-skia-less-and-ffmpeg-less-closure-packages-2026-11-24).

### Refuse partial Media/UI package options early on macOS (PAL-04)

The macOS replay had reproduced the same late verifier rejection of both partial Skia/FFmpeg shapes as Windows, so the decision is to refuse them before building there too.
`crt_media_dist_option_problem` now applies to every target OS instead of Linux and Windows only. On macOS/arm64 fresh Release trees refuse Skia ON/FFmpeg OFF and Skia
OFF/FFmpeg ON for `crt-media-dist` and `crt-ui-dist` in 0 s with the option diagnostic and no compile step; the supported all-OFF and all-ON rows keep the ordinary 900+ command
packaging paths and are not rebuilt. The option-matrix test now pins the unguarded application. No runtime, ABI, pin or asset change. A first check run configured the rows with
default options because of a `zsh` word-splitting slip and was discarded. [Evidence](docs/acceptance/crtui_acceptance.md#macosarm64-early-refusal-2026-11-24).

### Replay ordinary Media/UI packaging on Windows/x64 (PAL-04)

On the physical Windows/x64 host, fresh Release trees confirm the late failure of both partial shapes (Skia OFF/FFmpeg ON ships FFmpeg payload without its dependencies; Skia ON/FFmpeg OFF ships Skia payload without `libskia.a`) and that the
Skia-less and full controls verify at stages 04 and 05. The replay also found that the verified, relocated full package could not configure its `gfx-skia` example (the private Win32 shim and mingw-w64 headers were not packaged and the verifier did not require them).
The Linux early option refusal now covers Windows, the Windows Skia package installs and records those headers, and `verify_dist` requires them. Both partial shapes are refused in under 0.2 s; the supported controls rebuild all shipped examples from a
relocated `SDK space 한글` copy, run the UI 30-frame check and verify as extracted `.zip` archives; 62 tests pass (6 host skips). Windows/arm64, macOS early refusal and a complete partial-closure package remain open.
[Evidence](docs/acceptance/crtui_acceptance.md#windowsx64-packaging-replay-2026-11-24).

### Replay ordinary Media/UI packaging on macOS (PAL-04)

At CRT `cc86383`, build fresh Release minimum and partial-option trees on
macOS/arm64, reusing raw source caches only. The minimum package (Skia and
FFmpeg OFF, curl ON, LVGL ON) passes stage-04/05 verification. Both partial
shapes build but fail the current shared verifier: Skia-only omits `libskia.a`
and the Skia record; FFmpeg-only omits the ffmpeg/curl/mbedtls/zlib records.
Preserve those rejected outputs and first errors rather than calling them
successful packages. Early CMake refusal remains Linux-only; deciding whether
to extend it to macOS stays in TODO.

Reconfigure the saved Skia tree freshly with all options ON: both packages
verify. Minimum/full package binding audits pass for 102/110 Mach-O images.
Space/Hangul relocated SDKs rebuild three/five examples with explicit shared
CRT linkage; all eight consumer binding audits pass, and both UI consumers run
30 frames with pixel/input checks passing. Both stage-05 archives also verify
after extraction into new directories. The earlier full SDK at `a79a06b`
is retained only as a separate older-artifact control. Removing the Skia
record/archive from its disposable copy demonstrates the new verifier refusal;
the restored copy verifies. Packaging unit tests: 61 run, one Linux-only skip.
[Acceptance](docs/acceptance/crtui_acceptance.md#macosarm64-packaging-replay-2026-11-24)
owns inputs, logs, commands and limits. Windows is assigned to a separate
physical host; no Actions, release, runtime/ABI or consumer pin change. The
focused section is not retired while this host follow-up remains open.

### Close the native Linux stdio gate (PAL-01)

The Linux replay of the Windows PAL follow-ups had been recorded only in crt-vscode's history. Rebuilt on current `main` `21a5509` (runtime unchanged since
`312fec9`): Release core 172/172, and the 23 stdio, descriptor, process and socket cases pass 20 repetitions. PAL-01 closes on Linux, macOS and Windows; PAL-04's
wording now names merged #24. The focused CRT/PAL queue has no open item
([evidence](docs/acceptance/release_qualification.md#native-linuxx86_64-stdio-replay-pal-01-2026-11-24)).

## 2026-11-23

### Close the Linux partial-option packaging gaps (PAL-04)

Base CRT `312fec9` with the changes uncommitted. The Linux replay reproduced two gaps:
Skia OFF/FFmpeg ON/curl ON failed stage 04 (`lib/libcrtmedia.so -> undeclared libcurl.so.4`),
and Skia ON/FFmpeg OFF/curl ON built and verified but shipped `libcrtgfx_skia.*`,
`libcrtui_skia*.a` and `examples/gfx-skia` without `lib/libskia.a`. Both come from stage-04
dependency publication being all-or-nothing (`create_dist.py` gets the closure only when all
three options are ON), so the decision was to declare the ordinary package supported only for
Skia, FFmpeg and curl all OFF or all ON on Linux, and enforce it: `cmake/crt_media_dist_options.cmake`
is the one predicate, `CMakeLists.txt` replaces `crt-media-dist` and `crt-ui-dist` with a
failing target for other combinations (under a second, before any stage build), and
`verify_dist.py` rejects Skia or FFmpeg-backed payload that lacks `lib/libskia.a` or the
skia/ffmpeg/curl/mbedtls/zlib records. Shipping a Skia-less or FFmpeg-less closure was not
chosen: it would add manifest, verification and relink-kit variants no consumer needs.

Verification (fresh Release trees under `out/`, Ubuntu 26.04, Clang/LLD 21.1.8, curl ON,
LVGL ON): `pal04-g3` (Skia OFF/FFmpeg ON) and `pal04-g4` (Skia ON/FFmpeg OFF) refuse both
targets in 0.6 s; `pal04-c1` (all OFF, `crt-media-dist`), `pal04-c2` (all OFF, `crt-ui-dist`)
and `pal04-cf` (all ON, `crt-ui-dist`) build and pass `verify_dist` for 04 and 05; relocated
space/Hangul copies rebuild `gfx-simple` and `gfx-gpu` (all), `ui-basic` (c2, cf; 30 frames,
`pixel_check=pass input_check=pass`) and `gfx-skia`/`media-player` (cf). The full tree's
`crt-media-dist`/`crt-ui-dist` ninja commands are identical to those of a tree configured from
unmodified `312fec9`. The old `out/pal04-r4` package passed `verify_dist` before and is
rejected now. `tools.test_verify_dist`, `test_create_dist`, `test_sdk_workflows` and the new
`test_media_dist_options` pass (61 tests, 5 host skips). A first draft refusing only
`crt-media-dist` let `crt-ui-dist` run for 4.5 to 8 minutes before stopping, so both are replaced.
[Evidence and limits](docs/acceptance/crtui_acceptance.md#linux-partial-option-media-packaging-follow-up-pal-04-2026-11-23).
Limits: Linux/x86_64 only; macOS/Windows partial combinations are neither refused nor
verified; tools-wide `unittest discover` (674 tests) has one error, the argument-requiring script
`test_cli_fetch_https` (needs `--cli/--output`), unrelated to this change; no libc/ABI, STATUS or pin change.

### Replay the Windows ARM64 PAL changes on physical Windows/x64

After synchronizing CRT `52881f08`, CLI `6704aa8` and extension `119e894`,
the native x64 replay closes the remaining descriptor and PAL-02 host gates.
The descriptor regression fails three times on the PAL-06 CI SDK and passes
five times on the fix (400,000 cycles). A fresh Release core passes 20 tests
with one existing fork-heap skip; the restricted-token relocated SDK consumer,
Unicode CLI C++ consumers, C provider and host-tools checks pass. Release
stdio measurements also improve on the same baseline SDK.
[Acceptance](docs/acceptance/release_qualification.md#native-windowsx64-pal-replay-2026-11-23)
owns exact inputs, measurements, commands and limits. Previous ARM64/macOS
records keep their original host and artifact scope. No product, format,
release or pin change. Merged the macOS evidence at `cb3e0145` and the CLI's
`76a0f1a` without changing the tested runtime inputs; PAL-01 now retains only
native Linux, and PAL-04 retains its Linux packaging gate.

### Replay the Windows PAL follow-up work on macOS/arm64

Synchronize CRT `52881f0`, CLI `6704aa8` and crt-vscode `119e894`. A fresh
Release core build passes 146/146 native CTest cases; retain the initial two
sandbox socket refusals separately. Static/shared stdio, formatter and
independent-fd consumers pass 18 builds and 22 executions, with nine shared
binding audits and 20 repetitions of nine focused CTest cases. The controlled
stdio-object comparison improves buffered write/read medians from 201.6/342.5
to 302.7/1783.0 MiB/s on the same prior Mac SDK; these are cached temporary-file
measurements, not a device-throughput claim.

Independently verify the clean macOS SDK of integration `45159161995` at the
same CRT commit. Its relocated static/shared path/Unicode/HTTPS consumers pass
with the producer and original SDK hidden, and all eight imported-libc++ smoke
executions pass. Current CLI source self-builds through the SDK and builds/runs
the same utilities/path fixture through static/shared CLI projects under a
space/Hangul/U+1F600 path, without corrective flags or host-runtime fallback.
The full CLI contract replay also passes its five substantive suites and
SDK-hidden bare execution; input-gated port suites remain skips.
Only PAL-01's macOS gate closes; Linux stdio, physical Windows/x64 and the Linux
partial-option packaging gates remain. No runtime/product/pin or STATUS change.
[Inputs, results, commands and limits](docs/acceptance/release_qualification.md#macosarm64-pal-follow-up-replay-2026-11-23).

### Serialize Windows descriptor slots and copy stdio buffers in chunks

The PAL-06 ARM64 CI SDK still reproduced failed reads and wrong bytes from
two threads opening distinct files. The PAL now serializes descriptor slot
ownership, replacement and snapshot collection. The original reproducer and a
new four-thread regression pass with the fix. PAL-01 uses block transfers for
buffered I/O and for unbuffered descriptor writes. Release temporary-file
medians are 76.8 to 126.8 MiB/s for buffered writes and 97.9 to 272.8 MiB/s
for buffered reads. The focused results are 19 passes and one existing skip
on native ARM64, and 11/11 in x64 emulation. The focused CLI test with the
original stdio relay captures its expected 8 MiB and exits 0 in 8.625 seconds.
[Evidence](docs/acceptance/release_qualification.md#windows-descriptor-ownership-and-stdio-block-transfers-2026-11-23).

### Qualify the ARM64 Unicode workflow and decide Windows spawn policy

SDK installation, cold and warm CLI builds and static/shared execution pass
22/22 in each of two Unicode roots on native ARM64, and 22/22 on x64 under
emulation. Both public host-tools bundles exercise Unicode paths, with the
x64 check also under emulation. PAL-03 retains the explicit launcher hint for
known foreign Windows tools because ordinary PE metadata cannot identify
support for the CRT child protocol.
[Evidence and remaining native x64 gate](docs/acceptance/release_qualification.md#windows-unicode-and-native-child-selection-pal-02-and-pal-03-2026-11-23).

### Convert characters above U+FFFF at the libc++ Windows filesystem boundary (PAL-06)

libc++'s Windows `std::filesystem` truncated every character above U+FFFF to 16 bits. A `path` on this target holds UTF-32 (the 4-byte `wchar_t` the headers produce for `u8path()` and `wstring`) while every Win32 call is UTF-16, and the
recipe's `crt_win32_wide.h` narrowed and widened one element for one unit: `create_directory(u8path(U+1F600))` made a directory whose name ended in U+F600, `path::string()` of it was wrong, and `path(const char*)` with a 4-byte UTF-8 character threw.
`narrow()` now splits such an element into a surrogate pair and `widen()` combines pairs, both return their counts, the 13 `narrow()` scratch buffers are sized for the worst case (`narrow_capacity()` = `2 * len + 1`) and the 6 `widen()` call sites
(`GetTempPathW`, `getcwd`, `realpath`, symlink readlink, directory iteration, `path` conversion) use the returned count. `__char_to_wide` returns the element count in both calls of the header's "measure, then convert" idiom; returning the raw UTF-16 unit count would have
resized the string and left trailing NULs. Unpaired surrogate elements pass through unchanged (NTFS allows such names). The shared Dev SDK consumer runs `filesystem_unicode_checks` (create, list, rename, copy, `canonical`, `absolute`, `current_path`, `space`,
`permissions`, remove, with U+1F600 and U+20000 names, and stdio reaching the same names) on every OS, and `tools/test_libcxx_wide_patches.py` guards the buffer sizing and the use of the returned counts
([evidence](docs/acceptance/release_qualification.md#libc-windows-filesystem-names-above-uffff-pal-06-2026-11-23), [design](docs/design/cxx_runtime.md#windows-stdfilesystempath-and-stream-opens)).

### Stop the intermittent `bionic_surface_test` CPU-time failure by spinning on wall time (root cause)

`bionic_surface_test_runs` failed now and then on the hosted windows-x64 runner with `user CPU time advanced` (twice on 2026-11-23, in the PR 20 and PR 21 CI runs). Its history: on 2026-10-11 (`c9425d0`) a fixed 30M-iteration loop was seen
failing once and replaced by "up to 40 rounds of 2.5M iterations until the clock moves", whose message says to bound the wait instead of assuming a duration, but the bound was still a *count* of work, so it still depends on the CPU's speed. The
cause is the same: Windows CPU time (`GetProcessTimes`, behind `getrusage` and the `CLOCK_*_CPUTIME_ID` clocks) advances in scheduler ticks of about 15.6 ms, and a fast core can finish a fixed amount of work inside one tick. Measured on Windows/arm64
with the same loop shortened, 300 fresh processes each: a loop of about 10 ms ends with zero CPU time in 83, about 13 ms in 15, about 16 ms in 1, about 18 ms in 0. The `getrusage` check and its sibling `CLOCK_PROCESS_CPUTIME_ID`/`CLOCK_THREAD_CPUTIME_ID`
check (a fixed 20M iterations, the same defect) now burn CPU until the clock moves or 3 s of *wall* time pass, so they hold on any CPU and still fail when the clock never moves; 0 failures in 400 runs, idle and with 8 busy processes on 4 cores. The tick
resolution is documented where Windows `getrusage` reads it. Not reproduced here: the failure itself on the hosted runner (the shortened loop reproduces the mechanism, not the runner's exact loop length). No earlier written plan to measure time instead of counting was found in the
repositories, issues, pull requests or recorded sessions; `c9425d0` is the only trace of that intent.

### Open `std::fstream` streams with a `std::filesystem::path` on Windows (PAL-05)

The path constructors and `open()` of `ifstream`, `ofstream`, `fstream` and `filebuf` did not compile on Windows, where `path::value_type` is the CRT's 4-byte `wchar_t`, while the same source builds on Linux/macOS. libc++'s
`_LIBCPP_HAS_OPEN_WITH_WCHAR` is back on and the MSVC-CRT `_wfopen()` call inside `<fstream>` is replaced by a private inline UTF-32 to UTF-8 conversion and the ordinary narrow `open()` (recipe patch on `include/fstream`; the former `__config` exclusion
is removed, the `__cxx03` one stays). No libc ABI, libc++ export or consumer option is added; mode validation and failure states are the narrow open's, and a lone surrogate or a value above U+10FFFF is refused. The shared Dev SDK
consumer checks read, write and append through the constructors and `open()`, space, Hangul and supplementary-plane names, failure states and the preservation of an existing file, in a static and a shared runtime
([evidence](docs/acceptance/release_qualification.md#stdfstream-path-open-on-windows-pal-05-2026-11-23), [design](docs/design/cxx_runtime.md#windows-stdfilesystempath-and-stream-opens)). The work also exposed a separate, older defect: libc++'s Windows
filesystem layer truncates characters above U+FFFF (`create_directory(u8path("😀"))` creates a directory ending in U+F600); that is not fixed here and is tracked as PAL-06.

The CLI's C++17 replay now opens its file with the path itself, and on the CRT `111c351` CI artifact passes 22/22 on Windows/arm64 while the SDK before the fix fails at the `ofstream` constructor ([CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsarm64-direct-path-stream-open-replay-of-the-pal-05-fix-2026-11-23)); Windows/x64 is covered by the hosted job, and the physical x64 replay is open.

### Close the Windows C++ public compile options item (P1-01)

The Windows/arm64 replay of the corrected Dev profile closes the item. The CLI's `replay_cpp17_consumer_windows.py` consumes the `sdk (windows-aarch64)` CI artifact of `770ed07b` (hosted `sdk-integration` run 45099082349, content
`b592e3cc...d617`) on this physical arm64 machine with CLI `6e484cc` self-built by that SDK, and passes 22/22; the published `20261118` Windows/arm64 SDK fails the same driver (5/16) as the baseline. With the
Windows/x64 result above both Windows architectures are exercised, and the exact SDK-CLI pair is in the CLI's [compatibility record](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#windowsarm64-c17-persona-consumer-combination-2026-11-23)
([acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsarm64-external-c17-persona-consumer-replay-2026-11-23)). **Still open and not part of this item:** promotion of an SDK that carries the
correction and consumer pin adoption (crt-cli CI-01), and the `std::ofstream(path)` finding, which moved to Planned as PAL-05. No producer code changed in this step; the CI-built artifact is unsigned and unpromoted.

### Replay the corrected Windows C++ persona with an external C++17 consumer on Windows/x64 (P1-01)

The CLI's new `replay_cpp17_consumer_windows.py` consumes the locally built `770ed07b` candidate on this physical x64 machine and passes 22/22 (no consumer option, both CRT shims, `std::filesystem`, libcurl and mbedtls for a static and a shared runtime); the published `20261110`
SDK fails (5/16). Findings kept: `std::ofstream(path)` does not compile against the packaged libc++, and there is no shared libcurl. Windows/arm64 is not yet run, so P1-01 stays open. No product, pin or STATUS change. [Record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsx64-external-c17-persona-consumer-replay-on-a-physical-machine-2026-11-23).

### Complete the physical Windows/x64 replay of the pure C shared provider (Tranche 6.4 / P0-01)

Built a clean Release candidate of CRT `770ed07b` (`v0.0.1-rehearsal.11`; CTest 169/169, in-build C provider check 4/4) and replayed it on this physical Windows 11 x64 machine with CLI `c297a65`: the SDK alone builds the CLI (self-build cold 21/0, warm 0/21; native 1028, archive 214, SDK-store 113, recipe 147, store-lock 9)
and the C static/shared driver passes 36/36 with the loaded `libc.dll` equal to the installed SDK's. The published `20261110` SDK again fails the same six of 36. This closes the last verification slice of P0-01 (Linux x64/aarch64, Windows x64 hosted and physical, Windows arm64 and macOS arm64 are all recorded), so the item is removed from TODO.
**Still open and not part of this item:** promoting a Windows SDK that carries the correction (the newest published Windows/x64 SDK is `20261110`) and consumer pin adoption, tracked by crt-cli CI-01 and P1-01; Windows C++ `<filesystem>`. No product, published SDK, pin or STATUS change.
[Inputs, results and limits](docs/acceptance/sdk_split_acceptance.md#windowsx64-physical-machine-pure-c-shared-provider-replay-2026-11-23).

### Replay the pure C shared provider on macOS/arm64 (Tranche 6.4)

Synchronize CRT `3c8c712`, CLI `01d2532` and crt-vscode `8c13564`. Independently verify and consume the macOS SDK artifact of
CRT `5325cea` / SDK integration `45002729132`, the build already used by the Windows consumers. The SDK-only CLI self-build is
cold 21/0 and warm 0/21, with clean selection (the macOS replay no longer silently opts into dirty SDKs). Native 984, archive 221,
SDK-store 114, recipe 147 and store-lock 9 pass; the five input-gated suites' skips are not counted as passes.

A new public-PAL C image fixture and CLI-owned macOS replay pass 76/76: static/shared SDK zlib, a project dylib, same-install-name
provider precedence, actual libc/libz images, constructors and allocator/errno sharing, missing-runtime denial and three early mixed-runtime
refusals. The original public Mac SDK passes the same 76/76, unlike the failing Windows baseline. Actual upstream zlib static/shared
port/provider replay passes 20/20 gates (recipe 147, project 81). Binding/mixed-allocator audits pass on eight C/CLI/SDK images and seven
source-port consumer/images. Preserve the harness setup failures; no product runtime, upstream, published SDK, lock or pin changes.
Promotion and consumer pin adoption remain open; STATUS is unchanged.
[Inputs, exact results, reproduction and limits](docs/acceptance/sdk_split_acceptance.md#macosarm64-pure-c-shared-provider-replay-2026-11-23).

## 2026-11-22

### Record the Windows/x64 consumer replay of the CI SDK on a hosted runner (Tranche 6.4)

The x64 artifact of the same CI build (not a release) was consumed by the CLI at `6f7f8b9` on a `windows-2022` runner: the CRT contract replay passed and the new `replay_c_shared_windows.py` passes 36/36; the published `20261110` SDK fails the same six cases and is kept as the
baseline. Hosted, not a physical x64 machine; macOS, promotion and pin adoption remain open. [Acceptance](docs/acceptance/sdk_split_acceptance.md#windowsx64-consumer-replay-of-the-ci-sdk-for-the-pure-c-shared-provider-2026-11-22).

### Record the Windows/arm64 consumer replay of the CI SDK (Tranche 6.4)

The CI-built Windows/arm64 SDK of `5325cea` (not a release) was consumed by the CLI at `44fbf24` on a native machine: its suites pass (native 1,041 checks) and the new `replay_c_shared_windows.py` passes 36/36
(static control, shared runtime, shared runtime + project DLL with the loaded `libc.dll` equal to the installed SDK's, a mixed-runtime refusal). The published `20261118` SDK fails six of the same cases and is kept as the baseline.
Windows/x64 and macOS replays, promotion and pin adoption remain open. [Acceptance](docs/acceptance/sdk_split_acceptance.md#windowsarm64-consumer-replay-of-the-ci-sdk-for-the-pure-c-shared-provider-2026-11-22).

### Honor the runtime linkage in the Windows C executable wrapper (Tranche 6.4, local candidate)

`tools/crt-cc` ignored `CRT_RUNTIME_LINKAGE` for Windows executables, so the pinned Windows/x64 Dev SDK's
`executable-shared.link_c` was identical to `executable-static.link_c` (static `libc.a`/`libm.a`/`libdl.a`)
and a hello-world linked with either produced byte-identical executables importing only `KERNEL32.dll`
and `api-ms-win-core-synch`. The wrapper now selects `libc_dll.dll.a`/`libm_dll.dll.a`/`libdl_dll.dll.a` for a
shared runtime, as `tools/crt-c++` already did, accepts the legacy `CRT_CXX_RUNTIME_LINKAGE` hint and rejects
other values before linking. The exporter needed no change.

Verified on this Windows 11 x64 host (clang/lld 22.1.8) against a local, unpromoted copy of
`v0.0.1-preview.20261110.windows.1` with only `tools/crt-cc` replaced and its sidecar marked dirty: the exported
shared policy now links `libc_dll.dll.a`, and the new `tools/test_c_shared_provider_windows.py` passes four
cases (project DLL build, static runtime, shared runtime, shared runtime + project DLL). The shared executables
import `libc.dll`, the loaded `libc.dll` is the SDK's `bin\libc.dll`, the executable and the DLL share one libc,
and the executable fails to start when the SDK `bin` directory is absent from `PATH`. The same script fails the two shared cases against the
unmodified SDK. `test_crt_cc_linkage.py` (4 new Windows argv cases), `test_sdk_c_provider_gate.py` (5) and
`test_export_cli_toolchain.py` (9) pass. The Dev collector now runs the Windows check
(`crt_sdk_release.py`); that path was exercised only by its mocked gate test, not by a real release build.

Limits: this is not a clean SDK candidate, release or promotion; no CLI consumer replay of such a candidate was
run; Windows has no zlib or explicit same-name provider case (loader search differs from ELF); a C executable that
sets only `CRT_CXX_RUNTIME_LINKAGE=shared` now also links `libc.dll` (same domain as C++); only Windows/x64 was
run. Evidence is local: `C:\crt-replay\cand` (`provider-r1`, `baseline-r1`, `toolchain.json`).

### Record the consumer's Windows/arm64 replay for the pure C shared handoff

No CRT code, SDK or release changed. The CLI's Windows/arm64 port-integration replay at CLI
`8374470` passes all five port suites against the existing Dev SDK
`v0.0.1-preview.20261118.windows.arm64.1` (producer `e7c7630`), with CRT `42b8f7a` as the
local producer reference. The Linux-only wrapper correction `19bdf36` produces no Windows SDK
candidate, so this replay is not Tranche 6.4 qualification: replay of a new SDK candidate on
macOS and Windows stays open under P0-01.

The same investigation found two defects in the CLI's store `Lock` (fatal `create_directory`
contention errors and stealing of a live lock whose `owner` is not yet written), which explain
the scheduled `ports (win-arm64)` failure in CLI run
[44898897532](https://github.com/webos21/crt-cli/actions/runs/44898897532). Both are CLI
code; whether CRT's libc/libc++ `create_directory` behaves correctly under the same race was
not examined. Exact inputs, counts and limits are in the CLI's HISTORY entry for 2026-11-22.

## 2026-11-21

### Qualify a clean Linux/x86_64 C shared Dev SDK candidate

Build `v0.0.1-cshared.20261121.1` from clean CRT `19bdf36` using the standard
Dev release target. Independently verify the archive, portable policy and
source assets, then rerun all five C provider cases with both Linux loaders.
Core 147/147 and imported C++ static/shared smokes pass. The hosted SDK build
also passes the new five-case producer gate on both native Linux architectures.
[Exact inputs, evidence and remaining gates](docs/acceptance/sdk_split_acceptance.md#clean-linuxx86_64-c-shared-sdk-candidate-2026-11-21).

## 2026-11-20

### Correct the Linux pure-C shared provider and validate the local handoff

Reproduce host-libz/glibc fallback with the signed November 18 SDK. Make the Linux
C executable wrapper honor static/shared linkage and preserve explicit port search
precedence before the SDK fallback. Add installed-policy runtime qualification to
the Linux asset collector and regressions for wrapper argv and incomplete reports.
A separately identified dirty packaging candidate passes five provider modes with
two loaders; CLI closure validation, isolated application and actual source-port
replay pass. Runtime binaries and public pins are unchanged. Tranche 6.4 remains
open for clean artifacts and affected native-host qualification.
[Exact identities, commands, results and limits](docs/acceptance/sdk_split_acceptance.md#linux-pure-c-shared-provider-correction-2026-11-20).

### Retire the completed host qualification queue

Remove the completed TODO section after reconciling the consumer's signed Windows/arm64 workflow Preview and D1 32/32 public
replay, the five-host integration handoff and the Windows/x64 local private-DLL evidence. Refresh the SDK and CLI release snapshot
and retain the original failure and artifact scope in the owning acceptance records. [Host qualification closure](docs/acceptance/host_tools_acceptance.md#host-qualification-closure-2026-11-20).

### Qualify Windows/x64 host tools with actual private DLLs

Fix the copied-alias initial-load gap by placing byte-identical CRT DLL closure beside each executable copy. Re-derive and verify the PE
import graph, native DLL identity and adjacent closure before publication and verification, without a manifest version change or launcher.
Clean implementation `1e489d6` passes the 27-case native producer/CLI replay with real unmodified AWK and private libc/libm, two Unicode relocations,
eight mapped-module snapshots, working cwd/PATH poison controls, missing/modified DLL refusal and same-pin recovery. Main adopts `e8724b8`
with all five native CI rows/gate passing on PR and main; the hosted host-tools regression passes 44 cases (one skip) and static replay 18/18. Focused tests run 51 cases
(50 pass, one Windows-inapplicable POSIX permission skip); workflow wiring includes the nine new PE cases. Fresh static payload and original SDK/source
bytes are unchanged. Check the bounded local native gate and retain official all-shared release/SDK-policy/ARM64 distinctions.
[Exact inputs, receipts, commands and limits](docs/acceptance/host_tools_acceptance.md#windowsx64-private-dll-closure-replay-2026-11-20).

## 2026-11-19

### Record the completed CLI hosted integration handoff

The consumer's macOS/Windows integration pins and hosted Port/oracle jobs are adopted on CLI main (`f06a036`, documented at `858c04b`).
Integration run 44490048419 passes all 11 jobs at `9a417fc`; main CI 44493533992 passes all six native rows and `CI gate`.
The Windows job proves real symlinks and selected original/bare input denial with the signed public bundle. Check the producer's stale
CLI-owned handoff item and link the consumer-owned evidence, retaining both the original publication record and the open Windows/arm64 D1
and private-DLL gates. This is documentation synchronization, not a new producer replay, SDK/pin promotion or STATUS update.
[Scope and immutable consumer links](docs/acceptance/host_tools_acceptance.md#consumer-hosted-integration-handoff-completed-2026-11-19).

### Close the host-tools producer publication request (#9)

Closed [#9 as completed](https://github.com/webos21/crt/issues/9#issuecomment-6998966548): the requested macOS/arm64 and Windows/x64
Previews are public, signed, clean-provenance bundles with the archive/signature/checksum/release-record handoff and native producer evidence.
Windows/arm64 is additional coverage outside the original request. Completed publication moves out of the remaining-work item; its terse
checked prerequisite stays beside the open consumer integration handoff. CLI TODO records the completed native/D3 slices separately from
hosted integration and the ARM64 D1 large-output timeout (31/32). No test was rerun or failing result relabeled by this documentation closure.
[Acceptance scope](docs/acceptance/host_tools_acceptance.md#producer-request-9-closure-and-remaining-handoff-2026-11-19).

### Verify the Windows/aarch64 public host tools and retain the consumer timeout

Verified the public ARM64 archive with the pinned publisher key (OpenSSL 9/9 and the published ARM64 CLI's four real-URL fetch cases,
including three refusals with no output). Producer verification and the space/Hangul native replay pass 18/18; all 179 PE file paths are ARM64,
use only the declared OS imports and have no Authenticode table. The D1 harness now supports both Windows architectures and retains failed
timeout cases while finishing the remaining checks. D1 completes 31/32: its large-output job times out, including under a longer outer
observation budget, so full D1 qualification remains open. D3's six suites and the actual `port build zlib` pass 7/7 on this public bundle,
using an identical-input retained SDK-only contract replay. [Evidence and limits](docs/acceptance/host_tools_acceptance.md#windowsaarch64-host-tools-public-signature-and-native-replay-2026-11-19).

### Verify the Windows/x86_64 host-tools release natively

Downloaded the public `v0.1.0-hosttools.20261119.windows-x86_64.1` assets without authentication (digests equal the release page and record) and verified the publisher signature two ways: `openssl` (9/9, four refusals) and the published `crt-cli` Preview's
`fetch --publisher-key --signature` (`signature_verified`, three refusals leaving no output). The producer verify and the Windows replay pass 18/18, all 90 PE images import only the two OS libraries and carry no Authenticode, and the CLI's 32 fail-closed controls and
its port suites (142/36/97/83/33/21 checks plus a real `port build zlib`) pass on this exact bundle under a restricted token. Nothing was rebuilt or replaced. [Evidence](docs/acceptance/host_tools_acceptance.md#windowsx86_64-host-tools-public-signature-and-native-replay-2026-11-19).

### Verify the signed public macOS host-tools bundle through CLI D3

Fresh CLI source at `c79d57c` consumes the signed public macOS Dev SDK, bootstrap CLI and new host-tools Preview. All six D3 suites pass
(442 checks), with 27/27 replay gates including static/shared port consumers, oracle comparison, publication/reuse/verify, cold/warm self-build
and source-hidden Seatbelt denial probes. Nine Mach-O images pass CRT binding/mixed-runtime auditing. SDK and host-tools inventories are
unchanged. The [CLI-owned acceptance](https://github.com/webos21/crt-cli/blob/144581752cc492f0906fb9e422d6e91663bccb0a/docs/acceptance/sdk_install_acceptance.md#signed-public-host-tools-d3-replay-macosarm64-2026-11-19)
and compatibility ledger own the exact exercised combination; this does not enable hosted integration or change consumer pins.

### Attach macOS/Windows host-tools signatures and replay the public macOS bundle

Attached the owner's three detached signatures to the corresponding `v0.1.0-hosttools.20261119.<host>.1` releases
(macOS/aarch64, Windows/x86_64 and Windows/aarch64). Each public archive matches its release-record size/digest; RSA-PSS verification passes
before upload and after re-download, and each downloaded signature is byte-identical to the supplied file. No archive, checksum or release
record was replaced. Release notes now distinguish the signed archive from the unsigned packaging record.

On macOS 27.2.1/arm64, the published macOS archive passes extraction/inventory/native-image verification and the relocated Seatbelt replay
18/18 (empty PATH, space/Hangul bundle path, shell/awk/make/applets and every declared executable alias); 35 focused producer tests pass.
The four canonical tools import only `/usr/lib/libSystem.B.dylib`. Retained first-attempt sandbox restrictions and the Windows native-host
verification limit are documented in [host-tools acceptance](docs/acceptance/host_tools_acceptance.md#macoswindows-host-tools-publication-signatures-and-macos-replay-2026-11-19).
This is not CLI pin adoption, CLI fail-closed/port-suite acceptance, or Windows private-DLL closure qualification. STATUS is unchanged.

### Publish the independent host-tools Previews for macOS/arm64 and Windows x64/arm64

Extended the manual workflow to macOS/arm64 and Windows x64/arm64 with pinned
native Dev SDK inputs. All three clean bundles passed hosted producer replay
18/18 and were published as prereleases after comparison with workflow artifacts.
Windows/arm64 now has its first independent bundle. The
[producer record](docs/acceptance/host_tools_acceptance.md#hosted-producer-publication)
retains this publication step; subsequent signatures and consumer replays are
recorded above with their own scope.

## 2026-11-18

### Close the Dev SDK GitHub CI/CD group (S0-S7)

The producer-side CI/CD for the Dev SDK is complete and its TODO group is retired: four-host integration (S1) with the consumer smoke by each host's pinned,
signature-checked CLI Preview (S2, Windows/arm64 included), the manual approval-gated `sdk-release.yml` (S3), the first automated Previews for Linux/x64
(S4) and Windows/arm64 (S7, through the first-host bootstrap, now ended: `bootstrap` and `cli_preview: none` are removed and the host takes the normal gate),
and the CLI pin adoption (S5). S0 closes with the required check: `ci.yml` gained a `changes`/`CI gate` pair (PR 8) so a documentation-only change skips the
matrix without leaving a required check pending, and the repository ruleset `main requires CI gate` (identical to crt-cli's: no delete, no non-fast-forward, required
`CI gate`, admin bypass) is active on `main`. Left open on purpose: every other host's first publication starts with its own `dry_run: true` run, the ARM64 CLI's own
`release.yml` Preview belongs to crt-cli, `porting/**` still counts as documentation for the gate (as it did under `paths-ignore`), and the Full SDK release workflow
(was S6) moved to the Planned "Full SDK delivery" item. Evidence: [release qualification](docs/acceptance/release_qualification.md), procedure
[sdk_ci.md](docs/guides/sdk_ci.md).

### Publish the first Windows/arm64 Dev SDK Preview as a first-host bootstrap (S7 steps 2-3)

`windows-arm64` is offered by `sdk-release.yml` (PR 4) with S2 recorded `NOT RUN` and the source-hidden native consumer of the extracted archive in its place.
A `dry_run: true` run (44220444318) validated the workflow on a hosted runner; its draft was deleted because its zip bytes differ from any later build, and
`v0.0.1-preview.20261118.windows.arm64.1` was published by run 44224690103 after approval, replayed from public bytes, and signed with the owner's
publisher key (verified against the pinned key)
([evidence](docs/acceptance/release_qualification.md#first-windowsarm64-dev-sdk-preview-s7-steps-2-3-2026-11-18)). crt-cli still cannot install it until
its own first ARM64 bootstrap is published; the pins and the exception's end are S7 steps 4-6.

### Adopt the first automated Dev SDK Preview in the CLI CI (S5)

crt-cli#17 moved only the Linux/x86_64 SDK pin to `v0.0.1-preview.20261118.linux.x64.1` after publish and signature; pin check `OK` for all hosts, `CI gate` and the `main` push run green, both Ubuntu legs 913 / 221 / 114 / 142 with the signature verified ([CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/ci_acceptance.md#linuxx86_64-sdk-pin-moved-to-the-first-automated-preview-2026-11-18)); S5 is closed.

### Publish the first automated Dev SDK Preview (S4)

`v0.0.1-preview.20261118.linux.x64.1` was built, verified and published by `sdk-release.yml` (run 44088028759) after reviewer approval; its public bytes
re-verify, and the owner's offline publisher signature was verified against the pinned key and attached
([evidence](docs/acceptance/release_qualification.md#first-automated-dev-sdk-preview-s4-2026-11-18)); S4 is closed. The consumer pin (S5) is not yet updated.

## 2026-11-17

### Add the manual, approval-gated Dev SDK release workflow (S3)

`.github/workflows/sdk-release.yml` plans, packages, verifies on a separate runner (S2 smoke on the packaged bytes) and publishes through
`tools/ci_sdk_publish.py` behind the `release` environment, with `dry_run` true by default; the plan refuses a run whose environment has no required
reviewers. Writing the tests found and fixed trailing-newline acceptance (`$` vs `fullmatch`) in the verifier's version and commit checks and a notes
template whose own comment contained an unfillable token. The fake-`gh` flow tests pass and a hosted Linux/x64 dry run passes through the reviewer approval
([evidence](docs/acceptance/release_qualification.md#hosted-sdk-release-dry-run-s3-2026-11-17)); S3 is closed.

### Add the Dev SDK consumer smoke by the published CLI Preview (S2)

`tools/ci_sdk_cli_smoke.py` downloads the CLI Preview pinned in `.github/ci/sdk-pins.json`, checks its size, SHA-256 and publisher signature
(`openssl`, pinned key fingerprint), installs the SDK just built from its manifest pin, and runs C and C++ `new`/`build`/`test`/`run` plus a
warm rebuild that must reuse the cache. It is a step of every `sdk-integration` host. Locally, on Linux/x86_64, the published
`v0.1.0-preview.20261116.linux-x86_64.1` CLI passes against a locally built CI SDK. SDK integration run 44047616943 passes the smoke on all four hosts
([evidence](docs/acceptance/release_qualification.md#hosted-cli-consumer-smoke-s2-2026-11-17)); S2 is closed.

### Preserve the Windows SDK consumer's inherited test ACL

Reproduced SDK Integration #8 on Windows/x64 with Python 3.12.10 and the
actual CI SDK archive. Create the test PKI directory under the restricted
workspace so it inherits the run SID grant. Both C++ link modes pass HTTPS,
inflate and trust-rejection checks while the producer and original SDK stay
inaccessible. SDK Integration #9 passes the full hosted matrix, including the
Windows external consumer, and closes S1. [Local and hosted evidence](docs/acceptance/release_qualification.md#windows-sdk-integration-8-local-replay-2026-11-17).

### Keep macOS Dev SDK tests from overriding Apple libc++

Replayed `crt-dev-release` at `1c441e9` on macOS 27.2.1/Xcode 27.2. The full local producer
and byte verification pass; curl's configure executes the mbedTLS probe successfully.
A typed-allocation control reproduces the hosted dyld symbol failure under the old port-test
environment. Use fallback-only search for port tests and no dyld search override in the
relocated Dev consumer. Static/shared port and Seatbelt consumer replays pass against the
same SDK bytes. [Hosted macOS](docs/acceptance/release_qualification.md#hosted-macos-confirmation-and-core-ci-fixture-follow-up-2026-11-17)
also passes at `eb6acb1`. Core CI #513's Windows-only assertion came from mixed
path representations in the new unit test; normalize the fixture without
changing production lookup. The separate Windows SDK consumer remains open.

### Dev SDK integration workflow and the independent asset verifier

Planned the Dev SDK GitHub CI/CD (S0-S6 in TODO) and implemented its first part: `tools/ci_sdk_release.py verify` (stdlib, imports nothing of the producer),
`.github/workflows/sdk-integration.yml` (four native hosts, nothing published, not a required check), `tools/test_sdk_workflows.py` pin rules and the guide
[sdk_ci](docs/guides/sdk_ci.md). Local Linux/x86_64 evidence and its limits: [release qualification](docs/acceptance/release_qualification.md#dev-sdk-integration-workflow-2026-11-17).
No SDK, runtime or ABI change.

## 2026-11-12

### Remove completed SDK/CLI gates from the active work queue

Cross-checked the completed Tranche 1-4 entries against the bootstrap/source-asset, host-tools, D2 and bounded D3 acceptance records.
Removed 15 checked items and their completed summaries from TODO; the original evidence and dated completion records remain here and in acceptance.
Preserved Windows/x64 private-DLL and Windows/arm64 independent-tools/D3 qualification as explicit unchecked follow-ups, and repaired the host-tools work-queue link.
Tranches 5-8 and deferred self-upgrade 3.3 keep their existing identifiers and scope. No STATUS, runtime, SDK or consumer adoption change.

## 2026-11-11

### Synchronize the public Preview documentation

Compared the SDK production/public-download/consumer records and the CLI-owned release evidence with GitHub's public asset inventory.
README now includes Linux/aarch64 SDK and CLI downloads; the installation guide links the newly published native CLI rather than requiring a source bootstrap.
At the owner's request, refreshed STATUS to the exact reviewed snapshots, Linux/x86_64 loader-repair `.4` and the four published native SDK/CLI pairs.
Updated the stale README workflow and TODO's pin-adoption scope; retained Windows/arm64, pure C shared, signing and Full-delivery limits.
This is documentation reconciliation, not new native execution, artifact publication, pin adoption or a rewrite of historical acceptance evidence.

### Fail the Linux/aarch64 configure when compiler-rt is missing

The source-tag CI of the Linux/aarch64 Preview was red only on linux-arm64: the tag's workflow predates `422b45a` and installed LLVM without compiler-rt, so `printf`/`gdtoa` could not
link the binary128 helpers (`__netf2` and friends) and the break surfaced as undefined references in `shell/sh` and `shell/mksh` after a successful configure. `main` already installs
`libclang-rt-21-dev` and passes. Native Linux/aarch64 configures without the archive now fail immediately with the package name and `CRT_COMPILER_RT_BUILTINS_OVERRIDE` (cross configurations only warn);
`tools/test_compiler_rt_configure.py` pins it. The tag is not moved.
[Record](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-dev-sdk-public-promotion-2026-11-10).

## 2026-11-10

### Publish the Linux/aarch64 Dev SDK Preview

Promoted `v0.0.1-preview.20261110.linux.arm64.2` from clean `e6a94ab` (it includes the Linux TLS-descriptor and thread-destructor repair; the earlier aarch64 candidate `.1` predates it and was never published).
C 147/147, the shared-loader regression, `verify_dist --profile dev`, the relocated external check and a CLI self-built from the SDK (913/221/114/142 checks) pass; after the pins were frozen in a reviewed commit the
tag was created at the build commit, six unchanged assets went through a draft prerelease, and an unauthenticated public download matched 6/6 and passed the 30/30 native consumer replay. The contract
replay driver's source guard now ignores data fixtures. Glibc 2.39 and a Linux/aarch64 CLI executable are not covered.
[Record](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-dev-sdk-public-promotion-2026-11-10).

### Publish the Linux Dev SDK loader-compatibility repair

Published immutable `v0.0.1-preview.20261110.4` from clean `448fa32` after
rebuilding Linux/x86_64 runtimes/ports/source assets. TLSDESC removes the explicit
loader import that failed before main on glibc 2.39; CRT_1.0 remains intact.
An owned libc++abi build adapter also keeps weak GNU thread-exit registration
out of CRT clone threads. Both loader versions and GNU-libc/Wayland coexistence
pass TLS/destructor/exception checks. C 147/147, Dev external checks, local CLI
913/221/114/142 and 14/14 public-byte consumer checks pass. All six public
assets and four source files match the pre-publication pins. The old `.3`
release remains with an erratum; no project lock or other-host SDK changes.
Both required Ubuntu 24.04/26.04 CLI jobs also pass the full hosted workflow
on this same public SDK. [Acceptance](docs/acceptance/sdk_split_acceptance.md#linux-loader-compatibility-repair-2026-11-10)
owns exact hashes, preserved failed candidates, CI provisioning and replay limits.

### Windows/x64 CLI Preview publication handoff

The CLI consumes the unchanged public Windows Dev SDK, qualifies restricted HTTPS redirects
and real GitHub acquisition, and publishes its Windows/x64 package with notices and explicit
manual-pin trust. Linked consumer-owned evidence and refreshed README, STATUS and install
procedures at the owner's request; no producer runtime/SDK/format/default-lock change.
[Exact consumer checkpoint and scope](docs/acceptance/sdk_split_acceptance.md#windows-cli-preview-publication-handoff-2026-11-10).

### Acquire the macOS Dev SDK Preview from GitHub with the published CLI Preview

The `crt-cli` macOS/arm64 Preview (`v0.1.0-preview.20261110.macos-arm64.1`) was built from its sources by the published macOS SDK alone and, packaged, acquires that SDK directly from the
release URLs through the restricted `github-release` redirect policy (three redirects to `release-assets.githubusercontent.com`; manifest, policy and archive pins matched, installed
identity equals the promoted SDK). No SDK asset, tag or pin changed. [CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#macos-cli-preview-release-2026-11-10).

### Publish and promote the Windows Development SDK Preview

Built a clean Release Dev SDK `v0.0.1-preview.20261110.windows.1` from `b31276a` (CTest 169/169, symlink-free ZIP, 73 MB), froze its manifest pin
`b0fca4c8...6976`, trust policy, guide and notes in a reviewed commit, tagged the clean build commit and published a GitHub **prerelease** through `gh` (draft, digest check, authenticated
re-download, publish). An unauthenticated download verifies 6/6 against the pins, and the committed Windows CLI consumes the public bytes 31/31 under a restricted token. No asset was
rebuilt; no signature, Authenticode or default channel is claimed. [Evidence](docs/acceptance/sdk_split_acceptance.md#windows-dev-sdk-public-promotion-2026-11-10).

### Publish and promote the macOS Development SDK Preview

Published `v0.0.1-preview.20261110.macos.1` as a macOS/arm64 Dev prerelease at the clean producer `445f127`, built Release with Xcode's Apple clang 21.0.0 on macOS 27.2.
The manifest pin, trust policy, notes and guide were frozen in a reviewed commit before publication; the release was staged as a draft with the unchanged collector
assets, verified, then published as a non-latest prerelease through `gh`. Unauthenticated public downloads match the pins, and a `crt-cli` built from its sources by this
SDK alone installs and builds C and C++ projects from the actual public bytes without configure (30/30 under Seatbelt). The Linux Preview is untouched. Reaching
a self-build exposed two stale replay guards (a JSON fixture's documented `_WIN32`, the private lock helper's location); the header now lives under `include/`.
[Promotion acceptance](docs/acceptance/sdk_split_acceptance.md#macos-dev-sdk-public-promotion-2026-11-10) owns the release and receipt hashes. CLI redirect-aware
acquisition, a published CLI executable, Developer ID signing, Full and other-host packages remain separate; no STATUS or default-lock change.

### Publish and promote the Linux Development SDK Preview

Published `v0.0.1-preview.20261110.3` as a Linux/x86_64 Dev prerelease at the
original clean producer `0ab8f8c`. A reviewed manual-pin/TLS policy and immutable
manifest pin precede publication; no signer/default-channel authority is inferred.
All original collector assets and source/tag pins are retained. Authenticated
draft and unauthenticated public downloads match; the actual public bytes pass
native C/C++ installation/first-build consumers. [Promotion acceptance](docs/acceptance/sdk_split_acceptance.md#linux-dev-sdk-public-promotion-2026-11-10)
owns the release/receipt hashes, evidence and remaining CLI boundaries. Current
CLI follows no redirects, so this release qualifies manual HTTPS download plus
pinned native install; GitHub acquisition and CLI binary delivery remain next.
No Full/other-host artifact, default SDK migration or routine STATUS change.

### Qualify the clean Linux Dev SDK Preview handoff

Built clean producer `0ab8f8c` as local version `v0.0.1-preview.20261110.3`,
retaining earlier failed inputs and rebuilding the corrected source assets.
C/C++/Dev/host-tool entry points, actual archive/profile/portable-contract
collection, external hidden-source consumers and nested source-recipe checks
pass. [Producer acceptance](docs/acceptance/sdk_split_acceptance.md#linux-dev-preview-candidate-rehearsal-2026-11-10)
owns exact artifacts and results; [CLI Preview acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#linux-local-candidate-2026-11-10)
records the fresh consumer's final binary/package and first-user replay.
The input-rehearsal part of 7.3 is complete; source publication, official trust,
asset promotion and consumer pin adoption remain open. No runtime/public ABI,
default lock, Full/Web build or routine STATUS change.

### Repair the explicit native libc++ smoke entry point

The Linux Preview rehearsal configured explicit `x86_64`, exposing that CMake's
shared libc++ argument list passed `--target-arch` to a smoke driver that did
not accept it. Accept the explicit native architecture, pass it to the wrapper,
and reject foreign execution before creating output. Also give the project-owned
EH-register fixture's `main` C linkage for freestanding Linux consumers; its
Windows vector-register checks are unchanged. Static/shared runtime, string-ABI
and iostream smokes pass with the same SDK bytes; foreign aarch64 is refused.
Linux's Win64 vector-register branch remains not applicable. Both files ship in
the 02-cxx source asset, so the final Preview needs newly generated source assets
at this clean producer baseline rather than relabeling the earlier candidate.

### Keep the ucontext regression valid in optimized builds

The clean Linux Preview rehearsal exposed a hanging `ucontext_test` at `-O3`:
its automatic resume counter could remain in a saved register, so `setcontext`
restored the pre-increment value on every pass. The same source passes at `-O0`;
making the counter volatile passes at `-O3` against the identical runtime.
Only the test marker changes; no public header, runtime assembly or ABI changes.
Bound the CTest case to 15 seconds. The original failed rehearsal and the three
compile/run variants are retained under `crt-preview-20261110/ucontext-repro/`
outside this checkout. A new clean producer baseline/candidate is required for
the Preview rehearsal; the failed SDK identity is not relabeled.

### Approve and close the bounded D3 source-port contract

Reviewed CRT `dfb83c4` / CLI `8ad11dd`, including the completed macOS common-change
replay. [Producer acceptance](docs/acceptance/sdk_split_acceptance.md#d3-adapter-contract-approval-2026-11-09)
approves the digest-keyed zlib roles, supported fields, generated-Makefile
workaround, OS policies, key/store and provider/pin boundaries for the exercised
Linux/x86_64, macOS/arm64 and Windows/x64 combinations. The
[CLI adoption ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#d3-bounded-adapter-adoption-2026-11-09)
records the metadata-only adoption and focused key/existing-item/pinned-consumer
proof. No producer runtime, wrapper, recipe, SDK, ABI or format changes.

Tranche 4.1/4.2 close in that bounded scope. Windows/arm64 D3 and the unendorsed
Linux C shared SDK-provider policy remain separate; unchanged host proof is
retained at its original inputs. The next focus is clean Linux Dev SDK + CLI
Preview input qualification, with dirty developer fixtures kept as technical
regression inputs. Full delivery and D4 do not block that preparation. No SDK
promotion, production lock, public release or routine STATUS update is made.

## 2026-11-09

### Close the macOS affected-common-code D3 replay

The CLI rebuilds Linux's exact `22720f7d...bee16` product/test snapshot with the retained clean macOS rehearsal.7
SDK, not Linux's dirty fixture. All seven requested suites and 29/29 source-hidden/bare/command/cache/inventory gates
pass, with actual port/SDK mapping and nine-image binding proof. No producer implementation, SDK, recipe, ABI or
artifact changes were needed; completed Tranche 4.2's native macOS proof and linked the
[handoff](docs/acceptance/sdk_split_acceptance.md#macos-d3-affected-change-closure-replay-2026-11-09).
Formal CRT adapter approval/adoption, Windows/arm64 D3 and release decisions remain separate; STATUS stays unchanged.

### Record Linux D3 common-change proof and the macOS closure handoff

The public-SDK-only CLI replay passes seven input-enabled suites and 29/29
isolation/publication/reuse/inventory gates without producer/product/recipe
changes. Its [record](docs/acceptance/sdk_split_acceptance.md#linux-d3-common-change-closure-and-macos-handoff-2026-11-09)
links exact consumer-owned hashes/results and preserves the retained dirty
SDK/local host-tools provenance. The historical C shared SDK control maps host
libz and is explicitly not endorsed SDK binding; policy/runtime-search follow-up
6.4 remains open. Linux's affected delta is recorded; the owner performs native
macOS proof before final table adoption/D3 closure. No runtime/Full rebuild,
artifact promotion, root/default lock or binary refresh.

### Align D3 closure and Dev SDK/CLI preview planning

At the owner's request, refreshed the SDK/CLI part of STATUS, README/TODO and
producer contract against CRT `21b9d0e` / CLI `2ae74d4`, preserving unrelated
subsystem scope and original execution/approval records. Formal adapter review
and latest common-change Linux/macOS proof remain distinct from existing
three-host zlib evidence. Preview starts with Dev SDK + CLI; Full delivery is
separate and D4 does not extend D3. The [closure review candidate](docs/acceptance/sdk_split_acceptance.md#d3-closure-review-and-affected-host-plan-2026-11-09)
records pins/ownership; Linux execution and the owner's macOS handoff are
recorded separately when complete. No SDK/recipe/runtime/promotion change.

### Windows/x64 CLI source-port implementation handoff

Reviewed the CLI Windows adapter against the unchanged rehearsal.10 public SDK and
independent host tools. Fresh isolated consumer replay covers static/shared zlib,
C/C++ provider/lock proof, PE/store failures and recovery, and the packaged oracle.
No producer implementation/artifact changes were required. Formal table review and
other-host common-change replay remain open; STATUS is unchanged.
[Consumer commit and owning evidence](docs/acceptance/sdk_split_acceptance.md#windowsx64-cli-source-port-implementation-handoff-2026-11-09).

### Windows/x64 replay of the Linux/macOS D3 increment

Reused the clean rehearsal.10 public SDK after confirming the later producer delta
is documentation only. Corrected the Windows contract driver's source/header guards;
CLI common regressions and SDK-free/PE checks pass. Input-enabled CLI D3 suites
compile after fixture corrections but confirm unsupported Windows execution/layout/PE
audit; no producer runtime, recipe or SDK change or D3 acceptance is claimed.
[Producer boundary and consumer-owned evidence](docs/acceptance/sdk_split_acceptance.md#windowsx64-d3-public-sdk-replay-boundary-2026-11-09).

### Fresh macOS Dev SDK and CLI D3 source-port replay

A clean detached producer at `2f4103d` built local `v0.0.1-rehearsal.7`; Dev verification and its static/shared
HTTPS+inflate/trust-negative consumer pass. The public SDK was handed off outside the producer trees for the CLI's
native macOS zlib adapter replay: CRT host-tools configure/make, separate immutable port store, static/shared C/C++
consumer/provider/lock proof, Mach-O audit and packaged-oracle differential. No CRT implementation or recipe changes
were required. The source/SDK inventories stay unchanged; exact producer inputs and the CLI-owned evidence are linked
from [SDK acceptance](docs/acceptance/sdk_split_acceptance.md#macosarm64-d3-public-sdk-handoff-2026-11-09).
Formal adapter-table approval, Windows D3, affected Linux replay and release promotion remain open; STATUS is unchanged.

## 2026-11-06

### Document the Windows/x64 SIGSEGV fix in producer acceptance and the D3 producer obligations

Review of the pulled macOS/Windows replays. `docs/acceptance/sdk_split_acceptance.md` gained the fix section and marks its two older "stays open" notes as superseded; `docs/design/sdk_development_contract.md` gained the D3 source-port adapter producer obligations (adapter-table review, `crt-cc` under the host-tools `mksh`, SDK zlib untouched). Linux was checked on paper only: the libunwind patch `find` text occurs once in the pristine pinned `Registers.hpp` and the added code is `_WIN64`-only; no Linux rebuild was run. Design only, no implementation.

### Fix the Windows/x64 concurrent-install SIGSEGV: libunwind dropped XMM6-XMM15 on catch

Root cause of the crash recorded at the first Windows receipt replay: the pinned libunwind copied only general registers into
its cursor, so a catch resumed with indeterminate callee-saved XMM6-XMM15; a zeroed XMM6 that clang hoists for
`std::basic_string` initialisation then produced a string with a garbage size, and path conversion wrote past the stack or freed
a wild pointer. The wait loop of `sdk install --lock-wait` throws and catches per poll, hence the intermittent, layout-dependent
failure. Evidence: a live lldb stop at the access violation (write at stack top + 4 inside `wstring::resize` from
`path::path<char[6]>`); the cursor's XMM6 slot garbage at `jumpto` while the live register was intact at `__unw_getcontext`;
a minimal probe losing XMM6 in 400/400 catches before and 0/400 after; a probe crashing 16/300 before and 0/300 after; and the
original crt-cli objects relinked with the diagnostic allocator, in held-lock pairs: 3 of 4 unfixed pairs crashed (exit 139),
0 of 4 fixed pairs did.

Fix: a `src/Registers.hpp` patch in the libunwind recipe plus `imported_libcxx_eh_registers_test.cc` (600/600 catches lose
the registers without the patch, none with it). A fresh `crt-libcxx-smoke` build through the recipe applies the patch and passes the
new test for static and shared linkage. A Development SDK rebuilt from the committed fix (`v0.0.1-rehearsal.10`) and a CLI self-built against it pass the
Windows driver 101/101 three times, selection 68/68, first build 86/86 and pinned HTTPS 74/74; the CLI-owned
[record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsx64-concurrent-install-sigsegv-fix-2026-11-06) has the identities. Other targets are
unverified. The diagnostic allocator relinks and probes are local evidence under the ignored `out/sigsegv-b/`.

### Replay receipts, selection, first build and pinned HTTPS on Windows/arm64

Consumer-side replay against the existing clean-commit rehearsal.7 SDK (CRT `0631131`; the producer delta to `a00c6be` is the macOS-only host-tools dylib work, its replay script, tests and `AGENTS.md`, none shipped in the SDK): the CLI self-built from
`160a4a6` passes the installer/receipt driver 101/101, selection 68/68, first build 86/86 and pinned HTTPS 74/74 on the first run with the existing Windows drivers. No producer, runtime or PAL change was needed. Fifteen repeated
concurrent installs also succeed, as supporting evidence only: the Windows/x64 native crash is neither reproduced nor closed. The
[CLI-owned record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#selection-first-build-and-pinned-https-replay-windowsarm64-2026-11-06) has the details; the
[producer-side note](docs/acceptance/sdk_split_acceptance.md#windowsarm64-receipt-selection-first-build-and-https-replay-2026-11-06) records the SDK reuse.

### Replay selection, first build and pinned HTTPS on Windows/x64

Consumer-side replay against the existing rehearsal.8 SDK (CRT `436b4f2`; the delta to `4db4c85` is host-tools and docs only): selection 68/68, first build 86/86, pinned HTTPS 74/74 and the
installer/receipt driver 101/101 with a CLI self-built from `ea9b8cd`. No producer, runtime or PAL change was needed; the original envelope without top-level `format` is accepted. A junk DLL in the
caller's working directory shadows the shared runtime on Windows (documented, not defended). The [CLI-owned record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#selection-first-build-and-pinned-https-replay-windowsx64-2026-11-06) has the exact identities and harness findings.

### Close the macOS host-tools private-dylib replay slice

Rebuilt the existing AWK import with the retained public SDK's shared C policy
and assembled a separately identified, unpromoted host-tools fixture. Native
relocation/loader/collision/missing-or-modified-library replay passes 27/27.
Fixed copied Mach-O aliases' private closure by adding pinned internal runtime
links, and reject non-native or non-relocatable private dependencies before
publication. The earlier static bundle's complete payload/inventory stays
identical; 51 focused producer regressions pass. No upstream source, original
SDK, runtime ABI or global loader setting changed.

The [producer record](docs/acceptance/host_tools_acceptance.md#macosarm64-private-dylib-closure-replay-2026-11-06)
owns exact fixture hashes and retained failures. It records that sandbox-exec
strips DYLD variables: native test-only post-boundary injection now proves the
working poison control and CLI child filtering. The prior selection/first-build
replays are strengthened with that same mechanism and pass 66/66 and 86/86;
their original TLS/native receipts are retained, not rerun or relabeled.
Windows private-DLL/arm64 and production promotion remain separate gates.

### Replay SDK selection, first build and pinned HTTPS handoff on macOS/arm64

Reviewed CRT `82466be` and CLI's latest Linux increments `8788b59`, `4f55a52`
and `1dc3c03`, reusing clean CRT `784389b` Dev and its original rehearsal.6
release inputs. Native Seatbelt replay passes selection/collisions 66/66,
first build 86/86, HTTPS acquisition 73/73 and failure/retry 12/12, including
actual static/shared consumers and their Mach-O binding audit; the corrected
candidate/native fixture also passes 922 native checks. The replay
found a consumer-only envelope assumption: CRT release manifests have no
required top-level `format`; archive `install.format` owns version 1. CLI now
uses that existing contract and tests rejection of unsupported install metadata.
No producer runtime, schema or SDK bytes changed. The
[producer replay record](docs/acceptance/sdk_split_acceptance.md#macosarm64-selection-first-build-and-https-handoff-replay-2026-11-06)
links exact consumer inputs, retained failures and scope. Windows gates,
pure C shared-policy audit and public trust/promotion remain open; no lock,
committed binary, default or STATUS was refreshed.

### Exercise the explicit pinned HTTPS developer SDK handoff

Confirmed existing format-1 release assets can feed CLI-owned source format 0,
native HTTPS acquisition, immutable installation and first build. The
[Linux consumer evidence](docs/acceptance/sdk_split_acceptance.md#linux-pinned-https-sdk-handoff-2026-11-06)
records exact retained dirty developer inputs, TLS failures, installed-identity
reuse, live/dead fetch recovery and unchanged project pins on failure. Closed
bounded technical handoff 3.4 without new producer code/schema/artifacts. Public
trust/default URLs, actual promotion/preview assembly and other-host replay
remain separate work; existing Windows and C shared-policy gates are unchanged.

### Record the Linux installed-SDK first-build workflow

CLI now builds an unresolved native Dev project without a separate configure,
publishing exact existing-format pins after success. The [consumer slice](docs/acceptance/sdk_split_acceptance.md#linux-installed-sdk-first-build-consumer-slice-2026-11-06)
links SDK-only build, native regressions, isolated lock-preservation and
publication rollback evidence. Reused the dirty developer SDK/provider without
new producer artifacts. ELF review exposed the retained provider's static C
inputs under its shared policy; queued producer audit 6.4 rather than claiming
C shared acceptance. C static and actual C++ shared evidence stay separate.
Remote acquisition and other-host first-build replay remain open. No producer
code, SDK/lock/default promotion or STATUS update is involved.

## 2026-11-05

### Record Linux SDK selection collisions and the consumer identity fix

Reviewed the bounded CLI 3.2 slice against producer `d856a00`, reusing the
identified dirty developer SDK without rebuilding runtime artifacts. CLI fixed
full-identity references being truncated to the store's 12-digit suffix and
validated competing SDK selection, unchanged locks and hostile search paths
with actual static/shared consumers. The [producer record](docs/acceptance/sdk_split_acceptance.md#linux-installed-sdk-selection-collision-slice-2026-11-05)
links consumer-owned identities and evidence. No shared schema or producer
code changed. Affected-host collision replay and the Windows native crash
remain open; first-build/acquisition continues under its existing queue.

### Replay Windows/x64 receipt boundaries and retain a native concurrency blocker

Reused the clean `436b4f2` Development SDK and original rehearsal.8 release
inputs with CLI `51f1dc0` (receipt implementation `502992e`). SDK-only cold/warm
self-build, 113 Windows store checks, 14 producer fixtures and a real symlink
receipt probe pass. The isolated run records 93 passes before one concurrent
installer exits with SIGSEGV; a bounded continuation passes its remaining nine
checks. These are separate outcomes, not a clean full replay. Recorded the
[evidence and open investigation](docs/acceptance/sdk_split_acceptance.md#windowsx64-installation-receipt-and-repair-replay-2026-11-05);
CLI failure reporting now retains empty/crashed child results without raising
a Python sorting exception. No runtime/SDK, committed binary, lock or STATUS
change. Windows concurrency closure and Windows/arm64 delta replay remain open.

### Replay installation receipts and pinned repair on macOS/arm64

Validated the CLI receipt increment `502992e` against the producer handoff
`c3daf08`, reusing the clean `784389b` Development SDK and its original
`v0.0.1-rehearsal.6` release inputs. SDK-only CLI cold/warm self-builds,
114 store checks, 87 isolated installation/repair/project cases and the
14 producer metadata/exporter fixtures pass. No runtime or SDK change was
needed. The [scoped producer record](docs/acceptance/sdk_split_acceptance.md#macosarm64-installation-receipt-and-repair-replay-2026-11-05)
links CLI-owned identities and evidence, including a test-harness log-FD
failure retained before the passing run. Native Windows receipt replay and
installed-SDK collisions remain open; no binaries, locks, release or STATUS
were refreshed.

### Support the CLI installation receipt/pinned-repair handoff

CRT 3.1 reuses the existing release manifest's archive/install identity and
referenced portable-contract pin; CLI-owned receipts remain outside SDK bytes.
Recorded the [handoff and scoped proof](docs/acceptance/sdk_split_acceptance.md#installation-time-contract-pin-handoff-2026-11-05),
with unchanged metadata/exporter tests passing. The consumer now rejects
silent contract re-pinning and repairs only from verified original release
inputs. Linux tests use a labeled dirty-local SDK fixture; no runtime rebuild,
clean-release qualification, new producer schema or promotion is claimed.
Native macOS/Windows receipt replay and installed-SDK collisions stay open.

### Align SDK/CLI priorities and explicitly refresh the public snapshot

Reviewed CRT `433a7d6` and CLI `3978ced` with the owner. Retained the exercised
local install/portable-contract baselines, corrected stale project-selection
dependencies and moved Full metadata/general updates/self-upgrade outside the
first Linux user flow. The producer supports bounded local-store closure,
first-build acquisition and a parallel zlib adapter under
[change-impact gates](docs/design/sdk_development_contract.md#change-impact-and-first-user-workflow-adopted-2026-11-05).
Shared schema changes still require coordinated review and existing safety/ABI
requirements remain pre-merge. README and STATUS were explicitly refreshed;
this is planning/documentation work, with no new SDK build, runtime qualification,
artifact promotion or publication. Original native evidence stays in acceptance.

### Portable toolchain contract release on Windows/x64

Replayed the committed `436b4f2` natively: rehearsal `crt-dev-release` `v0.0.1-rehearsal.8` produces the archive, the portable toolchain contract (`os_sdk.kind` `windows-um-import-libraries`), the manifest and
checksums, all of which verify; `crt-c-test` 169/169. The CLI built itself from that SDK (919 native, 151 archive and 83 SDK-store checks) and its restricted-token D2 replay, including the contract and
project-selection cases, passes 83/83. No producer or CLI change was needed; the only download was the pinned `xz` source. This closes the Windows/x64 gap in the portable-contract record: every one of the five
native pairs has now run it. [Record](docs/acceptance/sdk_split_acceptance.md#windowsx64-portable-toolchain-contract-release-replay-2026-11-05).

### Portable toolchain contract release on Windows/arm64

Replayed the committed `0631131` natively: rehearsal `crt-dev-release` `v0.0.1-rehearsal.7` produces the archive, the portable toolchain contract (`os_sdk.kind` `windows-um-import-libraries`; this is the first Windows
collection of it), the manifest and checksums, all of which verify; `crt-c-test` 169/169. The CLI built itself from that SDK and its restricted-token D2 replay, including the contract and project-selection cases,
passes 83/83 (62 installer plus 21 contract cases). No producer change was needed; one CLI test assumed the new SDK was `list()[1]` although the store sorts by a target-dependent hash, and was fixed in crt-cli.
[Record](docs/acceptance/sdk_split_acceptance.md#windowsarm64-portable-toolchain-contract-release-replay-2026-11-05).

### Portable toolchain contract release on macOS/arm64

Replayed the committed `784389b` natively: rehearsal `crt-dev-release` `v0.0.1-rehearsal.6` produces the archive, the portable toolchain contract (`os_sdk.kind` `macos-xcode-sdk`), the manifest
and checksums, all of which verify; `crt-c-test` 146/146. The CLI built itself from that SDK and its Seatbelt D2 replay, including the contract and project-selection cases, passes 69/69
(a first driver run failed only on a working directory its own profile hid). No producer change was needed.
[Record](docs/acceptance/sdk_split_acceptance.md#macosarm64-portable-toolchain-contract-release-replay-2026-11-05).

### Portable toolchain contract release on Linux/aarch64

Replayed the committed `78ed80f` (the release now publishes the portable toolchain contract next to the Development SDK) natively on the aarch64 guest: rehearsal `crt-dev-release`
`v0.0.1-rehearsal.5` produces archive, contract, manifest and checksums that verify; `crt-c-test` 147/147. The CLI built itself from that SDK and its D2 replay, including the contract and project-selection
cases, passes 69/69 on the first run. No producer change was needed.
[Record](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-portable-toolchain-contract-release-replay-2026-11-05).

## 2026-11-04

### Tranche 3.1 on Windows/arm64: unwind tables for stack-probed frames, replay of the D2 installer

Replayed Tranche 3.1 and the CLI's D2 installer natively on the Windows/arm64 VM. The first producer (clean `e3deb32`) passed its gates, but the CLI's own archive and SDK-store
tests aborted on uncaught `crtcli::Error` exceptions that they expect to catch. Cause: with `-fhosted`, clang enables asynchronous unwind tables by default for aarch64-w64-mingw32 and
LLVM 22.1.8 then emits no CFA adjustment after the `__chkstk` probe of a large frame, so an exception unwinding through a ~149 KB frame (`extract_archive`) read its return address from
the wrong slot. The CRT wrappers now compile Windows/aarch64 with `-fno-asynchronous-unwind-tables` (`b20a460`, with a test pinning the flag per target); x86_64 and Linux/aarch64 were
not affected. With the clean `b20a460` producer (`rehearsal.6`: `crt-development-sdk-test`, `crt-c-test` 169/169 with one skip, `crt-dev-release`) the CLI self-builds from the SDK alone
(870 native, 151 archive and 66 SDK-store checks, cold 16/0, warm 0/16) and the installer replay passes 62/62. Full-stage metadata and promotion remain open.
[Record](docs/acceptance/sdk_split_acceptance.md#windowsarm64-tranche-31-and-d2-installer-replay-2026-11-04).

### Tranche 3.1 on Linux/aarch64: the `xz` port and install metadata, replayed natively

Detached worktree of clean `ffd0405`, rehearsal `crt-dev-release` with tag parameter `v0.0.1-rehearsal.4` (nothing uploaded): `crt-development-sdk-test` and `crt-dev-release`
pass, `crt-c-test` 147/147; the manifest carries install metadata format 1 (`tar.xz`, 2,797 entries including 17 symlinks, identity `0d11df55...8c48`). No producer change was
needed on this host. The CLI built itself from that SDK alone (cold 16/0, warm 0/16, no `DT_NEEDED`) and its installer replay passes 48/48 in bubblewrap. Evidence:
[this repository](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-tranche-31-and-d2-installer-replay-2026-11-04) and the CLI's
[D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#linuxaarch64-isolated-replay-2026-11-04).

### Tranche 3.1 on Windows/x64: Development-SDK `xz` port and install metadata

The first Windows Development SDK build from `a48000c` failed in three places that no earlier host exercised: `tools/build_dev_sdk.py` listed
`lib/lzma.dll` (which does not exist) instead of `bin/liblzma-5.dll` as the xz runtime artifact, so `verify_dist` rejected the SDK; the Windows shared
consumer in `crt-development-sdk-test` never linked `liblzma.dll.a`; and `crt_sdk_install_metadata.inventory` let Windows raise `OSError` rather than `ValueError`
for an escaping symlink target (its unit test failed only there). Fixed in [`f9dc911`](https://github.com/webos21/crt/commit/f9dc911) and [`1ee0379`](https://github.com/webos21/crt/commit/1ee037923ef6a3460c6f99c6e426d294fe5b7025). From clean `1ee0379`,
`crt-development-sdk-test` and `crt-dev-release` (tag parameter `v0.0.1-rehearsal.3`, nothing uploaded) pass natively with `crt-c-test` 168 of 169
(one skip); the manifest carries install metadata format 1 (2,917 entries, 0 symlinks, identity `075d5eb3...2c4d`). The self-built SDK-only CLI installs the zip and its isolated replay passes 62/62
(restricted token, hostile Windows names, kill during extraction, concurrency, `kill(pid, 0)` lock-owner liveness). Evidence is in the CLI repository's
[D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsx64-isolated-replay-2026-11-04) and [this repository's record](docs/acceptance/sdk_split_acceptance.md#windowsx64-tranche-31-and-d2-installer-replay-2026-11-04).
Not verified: Windows/arm64 and Linux/aarch64 replay, Full-stage metadata, promotion.

### Fix `access()` mode on Linux/aarch64

`__crt_sys_access` (`libc/src/arch/linux/aarch64/syscall.S`) forwards to `faccessat(AT_FDCWD, path, mode)` because aarch64
has no `access(2)`, but it cleared the third argument instead of carrying `mode` over, so every `access(path, R_OK|W_OK|X_OK)`
behaved as `F_OK` there. The bug was latent until the Windows `access` work added `access(<0666 data file>, X_OK) ==
EACCES` to `file_path_test`, which then failed only on the `linux-arm64` CI runner (from `b6bf52b`, 147 tests, one failure).
x86_64 uses `SYS_access` directly and was never affected. The other aarch64 wrappers with a shuffled argument list were
checked and are correct. Verified here only by assembling and disassembling the wrapper for `aarch64-linux`; the runtime
check is the existing `file_path_test` on the `linux-arm64` CI job.

### Tranche 3.1 on macOS/arm64: Development-SDK `xz` port and install metadata

Ran the real `crt-development-sdk-test` and `crt-dev-release` natively from the committed `53b1a80` in a detached
worktree (tag parameter `v0.0.1-rehearsal.2`; no Git tag, nothing uploaded). The macOS Dev SDK ships `liblzma` (static and
shared) and the dev release manifest carries install metadata format 1 (2,786 entries: 2,598 files, 178 directories, 10
symlinks; identity `80bfe046...e1da`); all `SHA256SUMS` entries verify. The consumer is the self-built SDK-only CLI of the
same SDK (16 sources, cold 16 executed / 0 reused, 807 native checks, 158 archive and 66 SDK-store checks), and its isolated
`sdk install|list|verify|recover` replay passes 48/48 under Seatbelt, including refusal of case-colliding names on the
case-insensitive default volume. The rehearsal from a committed tree closes that part of the Linux gap. Evidence and
details are in the CLI repository's
[D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#macosarm64-isolated-replay-2026-11-04).
No CRT code change was needed. Not verified: Windows replay and `lzma` import-library name, Full-stage metadata, promotion.

## 2026-11-03

### Tranche 3.1: Development-SDK `xz` port and install metadata (Linux/x86_64)

The Development SDK now ships liblzma (`xz` port, capability `archive-xz-1`; round-trip and damaged-input tests in the
consumer fixture, 147/147 core CTest) so `crt-cli` decodes tar.xz itself. To get there the `crt-cc`/`crt-c++` wrappers
add `-L$CRT_PORT_LIBRARY_DIR` only to link commands: on compile-only calls clang's unused-argument warning broke
`-Werror` autoconf probes (regression test in `test_link_rpath_first.py`). `tools/crt_sdk_install_metadata.py` defines
install metadata format 1 (see the [contract](docs/design/sdk_development_contract.md#install-metadata-format-1-crt-31)),
`crt_sdk_release.py` writes it into the dev release manifest, and `export_cli_toolchain.py` shares its identity rule
(on the real dev SDK the identity equals the provider's `sdk_content_sha256`). Verified: 5 metadata tests (counts agree
with real tar.xz and zip archives), prepare-release and export tests, and a CLI install of the real 186 MB dev SDK from
a manifest (2797 entries, 5.5 s, identical reinstall idempotent, `sdk verify` ok). A full `crt-dev-release` rehearsal (tag `v0.0.1-rehearsal.1`, scratch clone with the working-tree diff committed locally,
never pushed, cold build ~1 h) ended with `Release assets prepared locally; nothing uploaded`: the dev archive entry of
`release-manifest-dev-linux-x86_64.json` carries `install` (2797 entries, identity 143c10cd...), and
`crt-cli sdk install --release-manifest` installs and verifies that archive. A second rehearsal from the committed tree
(`53b1a80`, tag `v0.0.1-rehearsal.2`, identity 202303ca..., 2797 entries) also ended with `Release assets prepared locally; nothing
uploaded`; the CLI installed it from the manifest and the bubblewrap replay passed 48/48. Neither is a published release. Not verified: macOS/Windows replay, Windows `lzma` import-library name, Full-stage metadata.

### Tranche 3.1 Stage 2: portable toolchain contract in the Development-SDK release (Linux/x86_64)

`tools/export_cli_toolchain.py --portable` writes the contract without any machine path: on macOS and Windows the OS SDK
location becomes `@OS_SDK@` with `os_sdk.kind`, on Linux the output is identical to the unflagged export (checked on the
real SDK; only `exporter_sha256` differs). The unflagged export is unchanged for the replay scripts. `crt_sdk_release.py`
now exports it from the extracted archive, refuses an identity that differs from `install.content_sha256`, adds it as a
release asset (`crt-<v>-<os>-<arch>-dev-toolchain.json`) and names it in the archive entry (`contract`). Documented in the
[contract](docs/design/sdk_development_contract.md). Verified: 9 exporter tests (3 new, synthetic macOS/Windows paths), a
`crt-dev-release` rehearsal (`v0.0.1-rehearsal.3`, scratch clone with the diff committed locally, nothing uploaded) and
the CLI flow in the [crt-cli HISTORY](https://github.com/webos21/crt-cli/blob/main/HISTORY.md). Not verified: macOS/Windows
collection (needs Xcode / `CRT_WINDOWS_SDK_LIBPATH` at collection time) and a release from committed code.

### Close the Windows/x64 Tranche 2 replay

Replayed the single-executable CLI, the independently pinned host-tools bundle and the selected package CA on native
Windows/x64 against a dirty-local Development SDK (`9d8be12` base), after four producer repairs recorded below. The CLI
self-builds (13 executed, then 13 reused) and passes 857 native checks with only `kernel32` and one API-set DLL imported;
32 host-environment cases and 32 D1-07 HTTPS/signature cases pass under a restricted token with job process accounting;
the selected-project and previous-CLI bootstrap loops pass (stage binaries differ, no reproducibility claim). CLI product
inputs are unchanged from Linux/macOS. Receipts, hashes and the retained failures are in
[Windows host-tools acceptance](docs/acceptance/host_tools_acceptance.md#final-combined-replay-2026-11-03); the consumer ledger
is the [CLI Windows record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#windowsx64-independent-host-environment-replay-2026-11-03).
Private-DLL acceptance, Windows/arm64 and production adoption remain open.

### Respect explicit child environments in Windows rootfs bootstrap

The D1 hostile-environment replay showed private spawn bootstrap overwriting
an explicit child `CRT_ROOTFS` with the parent's value. Select the private
transport value from the supplied envp, falling back to the caller only when
envp is null, matching Bionic spawn's selection rule. Existing filtering still
removes stale private bootstrap entries. No public API or general environment
inheritance policy is added. The new native selected/omitted-rootfs regression
fails before the fix and passes afterwards; all 168 core cases pass with the
existing conditional fork skip. New C/tool and static/shared C++ candidates
verify independently; the final Development/CLI replay remains Tranche 2 work.

### Repair Windows shell async status and directory search for independent tools

The native D1 replay exposed two independent producer defects. The existing
Windows mksh adapter leaked its real-fork selection sentinel as background
status 255, incorrectly triggering `set -e`; restore its initial success status
without changing real `wait` results. The Windows PAL also rejected directory
`X_OK` as a non-executable PE image, causing mksh to discard its selected
TMPDIR and fail heredocs. Separate directory search from regular-file image
checks, preserving ordinary data-file execution rejection and public ABI.

Both new native C regressions fail before their fixes and pass afterwards;
all 168 core tests pass again with one existing conditional skip. The
[adapter patch record](shell/toybox/PATCHES.md#mksh-windows-asynchronous-launch-status)
owns its reason/removal condition. New tool candidates retain separate
identities; original CLI/Development artifacts and failed replay evidence are
preserved in [Windows acceptance](docs/acceptance/host_tools_acceptance.md#windowsx64-independent-bundle-replay-2026-11-03).

### Preserve pinned LLVM source bytes under Windows Git newline policies

Fresh Windows/x64 Tranche 2 production correctly rejected CRLF-rewritten
upstream caches. Scope LF-preserving Git settings to imported-libc++ fetch
commands, retaining long-path support and all pin/cache guards. Preserve failed
copies and recover 12,855 canonical files from retained pinned Git stores;
no upstream or global Git configuration is changed. An equivalent fix landed independently on `main` (`a8a4cb7`) while this
replay ran; the merged code keeps that version. All 16 fetch regressions
pass, including an actual ambient-CRLF primary/sibling checkout fixture.
The new local Development SDK and independent tools retain dirty provenance;
168 core tests plus the Development static/shared consumers pass, with one
conditional core skip. Exact inputs, failures and producer receipts belong to
[Windows host-tools acceptance](docs/acceptance/host_tools_acceptance.md#windowsx64-independent-bundle-replay-2026-11-03).

### Preserve pinned runtime bytes under Windows Git checkout defaults

CI run [41579703211](https://github.com/webos21/crt/actions/runs/41579703211/job/125731402511)
at `de4777d` passes Linux and macOS, but both Windows jobs reject
`cmake/Modules/HandleCompilerRT.cmake` in the canonical packaging fixture.
Reproduced on macOS with `core.autocrlf=true` / `core.eol=crlf`: fetch copies
CRLF bytes while the pinned Git object is LF. The fixture also now writes its
intended upstream LF explicitly, independent of Python's native newline mode.

Runtime-driver Git commands disable host CRLF conversion through command-local
`core.autocrlf=false` / `core.eol=lf`, preserving Windows long-path support and
leaving user/repository settings alone. Regression coverage proves exact LF,
genuine pinned CRLF and binary preservation, canonical packaging, and continued
rejection/preservation of CRLF drift. The canonical validator is unchanged;
old noncanonical caches still require a fresh source root, not relaxed hashes.

All 16 fetch tests pass both normally and with inherited CRLF defaults; 32
canonical fetch/source-stage tests pass. The existing CI tools selection runs
174 tests (167 pass, seven conditional skips), and the macOS target catalog
check passes. Original Windows logs, before/after reproduction and local test
logs are in `out/ci-windows-41579703211/`. This is macOS-hosted validation, not
new Windows native execution or a regenerated SDK/source-asset acceptance.
Remote Windows CI has not been rerun with this local fix. No runtime ABI,
upstream source, old artifact, consumer lock or STATUS change.

### Register independent host-tools targets in the public build catalog

CI run [41569195444](https://github.com/webos21/crt/actions/runs/41569195444/job/125697882509)
at `9d8be12` passes macOS's 146 core tests, then rejects the unlisted
`crt-host-tools-test`; all five jobs fail the documented-target check. Both
host-tools targets were already registered by CMake, but absent from the
catalog/README. Added them to the public list and Build commands, with a fixture
requiring both on Linux/macOS/Windows and preserving missing-target rejection.

Reproduced the original error locally, then passed the exact preset target
check and the separate producer-tree check. Twenty focused tests pass; the
unchanged CI offline selection runs 173 tests (166 pass, seven conditional
skips). Logs live in `out/ci-host-tools-catalog-20261103/`; target reports are
under `out/tranche2-macos-20261103/ci-target-catalog*-fixed.json`.
No runtime, classifier/check suppression, SDK or native replay status change.
Remote CI has not been rerun with this local fix.

### Replay Tranche 2 independent host tools and CLI on macOS/arm64

Assembled a new independent bundle at clean producer `518953f` from the retained
clean `8db69f4` public macOS C SDK; this does not relabel it as a new SDK build.
56 producer/cache/binding fixtures and first/cached CMake host-tools assembly
pass. The four tool images own their CRT allocator, import only libSystem and
have no RPATH/private dylibs. Exact provenance and limits belong to
[host-tools acceptance](docs/acceptance/host_tools_acceptance.md#macosarm64-independent-bundle-replay-2026-11-03).

Consumer `095afbc` passes fresh isolated self-build, previous-binary bootstrap,
807 native checks, 31 independent environment/resource cases, explicit selected-SDK
diagnosis/shell and 32 packaged-CA HTTPS/signature cases. Added macOS Seatbelt
test-host replay support; canonicalized the HTTPS work path after an initial
`/var` versus `/private/var` execution-policy failure. Failed evidence is retained.
Execution receipts and exact SDK/provider/package/binary identities are
[CLI-owned](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#macosarm64-independent-host-environment-replay-2026-11-03).
The Windows/x64 replay keeps Tranche 2 open. No product/runtime ABI, upstream,
SDK payload, production lock, release, committed CLI binary or STATUS change.

## 2026-11-02

### Implement independent host-tools for the Linux Tranche 2 increment

Added the separate local-only host-tools format/assembler and `crt-host-tools`
plus `crt-host-tools-test` entry points. A public native C SDK supplies mksh,
toybox, separate awk and make; the bundle preserves aliases and notices while
requiring no target SDK/C++ stack at runtime. Added pinned GNU make COPYING
and recipe provenance. Actual PT_INTERP inspection closes the C-only metadata's
missing loader declaration without changing runtime bytes or target SDKs.

21 producer/ELF tests and cold/cached CMake assembly pass. The consumer now has
scoped doctor, SDK-independent port-build/shell and one-executable resource
verification. Linux's 767 native checks, 31 isolated tools/resource cases and
32 package-trust HTTPS/signature cases are CLI-owned. Exact candidate identities,
source/notice provenance and remaining native Windows/macOS replay belong to
[host-tools acceptance](docs/acceptance/host_tools_acceptance.md). Tranche 2 stays
open for that new native matrix; no upstream/runtime ABI change, producer
promotion, production lock, public release or STATUS update was made.

### Close Tranche 1.7 with canonical common-source candidates

Fixed imported-libc++ cache handling in `9b5fab2`: the real TLS-selector file
accepts only pinned pristine/single-guard bytes, rejects repeated guards before
mutation, and remains unchanged under repeated application. Source packaging
validates whole upstream caches against pinned Git objects plus approved recipe
patches, emits canonical bytes/modes and input inventories, refuses named
candidate replacement and checks successor recipes against actual archives.

Fresh isolated builds exposed missing 02 loader/notices publication and
absolute RPATH handling; `c60bdeb`, `75f5340`, `cf646f0` reuse the ordinary
publisher contract without relaxing verification. `49879bf` repairs 03's
optional protocol-archive example dependency, target loader declaration and
owned ELF paths. That clean commit freezes the selected local
`v0.4.0-sourcefix.20261102.5` inputs. Earlier failed candidates remain retained.

77 focused tests, 147 core tests, actual 01->02->03 builds, static/shared C++
smokes, the packaged 03 example and Development consumers pass. The same 02
archive builds/links for aarch64 from this x86_64 host; it is cross-build evidence,
not ARM execution. Pristine/patched cache regeneration and full source/recipe
checks pass. Exact asset hashes, reproduction and scope belong to
[producer acceptance](docs/acceptance/release_qualification.md#common-source-candidate-selection-2026-11-02).
The original isolated CLI replay, bootstrap loop, exact SDK/provider/binary/tool
identities and local adoption are [CLI-owned](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#common-source-candidate-selection-2026-11-02).

Marked 1.7 complete and retained the checked Tranche 1 prerequisites. Next is
Tranche 2's independent host-tools/environment contract. This is technical
candidate selection, not production promotion, a lock update, a Git tag or
public release. STATUS is untouched; the code/input commits are local and have
not been pushed by this work.


### Close SDK/CLI technical bootstrap gates 1.5 and 1.6

The owner accepted the five native pairs' recorded SDK startup/link, basic
diagnosis/one-executable and relocated SDK-only build/run/ABI/import evidence.
Marked 1.5/1.6 complete and moved the detailed links to the
[technical gate review](docs/acceptance/sdk_split_acceptance.md#sdkcli-technical-bootstrap-gate-review-2026-11-02).
This is scoped completion at existing per-host combinations, not new runtime
execution, a shared latest revision or production adoption. Tranche 1.7 stays
open for canonical-source/cache repair, common asset/recipe generation and
final selection; broader D1 and release/promotion gates stay separate. No
runtime, SDK payload, consumer pin, committed CLI binary or STATUS change.

### Adopt common source-stage assets across architectures

The owner retained one source archive per version/OS/stage, with no architecture
suffix; binary SDK archives remain architecture-specific. The
[SDK contract](docs/design/sdk_development_contract.md#common-source-stage-assets-adopted-2026-11-02)
owns the common byte/hash identity and preservation rule; the guide, comparison
record and TODO now link it. This is a contract decision only: stale-cache
repair, new asset/recipe generation, affected validation and final candidate
selection remain open under Tranche 1.7. No archive, production recipe, runtime,
consumer lock or STATUS file was changed by this decision.

### Compare source-stage assets without conflating revisions or cache state

Added a read-only archive comparison tool and CI fixtures separating compressed
bytes, tar metadata/order, permissions, member payloads and embedded recipe
pins. Four user-supplied Linux archives have different clean project revisions
(`3741ea5` and `c4e19f3`), so they cannot resolve the original same-version
collision. The newer arm64 libc++abi member contains 24 repeated Windows guards
instead of the recipe's one; canonical upstream/recipe comparison and scratch
driver probes confirm preserved stale-cache state, not a CPU requirement.
Production recipes and old artifacts are unchanged. Focused tests: 93 pass,
two conditional skips; a real retained macOS archive/release copy also matches.
The existing CI tools selection also passes: 168 tests, seven conditional skips.
Tranche 1.7 stays open pending repair and final selection; the subsequent
common-source naming decision is recorded above.
[Evidence and limits](docs/acceptance/release_qualification.md#source-asset-comparison-checkpoint-2026-11-02).

### Replay the latest CLI on Windows/x64 with the spawn-environment fix

A fresh clean `b4c5b5a` Development SDK (contains `d12e210`; `crt-development-sdk-test` passes, `crt-c-test` 168 of 169 with one skip, 280 s) replays CLI `1499a7b` natively on Windows/x64 with 844 native checks, cold 11/0 and warm 0/11, bare PE and doctor audits, all 32 restricted-token HTTPS/signature cases and CRT host tools starting from a Unicode SDK path. Windows/x64 did not add `PROCESSOR_ARCHITECTURE` in a native Python probe, so the CLI's arm64 tolerance is unused there (one check fewer than arm64's 845). No CRT source change. [CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#windowsx64-latest-source-revalidation-and-spawn-environment-fix-2026-11-02).

### Replay the latest CLI on Windows/arm64 and stop CRT children seeing the bootstrap variables

Clean `ee0b5e3` (167 of 168 core tests, one skip) built a Development SDK, but the latest CLI suite failed on the child-environment
check: the OS adds `PROCESSOR_ARCHITECTURE=ARM64` to ARM64 processes (a CRT-free kernel32-only program shows it too), and the Windows
PAL left `CRT_CHILD_BOOTSTRAP`, `CRT_BOOTSTRAP_*` and `CRT_FD_SNAPSHOT_PIPE` in every CRT child's `environ`. `d12e210` removes them from the
child's own table after `__crt_child_bootstrap()` applies them (effects stay; the OS block that a fork relaunch inherits is untouched);
`windows_spawn_environment_test` fails before the fix and passes after, with the fork, owned-spawn and UTF-8 tests. A clean `d12e210` SDK
passes 168 of 169 core tests (one skip), and the CLI replays with 845 native checks, cold 11/0 and warm 0/11, bare PE and doctor audits,
and all 32 restricted-token HTTPS/signature cases, using the new `tools/test_cli_fetch_https_windows.py`. CRT host tools start from a
Unicode SDK path. Windows/x64 has not replayed the fix.
[Acceptance](docs/acceptance/sdk_split_acceptance.md#windowsarm64-latest-source-cli-replay-and-spawn-environment-fix-2026-11-02).

### Revalidate Linux/aarch64 SDK and latest CLI source boundaries

A fresh clean `c4e19f3` Development SDK passes 147 core tests, imported-libc++
static/shared smokes, profile checks and static/shared HTTPS/inflate consumers.
Exporter/profile fixtures pass 36 with two Windows-only skips. Unchanged CLI
`9e0110f` passes 766 isolated native checks, old-binary bootstrap to a
byte-identical fixed point, SDK-hidden ELF/mapping/env audits and all 32
controlled HTTPS/integrity/signature cases. No implementation change was needed.

Exact inputs, commands, artifact identities and native limits:
[Linux/aarch64 acceptance](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-latest-source-cli-replay-2026-11-02).
This closes this pair's latest-source replay, while Windows/arm64 process-fix
replay and coherent exact assets remain open. No SDK promotion, production
lock, public release or STATUS update is claimed.

## 2026-11-01

### Revalidate macOS/arm64 SDK and CLI boundaries at current sources

A fresh clean `8db69f4` Development SDK passes 146 core tests, imported-C++
static/shared smokes and static/shared transport consumers. Exporter/profile
fixtures pass 36 with two Windows-only skips. Unchanged CLI `a9d43c6` passes
732 isolated native checks, previous-binary self-bootstrap, import audits,
32 SDK-hidden HTTPS/signature cases and pinned-repository HTTPS.

No runtime implementation change was needed. Direct Xcode compiler use needs
explicit producer SDKROOT; initial failures are retained. CRT tool probes
retain the same direct make space-SHELL limitation as Linux and a passing
explicit temporary symlink bridge. Exact commands, provenance and limits:
[macOS producer acceptance](docs/acceptance/sdk_split_acceptance.md#macosarm64-latest-source-cli-replay-2026-11-01).
No SDK promotion, consumer production lock, release or STATUS change is claimed.

### Cancel owned Windows spawn trees, including descendants of reaped leaders

Windows PAL now attaches new owned spawn groups to private Job Objects before
resuming either CRT or native children. Group SIGKILL terminates the owned
tree; the separate registry and leader handle preserve identity after waitpid,
while failed/empty entries are reclaimed. Foreign group joining and catchable
signals remain unsupported rather than silently approximated.

A fresh local development SDK passes 167 core tests with one skip, the focused
ordinary/native/orphan/sibling/reuse regression, and static/shared transport
consumers. SDK-only CLI replay passes 788 checks, including
the original descendant timeout, and all 32 SDK-hidden HTTPS/signature cases.
Exact source/artifact identities, commands, failed attempts and host limits:
[Windows acceptance](docs/acceptance/sdk_split_acceptance.md#windowsx64-owned-spawn-cancellation-2026-11-01).
The exercised producer delta was uncommitted at build time; no SDK promotion, release, applet enablement
or STATUS change is claimed. Windows/arm64 replay remains open.
