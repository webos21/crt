# Full / Development SDK split acceptance

This record owns the current S2-S5 producer/consumer gates in
[TODO](../../TODO.md#crt-sdk-split-full-sdk-and-development-sdk). Content and
identity policy belong to the [SDK contract](../design/sdk_development_contract.md#full-sdk-and-development-sdk-adopted-direction-2026-10-22);
commands belong to the [distribution guide](../guides/distribution.md#development-sdk-producer-and-external-validation).
Original B4/B5 results remain in [build-system acceptance](build_system_acceptance.md#development-sdk-windows-replay).

## Remaining matrix

| Pair | Development producer / ordinary consumer | Producer-tree-hidden consumer |
| --- | --- | --- |
| Linux/x86_64 | Current producer and static/shared eight-case replay pass below | Pass below |
| macOS/arm64 | Current producer, archive/extraction, static/shared eight-case replay and shared-consumer binding audit pass below | Pass below, using the extracted archive |
| Windows/x64 | Current ports/package and static/shared eight-case replay pass below using an identified external `02-cxx`; earlier [Windows baseline](build_system_acceptance.md#development-sdk-windows-replay) retained | Pass below, using the extracted archive |
| Linux/aarch64 | Native producer, archive/extraction, static/shared eight-case replay and port recipe tests pass below | Pass below, using the extracted archive |
| Windows/arm64 | Native producer, archive/extraction, static/shared eight-case replay and port recipe tests pass below | Pass below under a restricted token, using the extracted archive |

S4's local collector implementation and negative/atomic fixtures are complete.
Actual clean/tagged positive collection remains the independent
[release qualification](../../TODO.md#release-qualification-and-assembly-rehearsal)
gate. No tag, upload, consumer lock or runtime adoption is part of this record.

## Windows libc++ long-path fetch recovery (2026-10-27)

The follow-up to the S3 failure below is resolved in CRT's host fetch driver,
not in upstream LLVM sources. `crt-libcxx-build.py` now delegates directory
copying to `crt_file_ops.copy_tree`, whose extended Win32 paths affect Python IO
only. Git/CMake/compiler arguments and the requested work root keep their
original spelling; no short alias, registry setting, global Git configuration,
source exclusion or recipe-pin change was introduced. Shared sibling trees use
the same path-safe copy. The `02-cxx` source asset already includes
`tools/crt_file_ops.py`; a regression asserts that packaging dependency. The
actual final source archive contains byte-identical current driver/helper files
(`libcxx-final-assertions.json`); recipe source SHA-256 is
`bc373028654ee77696f5773fad9112563dccffb25954d982eed5332edb2b3329`.

Each new source directory is copied in a private staging directory and published
only after the complete copy succeeds. Failed copies are not mistaken for
completed fetches, and copy failure leaves an existing destination intact. Fetch
also checks the presence of required shared sibling directories before taking
the already-fetched branch; a missing sibling recovers from the pinned clone.
This is per-directory publication, not an all-recipes transaction or a new
source-cache manifest format.

The legacy partial directory left by the old implementation was explicitly
preserved as
`producer/external/llvm-runtimes/libcxx.failed-copy-20261027` under the evidence
root below; it was not silently accepted as complete. Both resolved move targets
were checked inside that generated cache before the rename. The original
`.clone-libcxx` and failure logs remain. Recovery used locally cached LLVM commit
`37f38d1f3276b62fba09462ab4807dce846c732d`. All **five** previously missing source
files (original source paths 260..270 characters) now match the clone's SHA-256
values (`libcxx-restored-files.json`); no pinned test file was dropped.

The default C++ producer was restored and exercised in the **same** deep tree:

```powershell
cmake --fresh --preset windows-host-ninja-debug -B out/dev-sdk-windows-x64-20261027/producer `
  -DCRT_DEV_PREDECESSOR= `
  -DCRT_DEV_OUTPUT=C:/Users/Lee/DevWorks/crt/out/dev-sdk-windows-x64-20261027/producer/dist/dev-default-followup
cmake --build out/dev-sdk-windows-x64-20261027/producer --target crt-cxx-test crt-cxx-dist --parallel 8
```

Fetch, configuration, shared/static imported-runtime compilation and installation
succeed. Static/shared C++17 smoke, string-ABI boundary tests and iostream tests
pass, and `producer/dist/02-cxx` passes ordinary distribution verification.
There is no external-predecessor bypass in this replay. The distinct Dev output
path protects the earlier S3 SDK; a full new networking/Development producer
replay is **Not run** here. The S3 package still retains its original predecessor
identity and acceptance evidence; it is not relabeled to use this new C++ SDK.
This local C++ candidate records CRT `0b7456c` with uncommitted changes, not a
clean/tagged release or a new cross-host qualification claim.

The focused suite (`tools.test_libcxx_fetch`, `tools.test_libcxx_predecessor`,
`tools.test_crt_file_ops`) passes **26/26**. It covers real sparse Git primary and
extra directory copies with 350+ character paths, ordinary space-containing
work roots, preservation when copying a replacement fails, no partial publication, bounded
copy errors, destination confinement and cached-commit missing-sibling recovery.
The exact existing CI tooling selection passes **143/147**, with four
host-conditional skips; no second workflow/full build was added. Native
Linux/macOS replay and hosted CI of this fix are **Not run**.

Evidence root: `out/dev-sdk-windows-x64-20261027/`, including the original
`producer-entry.log` and `default-producer-failure.json`,
`libcxx-default-configure.log`, `libcxx-default-replay.log`,
`libcxx-default-final.log`, `libcxx-final-assertions.json`,
`libcxx-restored-files.json`, `libcxx-fix-unit-final.log` and
`libcxx-fix-ci-tooling.log`. The final assertions also confirm the earlier S3
Development SDK retains its original strict inventory. The resolved long-path
item is removed from TODO;
Windows/aarch64 source-hidden S3 and release qualification remain independent
open work. No SDK/profile schema, ABI, tag, upload or consumer lock changed.

## Windows/x64 source-hidden and package replay (2026-10-27)

Host: native AMD64 Windows NT 10.0.26100, Python 3.11.9, Clang 22.1.8 and
Windows SDK 10.0.28000.0 (`um/x64`). Reviewed CRT `0b7456c` plus the local
Windows acceptance-tool changes. This closes only Windows/x64 S3; aarch64 must
run the same procedure on its native host. No CPU emulation or WSL result is
substituted for that native execution.

### Producer and preserved failures

The old `out/build-system-development/cmake-dev` fails current ordinary
verification because it predates the required `tools/crt_relink_kit.py` payload.
It was preserved, not edited to pass. A fresh default CMake producer then failed
in `crt-libcxx-build.py:fetch_recipe`'s plain `shutil.copytree`: five pinned test
source paths could not be copied. The first source path has 261 characters;
normal `isfile` returns false while its Windows extended-path spelling exists.
`producer-entry.log` and `default-producer-failure.json` preserve this distinct
fetch/path defect. It was not fixed during that S3 replay or hidden by shortening
the work root, skipping source files or modifying upstream. The separate
[follow-up above](#windows-libc-long-path-fetch-recovery-2026-10-27) records its
subsequent fix and default C++ producer replay without relabeling S3 artifacts.

S3 continued through the already-supported external-predecessor entry point:

```powershell
cmake --fresh --preset windows-host-ninja-debug -B out/dev-sdk-windows-x64-20261027/producer `
  -DCRT_DEV_PREDECESSOR=C:/Users/Lee/DevWorks/crt/out/build-system-producer/dist/02-cxx
cmake --build out/dev-sdk-windows-x64-20261027/producer --target crt-development-sdk-test --parallel 8
```

The new SDK at `producer/dist/dev` under the evidence root below contains freshly
built static/shared zlib 1.3.1, Mbed TLS 3.6.7 and curl 8.21.0, plus current SDK
tooling. Ordinary and Development profile verification pass with dependency /
notice records and no `03+` payload. The ordinary SDK-only consumer passes all
eight HTTPS/inflate and explicit-trust cases. All **27** inherited runtime
digests equal the supplied plain predecessor. The current producer/port compile
identity is `0b7456c8bc9cd0176af17e2cacb63df95afb4b6d`, dirty; the predecessor
retains `1033c86e5c85a7bbe8594542ee890afda1cbb4da`. No core-runtime rebuild from
the current commit is claimed. The cache correctly declined to cache production
after acceptance-tool inputs changed during execution; no cache-hit claim or
guard relaxation was used.

### ZIP and relocated consumer

The existing archive helper, without a release collector, creates
`archives-final/crt-development-windows-x86_64-dev.zip` under the evidence root:

- Size: **144,976,001 bytes**.
- SHA-256: `d30507866893fdd5aa3e66a7cc530b56395766f5df9ab2502dbfa22880608369`.
- **2,695** payload paths; each extracted path has identical bytes to its source.
- Source link-aware inventory SHA-256:
  `4ce5cd3aee0776a8fe68eedb2aa55cf68f9d5f41f4cbe94070e4aa77de7474aa`.
- Extracted/externally relocated inventory SHA-256:
  `3575ceeb60102c3de03fa6e0781f4c2e1ba9dd54fae936ace5eb3541f7d2753f`.

The first comparison intentionally failed rather than silently equating links
with files (`package-replay.log`, `package-first-differences.json`). The existing
Windows ZIP helper materializes exactly two declared internal zlib file aliases,
`lib/libz.so` and `lib/libz.so.1`, targeting `libz.so.1.3.1`. Their extracted file
bytes equal that target; no other path/type/content difference is permitted.
The final replay records both strict inventories separately, checks payload
bytes at every path and checks each tree remains unchanged. The archive helper
and POSIX tar/link behavior were not changed. First archive/extraction evidence
is retained separately from the final attempt.

Extraction into `extracted verified SDK with spaces/dev` passes full
`verify_dist.py --stage 02-cxx --profile dev`, then an ordinary static/shared
consumer. It is relocated again outside the checkout for the actual hidden-tree
test:

```powershell
python tools/test_dev_sdk_external.py `
  --sdk-root 'out/dev-sdk-windows-x64-20261027/extracted verified SDK with spaces/dev' `
  --output out/dev-sdk-windows-x64-20261027/external-final
```

### OS-enforced original-tree denial and load evidence

The driver now supports native Windows through `crt_windows_external.py`.
Read-and-write restricting SIDs add a second OS access check, without changing
the checkout, original SDK, installed tools or user/account/machine policy. Only
a newly created private external directory receives a per-run synthetic-SID
ACL. A private copy of build-host Python (without user site packages), the
relocated SDK, reference C++ source and the same hashed **12-script** validation
capsule live there. No producer headers/objects, `build_dev_sdk.py` or
`crt-libcxx-build.py` are supplied outside the SDK. The bounded capsule no longer
needs a dynamic import of the full WebKit builder merely to convert a Windows CA
filename.

Before profile checks or compilation, the restricted process must fail directory
enumeration and direct reads of the original source, SDK manifest, header,
static libc and DLL. Fresh Python descendants must also receive `PermissionError`
on those files. The receipt records `descendant_denial_checked: true`. All child
processes inherit the restricted token and a kill-on-close job, run hidden and
receive no producer handles. Missing isolation, public-readable producer roots,
token/job setup failure or failed probes abort without an unrestricted fallback.
This is a trusted acceptance-fixture boundary, not a general hostile-code sandbox.

With this checkout and its entire `out/` inaccessible, fresh SDK-only C++17
static/shared consumers pass **eight** cases: verified HTTPS+inflate, wrong CA,
missing explicit CA despite ambient valid trust variables, and malformed CA.
Static linkage has no SDK DLL imports. The shared recursive closure contains
**ten SDK images** copied app-local, including the PE zlib image with its `.so`
spelling; successful execution with **System32-only PATH** proves imports resolve
at load. Both modes declare only `kernel32.dll` and
`api-ms-win-core-synch-l1-2-0.dll` as external imports. Original, extracted and
externally relocated inventories remain unchanged.

### Negatives, evidence and limits

Seven actual private-copy verifier negatives return failure: missing static
curl, missing shared curl, changed inherited libc, upper-stage content, missing
curl notice, missing dependency declaration and bundled/ambient trust policy.
Each mutation is restored and the private inventory matches its source again.
The focused Development suite passes **28/28**; the exact existing CI tooling
selection passes **136/140**, with four host-conditional skips. The broader
tooling/network-fixture selection also passes (its earlier run selected 143,
139 pass/four skips); no extra workflow or full native SDK CI build was added.
Hosted GitHub CI and native Linux/macOS replay of these changes are **Not run**.

Evidence root: `out/dev-sdk-windows-x64-20261027/`, retaining
`configure-{fresh,predecessor}.log`, `producer-{entry,predecessor}.log`,
`original-profile.log`, `default-producer-failure.json`, `package-replay{,-final}.log`,
`package-result.json`, `current-profile.log`, `extracted-profile.log`,
`consumer-extracted.log`, `external-final/{command.json,consumer.log,result.json}`,
`negative-*.log`, `unit-final.log` and `ci-tooling-final.log`. The successful
external work directory and launcher/host-Python/fixture hashes are in its
receipt. No native Windows/aarch64 replay environment was supplied for this run;
that S3 sub-gate stays open (closed later the same day by the
[Windows/arm64 replay](#windowsarm64-source-hidden-replay-2026-10-27)). Run the same driver there with native arm64 Python
and the matching `um/arm64` external import libraries; do not count x64 execution
or cross-compilation as arm64 acceptance.

This extends CRT-owned validation only: SDK/profile format, recipe schemas and
public ABI are unchanged. The consumer review baselines below remain unchanged;
CLI/Studio adoption, stale-Full native pairing and matching clean/tagged release
qualification are not claimed. No tag, upload or artifact promotion occurred.

## macOS/arm64 source-hidden and shared-consumer replay (2026-10-27)

Host: macOS 27.0.1 (26A434), native arm64, Apple Clang 21 and host Python 3.15.
Reviewed CRT `22cb03ead8ea73d3c78ea846088a458bf7f0356b` plus the local acceptance-tool
changes described here. First replayed the existing Development SDK successfully,
then regenerated the C/C++ predecessor and Development SDK through the ordinary
entry point:

```sh
cmake --fresh --preset macos-host-ninja-debug
cmake --build --preset macos-host-ninja-debug \
  --target crt-development-sdk-test --parallel 4
```

The old generated Ninja tree initially referenced the renamed `crt-cxx-dist`
without defining it; explicit fresh configuration resolved that stale build
graph. No CMake target definition, runtime implementation or upstream source
was changed for this replay. Fresh static/shared zlib 1.3.1, Mbed TLS 3.6.7 and
curl 8.21.0 were built through the existing shared producer. Profile verification
passes, with dependency/notice records and no graphics/media/UI/Web payload.
All **18** inherited runtime-file digests equal the newly generated plain
`02-cxx`. Ordinary static/shared HTTPS+inflate and explicit-CA negatives pass
all **eight** cases.

The existing `prepare_release_assets.archive_directory` helper created a local
`development` archive, without running a release collector. Extraction into
`out/dev-sdk-macos-20261027/extracted SDK with spaces/dev` passes
`verify_dist.py --stage 02-cxx --profile dev`. Original and extracted inventories
are identical:

- File: `archives/crt-development-macos-aarch64-dev.tar.xz` under the evidence root below.
- Size: **6,357,740 bytes**.
- SHA-256: `88dcfb1a67931f0c15d4a1273ea54f2e85fc7e91f7846732c95bc01cad3b4b35`.
- **2,570** file/symlink entries; inventory SHA-256
  `e634f3ac28dcb7e3a9c4b1d589d2ada29f09637f5f8fa6d11339d852dc438d74`.

### OS-enforced producer isolation

`test_dev_sdk_external.py` now supports native macOS as well as Linux. On macOS
it copies the package outside the checkout and invokes the validation capsule
through OS `sandbox-exec`. The inherited policy denies reads and writes below
the original SDK/producer roots, including this entire checkout and its `out/`.
It does not rename or chmod the shared checkout. The child probe verifies that
directory enumeration and direct reads of a known source file and the original
SDK manifest fail with `PermissionError` before building the consumer. Linux
retains its empty-mount bubblewrap policy. Missing tools or failed isolation
abort the gate; there is no unisolated retry.

```sh
python3 tools/test_dev_sdk_external.py \
  --sdk-root 'out/dev-sdk-macos-20261027/extracted SDK with spaces/dev' \
  --output out/dev-sdk-macos-20261027/external
```

The extracted package is relocated again into a fresh external directory with
spaces. Only the reference C++ source and a hashed **12-script** build-host
validation capsule accompany it; no producer headers, runtime objects or runtime
builder are supplied. Host Clang/Python/OpenSSL and OS tools remain available;
ambient Python and dynamic-library search settings are not inherited. With the
original trees inaccessible, both SDK-only C++17 linkage modes compile and pass
verified HTTPS+inflate, wrong CA, missing explicit CA despite ambient valid-CA
variables, and malformed CA: **eight cases**. Original, extracted and externally
relocated SDK inventories remain unchanged.

### Shared-consumer binding evidence

The existing SDK binding audit examines **101 Mach-O images** (10 dylibs and
91 executables) with no new violation under its documented boundary policy.
The reference consumer now additionally audits each external executable against
the SDK-owned symbols and retains the result in its receipt. The shared consumer
does not define its own `malloc`; its CRT-owned undefined references bind to
CRT `libc`, not Darwin `libSystem`. Its recursive import closure contains
**10 SDK images**, with only `libSystem.B.dylib` as the declared direct OS
boundary. The static consumer carries its own libc and imports no SDK dylibs.
No consumer-specific audit exception was added. Image-tool failures abort rather
than silently returning empty audit evidence.

A separate startup-only dyld trace, under the same producer-denying sandbox,
confirms all ten runtime images are actually mapped from the external relocated
SDK, including imported libc++/libc++abi; no second copy of those SDK runtimes
is loaded elsewhere. The trace also records Apple's transitive system images,
which are not CRT payloads. This probe exits at argument validation (exit 2);
the eight-case consumer receipt, not the trace, supplies HTTPS behavior evidence.
Because the protected `sandbox-exec` launcher strips `DYLD_*`, tracing is enabled
inside its policy by the unprotected host-Python launcher, without weakening the
file-denial policy.

### Negatives, evidence and scope

Seven private-copy verifier negatives are rejected: missing static curl, missing
shared curl, a changed inherited libc byte, upper-stage content, a missing curl
notice, a removed dependency declaration and bundled/ambient trust policy.
The producer SDK is untouched. The focused Development/binding unit suite passes
**32/32**; the existing CI tooling selection passes **129/134**, with five
host-conditional skips. It includes these small fixtures, not another native
SDK build. Native Linux/Windows execution of the changed tooling
and hosted GitHub CI are **Not run** here.

Evidence root: `out/dev-sdk-macos-20261027/`, including
`configure-fresh.log`, `producer-entry-fresh.log`, `package{.log,-result.json}`,
`extracted-profile.log`, `external-driver-final.log`,
`external/{command.json,consumer.log,result.json}`, `sdk-binding-audit{.log,.json}`,
`shared-dyld{.log,-result.json}`, `negative-profile{.log,-result.json}`, `unit.log`
and `ci-tooling-unit-final.log`. The external work path and script digests are
retained in the external receipt. The ordinary receipt is
`out/macos-host-ninja-debug/development-consumer/result.json`.

Producer/base/compile provenance honestly records `22cb03e` plus a dirty working
tree: these are local acceptance candidates, not clean/tagged release artifacts.
Older Full `05-ui`/`06-web` SDKs were not regenerated or promoted as a matching
release pair. This closes the macOS S3 sub-gate only; Windows/x64 and
Windows/aarch64 producer-tree-hidden replay remain open. No public ABI, SDK/profile
schema, CLI lock or Studio adoption changed. The consumer review baselines below
remain unchanged; this work extends the producer-owned acceptance procedure.

## Linux/aarch64 native producer and package replay (2026-10-27)

The native aarch64 host completed the ordinary Development producer entry point:

```sh
cmake --build --preset linux-host-ninja-debug \
  --target crt-development-sdk-test --parallel 4
```

The existing C/C++ build was updated incrementally, including imported libc++
and newly generated `01-c`/`02-cxx` SDKs; no upper-stage rebuild was needed.
The plain predecessor, producer and retained port compile provenance all identify
clean CRT `805d51012d409121ba27971039278e43087551e4`. The predecessor now declares
the architecture-specific `linux-elf-loader` prerequisite. Fresh port work
`out/linux-host-ninja-debug/development-sdk-work/dev-6pq_eoq6` compiled and
installed zlib 1.3.1, Mbed TLS 3.6.7 and curl 8.21.0 through the shared media
producer functions. Ordinary distribution and Development profile verification
pass, with no graphics/media/UI/Web payload. All **23** inherited runtime-file
digests match plain `02-cxx`.

All **six** recipe tests pass on this device: static/shared zlib round trips,
Mbed TLS SHA-256/AES checks and curl external HTTP/HTTPS round trips
(`http=200 https=200`). The SDK-only C++17 reference consumer additionally
passes **eight** cases: verified HTTPS+inflate and wrong, missing and malformed
explicit CA, for static and shared linkage. The missing-CA case rejects even
with a valid CA supplied through ambient environment variables.

The existing `prepare_release_assets.archive_directory` helper archived the
local `development` candidate without invoking the release collector. Extraction
to `out/dev-sdk-linux-aarch64-20261027/extracted SDK with spaces/dev` passes
`verify_dist.py --stage 02-cxx --profile dev`. Archive identity:

- File: `archives/crt-development-linux-aarch64-dev.tar.xz` under the evidence root below.
- Size: **8,831,520 bytes**.
- SHA-256: `4d3d05a292db88f67290f713b07d0d256f84e5b0da63f60030eec0a6bb6f7be7`.
- Original and extracted inventory: **2,579** files/symlinks, inventory SHA-256
  `b80e906e40877158991f32219596dc21d4e0444bafcd9f35952fcc87c18f5ed1`.

The extracted package then passed the same source-hidden acceptance procedure:

```sh
python3 tools/test_dev_sdk_external.py \
  --sdk-root 'out/dev-sdk-linux-aarch64-20261027/extracted SDK with spaces/dev' \
  --output out/dev-sdk-linux-aarch64-20261027/external-retry
```

Bubblewrap masks the whole producer checkout, including original/extracted
SDKs and build outputs. The visibility probe confirms the hidden root is empty;
both linkage modes build and run from the relocated package outside the checkout,
passing all **eight** trust/inflate cases. Static linkage has no dynamic imports.
The shared recursive closure contains **11 SDK images** and only the declared
`ld-linux-aarch64.so.1` OS boundary. Original, extracted and externally relocated
SDK inventories match and remain unchanged.

Private-copy profile negatives reject missing static curl, missing shared curl
and changed inherited libc bytes. The old local `05-ui` Full SDK is correctly
rejected by the collector's clean-source pair check; it was not regenerated or
relabeled. The existing Development boundary/profile/release unit suite passes
**17/17**. No producer code or public contract/schema change was required, and
CLI/Studio adoption baselines remain those in the review below. This completes
the Linux/aarch64 S2 replay and supplies native S3 evidence, not clean/tagged
release qualification or a matching new Full/Development release pair.

Evidence root: `out/dev-sdk-linux-aarch64-20261027/`, retaining
`producer-entry.log`, `producer-entry-network-retry.log`, `recipe-tests.log`,
`package.log`, `package-result.json`, `external-driver-retry.log`,
`external-retry/{command.json,consumer.log,result.json}`,
`negative-profile{.log,-result.json}` and `unit.log`. The ordinary consumer receipt
is `out/linux-host-ninja-debug/development-consumer/result.json`. The successful
external work directory is retained in its receipt. The initial sandbox attempts
failed at download DNS and user-namespace creation respectively; their logs
remain alongside the successful permission-enabled retries. Isolation was not
bypassed and there was no unisolated fallback.

## Windows/arm64 native producer and package replay (2026-10-27)

Host: Windows 11 arm64 VM (4 vCPUs), Clang 22.1.8, CMake 4.4.4, Windows SDK 10.0.28000 (newest installed, chosen
automatically). The ordinary Development producer entry point ran from a native arm64 shell on a clean tree (CRT
`376f06da5a463cfb3e2b8c781842a043a7b9d8b0`), reusing the existing C/C++ build:

```sh
cmake --build out/windows-host-ninja-debug --target crt-development-sdk-test --parallel 4
```

It built the plain `02-cxx` predecessor, compiled zlib 1.3.1, Mbed TLS 3.6.7 and curl 8.21.0 static and shared through the shared
media producer functions, and passed ordinary and `--profile dev` verification with no graphics/media/UI/Web payload. The sidecar
`share/crt/contracts/development.json` records producer and base commit `376f06d`, `producer_working_tree_dirty: false`, target
`windows`/`aarch64` and **27** inherited runtime digests. The ordinary consumer receipt
(`out/windows-host-ninja-debug/development-consumer/result.json`) shows static and shared linkage each passing verified HTTPS+inflate and
the wrong, missing and malformed explicit-CA cases; the static consumer imports only `kernel32.dll` and
`api-ms-win-core-synch-l1-2-0.dll`, the shared one adds ten packaged SDK images.

All **six** recipe tests pass on a private copy of the produced SDK (`crt-port-build.py --sdk-root … --port zlib --port mbedtls --port curl
--test`): static/shared zlib round trips, Mbed TLS SHA-256/AES-128-CBC checks, and curl round trips against the local fixture
(`http=200 https=200`, both linkages).

The existing `prepare_release_assets.archive_directory` helper archived the `development` candidate without invoking the release
collector. Extraction with `shutil.unpack_archive` (the release tool's own call) into `extracted SDK with spaces/dev` passes
`verify_dist.py --stage 02-cxx --profile dev`:

- File: `archives/crt-development-windows-aarch64-dev.zip` under the evidence root below.
- Size: **138,495,124 bytes**.
- SHA-256: `32e4d9ef3fc5352f60de1908e099817c9ab539a1dd831f2278e9359576059f3f`.
- Original and extracted inventory: **2,695** files with identical sizes and SHA-256 values; inventory SHA-256
  `b2c129f6737fe71b34ea312c6039c760efd99c59b1a2150038331ceaaa836466`.

`tools/test_dev_sdk_consumer.py` against the extracted SDK, compiling under `consumer path space`, again passes all eight cases. The shared
closure is **ten SDK images** plus the two declared Windows system imports; static linkage has no packaged imports.

Private-copy negatives are rejected by `verify_dist.py --profile dev`: missing static curl (`lib/libcurl.a`), missing shared curl
(`bin/libcurl-4.dll`) and one flipped byte in the inherited `bin/libc.dll` (inherited runtime digest mismatch). The Development unit
suite (`tools/test_dev_sdk.py`) passes **17/17**. No producer code or contract/schema change was required.

*Not claimed.* That replay predates the Windows restricted-token driver above,
so it is **not** a producer-tree-hidden replay: the extracted SDK sits inside the
checkout's `out/` and was not hidden from the build. The restricted-token driver
was then run natively on the same SDK: [source-hidden replay](#windowsarm64-source-hidden-replay-2026-10-27).
The stale-Full-SDK pairing negative (collector) and a clean/tagged
release pair are not part of this replay. The VM has no GPU, which does not matter for this profile.

Build-host inputs, all outside the repository: a native arm64 shell (an x64 Git Bash configures the x86_64 sources),
`CRT_WINDOWS_SDK_LIBPATH` in 8.3 form and `CRT_CC/CXX/AR/RANLIB` for the consumer driver. Evidence root:
`out/dev-sdk-windows-arm64-20261027/`, retaining `producer-entry.log`, `recipe-tests.log`, `package.log`, `consumer-extracted.log`,
`consumer path space/result.json`, `negative-profile.log` and the archive. A first run of `recipe-tests` without `--install-prefix` wrote a
`crt-port-work/` directory into the checkout; it was deleted, so pass an explicit prefix.

## Windows/arm64 source-hidden replay (2026-10-27)

Host: the Windows 11 arm64 VM of the producer replay above (4 vCPUs, no GPU), native arm64 Python 3.11.9, Clang 22.1.8, Windows SDK
10.0.28000 (`um/arm64`). Reviewed CRT `a26fe6b` plus one local test-tool change (below). The SDK under test is the one the producer replay
built at clean `376f06d` and extracted to `extracted SDK with spaces/dev`; it still verifies with the current `verify_dist.py --profile dev`.
The commits between `376f06d` and `a26fe6b` change acceptance tools only (`crt_windows_external.py`, `test_dev_sdk*.py`,
`audit_macos_bindings.py`, `crt-libcxx-build.py` and their tests); none of them is shipped in the SDK and no libc, header or runtime file changed,
so the SDK was not rebuilt. No x64 execution or cross-compilation is counted.

```powershell
python tools/test_dev_sdk_external.py `
  --sdk-root 'out/dev-sdk-windows-arm64-20261027/extracted SDK with spaces/dev' `
  --output out/dev-sdk-windows-arm64-20261027/external-final
```

The receipt reports `isolation: windows-restricted-token`, target `windows`/`aarch64`, the whole checkout (including its `out/`) as the hidden
root, `all_inaccessible: true`, `files_unreadable: true` and `descendant_denial_checked: true`; the checkout ACL was not modified
(`producer_acl_modified: false`). The restricted process runs a private copy of host Python, the relocated SDK, the reference C++ source and
the hashed **12-script** validation capsule in a new external directory. SDK-only C++17 static and shared consumers pass **eight** cases:
verified HTTPS+inflate, wrong CA, missing explicit CA despite ambient valid trust variables, and malformed CA. The built executables and
the packaged `libc.dll`, `libcurl-4.dll` and PE zlib image are `AA64` (checked in the retained work directory). Static linkage has no packaged
imports; the shared closure is **ten SDK images** run app-local with a System32-only PATH, and only `kernel32.dll` and
`api-ms-win-core-synch-l1-2-0.dll` are external imports, so imports resolve at load. The SDK payload stays unchanged: **2,695** entries,
inventory SHA-256 `7f8374cc5c2579e4c3f2849377caf74f2bf83b121322a48b318803c03938077e`, and the 27 inherited runtime digests in the receipt
equal the sidecar.

Build-host tools outside the SDK: `CRT_CC/CXX/AR/RANLIB` (LLVM, 8.3 paths), `CRT_WINDOWS_SDK_LIBPATH` (arm64 import libraries) and the
fixture's OpenSSL, which here is Strawberry Perl's **x64** `openssl.exe` running under emulation. It only creates the local PKI for the
fixture and is not part of any SDK or target binary.

**Verifier negatives.** Seven private-copy mutations of the extracted SDK are each rejected by `verify_dist.py --profile dev`: missing static curl
(`lib/libcurl.a`), missing shared curl (`bin/libcurl-4.dll`), one flipped byte in the inherited `bin/libc.dll`, upper-stage content
(`include/crtgfx`), missing curl notice (`share/licenses/curl/COPYING`), curl dropped from the manifest's dependency declarations, and a sidecar
that declares bundled CA trust. The copies were deleted afterwards; the SDK under test was not touched.

**Unit suite and a test fix.** `tools/test_dev_sdk.py` passes **28/28**. On this host it first failed once:
`test_windows_token_denies_private_files_and_inherits_to_cmd_children` ran `cmd /d /c probe.cmd`, and the session environment had
`NODEFAULTCURRENTDIRECTORYINEXEPATH=1` (set by the tool launcher), under which `cmd.exe` does not look for a bare name in the current
directory, so the probe never started. The real driver was unaffected because it builds a clean environment. The test now invokes
`.\probe.cmd`; it passes with and without the variable. This is a test-robustness change; no product code changed.

*Not run.* The exact CI tooling selection of the x64 record, the stale-Full-SDK pairing negative and any clean/tagged release pair. The SDK
and its sidecar record the clean producer commit `376f06d`; this is a local acceptance candidate, not a release artifact.

Evidence root: `out/dev-sdk-windows-arm64-20261027/`, retaining `external-driver.log`, `external-final/{command.json,consumer.log,result.json}`
and `negative-profile-7.log`, next to the producer-replay files. The retained external work directory and the launcher, host Python and
fixture hashes are in the receipt.

## Linux source-hidden replay (2026-10-27)

Fresh ports were compiled through the existing producer, reusing the media
stage's networking build and dependency-record functions:

```sh
python3 tools/build_dev_sdk.py \
  --sdk-root out/linux-host-ninja-debug/dist/02-cxx \
  --output out/dev-full-sdk-20261027/dev \
  --work-root out/dev-full-sdk-20261027/producer-work --jobs 6
python3 tools/test_dev_sdk_external.py \
  --sdk-root out/dev-full-sdk-20261027/dev \
  --output out/dev-full-sdk-20261027/external
```

`producer.log` exits 0 and ordinary plus Development profile verification pass.
Compile and producer identity are clean CRT
`181f74b2aaf644c82e21a57e4325355964ede3c0`. The supplied plain `02-cxx`
predecessor retains its own source identity
`857f8313199bd4e9dc1c6d8cc98c01743c80fe40`; no metadata was relabeled.

The new Linux-only acceptance driver copies the SDK and the reference consumer
to a fresh directory outside the checkout. Bubblewrap masks the original SDK
and the whole producer checkout, including its `out/` build roots, with empty
mounts. A child probe checks that the masked roots are empty before validating
the relocated profile, compiling or executing the consumer. Build-host Clang,
Python, OpenSSL and OS tools remain available. An eleven-script validation
capsule supplies only the reference fixture's local imports, with no CRT
headers/runtime objects, `build_dev_sdk.py` or `crt-libcxx-build.py`. The receipt hashes those scripts.

With the producer and original SDK inaccessible, fresh static and shared C++17
consumers compile using the relocated SDK wrappers, headers and libraries. Each
passes verified HTTPS+inflate, wrong CA, missing explicit CA despite ambient
valid-CA variables, and malformed CA: **eight cases**. PKI is generated fresh
for the local loopback server. The static consumer has no dynamic imports; the
shared consumer's actual recursive import closure contains **11 SDK images**
and only the declared `ld-linux-x86-64.so.2` OS boundary.

Both the original and relocated SDK retain all **2,579** file/symlink inventory
entries, with inventory SHA-256
`89e659fe1b257129fddc9979bdedc319ee6b2a30c18fe8e464449e8bac1490e7`.
Evidence: `out/dev-full-sdk-20261027/{producer.log,external-driver.log,
external/command.json,external/consumer.log,external/result.json}`. The result
also retains the external work location and confirms SDK payloads are unchanged.
Missing bubblewrap or failed namespace creation fails this gate; there is no
unisolated fallback. Cross-target SDKs are rejected before execution.

### Runtime correspondence and negatives

`pair-and-negative-results.json` records equality of all **23** inherited
runtime files between Dev, plain `02-cxx`, Full `05-ui` and Full `06-web`.
Private SDK copies exercise profile rejection of missing static curl, missing
shared curl and changed inherited libc bytes. Original artifacts are untouched.
The actual Dev/Full pair above is also rejected by the collector's pair check
because producer and Full/base source commits differ, despite equal runtime
bytes. This is a local development candidate, not a matching clean release pair.

The existing CI tooling suite includes regressions for foreign-target execution
rejection, hidden/work-tree overlap, stale-success receipt removal and the
bounded validation capsule. All **17** Development profile/release/boundary unit
tests pass (`unit.log`); the full existing CI tooling selection passes
**115/119**, with four conditional skips (`ci-tooling-tests.log`).
Linux/amd64 PR CI now builds/tests the Dev SDK and
runs the source-hidden driver; other matrix hosts keep bounded offline fixtures.
The CI step is implemented; a hosted GitHub run is **Not run** in this replay.

## Producer/consumer review

The producer owner is CRT's `build_dev_sdk.py`, profile verifier, reference
consumer and external acceptance driver. Initial review baseline: CRT `181f74b`;
later host extensions retain their exact baselines in the host sections above.
Consumer baselines remain CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93` and
Studio `27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`, with no approved runtime
combination in CLI's `docs/compatibility.md`. This adds an acceptance procedure,
not a new SDK/CLI wire contract: SDK manifest/profile format 1, port recipe
version 1 and isolated-stage recipe version 2 remain unchanged. The general
SDK development-contract v1 remains proposed. CLI/Studio live integration is
**Not run**; artifact promotion precedes any later consumer lock update.
