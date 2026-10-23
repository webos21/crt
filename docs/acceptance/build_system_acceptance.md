# Build-system consolidation acceptance

## Scope and ownership

This record covers the in-tree build entry points, not renewed acceptance of
the isolated SDK chain. [The target catalog](../guides/build_targets.json)
owns user-facing target names; [distribution](../guides/distribution.md) owns
SDK construction and isolation. [TODO](../../TODO.md#build-system-consolidation)
owns the ordered B0-B5 work. Existing Full SDK and proposed Development SDK
production remain separate from release publication approval.

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

This records the initial 2026-10-22 checkpoint. The later
[deep-work-root repair](#deep-windows-work-root-repair-2026-10-23) owns the current
attempt and its active log; earlier failed attempts are preserved as evidence.

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
Positive current Web production/replay remains pending. Linux/macOS replay will
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
dependency/compile cache guard was relaxed for the changed drivers. Active log:
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
