# Build-system consolidation acceptance

## Scope and ownership

This record covers the in-tree build entry points, not renewed acceptance of
the isolated SDK chain. [The target catalog](../guides/build_targets.json)
owns user-facing target names; [distribution](../guides/distribution.md) owns
SDK construction and isolation. [TODO](../../TODO.md#build-system-consolidation)
owns the ordered B0-B5 work. Existing Full SDK and proposed Development SDK
production remain separate from release publication approval.
Build-system consolidation closes after native entry-point replay and final
review, not after an actual clean/tagged release collection. The separate
[release qualification item](../../TODO.md#release-qualification-and-assembly-rehearsal)
retains that positive rehearsal and its final artifact review; see the
[scope split](#release-qualification-scope-split-2026-10-23).

## B0 baseline review (2026-10-22)

Reviewed commits `0208bcd` (preset features and build-time dependencies),
`027b5db` (stage caching), `0443910` (target catalog/classifier/CI) and
`aa81932` (measured OS differences), from a clean worktree at `aa81932`.

The supplied five-host CI report states that all 17 user-facing targets exist,
Linux and Windows each have equal target sets across their two architectures,
and the per-OS differences match `internal_os_scope` in the catalog. That is
prior reported inventory evidence, not five fresh builds performed here.
The CI workflow runs the classifier after its C-stage workflow and uploads
the inventory; it does not build every listed upper-stage target.

Local Windows/x64 replay:

```powershell
python -m unittest -v tools.test_list_cmake_targets tools.test_crt_stage_cache tools.test_lvgl_sources
cmake --fresh --preset windows-host-ninja-debug -B out/build-system-review
python tools/list_cmake_targets.py out/build-system-review --check --json out/build-system-review/targets-baseline.json
```

All 18 baseline tool tests pass. Fresh configure succeeds without pre-fetched Skia/LVGL
or manual option changes, registering 209 tests. The resulting Windows tree
contains 17 user-facing and 378 internal names, with no missing or unlisted
stage-like targets. Target counts are observations of this configuration, not
portable constants. No upper-stage compilation is established by configure.

Built `crt-c-build --parallel 8` from this fresh tree, including the recipe-built
Make and CRT-owned shell, then ran `crt-c-test`: 163 selected tests, 162 executed
passes and the existing `fork_heap_snapshot_test_runs` skip, zero failures,
41.18 seconds. Logs are `out/build-system-review/core-build.log` and
`out/build-system-review/core-test.log`. Native processes ran without visible
console windows, outside the sandbox's native-launch restriction.

### Findings and limits

- The five internal Wayland/xkbcommon entry points are still defined on all
  operating systems. The catalog documents this but does not prevent execution;
  they are not macOS/Windows backend targets. Preserve explicit
  cross-build uses before restricting their definitions.
- Recent build-system commits had no matching HISTORY entries and no active
  TODO checklist. This review restores that tracking instead of claiming their
  implementation was absent.
- Source hashing is useful, but the existing cache only checks whether declared
  output paths exist, often a directory. Removing a DLL or archive inside that
  directory can leave a false hit. CMake passes only a subset of build-affecting
  options, and the input globs exclude executed `test_*.py` drivers. B3 must
  tighten these boundaries before release targets rely on the cache. Two
  temporary-fixture probes reproduce false hits: deleting `payload/runtime.dll`
  inside a declared output directory skips the second run, as does changing an
  executed `tools/test_driver.py` from success to exit 7. Both still return
  success without executing their commands. This review does not fix that cache.
- `crt-libcxx-test` delegates to an uncached smoke/sysroot target. Do not describe
  all stage tests as cached. The classifier's current OS-specific exemption also
  was corrected in B1 to require an OS-specific target on its declared OS.
- `crt-web-*`, `crt-dev-release` and `crt-full-release` remain planned, not
  implemented. Development release assembly depends on the SDK split's S0-S3;
  an alias for `02-cxx` would omit the required networking ports.

## B1 configuration entry-point contract

`cmake --preset <preset>` remains the initial configure step. A
`crt-<stage>-config` target is a bounded preflight of that already configured
tree: it reports the effective target pair, compiler and cumulative feature
requirements and writes a local report under `stage-config/`. It neither
changes cached feature options nor starts downloads, target compilation or
nested CMake configuration. Third-party configure actions remain ordered
inside their existing build targets.

The public preflight requires the complete advertised stage. It fails with
the missing CMake option and the explicit reconfigure command when features
were intentionally disabled; internal and reduced-feature diagnostic build
targets remain usable without that strict public preflight. Web additionally
rejects unsupported target pairs, notably Windows/aarch64. A successful
preflight is not dependency build, runtime, hardware or release acceptance.

## B1 local implementation and replay (2026-10-22)

Added six preflight targets through `cmake/crt_stage_config.cmake` and
`tools/crt_stage_config.py`. They are opt-in and do not add dependencies to the
existing build targets. Reports are private build-tree diagnostics, not a new
SDK manifest/schema or CLI capability. No SDK/consumer lock is changed.

On a fresh Windows/x64 configure in `out/build-system-config spaces`, all six
targets pass, and no `external`, `port-tests`, `lvgl`, `rootfs` or `dist` tree
is created by preflight. With LVGL explicitly OFF, C and media configs still
pass, UI config rejects the missing option and removes its older success
report. Reapplying the preset restores the full configuration and all six
preflights pass again. That replay caught and fixed bare compiler names in a
reapplied preset's cache: CMake supplies its resolved tools, with PATH resolution
only as the direct-script fallback. No compiler is executed by the preflight.

Fresh Windows/aarch64 **cross configuration only**, in
`out/build-system-config-arm64`, passes the five lower-stage preflights and
rejects Web explicitly. The x64 and aarch64 inventories match: 23 public and
378 internal targets each. The configure reports missing aarch64 compiler-rt
builtins in this local installation; no aarch64 compilation or execution is
claimed. This is not a claim that every external build prerequisite was found.

Final automated checks: 30 unit tests cover stage requirements, missing tools,
wrong source/target identity, Linux libva, unsupported Web pairs, spaces, stable
reports, stale-report removal, resolved-vs-cached compilers, target catalog and
cache behavior, and the LVGL source list. README/catalog parity is tested;
the classifier now checks OS-scoped public targets on their intended OS.
These tests and C/C++ preflights join the **existing** five-host CI job, not a
second workflow/build. The changed CI and native Linux/macOS preflights have
not been run here; the native Windows C-stage result above is the build replay.

### Reproduction entry points

```powershell
python -m unittest tools.test_crt_stage_config tools.test_list_cmake_targets tools.test_crt_stage_cache tools.test_lvgl_sources
cmake --fresh --preset windows-host-ninja-debug -B "out/build-system-config spaces"
cmake --build "out/build-system-config spaces" --target crt-c-config crt-libcxx-config crt-gfx-simple-config crt-gfx-media-config crt-ui-config crt-web-config
python tools/list_cmake_targets.py "out/build-system-config spaces" --check
cmake --fresh --preset windows-host-ninja-debug -B out/build-system-config-arm64 -DCRT_TARGET_ARCH=aarch64
cmake --build out/build-system-config-arm64 --target crt-web-config
```

The last command must fail with the unsupported Windows/aarch64 message.
For the feature-off negative case, reconfigure the spaces tree with
`-DCRTUI_ENABLE_LVGL=OFF` and require `crt-ui-config` to fail; restore it by
reapplying the preset. B2-B5 stay open. In particular no Web aggregate,
Development SDK package, release target, upload, or new upper-stage acceptance
is claimed by this tranche.

## B2 Web target wiring and Windows gate

This records the initial 2026-10-22 checkpoint. Its pending/failed observations
are superseded by [Windows B2 completion](#windows-b2-completion-2026-10-23),
with intermediate repair attempts retained below as evidence, not active runs.

`1033c86` committed/pushed B0/B1 before further implementation. Added native-only
`crt-web-build/test/dist`, reusing `crt-stage-build.py` and the existing isolated
Web producer. Build starts with the complete 05-ui SDK; it includes the producer's
mandatory browser/network/ABI validation, not just compilation. A changed-input
run uses a new object tree and retains the previous published SDK. Publication
failures restore the old name. Test validates a disposable copy, leaving the SDK
immutable; dist verifies and archives it without granting release approval.

Explicit `CRT_WEB_PREDECESSOR`, `CRT_WEB_STAGE_RECIPE`, `CRT_WEB_STAGE_ASSET`,
`CRT_WEB_SOURCE_CACHE`, `CRT_WEB_WORK_ROOT` and `CRT_WEB_JOBS` select producer
inputs. `CRT_WEB_REPLAY_SDK` selects an existing SDK for test/dist only and does
not replace the build path or establish fresh-build acceptance. Supported native
pairs are Linux/x86_64, macOS/aarch64 and Windows/x64. Windows/aarch64 fails before
building a predecessor; explicit Linux cross builds keep their existing separate
procedure rather than claiming native runtime acceptance.

The attempted Windows replays correctly rejected two old SDKs: the 2.54.1 SDK
lacked the later generated SOURCE-OFFER, and the subsequent source-offer SDK
lacked the current libcrtmedia relink declaration. No gate was weakened and no
old SDK was edited to pass. Logs are under `out/build-system-web replay/`.

A separate fresh `out/build-system-producer` completed the current predecessor
(`crt-ui-dist`, outer Ninja parallelism 8). C++ and Skia object directories are
separate; the first Skia attempt also reused its old mutable install-cache prefix,
then this was corrected to `out/build-system-producer/skia-install`. Frozen SDKs
were not modified. This exposed and fixed ordinary Windows
build gaps: missing libc++ DLL/import-library BYPRODUCTS, and media/Skia preparation
edges accidentally nested under `if(CRT_WEB_PORT_TARGETS)`, false on Windows.
The initial run stopped on the import library; the next stopped on missing
FFmpeg/curl headers. The corrected graph orders media object compilation after
both port-build targets, as verified with Ninja's query tool. Do not call the
full Web producer accepted before it finishes. The final 05-ui SDK passes both
ordinary verification and `build_stage_06_web.require_predecessor`.

The completed predecessor log is `out/build-system-producer/ui-producer-final.log`;
earlier failures are retained in `ui-producer.log`, `ui-producer-retry.log` and
`ui-producer-ordered.log` (the last caught a missing Python import in the new
network-runtime copy path, corrected before success). Current `crt-web-build`
uses the pristine pinned source-only cache at
`C:/crtw/t10w2541b/build/06-web/webkit/product`, accepted by the existing commit,
tree and clean-checkout validator; no Web objects are reused. Its active log is
`out/build-system-producer/web-producer-retry.log`. The first Web attempt
(`web-producer.log`) stopped before host compilation because Git's POSIX Perl
lacked Locale::Maketext::Simple, reached through IPC::Cmd. Reuse the established
host-only `CRT_WEB_HOST_PERL_LIB=C:/crtw/perl-overlay`; its native preflight passes.
CMake now exposes that input and fingerprints its files plus selected host
generator executables. The adapter checks host prerequisites before creating
a producer run or fetching ports. No Perl module is installed globally or
bundled in the target SDK. The retry explicitly fixes the completed 05-ui and
its 06-web recipe, without rerunning the unrelated predecessor graph.
After it verifies, run
`crt-web-test` and `crt-web-dist` from this same tree.
At that checkpoint positive Web production/replay remained pending. Linux/macOS replay will
be collected as one batch **after** the Windows implementation/replay, per owner
direction, not performed in parallel here.

## B3 cache correctness

The stage cache now records output content/type/link/execute-bit inventories,
not just directory existence. It hashes executed scripts even when their names
start with `test_`, explicit predecessor artifacts, resolved tools, complete
CMake cache configuration, selected dependency target files and build-affecting
environment. Missing output after exit 0 is a failure; failed/spawn-interrupted
commands invalidate older success. Inputs changed during execution do not create
a cached success. Old cache states cause a safe miss. File-content hashes retain
a size/mtime/ctime acceleration cache; this is local build reuse, not publisher
authentication against adversarial filesystem metadata.

Stage-source outputs now include their tarballs; dist outputs include archives.
Deleted archives therefore cannot hide behind a retained SDK directory. libc++
fetch/configure outputs name component source roots and configure cache files,
not a mutable parent that also contains later build products. Nested C++ builds
and cumulative dist steps receive actual predecessor inputs.

Windows regression fixtures cover deleted/corrupt nested payloads, changed
predecessor bytes/configuration/environment, changed executed drivers, missing
outputs despite exit 0, spawn failure, symlink retargeting, source edits during
execution, force/reset behavior and unchanged/touch-only reuse. The native
Windows unit pass includes the symlink case, which sandbox-only execution skips
for WinError 1314. Fresh C-stage replay in `out/build-system-review` selects
163 tests: 162 passes, the existing COW skip and zero failures (49.51 seconds).
Its immediate second `crt-c-test` call reports `up to date`; see
`out/build-system-review/cache-core-final.log`.

Native fresh-CMake/tooling regression passes 114 tests in 11.982 seconds
(`out/build-system-review/final-unit.log`), including real archive filtering,
directory publication/rollback and symlink cases. Final-review coverage includes
stale Web-verification receipt removal and requires all three Mbed TLS
shared components, not just any one Mbed TLS DLL.

The ordinary Windows producer exposed another source of spurious cache misses:
configure copied the unfiltered compiler-rt archive over the filtered archive
every time, changing mtime even when the final bytes were identical. libc DLL
relinking then changed actual PE bytes and correctly invalidated nested C++
fingerprints. `cmake/filter_windows_builtins.cmake` now filters a private candidate
and updates the final archive only if bytes differ. The real archiver fixture
proves emutls/eprintf removal, unchanged vendor input, stable output mtime and
updates for changed retained members. An ordinary native reconfigure followed
by `c_shared` reports `ninja: no work to do`
(`out/build-system-review/stable-reconfigure.log`); no fingerprint gate was relaxed.

Existing distribution/wrapper plus profile/shared-layer regressions select
49 tests: 47 executed passes and two host-inapplicable skips, zero failures
(`out/build-system-review/distribution-regression.log`). Development fetches can
explicitly keep downloads under their work root instead of deriving a path
above the source repository; isolated media's existing default is unchanged.

## Development SDK Windows replay

Implemented the `dev` packaging profile without creating another cumulative
stage. The normal producer is `tools/build_dev_sdk.py`; CMake's internal
`crt-development-sdk` and `crt-development-sdk-test` feed the public release
targets. The producer consumes plain 02-cxx and reuses the media builder's
network fetch/build/dependency-record functions for zlib, Mbed TLS and curl.
It preserves all inherited runtime bytes, installs notices/Apache license choice
and an SPDX package inventory, relocates text metadata through the existing
shared policy, and invokes ordinary verification plus `--profile dev`.

Actual Windows build: predecessor `out/iostream-fix/packaged-02-cxx`, independent
work `out/build-system-development/work/dev-npojw3je`. All three ports were
compiled static/shared through CRT mksh and the packaged toolchain. The first
packaging check exposed a wrong filename assumption, not a missing build:
Windows zlib intentionally produces a **PE DLL named libz.so.1.3.1**. Profile
verification now reads the actual DLL characteristic and excludes import
archives. Mbed TLS DLLs live under `lib/`, curl's DLL under `bin/`; the shared
metadata helper and ordinary Full SDK copy path now include both locations.
The next check caught absolute curl-config/pkg-config/libtool metadata; existing
text relocation fixed these. No upstream source or host SDK exposure was added.

These metadata-only retries retained the completed compiled ports. The explicit
`--packaging-work` path checks predecessor identity/runtime digests, completed
installation and unchanged source/build recipe data (status/notes are not build
inputs for this check). It does not turn unknown/dirty compile provenance into
a clean release. Earlier logs remain failed checkpoints, not successful builds.

`out/build-system-development/dev` then passes `verify_dist.py --stage 02-cxx
--profile dev`. A copy at `relocated Development SDK final`, compiled consumers
under `consumer path space final`, passes **static/shared x four cases**:
verified HTTPS carrying zlib data, wrong CA, missing CA despite ambient CA
variables, and malformed CA. The SDK-owned POSIX/Bionic header options avoid
curl's native Winsock-header branch. Fresh PKI is local/short-lived; no Internet
service or global CA store is used by the target.

The shared import closure is computed with `crt_binary_dependencies.py`, not a
guessed deployment list: ten SDK images (zlib PE/.so, curl, three Mbed TLS DLLs,
libc, libm, libc++, libc++abi and libunwind) and declared Windows kernel imports.
Those images are copied app-local; execution passes with System32-only PATH.
The static fixture imports only the two declared Windows system components.
This demonstrates this reference consumer, not a shipping static CLI policy.
Logs and measured digests: `producer.log`, `packaging-final.log`,
`consumer-replay.log`, and `consumer path space final/result.json` under
`out/build-system-development/`. Original predecessors and old accepted SDKs
were preserved; generated replacement outputs retain `.previous-*` checkpoints.

The subsequent normal CMake target also completes using a copy of the current
ordinary `out/build-system-producer/dist/02-cxx`, not the older unknown-commit
SDK above. Configure `out/build-system-targets-final` with
`CRT_DEV_PREDECESSOR=out/build-system-development/current-02-cxx`,
`CRT_DEV_WORK_ROOT=out/build-system-development/cmake-work` and
`CRT_DEV_OUTPUT=out/build-system-development/cmake-dev` (absolute values), then
build `crt-development-sdk-test`. Actual static/shared port installation,
profile verification and all eight reference-consumer cases pass; see
`out/build-system-development/cmake-producer.log` and
`out/build-system-targets-final/development-consumer/result.json`. Producer code
changed during this run while the host preflight/download-cache handling was
being finalized: the outer cache correctly refuses to record a reusable
producer success; this is not a failed native build.

A copy at `out/build-system-development/relocated current Development SDK`
also passes the two link modes/four cases from `current consumer path spaces`.
Its inherited-runtime SHA-256 map exactly equals the current ordinary Full
05-ui predecessor (`current-relocation.log`). Its known CRT source commit is
retained separately from its dirty compile/producer provenance: it is a local
development candidate, not a clean tagged release. Full 06-web comparison still
waits for the current Web build, and complete source/build-tree-hidden replay
and other-host native results remain open. The general SDK/CLI contract and
CLI bootstrap product are not closed; no consumer lock or compatibility approval
changed.

## B4 local release gates and remaining acceptance

Added `crt-dev-release` and `crt-full-release`. Both reject the default local
`development` tag before building anything expensive. The helper requires a
clean producer, equal known producer/base/compile commits, matching target and
VERSION, and equal inherited runtime digests. Metadata-only retries retain the
original compile provenance. Dev archives extract to `dev/`, carry profile-
specific checksum/manifest filenames and are reverified after extraction.
Full archives remain cumulative `06-web`, using existing security/source-offer/
owner-decision checks; lower Full archives keep their existing procedures.
Fresh output roots and staged publication prevent replacing an existing release
identity or exposing partially collected output. Nothing is uploaded or tagged.

The collector's clean HEAD must match the recorded producer commit, not just
be an unrelated clean checkout. New source assets add `working_tree_dirty` to
`stage.json` without changing the recipe/manifest schema. The new Full collector
checks the pinned Web source asset's size/SHA-256, commit and explicit clean
creation flag. Missing legacy flags remain unknown and dirty flags remain dirty,
even if the collector is now clean. Such assets still work for local SDK builds
and replay; they cannot pass this new release-target gate. This is local source
provenance, not signing or publisher authentication. Existing lower-stage release
procedures are unchanged.

Actual source-generation smoke at `out/build-system-review/provenance-source`
rejects the dirty checkout without `--allow-dirty`; the explicitly allowed local
asset records `working_tree_dirty: true` and still validates as recipe schema 2
(`provenance-source.log`). Clean, dirty and legacy creation states plus an
unrelated clean collector commit are covered by the release regression fixtures.

The default-tag negative target replay and malformed/stale/unknown/dirty/missing-
port/runtime-mismatch fixtures pass. Synthetic release fixtures test checksums,
canonical extraction names, failure cleanup and refusal to overwrite; they are
not a native tagged release rehearsal. A real matching clean/tagged positive
rehearsal remains open, as does fresh current Windows Web production above.
No release version was invented, no source tag or skip-CI behavior was changed,
and no Linux/macOS native acceptance is claimed.

Final fresh Windows configuration (`out/build-system-targets-final`) catalogs
28 public and 380 internal targets with no missing/unlisted stage targets.
Both release targets were invoked separately: each rejects `development` before
running any SDK producer (`entrypoints-final.log`). Full/Dev source comparison
uses the CRT manifest/overlay-recipe source identity and matching source-asset
SHA-256, never WebKit's unrelated upstream `source_commit` field in Web source
correspondence. A fixture covers distinct CRT/WebKit commits and stale overlays.

Windows/aarch64 fresh configuration and catalog checks also pass; invoking
`crt-web-build` rejects the unsupported native pair before creating any SDK
predecessor (`out/build-system-config-arm64/web-target-final.log`). This is a
negative configuration/entry-point test, not an arm64 compile/runtime claim.

After the final module changes, a new `out/build-system-entrypoints-final`
configuration passes all six config targets and the catalog check; each default-
tag release invocation independently fails as intended. No `external/` or
`dist/` directory is created by those offline/negative entry-point checks
(`entrypoints.log`). Native Linux/macOS replay remains deferred.

## Deep Windows work-root repair (2026-10-23)

`web-producer-retry.log` finished its target-port builds and then failed before
WebKit configure/compile in `shutil.copytree`. Its 411 errors comprise 345 files
and 66 directories, including long disabled-third-party metadata paths; these
are still pinned source and must not be excluded. The original source exists.
The source cache, failed work `isolated-web/run-tbih9kzd` and its original source
asset are retained. That run is **failed**, not an overnight Web pass.

`crt_file_ops.py` supplies extended Win32 paths only at Python filesystem IO
boundaries, not in compiler/CMake arguments. Cache copying and the hardlink-
based Web composition use it; overlays still break links before writes. The
Git wrapper selects `core.longpaths=true` per command. No global registry/Git
configuration, short work-root alias or upstream patch was added. Aggregated
copy exceptions now retain the total and first three bounded examples.
The helper is included in SDK tools/source-stage closure and its small fixtures
join the existing CI tooling job.

Native proof: the complete source cache copied into
`isolated-web/long-path-source-probe/driver/build/06-web/webkit/product` (a deeper
root), then passed the existing commit/tree/listing and pristine-checkout checks.
All 345 formerly failing files hash identically, all 66 formerly failing
directories exist; their new paths measure 259..302 characters. Evidence:
`out/build-system-review/long-path-source-result.json`. The earlier diagnostic
that incorrectly treated directory entries as files is retained separately as
`long-path-source-files-only-assumption.json`, not as a product failure. Real
>320-character/spaces, UNC spelling, hardlink/overlay and bounded-error fixtures
also pass. Combined regression: 206 selected, 203 passes, three existing skips,
zero failures (`out/build-system-review/continued-unit.log`, 14.537 seconds).
The exact existing-CI tooling command separately passes 65 cases in 7.000 seconds
(`ci-tooling-continued.log`); only small filesystem/recipe fixtures were added,
not a new workflow or repeated full build.

An override recipe is now carried as explicit `stage-build-recipe.json` execution
metadata after verified extraction (the archive is unchanged). Before expensive
builds, the Web entrypoint checks its schema, OS, source SHA-256 and source commit,
then installs that recipe into the staged SDK. It never mutates the predecessor.
Without a receipt, direct legacy invocation requires the installed recipe already
to match. A fixture rejects stale/wrong-OS recipes and proves override retention.

Current fresh source: `out/build-system-producer/stage-sources-deep/06-web.json`,
SHA-256 `40873c8c61d8e92ac70b5005e2bfd6d6b16b5fc76a1cf4e31a463a36fe599a4a`.
The same completed 05-ui and pristine upstream cache remain inputs; no failed
dependency/compile cache guard was relaxed for the changed drivers. Retained log:
`out/build-system-producer/web-producer-deep.log`. Whole Web production/test/dist
and clean/tagged positive collection are **not yet accepted**. Continue Windows
first; native Linux/macOS replay remains one later batch.

The native full composition also completes in the deep probe (56.2 seconds),
with the upstream checkout still pristine afterwards; see
`out/build-system-review/long-path-composition-result.json`. During the new real
run, `executed-recipe-result.json` proves the staged 06 recipe equals the override
while the complete predecessor inventory remains unchanged. The old source SHA
remains only in that untouched predecessor, not the new staged rebuild recipe.

The Development SDK's already-built ports are retained through the official
`--packaging-work` retry, after its unchanged predecessor/runtime/recipe checks.
This adds the new shared helper to installed tool closure and passes verification;
its original compile provenance is retained and the prior SDK is backed up.
Static/shared HTTPS/inflate plus trust negatives pass again from a spaces-named
consumer directory (`helper-closure-refresh.log`, `refreshed-consumer.log` under
`out/build-system-development`). No SDK is relabeled clean or release-approved.

### B2 host-patch failure (2026-10-23)

The fresh `isolated-web/run-41yecki4` run is **failed**, not still running.
`out/build-system-producer/web-producer-deep.log` ends at 09:38 local time with
the outer `crt-web-build` exiting 1. Target dependency installation/smokes
finished under their existing platform policies; source materialization progressed
past the previous copy failure. WebKit configure/compile had not started.

The first failing command (log lines 16735..16744) is `patch -p1 -i` for the
unchanged carried `libcrtweb/patches/0003-coff-assembler-flavor.patch`:

```text
patching file Source/WTF/wtf/InlineASM.h
Program: C:\Strawberry\c\bin\patch.exe
File: .\src\patch\2.5.9\patch-2.5.9-src\patch.c, Line 354
Expression: hunk
```

The host tool exits 3; the Python drivers propagate that failure. Current host
resolution also selects this Strawberry executable. This establishes an immediate
build-host tool failure, not its underlying trigger (tool version, input format
or path handling), and does not establish a WebKit/CRT compile/runtime defect.
Next: compare carried-patch application with explicitly identified host tools in
a disposable bounded reproduction, then fix tool selection/preflight and cache
identity as supported by that evidence. Do not change upstream source or shorten
the work root merely to bypass the assertion.

No `out/build-system-producer/dist/06-web` or run-local `candidate/06-web` exists
after this failure. Packaged test/dist and Full06/Development runtime comparison
were therefore not run. B2 stays unchecked; B4 clean/tagged rehearsal and B5 native
Linux/macOS replay remain open. The source asset SHA above, its
`1033c86`/dirty creation identity, complete predecessor and failed work are retained.
No automatic retry or release was started.

### B2 host-patch repair (2026-10-23)

`out/build-system-review/reproduce_host_patch.py` copies only the pinned files
listed by the unchanged 0003 manifest entry. Strawberry's selected patch 2.5.9
again asserts on `hunk` and exits 3, even in this smaller spaces-named root.
Git for Windows applies that same LF patch with exit 0 and all recorded output
hashes match (`host-patch-comparison.json`). This isolates the actionable host
tool incompatibility without attributing it to WebKit, CRT target code or deep
paths; the internal defect in Strawberry's old patch is not further diagnosed.

`host_patch_command()` selects an absolute Git executable on Windows build
hosts, retaining native `patch` on POSIX build hosts. Git's command-local long
path/autocrlf/whitespace settings preserve the manifest bytes. No fuzzy/whitespace
matching, unsafe-path option, source patch rewrite or global configuration is
introduced. A discovery ceiling and removal of inherited Git repository/index
overrides keep the enclosing CRT repository/index outside this operation.
The early host-tool preflight records tool command/version; the Web port records
`host-patch-tool.json`, and CMake includes the resolved host executable as a cache
input. Subprocesses launched through the common WebKit runner are hidden on
Windows.

Fresh copied-file replay of every carried patch set on this Windows build host
passes each existing before/after SHA check: Windows 18 patches over 50 files,
Linux 17/39, macOS 22/48 (`carried-patch-probe-result.json`). Original cache files
remain unchanged. Sequential patches can touch the same file; the diagnostic
therefore composes each set once from pristine inputs, not by replaying an early
patch's intermediate hash over a fully patched tree. The isolated single-patch
unit fixture separately covers idempotence, tamper rejection, spaces paths and
parent-index protection. These probes do not claim native Linux/macOS replay.

Combined tooling/distribution regression (`patch-fix-unit.log`): 210 selected,
207 pass, three existing skips, zero failures, 12.929 seconds. The new small
host-patch fixtures join the existing CI tooling command, not a new workflow or
another full WebKit CI build. B2 remains open until the new Windows source asset
finishes real production and packaged browser/test/dist verification.

The exact existing CI tooling command passes all 69 cases in 6.427 seconds
(`patch-ci-tooling.log`). A new `out/build-system-patch-config` tree configures
successfully and its target catalog check passes; this is separate from the
long-lived producer cache (`patch-fresh-config.log`, `patch-target-catalog.log`).

The next immutable local source asset is `stage-sources-patch/06-web.json` under
`out/build-system-producer`, SHA-256
`6b2a5e04e7c68168b5e9694951d80f905703c2c7556efe4493629e30c5e7b5d3`.
Its creation identity is `61ea067` plus dirty changes, not a clean release.
The new producer is `isolated-web/run-jgfd4365`, logging to
`out/build-system-producer/web-producer-patch.log`; it passed the early Git tool
preflight and started dependency production. No object-cache/source identity
guard was loosened to reuse the failed run's objects. The hidden one-shot runner
`out/build-system-review/run_patch_replay.py` will run packaged test/dist only
after producer success, against that just-built output via explicit replay,
then compare the Development profile's inherited runtime digests. Its durable
status is `patch-replay-status.json`; passing small tests is not B2 completion.

### Ordinary media relink predecessor repair (2026-10-23)

`run-jgfd4365` is now **failed**, not active. The real pinned patch set and WebKit
compilation passed, as did SDK consumer assembly. At final candidate verification
`web-producer-patch.log` reports `the FFmpeg dependency does not declare its
libcrtmedia relink kit`. No final 06 SDK was published and packaged test/dist did
not start. Both the ordinary 05 input and staged 06 manifest lack the declaration;
the original 05 also has no kit. This is a producer-boundary gap: legacy ordinary
developer verification skipped kits without `built_from`, but isolated 06 requires
one. Do not waive that final gate or add a declaration without the actual objects.

`create_dist.py` now builds/checks/declares the kit when it installs `crt-media`;
later UI packaging validates and inherits it. Existing isolated kit creation is
unchanged. The exporter substitutes ordinary build-prefix libraries only when
their installed SDK bytes match; absent build-tree libraries still fail. The
ordinary bootstrap C++ import descriptor is retained verbatim as support, not
substituted with an unrelated imported-libc++ archive. Support files receive an
additive checksum manifest checked when present. No kit format version changed.
Ordinary SDK verification checks declared kits; only unupgraded legacy developer
SDKs retain their old exemption. Web's predecessor check now rejects missing
declarations or invalid kit contents before dependency work starts.

`out/build-system-review/rebuild_media_predecessor.py` runs the existing ordinary
04/05 packaging commands against their already-built objects, into new output
directories, never overwriting the failed input. Current outputs:
`out/build-system-media-kit-current/{04-gfx-media,05-ui}`. Both verify. Their
`predecessor-payload-comparison.json` confirms all 239 files under the original
05 SDK's bin/lib/libexec/system-bin are present and byte-identical. Old SDKs,
source assets, compile objects and failure logs are preserved.

The generated kit's real native `relink.py --cc <LLVM clang>` succeeds, followed
by `test_relinked_media.py`: a C consumer compiled through `crt-cc.cmd` imports
`libcrtmedia.dll`, executes against the **relinked** app-local DLL and validates
frame plane geometry with System32-only PATH (`relinked media spaces/consumer-result.json`).
The initial raw-mksh diagnostic lacked the official CMD wrapper's shell setup;
the accepted consumer uses the documented Windows wrapper, not a Git Bash build.
The temporary smoke adds no public header/API or upstream patch.

Full regression (`out/build-system-review/relink-fix-unit.log`): 227 selected,
224 pass, three existing skips, 14.374 seconds. Exact existing CI tooling command
(`relink-ci-tooling.log`): 90 selected, 89 pass, one Darwin-only skip, 7.124 seconds.
These include missing/damaged inherited-kit, byte-mismatch, native import-support
and declared ordinary-kit validation checks. Linux/macOS native replay remains B5.

New immutable source recipe: `out/build-system-producer/stage-sources-relink/06-web.json`,
SHA-256 `fbcfad5007ae2b12d3c48be1f3a12495f38eacf0f45da2bc91c0a4778bb8ced4`,
created from `61ea067` plus dirty changes. It uses the new relink-capable 05 input;
neither source nor predecessor guards are relaxed to reuse the failed run's
objects. The one-shot `run_relink_replay.py` records `relink-replay-status.json`,
logs `web-producer-relink.log`/`web-packaged-relink.log`, and only on successful
production runs packaged test/dist then compares Development runtime digests.
B2 remains open until those final results; B4 tagged rehearsal is also still open.

At 13:35 local time the `run-f2n0wl7c` session was no longer available. Native
Windows process enumeration found no producer/compiler processes; its log ended
during host gperf configure without a terminal error or exit status. Record this
attempt as **interrupted**, not a compile failure or pass. The original status,
work and log are retained. A fresh one-shot attempt with identical frozen source
and predecessor inputs uses `relink-replay-restart-status.json` and
`web-producer-relink-restart.log`/`web-packaged-relink-restart.log`. No source,
runtime or cache validation guard changed for this recovery.

### Windows atomic publication and guarded recovery (2026-10-23)

`run-ucq8w1dm` passed its complete SDK validation, including browser observation,
HTTP/TLS/host ABI and `verify_dist` (`web-producer-relink-restart.log`, lines
16864..16873), then failed at `os.replace(hidden-copy, candidate/06-web)` with
WinError 5. Thus the producer target failed and no final SDK was published.
Native process inspection afterwards found no browser/compiler processes. The
exact production access-denial cause is not attributed to antivirus or any
particular process without evidence. The original staging SDK and log remain.

`crt_file_ops.atomic_replace` uses extended Windows filesystem paths and retries
only WinError 5/32/33, at most eight attempts with 4.5 seconds total backoff. It
propagates persistent access denial and unrelated errors. 04 and 05/06 publication
still copy to a hidden sibling and expose the final name only through a successful
atomic rename. No ACL change, process termination, target deletion or non-atomic
fallback is introduced. A native directory handle deliberately opened without
delete sharing blocks rename until released; the helper then succeeds. Fixtures
also prove bounded exhaustion, immediate unrelated-error propagation and no POSIX
retry. Existing rollback tests remain passing.

The whole validated SDK also publishes to a disposable probe directory through
the new helper (`publication-probe.log`, `publication-probe-result.json`), with
equal full tree inventories and unchanged staging source, 15.719 seconds. This
is a publication-copy probe, not final B2 acceptance or a new SHA-256 algorithm;
its inventory fingerprint uses the existing truncated helper.

The optional in-tree recovery inputs `CRT_WEB_RETRY_RUN` and
`CRT_WEB_PREVIOUS_SOURCE_ASSET` select an explicit owned direct `run-*` child of
the work root. Missing/outside/symlinked recovery roots are rejected. The adapter
forwards `--reuse-work-root` and the old archive to the **existing** isolated
driver: an unchanged predecessor and unchanged compiled inputs remain mandatory.
Fresh runs remain the default. No arbitrary object cache or permissive resume
flag was added; final browser/ABI/dist checks are still required.

Full regression (`publication-full-unit.log`): 231 selected, 228 pass, three
existing skips, 14.314 seconds. Exact CI tooling (`publication-ci-tooling.log`):
94 selected, 93 pass, one existing skip, 7.453 seconds. The new helper is already
part of the shared SDK/source tool closure.

New frozen source recipe: `stage-sources-publication/06-web.json` under
`out/build-system-producer`, SHA-256
`a16492e42773c35f06ca14f3d448dd402ed1c6c3d1821e5ad91d3f5cddc65525`,
created from `61ea067` plus dirty changes. The old `fbcfad...` archive is retained
as the verified previous source. `run-ucq8w1dm/driver/build/06-web/inputs.json`
records `packaging_retry: true` after the existing compile-input comparison and
unchanged predecessor check. Source identity was updated honestly, not relabeled
clean. Current one-shot recovery status is `relink-replay-publication-status.json`,
logs `web-producer-relink-publication.log`/`web-packaged-relink-publication.log`.
The final B2 gate remains open until successful publication, packaged test/dist
and Full/Development inherited runtime comparison.

## Windows B2 completion (2026-10-23)

**Accepted here:** Windows/x86_64 native `crt-web-build/test/dist`, including
real isolated Web production, packaged and extracted/relocated SDK validation.
This supersedes the pending checkpoints above, not the preserved failure evidence.
B5 still owns Linux/macOS native replay. Clean/tagged matching Dev/Full release
assembly remains unaccepted and is now tracked in the separate release
qualification item below, not B4's implementation scope. No consumer lock,
public upload or source tag changed.

The guarded publication-only recovery completed `crt-web-build` successfully
(`web-producer-relink-publication.log`). It used the explicitly recorded unchanged
relink-capable predecessor and compile-input checks; dependency inputs were not
relabeled reusable after a code change. Both producer and disposable packaged
test ran the existing browser, network and host ABI gates without bypasses.
The published SDK is `out/build-system-producer/dist/06-web`.

The next failure was confined to ZIP creation: `shutil.make_archive` rejected
epoch-dated source files with `ZIP does not support timestamps before 1980`.
The SDK and packaged test had already passed. `prepare_release_assets` now uses
the standard library's `strict_timestamps=False` for Windows ZIP metadata,
retaining source bytes/mtimes and empty directories; POSIX tar behavior remains
unchanged. Archive construction uses a temporary sibling and atomic replacement,
so failed writes preserve an existing archive. This follows the documented
[ZIP timestamp bounds](https://docs.python.org/3/library/zipfile.html#zipfile.ZipFile),
not a workaround that retimestamps the SDK. The failed partial ZIP is retained as
`crt-development-windows-x86_64-06-web.zip.failed-timestamp`.

Final native commands/results, driven by `out/build-system-review/finish_web_acceptance.py`:

```powershell
cmake --build out/build-system-producer --target crt-web-dist --parallel 4
python tools/verify_dist.py --dist "out/build-system-final replay spaces/06-web" --stage 06-web
python tools/crt_web_stage.py --phase test --sdk "out/build-system-final replay spaces/06-web" --work-root "out/build-system-final replay work spaces" --target-os windows --target-arch x86_64 --jobs 4
```

The configured explicit replay input is the **newly produced** SDK, not an old
artifact substituted for build acceptance. All three commands exit 0. The first
includes `crt-web-test` as a prerequisite. The ZIP's full CRC check passes and
all 3,674 extracted payload files hash identically to the producer. The extracted
spaces-path SDK repeats packaged browser observation, positive/negative HTTP/TLS
and host ABI checks and ordinary verification. All 27 files named by the existing
Development profile's runtime digest map match Full06. Full producer file hashes
remain unchanged after all replay; no source/build-tree-hidden Dev test is claimed.

Final artifact:

- Name: `crt-development-windows-x86_64-06-web.zip`
- Size: 380,422,542 bytes
- SHA-256: `1ca97ccdedc5c115d37959002848e5c89ad12171c0a1ccfa6c5158cdbfb36c98`

Evidence in `out/build-system-review`: `final-web-acceptance.json` (`passed: true`),
`final-web-dist.log`, `final-web-extracted-verify.log` and
`final-web-extracted-test.log`. The relocated test receipt is
`out/build-system-final replay work spaces/test-result.json`; its evidence copy
is `test-ici269h3`. Earlier in-place disposable test receipts remain under
`out/build-system-producer/isolated-web`.

Final regression (`final-tooling-unit.log`): 233 selected, 230 pass, three existing
host-inapplicable skips, 14.408 seconds. Exact existing CI tooling command
(`final-ci-tooling.log`): 96 selected, 95 pass, one existing Darwin-only skip,
7.244 seconds. Fresh CMake/catalog checks also passed. Small regression fixtures
extend the existing job only; no duplicate workflow/full Web build was added.

Provenance remains the frozen `a16492...` source asset above, CRT commit `61ea067`
with dirty changes at creation. The final host-side ZIP helper fix does not alter
that compiled SDK/source identity. The local tag is `development`, and the older
Development SDK's compile identity is retained too: matching runtime bytes is not
a claim of matching clean producer commits. The separate release qualification
must regenerate matching clean, tagged artifacts before release collection.
No artifact was uploaded or promoted.

## Release qualification scope split (2026-10-23)

B4 is complete for local Development SDK production, the strict
`crt-dev-release` / `crt-full-release` implementations, regression fixtures and
the recorded Windows producer/consumer/relocation replay. It does **not** accept
actual clean/tagged release assembly. Earlier checkpoints calling that remaining
rehearsal "B4" describe the former combined scope; their test results and failure
evidence are unchanged.

The independent [Planned work](../../TODO.md#release-qualification-and-assembly-rehearsal)
starts after build-system consolidation and the required native SDK producer /
verification gates. Before accepting local release assembly it must establish:

1. A selected release/rehearsal version and one fixed, clean CRT source commit
   for both Development and Full SDKs and the corresponding source assets.
   Producer/base/compile and collector identities must satisfy the existing
   equality/cleanliness checks; `VERSION` and recipe release URLs must match the
   chosen version. A later clean commit cannot relabel an older dirty SDK.
2. Real positive `crt-dev-release` and `crt-full-release` runs on those matching
   artifacts, not just synthetic fixtures or default-tag rejection. Retain the
   archive/extraction verification, checksums, release manifests, source-offer /
   security checks and inherited-runtime byte equality without bypasses.
3. Final review of the exact artifact identities, commands, results and limits
   before marking release assembly accepted. Tag creation, upload and public
   publication remain separately authorized actions; a local rehearsal grants
   none of them implicitly.

This is a scheduling/ownership split only: no release gate is weakened and no
native replay is newly claimed. B5 Linux/macOS replay and final build-entry-point
review remain required for build-system consolidation. The existing dirty
`development` artifacts remain useful Windows acceptance evidence, not matching
clean release inputs. No build, SDK regeneration, tag or upload was performed
for this documentation change.

## macOS/arm64 native replay (B5, 2026-10-23)

Replay of the in-tree entry points on this macOS/arm64 host, under `out/macos-host-ninja-debug` (preset
`macos-host-ninja-debug`), source `925b1d1` plus the uncommitted fixes listed below. Logs and helper scripts are
local under `out/build-system-macos-20261023`. This accepts macOS/arm64 entry-point behavior only; it is not a
clean/tagged release assembly, and no tag, upload or public artifact was produced.

Results:

- `cmake --workflow --preset macos-host-ninja-debug`: 142/142 C-stage tests; `crt-<stage>-config` preflights and the
  catalog check (`tools/list_cmake_targets.py --check`) pass.
- `crt-gfx-media-test` 29/29, `crt-ui-test` 9/9; `crt-c/libcxx/gfx-simple/gfx-media/ui-dist` produce and verify
  their SDKs.
- `crt-web-build/test/dist` complete on the real isolated Web driver: about 35 minutes for the build (8,302 WebKit
  steps), packaged browser, HTTP/TLS and Host ABI checks, `verify_dist`, and
  `crt-development-macos-aarch64-06-web.tar.xz` (83,880,440 bytes, SHA-256
  `a1be973f458f64fe82df52539e70bf7af673cc60c82a569a845507fa533a0e7b`).
- `crt-development-sdk` and `crt-development-sdk-test`: the SDK verifies and an SDK-only consumer passes its four
  static-link HTTPS cases (verified, wrong-ca, missing-ca, malformed-ca).
- `crt-dev-release` / `crt-full-release` reject the default `development` tag in 0 s with the version-format message,
  before any build.
- Cache: with nothing changed, `crt-web-test` returns in 5 s and every upstream step reports "up to date";
  `crt-web-dist` still spends about 100 s re-verifying and re-archiving `dist/06-web` (not a cached step).
- `tools/audit_macos_bindings.py` over the new `06-web` (148 images) and Development SDK (101 images) reports no
  binding violation beyond the recorded pure-math note.

Defects found by the native replay and fixed (each has a regression test that fails without the fix):

1. macOS static link ordered the application's constructors before libc++'s standard-stream initialization:
   `iostream` smokes crashed at start. `tools/crt-c++` now places the static C++ runtime before application objects.
2. libc++abi patch application appended its condition on every configure, changing a source and invalidating the
   cache; it is idempotent now (`tools/crt-libcxx-build.py`).
3. Selected CTest entries had no build dependency on their executables: 24 of 29 media tests were Not Run on a clean
   tree (`cmake/crt_test_dependencies.cmake`).
4. The default `02-cxx` sysroot and dist overwrote each other; the default preset now has one publisher.
5. Shared networking libraries (zlib, Mbed TLS, curl) copied into the media SDK and the Development SDK kept the
   port link's absolute `LC_RPATH`, which `verify_dist` rightly rejects: `portable_macho_dependency_paths` now removes
   absolute build-tree rpaths and keeps relative and operating-system entries.
6. `crtui_contract_shared_test` could not find the SDK's libc++ after `crtui_shared` moved to it.
7. The packaged network probe linked a static libc next to `libcrtweb`, which binds to the shared libc: two `environ`
   tables, so `CRTWEB_PROCESS_DIR` never reached the launcher and all ten network cases failed with the same static
   program that passes when linked against the shared libc. The probe is now compiled hosted and against the shared
   libc on POSIX hosts (the same class as the Tranche 3 `curl` finding).

Limits and open questions: Linux native replay remains pending. Windows/arm64 is not claimed. A cascade rerun of the
upstream steps after a failed `crt-web-test` was observed once and could not be traced to a changed input (sources,
command lines and `HEAD` were unchanged); the cache key then included the complete `PATH`, so a different launching
shell invalidated every step (narrowed below). The Wayland/xkbcommon target scope (B5's second item) is unchanged.

### Stage cache key: tool identity instead of the `PATH` string (2026-10-26)

`tools/crt_stage_cache.py` no longer puts the `PATH` string in a step's key. It records the identity of the programs a
step may run (`tool_identity.programs` in `tools/stage_cache.json`): each is resolved through `PATH`, followed to its
real file, and recorded as real path, size and modification time. Reordering `PATH`, or adding or removing a directory
that does not change which of those files is found, keeps the cache; a different file found first, a modified tool,
or a newly installed tool that was absent changes the key. `CRT_*`, `CC`, `CXX`, `AR`, `RANLIB` and the flag
variables stay in the key by value. No tool version is executed, so a check costs a stat per listed program. The identity is the file's path, size and
modification time, not its content: a tool replaced by a different file of the same size and the same timestamp at the
same path is not noticed.

Evidence on macOS/arm64: six new `ToolIdentity` tests and a step-level test (25 tests in the module; the step-level
test fails with the old `PATH`-string key). On the real tree, after one run to record the new keys, the same
`crt-libcxx-dist` ran four times with the `PATH` prefixed with an unrelated directory, suffixed with unrelated and
nonexistent directories, and replaced by a different minimal `PATH`: 0 steps re-ran and 7 reported "up to date" each
time (1-2 s). With a different `cmake` placed first the key changes, as it should.

Windows note: `shutil.which` only finds a program whose extension is in `PATHEXT`, so the tests create their fake
programs as `.cmd` there. The first revision (30beb3a) built them without an extension; macOS and Linux passed and the
two Windows hosts failed four of the new tests for that reason, not because of the implementation.

Cost: the key format changed, so every step reruns once after this commit (a full `crt-web-build` is about 35 minutes
on this host). The program list is a hand-kept inventory of what the build resolves through `PATH`; a step that starts
depending on another program must add it, or a change to that program is not noticed.

## Linux/amd64 native replay (B5, 2026-10-26)

**Initial replay failed:** three native failures were found below. Their repair
and subsequent replay are recorded in [the repair evidence](#linuxamd64-repair-replay-2026-10-26).
Started from clean `6af554105a9e4aa0515d93e459410956ca8c2137`, with no existing
`out/`, using `linux-host-ninja-debug`. Host: Linux/x86_64 kernel
`7.0.0-38-generic`, Clang 21.1.8, CMake 4.2.3, Python 3.14.4. Logs, diagnostic
helpers and JSON measurements are local under `out/build-system-linux-20261026`;
the build is `out/linux-host-ninja-debug`. No implementation or SDK contract was
changed to pass this replay.

Passing checks:

- Fresh default configuration and all six `crt-<stage>-config` targets pass.
  The catalog has 28 public and 378 internal targets, no missing or unlisted
  stage targets (`targets.json`). A second configuration in `offline tree with
  spaces` passes all six without creating dependency/build payload directories.
  With LVGL disabled, C/media preflights still pass; UI preflight rejects the
  missing option and removes its old success report. Reapplying the preset
  restores all six. Both release targets independently reject `development`
  before any SDK producer runs (`preflight-result.json`, `offline-*.log`).
- `crt-c-build` and `crt-c-test`: 142/142 pass (`c-build.log`, `c-test.log`).
  The ordinary `cmake --workflow --preset linux-host-ninja-debug` also passes
  its configure/build/test sequence and all 142 tests (`workflow.log`).
  `crt-gfx-simple-test`: both window/input tests pass. `crt-gfx-media-test`:
  Graphics 10/10 and Media 30/30 pass, including capture/encode lifecycle,
  VA-API and network tests. `crt-ui-test`: 8/8 pass. No selected executable is
  missing (`upper-entrypoints.log`, `upper-tests.log`).
- `crt-c-dist`, `crt-libcxx-dist`, `crt-gfx-simple-dist` produce and verify
  `01-c`, `02-cxx`, `03-gfx-simple` and their archives. Imported C++ compilation
  succeeds; its runtime smoke is only partially passing, as recorded below.
- `crt-development-sdk` produces `dist/dev`, and ordinary plus `--profile dev`
  verification passes. All 23 inherited runtime file digests equal both the
  plain `02-cxx` and `03-gfx-simple` SDKs (`runtime-identity.json`). This does
  not establish equality with a newly built Full `06-web` SDK.
- The Development reference consumer's **static** link passes verified HTTPS
  with inflate, wrong CA, missing CA despite ambient CA variables, and malformed
  CA. A diagnostic rerun logs each actual subprocess result without changing
  validation (`dev-consumer-cases.json`). The complete consumer gate fails on
  shared linkage below; no successful overall `result.json` is claimed.
- Actual `crt-libcxx-dist` cache replay: unchanged PATH, unrelated prefix,
  unrelated/nonexistent suffix, and a PATH containing only symlinks to the same
  selected tools each report **7 up to date, 0 running**, in 1.68–1.70 s.
  Putting a different `cmake` file first changes the tool identity. No alternate
  tool build is claimed (`cache-result.json`, `cache-*.log`, `cache_replay.py`).
- Tooling regressions: 249 tests total, 242 pass and 7 explicit skips, across
  `tooling.log`, `distribution-tooling-replay.log`, and
  `macos-fix-regressions.log`. Modules with script-relative imports are run with
  `PYTHONPATH=tools`; the earlier invocation without it is retained as a harness
  error in `distribution-tooling.log`, not a product failure.

Blocking failures:

1. **Ordinary media packaging rejects an installed ELF input.**
   `crt-ui-dist` reaches `crt-gfx-media-dist`, where
   `crt_relink_kit.rewrite()` rejects `libm.so` as different from the build.
   Build SHA-256 is `4609cebae46efc924ddec9968c331dbc77e9edb80ab26f339cc3948c6319ff7d`;
   installed SHA-256 is `8497286e384e59c4a91f3b6f1bd4bff0796c13874fae74795bf3404a3e8a0822`.
   CMake installation changes RUNPATH from `$ORIGIN:<build>/lib:` to `$ORIGIN`.
   Applying CMake's exact `file(RPATH_CHANGE)` to a disposable copy of the
   build input yields byte-for-byte equality with the installed input
   (`relink-mismatch.json`). Thus the first rejection is an expected install
   transformation, not evidence of a foreign `libm`. The fix must preserve
   rejection of unrelated libraries and archive corruption; matching a basename
   alone remains insufficient. The failed `dist/04-gfx-media` directory is a
   partial candidate, not an accepted SDK.
2. **Shared imported C++ exceptions fail.** `crt-libcxx-test` passes the static
   exception/vector, string ABI and iostream smokes, then its shared
   `imported_libcxx_test` terminates with exit 134 and
   `uncaught exception of type std::runtime_error: caught`. Direct rerun
   reproduces static success/shared failure (`cxx-reproduce.json`). GDB shows
   SDK-owned libc++, libc++abi, libunwind, libc, libm and libdl loaded;
   `_Unwind_RaiseException` returns `_URC_END_OF_STACK` (5) to `__cxa_throw`
   (`cxx-shared-throw.log`, `cxx-shared-unwind.log`). Root cause and whether
   this is a regression from the consolidation are not established. Later
   shared string/iostream checks are not reached by the driver.
3. **Development shared consumer's loader is undeclared.** The producer and
   static trust cases pass, but `crt-development-sdk-test` rejects
   `ld-linux-x86-64.so.2` in the shared consumer's import closure before its
   HTTPS cases run (`development-sdk.log`, `dev-consumer-trace.log`).
   `crt-c++` explicitly records the OS loader for shared-runtime TLS, while
   `crt_dist_prerequisites.py` currently declares it only at `06-web`.
   Aligning the earlier-stage producer/consumer contract requires the owning
   SDK contract review; this replay does not edit a manifest, relax the closure
   check or authorize consumer adoption.

Reproduce the failing gates with:

```sh
cmake --preset linux-host-ninja-debug
cmake --build out/linux-host-ninja-debug --target crt-ui-dist --parallel 8
cmake --build out/linux-host-ninja-debug --target crt-libcxx-test --parallel 8
cmake --build out/linux-host-ninja-debug --target crt-development-sdk-test --parallel 8
```

`crt-ui-dist` cannot publish `05-ui`, so native `crt-web-build/test/dist` has
**not run** in this replay. Relocated Development shared-consumer replay and
the Full `06-web` runtime comparison also remain open. B5 and final closure
stay open until these failures are repaired and the dependent gates pass.
Wayland/xkbcommon scope and the separate Planned clean Wayland dependency gate
are unchanged. Clean/tagged release qualification remains separate; no tag,
upload, public release, CLI compatibility claim or consumer lock update was made.

## Linux/amd64 repair replay (2026-10-26)

Following the initial evidence commit `b166bf138e0bf5b3837230f75bbd9856fe528e79`,
the three lower-stage failures are repaired. Implementation identities and logs
are local under `out/build-system-linux-fixes-20261026`; the initial failed
logs and original runtime identities remain under the preceding replay root.
These are local `development` builds with explicit dirty-source provenance,
not clean/tagged release qualification.

- **L1, relink packaging:** `installed_input_matches` retains exact comparison
  for archives/non-ELF files, and permits only byte-for-byte results of the
  existing CRT portable RUNPATH rewrite or CMake's install `RPATH_CHANGE` on a
  disposable ELF copy. It does not mask byte ranges or trust basenames. The
  native regression builds an ELF library, applies both real transformations,
  accepts them and rejects other byte changes or a foreign RUNPATH. Source
  inputs remain untouched. Recipe rewriting also preserves the shell-escaped
  `$ORIGIN`, drops absolute build RUNPATHs and removes CMake's link dependency
  output (`-Xlinker --dependency-file=...`). The actual captured media link
  now creates a kit, relinks `libcrtmedia.so`, and runs a C frame/runtime API
  consumer through the SDK and LLD (`actual-relink.log`,
  `relinked-consumer-lld.log`). The published `05-ui` kit also passes actual
  relink and SDK consumer execution (`published-relink.log`). The earlier diagnostic C++ driver invocation
  with a persistent `-x c` was a harness error; GNU ld's separate host-libva
  loader-import error is retained in `relinked-consumer-final.log`. LLD is
  the project linker used for the successful consumer; no host SDK or runtime
  library was added to its command to make it pass.
- **L2, shared unwind:** Linux libdl locates the main executable's dynamic
  table through `AT_PHDR`/`PT_DYNAMIC`, reads its `DT_DEBUG`, and skips that
  exact main image in the rendezvous walk. A shared library's own `_DYNAMIC`
  lacks the main `DT_DEBUG`; using it reported only the main program to LLVM
  libunwind. The new CTest regression uses shared libc/libdl and requires the
  main and shared-libdl images exactly once. It fails against the previous
  SDK (`main=1 shared-libdl=0`) and passes against the fix
  (`shared-dl-regression.json`). Normal `crt-libcxx-test` now passes exception,
  string ABI and iostream tests with both static and shared linkage
  (`fixed-entrypoints-replay.log`). No upstream source patch or public ABI
  change was needed.
- **L3, Development loader closure:** the canonical OS loader prerequisite
  starts at `02-cxx` and is inherited exactly once through `06-web`. Explicit
  x86_64/aarch64 manifests name only their own loader. Both architecture/stage
  fixtures pass; the C-only prerequisite contract is unchanged. The normal
  Development producer, profile verification and all eight static/shared
  HTTPS/inflate and trust-negative cases pass
  (`fixed-entrypoints-replay.log`,
  `out/linux-host-ninja-debug/development-consumer/result.json`).

The same normal run verifies `01-c` through `05-ui` and passes 143/143 C-stage
tests, including the shared libdl regression. Final tooling checks pass 199 of
204 tests with five explicit skips (`tooling-final.log`). Intermediate runs
made while relink fixes were being completed correctly refused to cache success
when an input changed. The subsequent `native-final.log` run compiled all 8,306
WebKit steps and passed `crtweb_api_test`, then exposed L4 below before browser,
network or Host ABI candidate acceptance. No successful Web publication is
claimed for that run.

- **L4, cold Wayland outputs:** the SDK-only GPU worker could not link because
  `libxdg-shell-protocol.a` was missing from the ordinary predecessor. This is
  the Planned configure-time `EXISTS` gate becoming a real Web blocker.
  The native backend now depends on the Wayland producer, whose generated
  archive/header are Ninja byproducts. The configured native-backend flag is
  exported to the stage publisher, which passes the protocol installation
  inputs before the archive exists. The old cycle rationale no longer applies:
  `sysroot` installs only crt-c, so Wayland -> expat/libffi -> sysroot does not
  depend back on graphics. Starting without Wayland outputs, an ordinary
  `crtgfx_window_demo` build creates the archive/header and native backend
  (`wayland-dependency.log`). A separate empty-tree configure/graph/dry-run
  regression confirms producer rules, native activation and packaging arguments
  before any fetch/build (`wayland-graph-test-final.log`); it is registered in
  CTest. Explicit cross builds can pass `CRTGFX_WAYLAND_HOST_SCANNER`, forwarded
  to the existing pinned `--host-scanner` tool interface. Full cold SDK/example
  proof remains with the Planned gate until publication/rebuild is exercised.
  The failed Web predecessor and source asset are retained in
  `failed-web-predecessor-05-ui` and `failed-web-source.tar.xz`. Regenerating that
  predecessor changes its identity and requires a fresh Web work-root; compiled
  objects from the failed run are not silently adopted.

### SDK prerequisite contract review

Producer owner: CRT's `crt_dist_prerequisites.py`, ordinary/Development SDK
publishers, `verify_dist.py` and import-closure fixtures. Review baseline:
CRT `b166bf138e0bf5b3837230f75bbd9856fe528e79`; inspected consumer repositories:
CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93` and Studio
`27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`. Their local design/acceptance
documents and CLI-owned `docs/compatibility.md` still claim no approved
runtime combination. No executable consumer implementation or artifact lock
is updated here.

The implemented change moves the existing `linux-elf-loader` declaration from
Web-only to C++/Development and later Full stages, with `required_for:
shared-cxx-runtime-tls`. SDK manifest format 1, profile format 1, port recipe
version 1 and isolated-stage recipe version 2 remain separate and unchanged.
The existing OS loader boundary remains external; host libc/C++ dependencies
are not authorized. Older candidates with missing or Web-only loader metadata
must be regenerated using the matching producer/verifier. Strict verification
continues rejecting missing, duplicate, wrong-architecture and contradictory
metadata; no installed manifest was hand-edited. Producer gates L3 are exercised
on Linux/amd64; Linux/aarch64 and other hosts are fixture-only for this change.
CLI/Studio live integration and adoption are **Not run** and require later
producer artifact promotion and review in the CLI-owned compatibility ledger.

