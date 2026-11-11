# Full / Development SDK split acceptance

This record owns the S2-S5 producer/consumer gates of the Full / Development SDK split (complete
2026-10-27; the TODO tranche was closed and summarized in [HISTORY](../../HISTORY.md)). Content and
identity policy belong to the [SDK contract](../design/sdk_development_contract.md#full-sdk-and-development-sdk-adopted-direction-2026-10-22);
commands belong to the [distribution guide](../guides/distribution.md#development-sdk-producer-and-external-validation).
Original B4/B5 results remain in [build-system acceptance](build_system_acceptance.md#development-sdk-windows-replay).

## SDK/CLI technical bootstrap gate review (2026-11-02)

The owner accepts work-queue gates **1.5 and 1.6 as complete at the recorded
local/unpromoted combinations**. Startup/link policy, basic CLI diagnosis,
one-executable feasibility, relocated SDK-only build/run and native ABI/import
evidence have been reviewed across five native pairs:

| Pair | Producer and consumer evidence |
| --- | --- |
| Linux/x86_64 | [Latest clean SDK/CLI review](../../HISTORY.md#revalidate-the-latest-cli-on-linuxx86_64-with-a-fresh-clean-dev-sdk), linking the original consumer acceptance |
| Linux/aarch64 | [Native SDK/CLI replay](#linuxaarch64-latest-source-cli-replay-2026-11-02) |
| macOS/arm64 | [Native SDK/CLI replay](#macosarm64-latest-source-cli-replay-2026-11-01) |
| Windows/x64 | [Clean SDK replay including the spawn-environment fix](#windowsx64-latest-source-cli-replay-2026-11-02) |
| Windows/arm64 | [Clean SDK replay including the spawn-environment fix](#windowsarm64-latest-source-cli-replay-and-spawn-environment-fix-2026-11-02) |

This is a review of existing execution records, not five new executions on the
reviewing Mac, one common latest SDK/CLI revision, or approval of untested
combinations. Exact source/SDK/provider/tool identities, conditional skips,
isolation mechanisms and per-host limitations remain in those records and the
[CLI-owned ledger](https://github.com/webos21/crt-cli/blob/8fca58af136cfc50efed196529e23cf9b6572629/docs/compatibility.md),
reviewed at CLI `8fca58a`. Windows process accounting is not relabeled as an
execution-denial sandbox. Previously recorded failures remain retained.

The stale-cache/source-asset finding does not undo these scoped runtime passes.
**At that review, 1.7 remained open** for canonical-source/cache repair, common asset/recipe
generation, affected validation and final selection under the
[adopted naming contract](../design/sdk_development_contract.md#common-source-stage-assets-adopted-2026-11-02).
New or regenerated candidates need their own affected-scope validation; they
do not inherit these exact artifact results automatically. Producer promotion,
production consumer pins, public release and complete D1 acceptance are not
granted by closing these two technical gates. STATUS is unchanged.

### Tranche 1.7 local technical closure (2026-11-02)

The [common-source selection](release_qualification.md#common-source-candidate-selection-2026-11-02)
now closes 1.7 for the selected Linux 02/03 source assets and the SDK/CLI
technical candidate. It records canonical-cache repair, fresh isolated source
builds, native C++/Development consumers and the same-asset aarch64 cross
compile/package scope. The [CLI-owned selection](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#common-source-candidate-selection-2026-11-02)
records the external handoff, exact provider/CLI/toolchain and original consumer
results. Earlier five-pair native evidence above keeps its own identities;
new ARM execution or macOS/Windows SDK acceptance is not inferred. No production
promotion/pin or public release is performed. Tranche 2/D1 remains open.

## Accepted native matrix

| Pair | Development producer / ordinary consumer | Producer-tree-hidden consumer |
| --- | --- | --- |
| Linux/x86_64 | Current producer and static/shared eight-case replay pass below | Pass below |
| macOS/arm64 | Current producer, archive/extraction, static/shared eight-case replay and shared-consumer binding audit pass below | Pass below, using the extracted archive |
| Windows/x64 | Clean default C/C++/Dev and cumulative Full production plus packaged S5 pass below; earlier external-predecessor evidence is retained | Pass below, using the extracted archive |
| Linux/aarch64 | Native producer, archive/extraction, static/shared eight-case replay and port recipe tests pass below | Pass below, using the extracted archive |
| Windows/arm64 | Native producer, archive/extraction, static/shared eight-case replay and port recipe tests pass below | Pass below under a restricted token, using the extracted archive |

S4's local collector implementation and negative/atomic fixtures are complete.
Actual clean positive collection belongs to the independent
[release qualification record](release_qualification.md), which retains the
later matching artifacts and assembly review. No tag, upload, consumer lock or
runtime adoption is part of this SDK split record.

## S5 native negative and candidate handoff gate

S2/S3's five native producer and isolated-consumer results remain accepted at
their recorded baselines. S5 adds required negative/pairing evidence and hands
off identified producer candidates; it does not wait for a CLI implementation
or grant release qualification. The accepted native scope is:

| Pair | Native negative evidence | Candidate identity |
| --- | --- | --- |
| Linux/x86_64 | Eight S5 negatives and packaged isolated consumer pass below | New `handoff.json` below |
| Linux/aarch64 | Missing static/shared curl, changed inherited runtime and stale Full rejection already passed in the [native replay](#linuxaarch64-native-producer-and-package-replay-2026-10-27) | Existing native archive/source identity retained there |
| macOS/arm64 | Eight S5 negatives and packaged isolated consumer pass [below](#macosarm64-s5-replay-2026-10-27) | New `handoff.json` below |
| Windows/x64 | Eight S5 negatives and packaged isolated consumer pass [below](#windowsx64-clean-production-and-s5-replay-2026-10-27) | New `handoff.json` from clean frozen Full/Dev inputs below |
| Windows/arm64 | Eight S5 negatives (including stale Full) and packaged isolated consumer pass [below](#windowsarm64-clean-production-and-s5-replay-2026-10-27) | New `handoff.json` from a clean Dev + Full `05-ui` pair below |

### Linux/x86_64 S5 replay (2026-10-27)

Validation baseline: CRT `4a79120` plus the S5 driver/tests/CI changes. This
replays the existing identified Dev candidate compiled at clean `181f74b`,
based on plain `02-cxx` at `857f831`; it does not relabel or rebuild those
artifacts as current clean source. The Full reference is the existing `06-web`
at `857f831`. All **23** inherited runtime digests actually match. Full reference
target and runtime bytes are checked before a success receipt is possible.

```sh
python3 tools/test_sdk_split_s5.py \
  --sdk-root out/dev-full-sdk-20261027/dev \
  --full-sdk out/linux-host-ninja-debug/dist/06-web \
  --output out/sdk-s5-linux-20261027/replay-verified
```

The ordinary verifier accepts both inputs. The existing archive helper packages
the Dev SDK as `dev/`, without invoking a release collector. Extraction into a
space-containing path passes ordinary/profile verification and exact payload
inventory comparison. The relocated SDK then passes the current source-hidden
driver: static/shared verified HTTPS+inflate and wrong/missing/malformed explicit
CA, **eight cases**. The shared closure contains **11 SDK images** and the
declared `ld-linux-x86-64.so.2` boundary; static linkage has no dynamic imports.

Archive: `archives/crt-development-linux-x86_64-dev.tar.xz` under the evidence
root above, **8,938,536 bytes**, SHA-256
`824b9f28df025e80c935aad80f177131d3160b4979db32926d8b7172ef2fb619`.
Original/extracted payloads have **2,579** identical file/symlink entries.
Both original Dev and Full inventories remain unchanged after replay.

Seven separate private Dev copies are mutated and rejected by
`verify_dist.py --profile dev`: missing static curl, missing shared curl, changed
inherited libc, upper-stage content, missing curl notice, missing curl
declaration and bundled/ambient trust. Each must fail with its expected
diagnostic, not merely any process error. A private Full copy changes only its
source identity (and the corresponding Web overlay source identity so that the
unrelated overlay-binding gate still holds). The collector's unchanged
`check_pair` rejects that stale source. A unit fixture starts from an accepted
clean matching pair before applying the same stale-Full mutation; actual
release artifacts are never relabeled. These are **eight** negative cases.

Evidence is retained in `out/sdk-s5-linux-20261027/replay-verified/`: input and
extracted verifier logs, seven negative logs, `external/{command.json,
consumer.log,result.json}`, archive and `handoff.json`. The receipt records the
archive size/hash, exact Dev manifest/profile hashes, original producer/base/
compile identities, runtime correspondence, native import closure, negative
results and validation source revision. `source-identity.json` in its parent
identifies the validation working-tree files. Earlier local attempts remain
separate; the final run uses the existing long-path-safe copy helper.
The focused Development/S5 unit suite passes **29/31** with two native-Windows
skips (`unit.log`). The exact existing CI tooling selection passes **144/150**
with six host-conditional skips (`ci-tooling.log`), both under
`out/sdk-s5-linux-20261027/`.

### macOS/arm64 S5 replay (2026-10-27)

Validation baseline: CRT `a79a06b`, a clean working tree. Unlike the Linux/x86_64 candidate above, the Development SDK
and the Full reference were both produced here from that one commit: Dev `producer`, `base` and `compile` commits are
`a79a06b585a7da2322c6acadb3da963c9208d128` with no dirty tree, the Full `06-web` and the plain `02-cxx` carry the same
commit, and all **18** inherited runtime digests match. The candidate is therefore a matching clean pair by source
identity; it is still local `development`-versioned evidence, not a tagged release.

```sh
python3 tools/test_sdk_split_s5.py \
  --sdk-root out/macos-host-ninja-debug/dist/dev \
  --full-sdk out/macos-host-ninja-debug/dist/06-web \
  --output out/sdk-s5-macos-20261027/replay
```

The driver exits 0 in 61 s. The archive `archives/crt-development-macos-aarch64-dev.tar.xz` is **6,358,972 bytes**,
SHA-256 `c8689bba4ec3d352e43f54e3b6233e3290d5f08197108a501c74d2b87ae9b75b`; the extraction into a path with spaces has the
same **2,570** payload entries and inventory digest (`4b35271568c0e66a...`) as the original. The relocated SDK passes
the producer-tree-hidden consumer in both link modes with verified HTTPS plus inflate and the wrong, missing and
malformed explicit CA cases (four each); the static consumer imports only `libSystem` and owns its own `malloc`, the
shared consumer has no `malloc` of its own and binds every libc symbol to `libc.dylib`. `tools/audit_macos_bindings.py`
over the extracted SDK reports no violation (101 images).

The eight negatives each fail with their expected diagnostic: missing static curl, missing shared curl, changed inherited
libc digest, upper-stage content, missing curl notice, missing curl declaration, ambient/bundled trust, and a Full
reference whose source commit alone is changed (`Development/Full SDK need the same known clean source commit; rebuild
stale predecessors`). `handoff.json` records `promotion: not-promoted`, `release_qualified: false` and
`cli_adoption: not-run`; the original Dev and Full payloads are unchanged afterwards.

A defect found on the way: the first Dev SDK built here did **not** match the Full SDK, although both came from the same
commit. `copy_libcxx` copied `libc++.a` and `libc++abi.a` with their build-time member dates and owner (`501/20`), while
every later stage rewrites its archives deterministically (`0/0`, 1970). All 74 members were identical (55 and 19), only
the archive headers differed, and `ranlib -D` on a copy reproduced the Full bytes exactly. The macOS packaging now runs
`normalize_macos_archives` after `copy_libcxx` (`a79a06b`, with a regression test that fails without it); Linux was not
affected (23 digests matched there). The SDKs in this record were rebuilt from the clean fix commit.

Evidence is under `out/sdk-s5-macos-20261027/`: the build logs, `driver.log`, and `replay/` (verifier logs, eight
negative logs, `external/`, the archive and `handoff.json`). Not run here: hosted CI execution of this driver on macOS,
Windows/x64 and Windows/arm64 rows, and any release collection.

### Windows/x64 clean production and S5 replay (2026-10-27)

**Windows/x64 S5 accepted** (Windows/arm64 follows in its own section below). Started with no `out/` directory and a
clean checkout at CRT `b99ac7462a3b5c3230f55f7059320d01ceb2a9dc`. The native
Windows/x64 preset keeps imported libc++, Skia, FFmpeg, curl and LVGL enabled;
no external predecessor or old source/build cache is supplied. Build-host
Clang is 22.1.8, CMake 3.29.2 and Python 3.11.9. The installed Windows SDK is
10.0.28000.0 (`um/x64`).

The one-shot driver `out/s5-windows-x64-20261027/run_clean.py` records commands,
exit codes and phase logs in `status.json` and `logs/`. Build trees, downloads,
host tools, port work, ordinary temporary files and SDKs stay under `out/`.
Git POSIX Perl's missing pure modules are copied from the installed Strawberry
distribution to `host-perl-modules/` within that evidence root, with original
paths and SHA-256 values in `host-perl-inputs.json`. The native Perl requirement
probe passes; there is no global module installation.

The C stage completes 164 tests with no failures (one conditional skip), C++
static/shared/string-ABI/iostream smokes pass, and the new Development SDK
passes its profile and eight ordinary static/shared HTTPS/inflate/trust cases.
Window/input passes 2/2, GPU/Skia 10/10, media 27/28 (one conditional skip),
and UI 8/8. The Media Foundation capture test is conditionally skipped;
no physical camera observation is claimed.

All `01-c` through `05-ui` manifests and the Development producer/base/compile
identities name this same commit. `producer-source-checkpoint.json` records
the clean checkpoint and five stage-source recipes. The frozen `06-web` source
archive is 19,535,080 bytes with SHA-256
`834605caf2eaec3959f22400aadfb6cb2aa1c516b0c6983816508f4f87125ba5`;
its `stage.json` records `working_tree_dirty: false`. The isolated Web producer
is `out/windows-host-ninja-debug/isolated-web/run-vuwgyp5y`. Its producer gates,
publication and the official adapter's disposable packaged test and archive
phases complete successfully. Browser observation, HTTP/TLS positive/negative
cases, source-offer checks, host ABI and ordinary verification are required
without bypasses. The published Full SDK is `out/windows-host-ninja-debug/dist/06-web`.

The first Web attempt stops at OpenSSL `configdata.pm`: Git POSIX Perl also
needs `Pod::Usage`. Seven additional pure modules from the installed host Perl
are recorded in `host-perl-repair.json`; actual `configdata.pm` syntax and help
probes then pass. The failure is retained in `logs/full-web-dist.log`.

Adding a retry option to CMake re-enters earlier packaging because the complete
`CMakeCache.txt` is fingerprinted. That owned recovery is explicitly stopped
before `05-ui` changes. Its log and intermediate source/packages are retained.
Task documentation/unit edits are temporarily saved as
`pending-validation-docs.patch` and reversed to return to the clean commit.
The four upper source assets reproduce exactly. Recreating initial `02-cxx`
requires the pristine pinned Git checkouts: the configured working copies
already contain the existing recipe patches/shims. All five initial source
SHA-256 values are recovered and checked in `source-restore-status.json`
(`passed: true`). Official `create_dist.py`/`verify_dist.py` repackage `01`-`03`
without compilation/relinking; inherited runtime bytes still match Dev and the
old intermediate packages remain under explicit backup names.

The Web recovery now calls the official adapter's build/test/dist phases
directly, avoiding another CMake dependency traversal. Both original source
SHA-256 and `05-ui` predecessor inventory are checked, then the existing
isolated reuse guard runs unchanged. `direct-status.json` and
`logs/web-direct-*.log` own this attempt. The saved validation/documentation
patch is restored after clean source/package recovery; compiled Web inputs
remain the original frozen clean asset.

After all target dependency builds/tests, the next failure is the checkout file
count in `fetch_webkit_commit.py`: plain `Path.rglob()` cannot traverse a
258-character directory whose child paths exceed the Win32 limit. Git has
already completed checkout. The current host helper now uses
`crt_file_ops.filesystem_path` for this diagnostic's IO only. A 350+ character
fixture fails before the fix and passes afterwards, preserving normal Git
arguments and the HEAD check. It is added to the existing CI-selected
`ExecutedRecipeTests`, not another workflow. The updated CI selection passes
**147/151**, four conditional skips (`logs/ci-tooling-after-count-fix.log`).

`actual-webkit-count.json` verifies the real failed checkout with the repaired
helper: **69,576 files**, commit
`537776966344c4982751692dd475c94124504271`, tree
`60295deedb6abea300bfae633223baed8c8f6258` and the pinned **462,083-entry**
listing. Its working tree remains pristine before and after. The original
frozen source archive still carries the earlier helper; it is not replaced or
claimed to include the local fix. Normal composition sees the already-complete
verified checkout and reuses it. Both source/predecessor guards remain in force.
The latest build attempt is `direct-cached-status.json` and
`logs/web-direct-*-cached.log`; the prior failure is retained separately.

After freezing these source inputs, two unit fixtures were adjusted to model
their own synthetic hidden producer rather than assume TEMP lies outside the
checkout. This lets their launcher-failure/no-fallback assertions run with
TEMP under `out/`; production visibility/disjointness checks are unchanged.
The first CI-tooling log retains those two failures and a transient release
fixture rename `WinError 5`; the latter passes an independent same-root rerun.
These validation-only edits do not relabel the frozen clean SDK inputs.
The first corrected CI tooling selection passes **146/150**, with four
host-conditional skips (`logs/ci-tooling-final.log`), using TEMP under `out/`.

The initial S5 replay passes its isolated consumer but stops at a notice
negative: the verifier prints a native Windows path, while the expected notice
path uses `/`. The driver now normalizes only path separators for diagnostic
comparison. Wrong filenames, unrelated diagnostics and case mismatches still
fail. `logs/s5-replay-cached.log` and `replay/` preserve that failed attempt.
The final CI tooling selection passes **148/152**, four host-conditional skips
(`logs/ci-tooling-path-diagnostic.log`); there is no workflow/skip-policy change.

Final native S5 command (external Clang/AR/RANLIB/OpenSSL and Windows SDK import
libraries, as in the source-hidden guide):

```powershell
python tools/test_sdk_split_s5.py `
  --sdk-root out/windows-host-ninja-debug/dist/dev `
  --full-sdk out/windows-host-ninja-debug/dist/06-web `
  --output out/s5-windows-x64-20261027/replay-portable
```

The driver exits 0. Its package has **2,695** payload paths. The restricted token
denies the whole checkout, its `out/`, original SDK headers/runtime and fresh
descendants' reads. Static/shared verified HTTPS+inflate and wrong, missing and
malformed explicit CA pass: **eight cases**. The shared app-local closure is
**ten SDK images** with System32-only PATH; static linkage has no packaged DLL
imports. Both modes import only `kernel32.dll` and
`api-ms-win-core-synch-l1-2-0.dll` outside the package. Seven private profile
mutations fail with their expected diagnostics, and the unchanged collector
pair check rejects stale Full source identity: **eight negatives**.

All **27** inherited runtime digests match plain `02-cxx`, Full `05-ui`/`06-web`
and Dev. Producer/base/compile identities are the same clean `b99ac74` above;
Full uses its original frozen clean source archive, not the local host-tool
count repair. The validation checkout retains the separate tool/test/doc edits.
The collector's read-only `check_pair` and clean `check_full_source` checks
also pass on the real candidates/source archive; no release assembly is run.
Original Full/Dev payloads remain unchanged. `replay-portable/handoff.json`
records `promotion: not-promoted`, `release_qualified: false` and
`cli_adoption: not-run`.

| Archive | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-development-windows-x86_64-dev.zip` | 144,972,586 | `700145a7b2229b4bef85f066bbb000cf6ae795dc87675399f8793b1379e2b5b9` |
| `crt-development-windows-x86_64-06-web.zip` | 380,413,782 | `07b73a9d6db51b933f0a49ceca013d2341694a42be3094051f84a2d6aed0654d` |

Full ZIP extraction into `Full SDK extracted with spaces/06-web` checks every
CRC, matches all **3,673** payload bytes/paths and passes `verify_dist.py`.
Full/Dev ZIPs materialize the two declared internal zlib file aliases; link
types are recorded separately from unchanged bytes. `full-archive-result.json`
retains those details. No relocated Full browser run is claimed by that archive
check; the disposable packaged browser/network run is the adapter test above.

Evidence root: `out/s5-windows-x64-20261027/`, notably
`replay-portable/{handoff.json,external/result.json}`, the eight negative
receipts/logs, `logs/s5-replay-portable.log`, `full-archive-result.json`,
`source-restore-status.json` and the failure/recovery logs described above.
`final-result.json` consolidates the successful gates and validation-file hashes;
earlier failure/interruption status files remain intact.
Only the restricted-token consumer's disposable copy must live outside the
checkout: hiding the whole producer tree also hides its `out/`. The final work
is `C:/Users/Lee/AppData/Local/Temp/crt dev external yljokfkf`, retained in the
receipt; logs return under `out/`. CMake's temporary retry-run setting is cleared
after success; test/dist replay explicitly selects this newly built `06-web`.

Artifact promotion, CLI adoption and tagged release collection remain separate. In particular `dist/06-web/share/crt/web-security.json` and the
manifest's release decision keep `public_release_allowed: false`: the stable
WebKit micro pin must move **2.54.1 -> 2.54.2** before release qualification.
That report is preserved, not bypassed or treated as an S5 runtime failure.

### Windows/arm64 clean production and S5 replay (2026-10-27)

Host: the Windows 11 arm64 VM of the earlier Windows/arm64 records (4 vCPUs, no GPU), native arm64 shell and Python 3.11.9, Clang 22.1.8,
CMake 4.4.4, Windows SDK 10.0.28000 (`um/arm64`). Validation baseline: CRT `3810c7c3ef0576c70da09613d578ac17106916c8`, clean working tree
before and after production. **The Full reference is `05-ui`, not `06-web`:** `tools/build_stage_06_web.py` does not support Windows/arm64 by design,
so `05-ui` is the highest Full stage this pair has. The driver accepts any cumulative stage from `02-cxx` to `06-web` as the Full
reference and the collector's pair check only inspects the Web overlay recipe when the stage is `06-web`; the clean-source asset check
(`check_full_source`) is Web-only and is therefore not applicable here.

Both SDKs were produced from that one commit (the earlier in-tree SDKs predated `source_commit` in the `03`-`05` manifests and could
not form a pair):

```powershell
cmake --build out/windows-host-ninja-debug --target crt-c-build crt-ui-dist --parallel 4
cmake --build out/windows-host-ninja-debug --target crt-development-sdk-test --parallel 4
```

The Development sidecar records producer, base and compile commit `3810c7c`, `producer_working_tree_dirty: false` and **27** inherited runtime
digests; the Full `01-c`, `02-cxx` and `05-ui` manifests carry the same commit. Ordinary and `--profile dev` verification pass and the ordinary
consumer passes its eight cases. The new ports were rebuilt, so the archive hashes differ from the earlier Windows/arm64 package while the 27
runtime digests are identical to it (deterministic runtime bytes).

```powershell
python tools/test_sdk_split_s5.py `
  --sdk-root out/windows-host-ninja-debug/dist/dev `
  --full-sdk out/windows-host-ninja-debug/dist/05-ui `
  --output out/s5-windows-arm64-20261027/replay
```

The driver exits 0. Package: `archives/crt-development-windows-aarch64-dev.zip`, **138,495,124 bytes**, SHA-256
`1d961c8ce159177d239bafd2d4beed34a0cce13b3d763f970e32176768a479c0`; **2,695** payload paths, with the two declared internal zlib aliases
(`lib/libz.so`, `lib/libz.so.1`) materialized as files by the ZIP helper, as on Windows/x64. The restricted token (`windows-restricted-token`)
denies the whole checkout, its `out/` and fresh descendants' reads (`descendant_denial_checked: true`); static and shared consumers pass
**eight cases** (verified HTTPS+inflate, wrong, missing and malformed explicit CA). Static linkage has no packaged imports; the shared closure is
**ten SDK images** run app-local with a System32-only PATH; both modes import only `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll`
outside the package.

Eight negatives fail with their expected diagnostics: missing static curl, missing shared curl, changed inherited runtime digest, upper-stage
content, missing curl notice, missing curl declaration, bundled/ambient trust policy, and a Full reference whose source commit alone is changed
(`Development/Full SDK need the same known clean source commit; rebuild stale predecessors`). The collector's read-only `check_pair` also
accepts the real, unmutated Dev + Full `05-ui` pair. Original Dev and Full payloads are unchanged afterwards (`original_payloads_unchanged: true`).
`replay/handoff.json` records `promotion: not-promoted`, `release_qualified: false` and `cli_adoption: not-run`; `full_reference` is
`05-ui` at `3810c7c` with `runtime_bytes_equal: true`.

Limits. This is a local `development`-versioned candidate, not a tagged release pair; no `06-web` Full SDK exists for Windows/arm64, so
the Web security snapshot, source-offer-for-WebKit and clean Web source asset checks do not apply to this pair. The existing CI
tooling selection and a full ctest were not re-run for this production (the earlier ctest of this host is 220/220). Build-host inputs outside
the repository are those of the earlier Windows/arm64 records: a native arm64 shell, `CRT_WINDOWS_SDK_LIBPATH` in 8.3 form,
`CRT_CC/CXX/AR/RANLIB`, and Strawberry Perl's emulated x64 `openssl.exe` for the fixture PKI.

Evidence root: `out/s5-windows-arm64-20261027/`, retaining `logs/{full-05-ui,dev-producer,s5-driver}.log` and `replay/` (verifier logs, eight negative
logs, `external/result.json`, the archive and `handoff.json`).

### Handoff and CI boundary

`handoff.json` is producer-owned **local S5 evidence** (`kind:
crt-sdk-s5-evidence`, its own `format: 1`), not the proposed SDK development
contract v1 or a CLI lock format. It explicitly records `promotion:
not-promoted`, `release_qualified: false` and `cli_adoption: not-run`. The
Linux/x86_64 candidate has equal runtime bytes but different Dev producer and
Full/base source commits. The macOS and Windows/x64 records above instead use
matching clean source identities; none is a tagged, release-qualified pair. CLI
may review each identity; adoption still requires producer artifact promotion and the CLI-owned
compatibility/lock gate. CLI/Studio repositories are untouched.

The bounded Linux/amd64 PR step now runs this S5 driver after the ordinary Dev
producer/consumer target. It adds package/negative/handoff checks to the same
host smoke rather than another native matrix or WebKit rebuild. Windows ZIP
comparison permits only the two already documented internal zlib aliases to
become byte-identical files; other path, type or byte differences fail. POSIX
archives retain exact link-aware identity. Hosted CI and native macOS/Windows
execution of the S5 driver were **Not run** at the original Linux checkpoint.
The later native macOS/Windows records above supersede that pending scope,
including Windows/arm64 (Full reference `05-ui`, since `06-web` is not built
there). The later clean-version local collector runs belong to
[release qualification](release_qualification.md); these S5 handoffs keep their
original identities and decisions.

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
consumer, external acceptance driver and S5 package/handoff driver. Initial review baseline: CRT `181f74b`;
later host extensions retain their exact baselines in the host sections above.
Consumer baselines remain CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93` and
Studio `27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`, with no approved runtime
combination in CLI's `docs/compatibility.md`. This adds an acceptance procedure,
not a new SDK/CLI wire contract: SDK manifest/profile format 1, port recipe
version 1 and isolated-stage recipe version 2 remain unchanged. The general
SDK development-contract v1 remains proposed. CLI/Studio live integration is
**Not run**; artifact promotion precedes any later consumer lock update.

## Windows/x64 Owned Spawn Cancellation (2026-11-01)

**Pass, local development candidate; Windows/x64 only.** The CLI's owned-tree
timeout regression failed against clean CRT `fa5dc30`: `kill(-pid, SIGKILL)`
returned `ENOSYS` and a descendant wrote its marker after the timeout. This
increment adds the [owned-spawn subset](../design/job_control.md#owned-spawn-cancellation-2026-11-01)
in the Windows PAL, keeping public Bionic API signatures unchanged. It uses
unnamed Job Objects attached before child execution, a separate locked group
registry that survives leader reaping, and a retained leader handle preventing
PID reuse. Unsupported group joining/catchable delivery still fails explicitly.

Producer base is `fa5dc305efa3cf3c807786550dd727023cad8bd3`, plus the uncommitted
PAL/test delta. Implementation SHA-256:
`0bf953484cbad0152ed545966580905f2fc897db2069191415591d1f11339589`.
The complete three-file manifest and tracked patch are retained as
`producer-source-inputs.json` and `producer-tracked.patch`; the new test's hash
is included separately because it was not yet tracked at build time.
No clean-source or new committed-revision identity is claimed.

The final fresh producer clone is
`C:/Users/Lee/AppData/Local/Temp/crt-timeout-final-20261101/producer`, output
`out/timeout-fix/build`. Configure uses `cmake --fresh --preset
windows-host-ninja-debug -B <build> -DCRT_RELEASE_TAG=development`.
`cmake --build <build> --target crt-development-sdk-test --parallel 8` and
`crt-c-test` both exit 0. `development` explicitly allows dirty local source
assets; no release tag is invented. LLVM is 22.1.8 and Windows SDK imports are
`10.0.28000.0/um/x64`. Only pinned source/download caches are reused, not
compiled libraries. The cache-copy long-path failure and preliminary candidate
remain separate evidence; the latter predates leader-handle hardening.

Core: **167 pass, one skip, no failures** in 168 tests. The skipped
`fork_heap_snapshot_test_runs` remains a skip. The focused
[Windows regression](../../libc/tests/windows_owned_spawn_group_test.c)
also passes independently (12.37 seconds), covering ordinary/native spawn,
live and reaped leaders, descendant termination, sibling survival, 80 failed/
successful-spawn cycles, and unsupported group attributes. The ordinary Dev
consumer passes static/shared HTTPS, inflate and trust negatives.

SDK inventory SHA-256 is `252ff44b22ab222692f84c29522ce4f2f9b971d0f1d111baf9ac31e9b33441de`;
consumer content selection is `7c04cfffe0b2faabe1cc149d0914f5ada6432b8cfae93a594ab1771f5d0b3b1e`;
provider SHA-256 is `b8fa1e9ab171732add99d696d48ed9ded4d1c5c21946238e9dd6cc3e21fca48f`.
Both original and relocated SDK inventories are unchanged after testing.
This is a local immutable handoff, not a promoted SDK release or production pin.

The SDK-only CLI replay passes **788 checks**, including the
original timeout marker/control, under a restricted token and OS job process
accounting. Self-build is cold 10/0 and warm
0/10. Bare help/version/doctor and import audit
pass with SDK/source/provider roots inaccessible. All 32 controlled HTTPS/
signature cases pass with fresh disposable identities and the same visibility
boundary. Consumer details remain owned by CLI's
`docs/acceptance/bootstrap_acceptance.md#windowsx64-owned-descendant-timeout-fix-2026-11-01`.

Raw evidence is in the sibling CLI's ignored
`out/windows-timeout-fix-20261101/final/`, including `sdk-status.json`, `logs/`,
`focused-test.log`, `replay/`, `https/` and aggregate `result.json`.
Windows/arm64, general POSIX group/session membership, catchable cross-process
signals and Toybox timeout acceptance remain open. No applet was enabled,
upstream source patched, production lock adopted, release published or STATUS
updated. Previous failed identities and binaries are preserved.

## macOS/arm64 Latest-source CLI Replay (2026-11-01)

**Pass, local/unpromoted producer gates and bounded native consumer.** Clean
CRT `8db69f404dcfa70b5aa0569cae51a962fd0615d2`, native macOS 27.0.1 arm64,
Apple Clang 21.0.0 (`clang-2100.4.1.1`), ld64 `ld-27106.1`, Xcode ar and
macOS 27.2 vendor SDK. A fresh output at
`out/cli-macos-revalidation-20261101` reuses only pinned LLVM source directories
and hash-verified download archives, with no prior compiled libraries copied.
`producer-inputs.json`, `producer-commands.json`, `tool-identities.json` and
logs are retained in the sibling CLI's ignored
`out/macos-revalidation-20261101/` (original orchestration root:
`/private/tmp/crt-macos-revalidation-20261101`).

```sh
(
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake -S /Users/jocheolmin/GitRepo/crt -B /Users/jocheolmin/GitRepo/crt/out/cli-macos-revalidation-20261101 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -DCMAKE_CXX_COMPILER=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ -DCRT_TARGET_OS=macos -DCRT_TARGET_ARCH=host -DCRT_USE_IMPORTED_LIBCXX=ON -DCRTGFX_ENABLE_SKIA=OFF -DCRTMEDIA_ENABLE_FFMPEG=OFF -DCRTMEDIA_ENABLE_CURL=OFF -DCRTUI_ENABLE_LVGL=OFF -DCRT_RELEASE_TAG=v0.4.0-cli.20261101
cmake --build /Users/jocheolmin/GitRepo/crt/out/cli-macos-revalidation-20261101 --target crt-development-sdk-test --parallel 6
cmake --build /Users/jocheolmin/GitRepo/crt/out/cli-macos-revalidation-20261101 --target crt-c-test --parallel 6
cmake --build /Users/jocheolmin/GitRepo/crt/out/cli-macos-revalidation-20261101 --target crt-cxx-test --parallel 6
)
```

The producer job explicitly sets `SDKROOT` from `xcrun --sdk macosx
--show-sdk-path` when invoking actual Xcode compiler paths. Without it the
first Dev candidate's zlib link could not find libSystem; that failed candidate
and log are retained, with no configure-capability override or upstream patch.
An earlier external-output attempt exposed the port preset's source-relative
`out/build` mapping; the fresh output above uses the existing supported layout.
These are producer/test-host prerequisites, not a CLI runtime shell dependency.

- Core **146/146**, no failures or skips. Imported libc++ static/shared
  exception/allocation, string-ABI and iostream smokes pass.
- Development profile consumer passes static/shared verified HTTPS+inflate
  plus wrong/missing/malformed CA cases; Mach-O binding/closure audits pass.
- Exporter/profile fixtures: **36 pass, two native-Windows-only skips**.
- Profile version `v0.4.0-cli.20261101` is a local parameter, not a release tag.
  Producer/base/compile provenance all name clean `8db69f4`; capability
  `crt-narrow-encoding-1` is present. Installed root/runtime Apache license bytes
  and both generated stage-source archives are checked against `LICENSE.md`.
  No Dev SDK archive, promotion or publication is performed.

The complete public SDK is handed off with symlinks outside the producer tree:
`/private/var/folders/p3/z_1_279n4bq6kgrrs25d8bdc0000gn/T/crt-cli-contract-6cjvlogl/SDK space 한글/dev`.
Its 2,587-entry inventory SHA-256 is
`7e187c845a9bdc65166f697f08fe13642f8f0dd98c3e6e159c26102febab30c4`; provider content selection is
`a438a4a08bc0bec98d53ad95d3cbc196525d655428086088508a3df9ea0f2f16` and native-policy digest is
`489ebea23e8531cfdd57cd3ba526a163f18c42616e9a856961af49ace673a21d`. Original and relocated inventories remain unchanged
after all consumer and host-tool probes.

Unchanged CLI product sources at `a9d43c6` pass 732 native checks under
Seatbelt, cold/warm self-build 10/0 and 0/10, the previous macOS binary's
bootstrap loop, bare SDK-hidden help/version/doctor and single-import audit.
All 32 SDK-hidden transport/signature cases and pinned-repository HTTPS pass.
The consumer and exact executable identities remain owned by
[CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#macos-latest-source-revalidation-2026-11-01).

SDK-derived mksh/sh, enumerated toybox applets, separate awk and make execute
with only declared CRT tools available. Direct make SHELL containing spaces
fails; an explicit temporary real symlink to the same mksh passes. The failed
result is retained, without claiming an independent Host Tools Bundle or CLI
port-build adapter. No runtime implementation change, upstream patch, full
WebKit build, STATUS update or release action was needed. The documentation
commit records this measured snapshot without rebuilding or relabeling the SDK.
Other-host latest-source gates and complete D1 remain separately scoped.

## Linux/aarch64 Latest-source CLI Replay (2026-11-02)

**Pass, local/unpromoted producer and bounded CLI gates.** Native Linux/aarch64
`u2404`, kernel `7.0.0-42-generic`, LLVM Clang/LLD/llvm-ar 21.1.8. Fresh clean
CRT `c4e19f3e47f6fd37f3d0d03c027bec5cfe251677` is built in
`out/cli-linux-aarch64-20261102`; unchanged CLI product sources are
`9e0110ff2bc9f7030ab66c726bac2286e5526cc2`. No runtime or consumer implementation
change was required. Existing immutable contract/design baselines are retained.

```sh
cmake -S /home/cmjo/GitLocal/crt -B /home/cmjo/GitLocal/crt/out/cli-linux-aarch64-20261102 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCRT_TARGET_OS=linux -DCRT_TARGET_ARCH=host -DCRT_USE_IMPORTED_LIBCXX=ON -DCRTGFX_ENABLE_SKIA=OFF -DCRTMEDIA_ENABLE_FFMPEG=OFF -DCRTMEDIA_ENABLE_CURL=OFF -DCRTUI_ENABLE_LVGL=OFF -DCRT_RELEASE_TAG=v0.4.0-cli.20261102 -DCRT_LIBCXX_FETCH_ROOT=/home/cmjo/GitLocal/crt-cli/out/linux-arm64-revalidation-20261102/llvm-sources -DCRT_DEV_WORK_ROOT=/home/cmjo/GitLocal/crt-cli/out/linux-arm64-revalidation-20261102/dev-work
cmake --build /home/cmjo/GitLocal/crt/out/cli-linux-aarch64-20261102 --target crt-development-sdk-test --parallel 4
ctest --test-dir /home/cmjo/GitLocal/crt/out/cli-linux-aarch64-20261102 --output-on-failure -E '^(crtgfx_|crtmedia_|crtui_|crtweb_|.*cxx)' -j 3
cmake --build /home/cmjo/GitLocal/crt/out/cli-linux-aarch64-20261102 --target crt-cxx-test --parallel 4
```

Only pinned LLVM source directories and cached download archives were reused;
no previously compiled libraries were copied. Download consumers verify recipe
hashes. Profile producer/base/compile provenance all identify clean `c4e19f3`,
local version `v0.4.0-cli.20261102`, and `crt-narrow-encoding-1`. This version
parameter is not a Git tag, release or promoted candidate.

- Fresh core CTest **147/147**, no skips, including aarch64 open/no-follow and
  owned process cleanup regressions. No stale upper-runtime tests are included.
- Imported libc++ static/shared allocation/exception, string ABI and iostream
  smokes pass through `crt-cxx-test`.
- `crt-development-sdk-test` passes profile verification and static/shared
  HTTPS+inflate, plus wrong/missing/malformed CA cases.
- Exporter/profile fixtures: **36 pass, two Windows-only skips**. Fixture
  outcomes are distinct from the actual native consumer replay.

The full public SDK is copied with real symlinks outside this producer checkout
to the sibling CLI's ignored output:
`out/linux-arm64-revalidation-20261102/work/crt-cli-contract-wqk8w3hx/SDK space 한글/dev`.
The 2,597-entry inventory SHA-256 is
`ae32504f7b1e7439275f867d4e7ee199a31bafa7767f20d20584fe3a2ca0b560`;
provider content is `a1d2028cd734743ebb76190066f7225759a74a7bcf783197574095670e317874`
and native policy digest is
`d256ccab539f4b9b65d1057e29a326119c499a58d4053b07e9fb5b11f339c306`.
No Development-profile archive was cut. Original and relocated SDK inventories
are unchanged after consumer verification.

CLI passes **766 native checks**, cold/warm self-build 11/0 and 0/11,
producer/source/interpreter-hidden Bubblewrap and execve audits, bare ELF/import
and mapped-image audits, SDK-free control/redaction and **32/32** disposable-key
HTTPS/integrity/signature cases. The existing aarch64 CLI also builds current
sources against the clean SDK; the new CLI rebuild reaches a byte-identical
fixed point. Consumer records and binary identities belong to
[CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#linuxaarch64-latest-source-revalidation-2026-11-02).

Raw logs, original input/tool hashes, CMake cache, producer consumer receipt,
fixture logs and replay scripts are retained in the sibling CLI's ignored
`out/linux-arm64-revalidation-20261102/`. The source/build inputs were clean
through SDK production; later evidence edits do not relabel compiled artifacts.
No source patch, full media/WebKit/device replay, SDK promotion, production
lock adoption, public release or STATUS update is implied. This closes the
Linux/aarch64 latest-source gap within Tranche 1; Windows/arm64 process-fix
replay and coherent exact asset selection remain separate open gates.

## Windows/arm64 Latest-source CLI Replay and Spawn-environment Fix (2026-11-02)

**Pass, local/unpromoted producer and bounded CLI gates; Windows/arm64.** Native Windows 11
Arm64, LLVM 22.1.8, Windows SDK imports `10.0.28000.0/um/arm64`, Developer Mode on (not changed).
This closes the "Windows/arm64 process-fix replay" gap of Tranche 1.5 with one producer fix.
Fresh clean worktrees under a short profile path; only pinned source/download caches are fetched,
no compiled libraries are copied; no tag, archive, release or promoted SDK is created.

### Baseline Producer Gate and the Failure It Exposed

Clean CRT [`ee0b5e3`](https://github.com/webos21/crt/commit/ee0b5e35ba91e8acd1a96ab9f2136243ac1b9adf)
(contains the owned-spawn fix `8db69f4`), local version `v0.4.0-cli.20261102`:
`crt-development-sdk-test` (profile verification, static/shared HTTPS and inflate, wrong/missing/malformed CA) and
`crt-c-test` pass, **167 of 168 with `fork_heap_snapshot_test_runs` skipped**; `windows_owned_spawn_group_test`
(16.4 s), `windows_utf8_narrow_test` and `platform_test` pass. The Python fixtures pass
(`create_dist` 15 with 6 conditional skips, `verify_dist` 16, exporter 6, Development profile 32, stage-source closure 10).

The CLI replay against that SDK **fails** in the native suite at `spawn environment exactly matches shared builder`,
a CLI check added after the last Windows replay. Two causes, separated by minimal reproducers:

1. **The OS adds `PROCESSOR_ARCHITECTURE=ARM64`** to any new ARM64 process whose environment lacks it. A
   kernel32-only program that includes no CRT (`probes/envdump.c`) given `LC_ALL`, `PATH` and `SystemRoot` receives it
   too, as does a native ARM64 Python. Neither CRT nor the CLI adds it, so the CLI's test now tolerates that one name,
   with the value for the host architecture, and nothing else. Whether native Windows/x64 does the same was not measured.
2. **A CRT child saw the PAL's bootstrap transport variables** (`CRT_CHILD_BOOTSTRAP`, `CRT_BOOTSTRAP_CWD`,
   `CRT_BOOTSTRAP_SIGMASK`, `CRT_BOOTSTRAP_SIGDEFAULT`, `CRT_FD_SNAPSHOT_PIPE`, and `CRT_BOOTSTRAP_ROOTFS` when the parent has a
   rootfs) in its `environ`. `__crt_child_bootstrap()` consumed them and left them in the child's table. Any program started
   through `posix_spawn` from a CRT parent could read them, and a stale snapshot pipe handle value outlived its pipe.
   A grandchild never inherited them (the PAL filters the parent's `envp` and adds fresh ones), so this was an observable
   leak, not a functional fault. `probes/prefix-runtime-test.txt` is the pre-fix reproducer's output (`leaked: CRT_CHILD_BOOTSTRAP=1`, exit 51).

The first two replay attempts (the second with the `PROCESSOR_ARCHITECTURE` tolerance) both stop at this check and are retained
(`replay/`, `replay2/`). A scratch copy of the suite that also tolerated the `CRT_*` names, run on the unfixed runtime,
passes **845** checks, so nothing else was hiding behind the first failure.

### Fix and Final Producer Gate

[`d12e210`](https://github.com/webos21/crt/commit/d12e21013c1da6eed0d9238100df93c8febdba1c) removes the seven transport
variables from the process's own environment table after `__crt_child_bootstrap()` has applied them
(`libc/src/arch/windows/common/syscall.c`); `CRT_ROOTFS`, derived from `CRT_BOOTSTRAP_ROOTFS`, stays. Only the table
changes: the operating-system block that a fork-capable relaunch inherits is untouched and the relaunched image runs the same
function, so `fork` still passes. The regression `libc/tests/windows_spawn_environment_test.c` checks that the child's
environment holds the parent's variables and none of the transport names, and that the parent's working directory still
arrives; it **fails against the pre-fix runtime** and passes after the fix. The Windows PAL only; Linux and macOS are
unaffected and not replayed.

Fresh clean `d12e210` (producer, base and compile provenance all clean): `crt-development-sdk-test` passes; `crt-c-test`
passes **168 of 169, one skip** (`fork_heap_snapshot_test_runs`), including `windows_spawn_environment_test`,
`windows_owned_spawn_group_test` (18.1 s), `windows_utf8_narrow_test`, `fork_test`, `fork_signal_test`,
`fork_runtime_reset_test` and `malloc_fork_regions_test`; total test time 122 s. The same Python fixtures pass.

### Consumer Replay

Driver `tools/replay_cli_contract_windows.py` with `PYTHONUTF8=1`, a Hangul/accented/space work path and
`--hidden <original CRT>` against the relocated SDK, CLI base `f36e858972c3342292a6bb09dc5fc18cb55eb8ef` plus one test
edit (the tolerance above; source digest `1f4729d9c02064fa6811c28e8f8fe5667763f3ebd8c0139b2783783c03e9c43c`):

| Identity | SHA-256 |
| --- | --- |
| SDK inventory (relocated; unchanged after the run) | `8a44f8a72511915a7da49b9a98ba085ffe20cc15c02aaa67c9e97696b2c6950b` |
| SDK content selection | `f86084868afe3e1e349f9407975cb4ebc20fdeaa49826fc0e5e7cdcd63b5ca71` |
| Provider | `86786a22aa555b67bb6e71b6c5041df683daeca2154dd615fbf50fd842c8becb` |
| Final executable (20,286,976 bytes) | `6fce97e028c9956716ad1729d8655532282944f8dfd7f9ec33f44d92cf0e787c` |
| `replay3/result.json` | `e911ffcdc8083564d1c8718b8333066bae714b374ea583aa505d5d5f0a6727db` |

Every step exits 0: provider and boundary probe, ten seed compiles and link, configure, build, cold self-build
**11 executed / 0 reused**, warm **0 / 11**, and **845 native checks**, including the original owned-descendant timeout control
and marker assertion, the shared child-environment and `env` diagnostics, publisher-signature argument cases and the Unicode
workflows. The job-based accounting records 404 declared native/OS processes (compiler, linker, archiver, the seed and CLI, generated
programs, the probe, `conhost.exe`) and no forbidden image; it is accounting, not launch denial. Bare help/version/doctor and the PE audit pass with
SDK, source and provider unreadable: imports are only `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll`, doctor reports
`narrow_encoding: utf-8` with C++, zlib and HTTP/TLS checks passing, integrity `not_verified`, network `not_run`.

**HTTPS and signatures.** `tools/test_cli_fetch_https_windows.py` (new) runs the unchanged cases of `test_cli_fetch_https.py
--publisher-cases` with every CLI call under a restricted token naming a per-run SID, an empty `PATH` and a job object, in a
private Hangul/accented/space work directory. A native probe built with the SDK proves under the same SID that eight roots
(this checkout, the CLI checkout, both producer worktrees, the SDK build, the exported source, the relocated SDK and the provider)
are unreadable. **All 32 cases pass** with fresh disposable CA, server and RSA publisher identities; 32 CLI calls, 64 process
records (the CLI and `conhost.exe`), no Python, OpenSSL or shell started by product fetch. `test_cli_fetch_https.py` now passes `-extfile`
relative to the PKI directory, because the OpenSSL used here cannot open a non-ASCII absolute path. `https-final/cases/result.json`
SHA-256 `08b7fb946440e828b79964cbf85b028edfef41b24cf29ded23bae7af10509efa`; CA certificate SHA-256
`261bc7c481ece920cdd0e8e21a38f3e1aafd797d9a9011fb1fdb96fbe0ab15bc`.

**CRT host tools.** From the relocated Unicode SDK path with an empty `PATH`, `mksh -c 'echo'`, `toybox echo`, `awk 'BEGIN{…}'` and
`make --version` each start and exit 0; all four are ARM64 PE images. `make` reports "Built for x86_64-pc-linux-gnu", the triple
text its recipe passed to configure; the image is ARM64. These are startup checks, not configure/make port acceptance.

### Evidence and Limits

Raw evidence is in the sibling checkout's ignored `out/cli-windows-arm64-20261102/`: baseline and fixed build logs, `replay/`,
`replay2/` (failures), `replay3/` (pass), `https-final/`, `probes/`, fixture and host-tool results. The earlier 2026-10-30/31 evidence
directories are separate and unchanged. Consumer details are in the CLI's
[acceptance record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#windowsarm64-latest-source-revalidation-and-spawn-environment-fix-2026-11-02).

Windows/arm64 only. Native Windows/x64 later replayed this fix: see the [Windows/x64 section](#windowsx64-latest-source-cli-replay-2026-11-02). Not run: Linux and macOS, ports in Unicode directories, Windows before 10 version 1903, a promoted
SDK or release, official publisher keys, host-tools contract. No production lock, tag, release or STATUS change.

## Windows/x64 Latest-source CLI Replay (2026-11-02)

**Pass, local/unpromoted producer and bounded CLI gates; Windows/x64.** Native Windows 11 x64 (10.0.26100), LLVM 22.1.8, Windows SDK imports `10.0.28000.0/um/x64`.
Fresh clean clone of CRT [`b4c5b5a`](https://github.com/webos21/crt/commit/b4c5b5a63196c48ea21a35bad8e73423488dd046) (contains `d12e210`), preset `windows-host-ninja-debug`,
local version `v0.4.0-cli.20261102` (not a tag). Only pinned LLVM-runtime sources and download archives were reused; no compiled libraries. Producer, base and compile provenance are clean.

`crt-development-sdk-test` exits 0 and `crt-c-test` passes 168 of 169 with `fork_heap_snapshot_test_runs` skipped (280 s), including `windows_spawn_environment_test`.
The unchanged CLI driver replays CLI `1499a7b` against the relocated SDK (inventory `c3def6cc25f3d4cf0c6522f0292a04fba55014e8fc1a8b3f2c9ebb9ff33d26c3`, content
`0361a7494e1c98f0a81ff58eae2280a6f9e328ae19671cbd91204ee82b364380`, provider `ce07aac6f3b68d79de1c0b52bb92f8f11ef862f530c53f39d0012098722e25c2`): cold self-build 11/0, warm 0/11,
**844 native checks**, 460 recorded processes with no forbidden image, bare help/version/doctor with PE imports `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll` only.
`tools/test_cli_fetch_https_windows.py --publisher-cases` passes all 32 cases (64 process records). CRT `mksh`, `toybox` (`system/bin`), `awk` and `make` start from the Unicode SDK path
as x86-64 PE images. A native x64 Python given only `LC_ALL`, `PATH` and `SystemRoot` received no `PROCESSOR_ARCHITECTURE`, unlike Windows/arm64, so the CLI test's tolerance is unused here;
a CRT-free kernel32 probe was not run.

Consumer details and the executable hash: [CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#windowsx64-latest-source-revalidation-and-spawn-environment-fix-2026-11-02). Raw evidence is in the CLI checkout's ignored `out/windows-x64-20261102/`.
Not run: bootstrap loop for the refreshed binary, ports in Unicode directories, Windows before 10 version 1903, a promoted SDK or release, official publisher keys. No tag, release or STATUS change.

## Windows/x64 Tranche 3.1 and D2 Installer Replay (2026-11-04)

**Pass, local/unpromoted producer gate and consumer replay; Windows/x64.** Native Windows 11 x64 (10.0.26100), LLVM 22.1.8, Windows SDK imports
`10.0.28000.0/um/x64`, Developer Mode on (not changed). Fresh clone of CRT `a48000c` plus the producer fixes below, preset `windows-host-ninja-debug`, build
directory under the checkout's `out/` (the documented `CRT_BUILD_PRESET_NAME` limitation), `CRT_RELEASE_TAG=v0.0.1-rehearsal.3` (not a tag).

### What the first Windows build exposed

1. **xz runtime artifact (`f9dc911`).** `tools/build_dev_sdk.py` matched `lib/lzma.dll` for the xz runtime artifacts, but the Windows build installs
   `bin/liblzma-5.dll` (the import libraries `lib/lzma.lib`, `lib/lzma.dll.lib`, `lib/liblzma.dll.a` and the static `lib/liblzma.a` are in `lib/`). The manifest therefore listed
   only `lib/lzma.dll.lib`, and `verify_dist` failed with "Development SDK missing xz shared runtime: liblzma". The pattern is now `bin/liblzma-*.dll`;
   Linux and macOS patterns are unchanged.
2. **Windows shared consumer (`1ee0379`).** `libstdc++/tests/development_sdk_consumer.cc` has decoded xz since 3.1, but the Windows shared lane in
   `tools/test_dev_sdk_consumer.py` listed no liblzma import library (undefined `lzma_*`). It now links `liblzma.dll.a`.
3. **Install-metadata inventory (`f9dc911`).** For an escaping symlink target Windows' `Path.resolve(strict=True)` raises `OSError` (invalid name) instead of
   `ValueError`; `tools/test_sdk_install_metadata.py` failed there before this work. The inventory now converts it to `ValueError`.

A first producer build that reused the LLVM runtime source trees of an older build was rejected by CRT ("noncanonical source cache"); the sources were fetched again.
Immutable stage source candidates must be removed from the build directory (`stage-sources`, `stage-cache`) when the producer commit changes between attempts.

### Final Producer Gate and Release

Clean `1ee0379`: `crt-development-sdk-test` exit 0 (xz round trip static and shared, mbedTLS, `verify_dist`, static/shared consumer), `crt-c-test` 168 of 169 pass with
`fork_heap_snapshot_test_runs` skipped (317 s), `crt-dev-release` exit 0. Asset `crt-v0.0.1-rehearsal.3-windows-x86_64-dev.zip` (146,453,507 bytes, SHA-256
`fc3eeb74...810b`); manifest SHA-256 `fe86e1d1...5886`; install metadata: `zip`, root `dev`, 2,917 entries (2,736 files, 181 directories, 0 symlinks), 402,317,041
bytes, identity `075d5eb3...2c4d`. `liblzma` ships static and shared; the SDK-only CLI links `-llzma` through the `lzma.lib` alias.

### Consumer

The CLI's contract replay (driver unchanged) self-builds from the SDK alone (16 sources, cold 16/0, warm 0/16) with 857 native, 151 archive and 66 SDK-store checks; the
installer replay `tools/replay_sdk_install_windows.py` (restricted token, job-object accounting) passes **62/62**. Details, Windows-specific hostile names, the two CLI fixes and
limits (no network denial, no mode bits) are in the CLI's [D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsx64-isolated-replay-2026-11-04).

### Evidence and Limits

Raw evidence is in the CLI checkout's ignored `out/windows-d2-20261104/` (producer build logs, replays, the failed attempts). Windows/x64 only; Windows/arm64 and Linux/aarch64
installer replays, Full-stage install metadata, a published release asset and promotion are not run. No tag, release, production lock or STATUS change.

## Linux/aarch64 Tranche 3.1 and D2 Installer Replay (2026-11-04)

**Pass, local/unpromoted producer gate and consumer replay; Linux/aarch64.** Native Ubuntu 26.04 aarch64 QEMU guest (Linux 7.0.0-42, 4 CPU), Clang/LLD/llvm-ar 21.1.8.
Detached worktree of clean CRT `ffd0405` (it contains the 3.1 producer work, the Windows fixes `f9dc911`/`1ee0379` and the aarch64 `faccessat` access-mode fix `a48000c`),
preset `linux-host-ninja-debug`, build directory under the worktree's `out/`, `CRT_RELEASE_TAG=v0.0.1-rehearsal.4` (not a tag, nothing uploaded).

### Producer Gate and Release

`crt-development-sdk-test` exit 0 (xz round trip static and shared, mbedTLS, `verify_dist`, static/shared consumer); `crt-c-test` **147/147**, no skips; `crt-dev-release` exit 0.
`SHA256SUMS-dev-linux-aarch64` verifies all three assets. Asset `crt-v0.0.1-rehearsal.4-linux-aarch64-dev.tar.xz` (9,065,788 bytes, SHA-256
`4058bcaea679311c73505f1efe4c7e4ba24388b8dc56d7a926f3c0a2115eb9dd`); manifest `release-manifest-dev-linux-aarch64.json` SHA-256
`0cd62bf7939cf15147f79835530f7ad1a9da3d5e097166f09aabf034cbc59cbe`; install metadata format 1: `tar.xz`, root `dev`, 2,797 entries (2,602 files, 178 directories,
17 symlinks), 206,258,306 bytes, identity `0d11df55b226b50e09dfd7bb59c0bea0e94ad9ff2f4faed428563f8b998c8c48`. Producer, base and compile provenance all name `ffd0405`
with `working_tree_dirty: false`. No producer change was needed on this host.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_linux.py`, unchanged) self-builds from the SDK alone (16 sources, cold 16/0, warm 0/16) with 779 native, 158 archive and
66 SDK-store checks, 24 distinct executed paths under the strace audit (none forbidden), SDK-hidden help/version/doctor, no `DT_NEEDED`, and an unchanged SDK inventory. The installer
replay `tools/replay_sdk_install_linux.py` (bubblewrap, no network, read-only root) passes **48/48**, including the 17 symlinks of the SDK. Details are in the CLI's
[D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#linuxaarch64-isolated-replay-2026-11-04).

### Evidence and Limits

Raw evidence is in the CLI checkout's ignored `out/linux-arm64-d2-20261104/` and the producer worktree's `out/d2-linux-aarch64-20261104/` (not committed). Linux/aarch64 only; Windows/arm64
installer replay, Full-stage install metadata, a published release asset and promotion are not run. No tag, release, production lock or STATUS change.


## Windows/arm64 Tranche 3.1 and D2 Installer Replay (2026-11-04)

**Pass, local/unpromoted producer gate and consumer replay; Windows/arm64.** Native Windows 11 arm64 VM (10.0.26300, 4 vCPUs, no GPU), LLVM 22.1.8, Windows SDK imports
`10.0.28000.0/um/arm64`, Developer Mode on (not changed), native arm64 shell and Python (an x64 Git Bash would configure the x86_64 sources). Detached worktree of clean CRT
`b20a460` at a short ASCII path, preset `windows-host-ninja-debug`, `CRT_RELEASE_TAG=v0.0.1-rehearsal.6` (not a tag, nothing uploaded).

### What the replay found

**One producer defect, in the compile policy (`b20a460`).** The first producer (clean `e3deb32`, `rehearsal.5`) passed `crt-development-sdk-test` (2,305 s), `crt-c-test` (169 of 169, one
skip) and `crt-dev-release`. The CLI's contract replay then self-built the CLI and passed 858 native checks, but `archive-test` and `sdk-store-test` aborted with exit 134 and
`libc++abi: terminating due to uncaught exception of type crtcli::Error` where the tests expect to catch it. lldb showed the unwinder leaving `extract_archive` for a bogus caller:
the function allocates about 149 KB through `bl __chkstk; sub sp, sp, x15, lsl #4`, and its eh_frame CFI still said `CFA=sp+96` afterwards, so the return address was read from the
wrong stack slot. clang turns asynchronous unwind tables on by default for aarch64-w64-mingw32 once `-fhosted` is given, which the CLI's native driver needs for `main`; LLVM 22.1.8 then
emits no CFA adjustment for the stack-probed frame. With the same argv plus `-fno-asynchronous-unwind-tables` it does, with `-fno-omit-frame-pointer` it uses `w29`, and x86_64
emits `.cfi_def_cfa_offset 150048` for the same 150 KB reproducer (Linux/aarch64 does not use `__chkstk`). The `-ffreestanding` wrapper builds of CRT (libc++, ports, tests) do not enable them, which is why
CRT's own suites passed. `tools/crt-cc` and `tools/crt-c++` now pass `-fno-asynchronous-unwind-tables` for Windows/aarch64, so the policy the exporter records from the
wrappers carries it to the CLI; `tools/test_wrapper_unwind_tables.py` (in the CI-selected tooling list) pins that the flag is present there and absent for Windows/x86_64 and Linux. The
condition is a compiler defect: remove the flag when the toolchain pin moves to a fixed LLVM.

A first attempt to re-run `crt-dev-release` after the fix was refused because the earlier release output existed (`release already exists; choose a fresh output root`); that output was
moved aside, not replaced. The libc++ and port sources of the first producer were removed (`stage-sources`, `stage-cache`) because the producer commit changed, as in the Windows/x64 record.

### Producer Gate and Release

Clean `b20a460`: `crt-development-sdk-test` exit 0 (1,669 s: libc++ and the Development ports rebuilt with the wrapper change), `crt-c-test` 169 of 169 with `fork_heap_snapshot_test_runs`
skipped (91 s), `crt-dev-release` exit 0 (75 s). Asset `crt-v0.0.1-rehearsal.6-windows-aarch64-dev.zip` (141,785,359 bytes, SHA-256
`7b63ccdde3f4e53602a7608d3c833181e0befcc42906a25d28f931f4d0c17bec`); manifest `release-manifest-dev-windows-aarch64.json` SHA-256 `488c2ac6e90730fc4dfb9ec76fd31e6c28abd42dc10997a1ac4963de3a725f95`;
install metadata format 1: `zip`, root `dev`, 2,917 entries (2,736 files, 181 directories, 0 symlinks), 398,216,734 bytes, identity
`1239f18c392cc53630f2fb3d8d3116b663ac4388201d2133764178eeec043b37`. Producer, base and compile provenance all name `b20a460` with `working_tree_dirty: false`.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_windows.py`, unchanged) self-builds from the SDK alone (16 sources, cold 16/0, warm 0/16) with 870 native, 151 archive and 66 SDK-store
checks (all three test executables exit 0), 471 recorded processes (only the declared native tools, the CLI and its generated executables), SDK-hidden help/version/doctor and unchanged SDK
inventories; the bare CLI is 29,425,664 bytes, PE machine `0xAA64`, importing only `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll`. The installer replay
`tools/replay_sdk_install_windows.py` (restricted token, job-object accounting) passes **62/62**, including fifteen hostile archives. Details are in the CLI's
[D2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsarm64-isolated-replay-2026-11-04).

### Evidence and Limits

Raw evidence is in the CLI checkout's ignored `out/windows-arm64-d2-20261104/` (producer logs for both attempts, the failed first contract replay, the final replays) and the producer worktree's
`out/` (not committed). Windows/arm64 only; no network denial (Windows has none for this driver), Full-stage install metadata, a published release asset and promotion are not run. No tag, release,
production lock or STATUS change.

## Linux/aarch64 Portable Toolchain Contract Release Replay (2026-11-05)

**Pass, local/unpromoted producer gate and consumer replay; Linux/aarch64.** Native Ubuntu 26.04 aarch64 QEMU guest (Linux 7.0.0-42, 4 CPU), Clang/LLD/llvm-ar 21.1.8. Detached worktree of
**committed** CRT `78ed80f` (the portable-contract export `01a208d` and the release-fixture stand-in), preset `linux-host-ninja-debug`, `CRT_RELEASE_TAG=v0.0.1-rehearsal.5` (not a tag, nothing uploaded).

### Producer Gate and Release

`crt-development-sdk-test` exit 0; `crt-c-test` **147/147**, no skips; `crt-dev-release` exit 0. `SHA256SUMS-dev-linux-aarch64` verifies four assets: the archive
`crt-v0.0.1-rehearsal.5-linux-aarch64-dev.tar.xz` (9,059,308 bytes, SHA-256 `89f6437e92acf2eae502f765831277956f06ba8f04dfd659f42cc80c923baa70`), the **portable toolchain contract**
`crt-v0.0.1-rehearsal.5-linux-aarch64-dev-toolchain.json` (403,527 bytes, SHA-256 `8dbb76b961ba783b029e373829ddc38afb169186ddd5b3721bc2e68b2fece9df`) and the 02/03 stage-source assets. Manifest
`release-manifest-dev-linux-aarch64.json` SHA-256 `cf246f85f4a8f918b20b01acb0a3d3f3eab8f318db69b17623eb897b53457ca5`; the archive entry names its contract and carries install metadata format 1: `tar.xz`,
root `dev`, 2,797 entries (2,602 files, 178 directories, 17 symlinks), 206,260,348 bytes, identity `9f8f07ac0cb19084b26868bf414ae5611be217ec1af0fac5eb433c0a5f448191`. Source commit in the manifest:
`78ed80f`. No producer change was needed on this host; the Linux contract has no `os_sdk` entry (no OS SDK is needed), as on x86_64.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_linux.py`, unchanged) self-builds the latest CLI sources (`9d711d2`) from the SDK alone (16 sources, cold 16/0, warm 0/16) with 841 native, 158 archive and
83 SDK-store checks, no `DT_NEEDED`, 24 distinct executed paths (none forbidden) and an unchanged SDK inventory. The CLI's D2 replay (`tools/replay_sdk_install_linux.py`, bubblewrap) passes **69/69** on its first
run: the 48 installer cases plus the 21 contract and project-selection cases (contract stored beside the SDK, `configure` with no `--sdk-contract`, lock without machine paths, build/test/run, damaged or
missing contract, release without or with a damaged contract asset refused). Details: the CLI's
[Stage 2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#toolchain-contract-and-project-sdk-selection-replay-stage-2-linuxaarch64-2026-11-05).

### Evidence and Limits

Raw evidence is in the CLI checkout's ignored `out/linux-arm64-s2-20261105/` and the producer worktree's `out/s2-linux-aarch64-20261105/` (not committed). Linux/aarch64 only; the macOS and Windows contract
collection (`@OS_SDK@` discovery), Full-stage install metadata, a published release asset and promotion are not run. No tag, release, production lock or STATUS change.

## macOS/arm64 Portable Toolchain Contract Release Replay (2026-11-05)

**Pass, local/unpromoted producer gate and consumer replay; macOS/arm64.** Native arm64, macOS 27.2. Detached worktree of **committed** CRT `784389b`
(the portable-contract export `01a208d` and the release-fixture stand-in), preset `macos-host-ninja-debug`, `CRT_RELEASE_TAG=v0.0.1-rehearsal.6` (not a tag,
nothing uploaded), built from a fresh output root with no source cache.

### Producer Gate and Release

`crt-development-sdk-test` exit 0, `crt-c-test` **146/146** (Linux counts 147; one test is host-conditional) and `crt-dev-release` exit 0.
`SHA256SUMS-dev-macos-aarch64` verifies four assets: the archive `crt-v0.0.1-rehearsal.6-macos-aarch64-dev.tar.xz` (6,675,764 bytes, SHA-256
`bf0d92a1948e8eb44b674bdf0cd6cf1c120fe2d2ede0735b231ad5712c26c1a9`), the **portable toolchain contract** `crt-v0.0.1-rehearsal.6-macos-aarch64-dev-toolchain.json` (402,864 bytes, SHA-256
`200d9a105283deaa6c23b31e451555208a930de23aaa4990efc246d5f070faec`) and the 02/03 stage-source assets. Manifest `release-manifest-dev-macos-aarch64.json` SHA-256
`c08c25e8abd3ba71d90db89690db40f5ccec8001402675f9d82a9991213d156b`; the archive entry names its contract and carries install metadata format 1: `tar.xz`, root `dev`, 2,786 entries,
identity `72d1b37593abb2f5dd9d58a8338f44191b0b2023d6cbf2621cf7cd688ce80b4c`. Source commit in the manifest: `784389b`. Unlike Linux, the macOS contract declares
`os_sdk.kind` = `macos-xcode-sdk`: the collector recorded that an Xcode OS SDK is needed, not where it is. No producer change was needed on this host.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_macos.py`) self-builds the latest CLI sources (`da3f3a9`) from the SDK alone (16 sources, cold 16/0, warm 0/16) with 869 native, 158 archive
and 83 SDK-store checks, an import audit that finds only `libSystem`, a bootstrap loop from the previous committed binary and an unchanged SDK inventory. The CLI's D2 replay
(`tools/replay_sdk_install_macos.py`, Seatbelt) passes **69/69**: the 48 installer cases plus the 21 contract and project-selection cases, with `xcrun` discovery of the OS SDK inside the
sandbox. Details: the CLI's
[Stage 2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#toolchain-contract-and-project-sdk-selection-replay-stage-2-macosarm64-2026-11-05).

### Evidence and Limits

Raw evidence is in the producer worktree's ignored `out/s2-macos-20261105/` and the CRT checkout's `out/s2-macos/` (not committed); a first driver invocation failed on a hidden working
directory and is retained beside the passing one. macOS/arm64 only; the Windows contract collection (`@OS_SDK@` discovery), Full-stage install metadata, a published release asset and promotion
are not run. No tag, release, production lock or STATUS change.


## Windows/arm64 Portable Toolchain Contract Release Replay (2026-11-05)

**Pass, local/unpromoted producer gate and consumer replay; Windows/arm64.** Native Windows 11 arm64 VM (10.0.26300, 4 vCPUs, no GPU), LLVM 22.1.8, Windows SDK imports `10.0.28000.0/um/arm64`, Developer Mode
on (not changed), native arm64 shell and Python. Detached worktree of **committed** CRT `0631131` (the portable-contract export `01a208d` and the release-fixture stand-in) at a short ASCII path, preset
`windows-host-ninja-debug`, `CRT_RELEASE_TAG=v0.0.1-rehearsal.7` (not a tag, nothing uploaded), built from a fresh output root.

### Producer Gate and Release

`crt-development-sdk-test` exit 0 (2,339 s), `crt-c-test` **169/169** with `fork_heap_snapshot_test_runs` skipped (100 s) and `crt-dev-release` exit 0 (78 s). `SHA256SUMS-dev-windows-aarch64` lists the archive
`crt-v0.0.1-rehearsal.7-windows-aarch64-dev.zip` (141,785,345 bytes, SHA-256 `976150ba98d3e5d6dc609a6218a5d4e4ad9ab0f4746e1447694765fdbd316294`), the **portable toolchain contract**
`crt-v0.0.1-rehearsal.7-windows-aarch64-dev-toolchain.json` (441,104 bytes, SHA-256 `08ae2c51559704ea6791dab7e4555448d9fbeeef29f7f92bdd66bfa8a6301d68`) and the 02/03 stage-source assets. Manifest
`release-manifest-dev-windows-aarch64.json` (1,701 bytes) SHA-256 `167f326094539ec92cd86a7aed723cc8a23b191d7fdcf38a0ee8f7de43232b4e`; the archive entry names its contract and carries install metadata
format 1: `zip`, root `dev`, 2,917 entries (2,736 files, 181 directories, 0 symlinks), 398,216,734 bytes, identity `c4b352012b426c60e2d4d666febf565e17239f8f534714e7b67d89430f051233`. Source commit in the
manifest: `0631131`. This is the first Windows collection of the contract: it declares `os_sdk.kind` = `windows-um-import-libraries`, i.e. the collector recorded that the Windows SDK import libraries
are needed (found through `CRT_WINDOWS_SDK_LIBPATH`), not where they are. No producer change was needed on this host.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_windows.py`, unchanged) self-builds the latest CLI sources (`b03d127`) from the SDK alone (16 sources, cold 16/0, warm 0/16) with 932 native, 151 archive and
83 SDK-store checks (all three test executables exit 0), 487 recorded processes (only the declared native tools, the CLI and its generated executables) and an unchanged SDK inventory; the bare CLI is
30,089,728 bytes, PE machine `0xAA64`, importing only `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll`. The CLI's D2 replay (`tools/replay_sdk_install_windows.py`, restricted token) passes **83/83**
on its first run of the corrected sources: the 62 installer cases plus the 21 contract and project-selection cases (contract stored beside the SDK, `configure` with no `--sdk-contract`, lock with the OS SDK
kind but no machine path, build/test/run, damaged or missing contract, release without or with a damaged contract asset refused). Details: the CLI's
[Stage 2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#toolchain-contract-and-project-sdk-selection-replay-stage-2-windowsarm64-2026-11-05).

### What the replay found

No CRT defect. One CLI test defect (`b03d127` in crt-cli): the first contract replay failed `an SDK without a contract installs` in `sdk-store-test` because the test read the new SDK as `list()[1]` while the store
sorts by a target-dependent content hash; the test now looks the SDK up by identity and the corrected sources pass all 83 store checks. The Windows `@OS_SDK@` collection and discovery, the open item of the other
hosts' records, worked as designed.

### Evidence and Limits

Raw evidence is in the CLI checkout's ignored `out/windows-arm64-s2-20261105/` (producer logs, the failed first contract replay, the final replays) and the producer worktree's `out/` (not committed). Windows/arm64
only; the Windows/x64 run of the same replay, Full-stage install metadata, a published release asset and promotion are not run. No tag, release, production lock or STATUS change.

## Windows/x64 Portable Toolchain Contract Release Replay (2026-11-05)

**Pass, local/unpromoted producer gate and consumer replay; Windows/x64.** Native Windows 11 x64 (10.0.26100), LLVM 22.1.8, Windows SDK imports `10.0.28000.0/um/x64`, Developer Mode on (not changed).
Detached worktree of **committed** CRT `436b4f2` (the portable-contract export `01a208d`, the release-fixture stand-in and the Windows/arm64 record) at a short ASCII path under the user profile, preset
`windows-host-ninja-debug`, build directory under the worktree's `out/` (the documented `CRT_BUILD_PRESET_NAME` limitation), `CRT_RELEASE_TAG=v0.0.1-rehearsal.8` (not a tag, nothing uploaded). Source-only caches (LLVM runtime
source trees and their clone stores, the cJSON, nlohmann/json, curl, mbedTLS, zlib and make archives) were copied from the Tranche 2 build and accepted by CRT's canonical-source checks; `xz-5.8.3.tar.gz`
(2,771,455 bytes, SHA-256 `3d3a1b97...74a0`, equal to the recipe pin) was downloaded with the owner's permission.

### Producer Gate and Release

`crt-development-sdk-test` exit 0 (1,154 s), `crt-c-test` **169/169** with `fork_heap_snapshot_test_runs` skipped (297 s) and `crt-dev-release` exit 0 (88 s). `SHA256SUMS-dev-windows-x86_64` lists the archive
`crt-v0.0.1-rehearsal.8-windows-x86_64-dev.zip` (146,110,051 bytes, SHA-256 `2575deb4cacc3144dbd9ad03165a5a2b85f9a1e597abd61525bfeb44d42cf304`), the **portable toolchain contract**
`crt-v0.0.1-rehearsal.8-windows-x86_64-dev-toolchain.json` (440,297 bytes, SHA-256 `e9758d599f359949274ef69bd378b84a4973190970adfaf1fdf5984adbc9aeab`) and the 02/03 stage-source assets. Manifest
`release-manifest-dev-windows-x86_64.json` (1,696 bytes) SHA-256 `2ce6c779b83a0174cd57e7cd2b5539b592278a38cfd28578067df8b9070730f3`; the archive entry names its contract and carries install metadata format 1:
`zip`, root `dev`, 2,917 entries (2,736 files, 181 directories, 0 symlinks), 396,523,706 bytes, identity `e80fa143094eae0338716e0d1419bda07ea455ceadc21e0309c7e88622487159`. Source commit in the manifest:
`436b4f2`. The contract declares `os_sdk.kind` = `windows-um-import-libraries` (found through `CRT_WINDOWS_SDK_LIBPATH` at collection; the location is not recorded). No producer change was needed on this host.

### Consumer

The CLI's contract replay (`tools/replay_cli_contract_windows.py`, unchanged) self-builds the latest CLI sources (`f984387`) from the SDK alone (16 sources, cold 16/0, warm 0/16) with 919 native, 151 archive and
83 SDK-store checks, 559 recorded processes (only the declared native tools, the CLI and its generated executables) and an unchanged SDK inventory; the bare CLI is 30,184,448 bytes, importing only
`kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll` (`result.json` SHA-256 `7fd87be4...5e73`). The CLI's D2 replay (`tools/replay_sdk_install_windows.py`, restricted token, work directory with Hangul and spaces)
passes **83/83** on its first run: the 62 installer cases plus the 21 contract and project-selection cases (contract stored beside the SDK as `.contracts/v0.0.1-rehearsal.8-e80fa143094e.json`, `configure` with no `--sdk-contract`,
lock with the OS SDK kind but no machine path, build/test/run, damaged or missing contract, release without or with a damaged contract asset refused). Details: the CLI's
[Stage 2 acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#toolchain-contract-and-project-sdk-selection-replay-stage-2-windowsx64-2026-11-05).

### Evidence and Limits

Raw evidence is in the producer worktree's ignored `out/s2-windows-x64-20261105/` (`producer-status.json`, `logs/`, `contract/`, `d2/`, the previous CLI under `previous/`; not committed). Windows/x64 only; Full-stage
install metadata, a published release asset and promotion are not run. The Windows Development SDK contains no symlinks and the driver has no network denial or read-only root. No tag, release, production lock or STATUS change.

## Installation-time contract pin handoff (2026-11-05)

The next CRT 3.1/CLI D2 increment reuses the existing format-1 install metadata
and referenced portable-contract digest/size. The
[contract handoff](../design/sdk_development_contract.md#installation-time-contract-pin-handoff-crt-31)
specifies CLI-owned receipts outside SDK identity and explicit recovery from
original pinned release inputs. No new producer asset/schema, runtime API or
SDK bytes are required. The existing metadata/exporter suite passes 14 checks.

Original native clean-release/Stage 2 records above are unchanged. The new
[CLI Linux receipt/repair proof](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#installation-receipts-and-explicit-repair-linuxx86_64-2026-11-05)
uses a copied, explicitly dirty `b6bf52b` local SDK and labeled archive/manifest
fixtures, not a new clean collector result. It owns exact source/binary hashes,
114 store checks, 834 native checks, the 84-case isolated transaction replay
and three focused payload-preservation cases.
The subsequent [macOS replay](#macosarm64-installation-receipt-and-repair-replay-2026-11-05)
passes; native Windows receipt replay remains open. Source/runtime rebuilding
or public artifact promotion is not inferred from these results.

## macOS/arm64 installation receipt and repair replay (2026-11-05)

**Pass for the bounded receipt/repair increment.** Reviewed CRT handoff
`c3daf0888882a70883b713b38728c8e54d8a8f9d` and CLI
`502992e76e58f271114dc4abf97930cd5979d17d` on native macOS 27.2/arm64.
Reused the earlier clean CRT `784389b0aaad10545598eda4d2b7e3134e640a71`
Development SDK (`v0.0.1-rehearsal.6`, content
`72d1b37593abb2f5dd9d58a8338f44191b0b2023d6cbf2621cf7cd688ce80b4c`)
and its original archive, portable toolchain contract and pinned manifest.
This is a new consumer replay, not a rebuild or requalification of current
CRT runtime HEAD. No dirty-SDK opt-in or new producer assets are needed.

The existing metadata/exporter suite passes **14/14**. The new CLI builds
from a relocated, read-only public SDK with original CRT/CLI/producer trees
hidden (cold 16/0, warm 0/16), passes **114 store checks**, and passes the
Seatbelt installation/contract/project replay **87/87**. This includes
valid-looking policy changes rejected at verify/fresh configure, immutable
installation pins, explicit original-release repair, legacy metadata recovery,
payload-preserving repair refusal, interrupted/concurrent installation and
actual project build/test/run. Store unit checks cover live/dead receipt and
contract temporary-file owners. Original and copied SDK inventories stay
unchanged; the bare CLI imports only the declared libSystem PAL dependency.

The [CLI acceptance record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#installation-receipts-and-explicit-repair-macosarm64-2026-11-05)
owns exact source/binary/tool/contract identities, isolation, commands and
limitations; CLI compatibility records this specific adopted combination.
Raw evidence is retained in ignored `out/d2-receipts-macos-20261105/`
(`candidate.json`, `build-r2/`, `isolated/result.json`, invocation and logs).
Initial build-harness attempts connected log FDs directly into the hidden
producer tree and failed the compiler resource query. Pipe-capturing output
in the host checker, with the same policy, passes; the comparisons and failed
attempts remain alongside the accepted run. No product/PAL workaround was used.

Native Windows receipt replay, installed-SDK path/tool/DLL collisions and the
automatic first-build/acquisition workflow remain separate work. No SDK
identity/schema, public artifact, default selection, committed CLI binary,
project lock or STATUS was changed or promoted by this replay.

## Windows/x64 installation receipt and repair replay (2026-11-05)

**Receipt/repair boundaries pass; the whole Windows installer gate remained
open after a native concurrent-install crash (closed later by the
[root-cause fix](#windowsx64-concurrent-install-sigsegv-fix-2026-11-06)).** Reviewed CRT `dc62e8d` and CLI
`51f1dc0`, whose product/test input snapshot matches receipt implementation
`502992e` and the Linux/macOS replay. Reused the retained clean CRT
`436b4f2e18db4ce2ac5d1eb56d6f2c7b37606296` Development SDK and its original
`v0.0.1-rehearsal.8` archive, portable contract and independently pinned
manifest from the [earlier Windows producer run](#windowsx64-portable-toolchain-contract-release-replay-2026-11-05).
All three input hashes still match that record. No new runtime build,
dirty-SDK opt-in, release fixture or artifact promotion is involved.

The scoped restricted-token CLI replay passes SDK-only cold **16/0**, warm
**0/16**, **113 store checks**, bare doctor/import/ownership checks and unchanged
original/copied SDK inventories. Windows intentionally excludes one symlink
receipt unit case; a separate real Windows symlink fixture passes **3/3**
(regular receipt accepted, symlink refused, restored receipt accepted), leaving
payload and target bytes unchanged. Producer metadata/exporter fixtures pass
**14/14**; unchanged full native/archive/runtime suites retain their baselines.

The isolated installer records **93 passing checks** before the concurrent
phase: valid-looking policy tampering, fresh-configure lock preservation,
original-release repair, legacy receipt recovery, payload-preserving refusal,
hostile archives and interrupted-install recovery all pass. One concurrent
child then returns no JSON and reports `0xc0000005`/SIGSEGV. The old checker
raises `TypeError` while sorting a missing `already_installed` value, leaving
its result incomplete. A fresh, bounded continuation of phases 7–9 passes
**9/9**, including concurrent installs, read-only store rejection and tar.xz
identity. One boundary probe overlaps the first run; this covers 101 distinct
cases across attempts, **not a single 101/101 acceptance**. CLI reporting is
fixed to retain native exits and count empty replies as failures (four local
reporting fixtures pass). The native crash itself is not fixed or excused.

Five small missing/empty/malformed/dead/live lock-owner diagnostic fixtures
behave as expected, without reproducing the crash. A later image-base sample
of the same failed executable suggests `free_unlocked` as an address candidate;
the fault-time base/stack were not captured, so allocator ownership, caller and
root cause remain unproven. Next work is a bounded concurrent-install repro
with fault-time module/stack evidence before changing CLI or PAL behavior.

The [CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#installation-receipts-and-explicit-repair-windowsx64-2026-11-05)
owns exact identities, commands and per-case evidence. Raw files remain in
ignored `out/d2-receipts-windows-20261105/`: `candidate.json`, `status.json`,
`build/`, failed `isolated/`, passing `remaining-r2/`, `receipt-symlink/`,
`lock-owner-probe/` and reporting fixtures. The failed run and first local
continuation harness error are preserved. Windows/x64 concurrency closure,
Windows/arm64 receipt replay, collisions and first-build/acquisition remain
open. No STATUS, SDK, committed CLI binary, default, project lock, tag or
release was changed.

## Linux installed-SDK selection collision slice (2026-11-05)

Reviewed producer documentation baseline `d856a009358dcf6391c89ee0af7390b54db2419b`
and CLI base `483e76d846808aa3391b329908c4b5883c815c66`. The bounded CLI
selection replay found that content references longer than 12 digits matched
only the store directory suffix. CLI now checks every supplied digit against
the installation receipt, then applies the existing SDK/provider content
validation. This is a consumer matcher correction: SDK identity, producer
metadata, portable compiler policy, receipts and project-lock formats do not
change. No producer artifact rebuild or promotion is needed for this fix.

The [consumer-owned evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#installed-sdk-selection-and-search-path-collisions-linuxx86_64-2026-11-05)
records fresh SDK-only CLI build, native regressions, competing same-version
SDK identities, lock preservation, hostile path/tool/header/runtime search
inputs and actual static/shared build/test/run on Linux/x86_64. It reuses the
explicitly dirty local developer SDK from the Linux receipt slice above;
its additional synthetic resolver SDK is not a producer release. Exact input/
source/binary/tool hashes, retained failing baselines and reporting checks live
in that record and CLI ignored `out/d2-selection-20261105/`.

This completes the Linux collision slice only. Affected-host macOS/Windows
matcher replay, actual Windows DLL collisions, Windows/x64 native concurrency
investigation and Windows/arm64 receipt replay remain open. First-build binding
and pinned acquisition remain consumer workflow work, with producer handoff
tracked separately in 3.4. No runtime/SDK bytes, production pin, lock, STATUS
or public artifact is changed by the producer documentation update.

## Linux installed-SDK first-build consumer slice (2026-11-06)

Reviewed producer baseline `c9e6aa9ecb73c85c73ba32761b2715d0b09fbe71`
and CLI base `8788b59a77841537033915b34bbbf3c9a013b906`. CLI implements
first `build` binding of an unresolved format-1 project from explicit LLVM
paths or local configuration, with exact installed SDK/provider/tool pins
published after success. Failed selection/compilation/publication preserves
the existing binding; resolved/partial state is not silently reselected.
The [consumer-owned evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#first-build-without-configure-linuxx86_64-2026-11-06)
owns exact source/binary/SDK/tool inputs, native regressions, isolated workflow,
publication failure/rollback/retry and actual C static/C++ shared consumers.

No new producer format or artifact is needed. The consumer reuses the explicitly
dirty developer SDK and immutable provider from the Linux receipt record;
this is not producer promotion or default/public acquisition. Tranche 3.4 still
covers the pinned metadata/archive/contract handoff for subsequent acquisition.
macOS/Windows first-build replay and existing native installer gates remain open.

ELF review of the retained provider found a separate limitation:
`executable-shared.link_c` contains `libc.a`/static runtime inputs, and a C
shared request emits no dynamic imports. Do not count that as C shared proof.
The final consumer replay distinguishes C static from actual C++ shared output.
Tranche 6.4 tracks review of the intended producer C shared policy and any new
qualified contract/artifact needed; the retained pin is not rewritten, and no
root cause or current-HEAD runtime fix is claimed from this legacy fixture.

## Linux pinned HTTPS SDK handoff (2026-11-06)

Reviewed CRT `6e064f2cc2ff84b5507d7be8000668c730dfe73a` and CLI base
`4f55a52bd75660b4c8d2543da58da6337efd45de`. Existing producer release-manifest
and install metadata format 1, named archive/portable-contract assets, exact
sizes/digests and SDK content identity are sufficient for this developer handoff.
CLI owns the implemented `crt-cli.sdk-source`, format 0: explicit HTTPS URLs,
CA bytes/pin, manifest size/digest and expected SDK/contract pins. No producer
wire/schema, ABI, source-stage or runtime artifact changes are needed.

The [consumer-owned record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#pinned-https-sdk-acquisition-linuxx86_64-2026-11-06)
contains exact source/binary/SDK/tool/trust inputs, SDK-only CLI build, native
regressions, real loopback TLS failure/recovery and first-build behavior. It
reuses the original dirty local Linux Dev fixture from receipt acceptance;
its assets are not overwritten or relabeled as a clean/promoted release.
Technical tests use a disposable explicit CA, not approved public trust.

Tranche 3.4's bounded developer handoff is exercised: metadata/contract/archive
are verified before normal immutable installation; exact installed identities
need no redownload; failed transfer/compile/publication preserves project pins.
PID-owned network staging uses the existing live/dead recovery boundary.
Caller-supplied SDK/contract pins are checked on installed reuse without claiming
a fresh remote-manifest verification. CLI owns transaction/selection semantics
and keeps the compatibility ledger, not this producer record.

Official/default URLs and publisher trust, producer promotion before production
consumer adoption, preview assembly and affected-host macOS/Windows replay
remain separate gates. Existing Windows concurrency/DLL and pure C shared-policy
work stays open. No producer code/SDK bytes, root/default lock, binary, release
or STATUS is changed by this documentation update.

## macOS/arm64 selection, first build and HTTPS handoff replay (2026-11-06)

**Pass for the bounded three-increment consumer replay.** Reviewed CRT
`82466be582e57c8770a6816f1c6b9ed0bf819f76` and CLI `1dc3c03` (the
Linux full-identity selection `8788b59`, first-build binding `4f55a52` and
pinned acquisition `1dc3c03`). Reused the clean CRT `784389b` Development SDK
and original `v0.0.1-rehearsal.6` archive/portable-contract/manifest from the
[earlier macOS record](#macosarm64-installation-receipt-and-repair-replay-2026-11-05),
not newly built runtime assets or rewritten release fixtures.

The original CLI's HTTPS path failed after the first metadata GET: it required
`manifest.format == 1`, a field neither the retained nor current producer's
`tools/crt_sdk_release.py` emits. The actual versioned contract is the archive
asset's **`install.format == 1`**. The local CLI correction removes that false
envelope requirement while retaining version/native-target/profile/content,
size/digest and install-format checks. It is common consumer code, not a macOS
branch or a new producer schema; original release inputs and pins stay intact.
The shared TLS suite now also rejects unsupported install metadata before any
asset download. Other-host replay of that correction is not claimed here.

The corrected SDK-only CLI self-build passes cold **17/0**, warm **0/17** and
bare import/ownership checks. Native Seatbelt replay passes:

- **66/66 selection/collision** checks: full supplied identity digits,
  competing SDKs, explicit tools, preserved locks, hostile search paths and
  actual static/shared consumers.
- **86/86 first-build** checks: source-only new, exact pins, failed compilation,
  ambiguity/partial-state refusal, real publication-rename failure with rollback,
  cache-preserving retry and C static/C++ shared execution.
- **73/73 HTTPS acquisition** checks, with explicit disposable test trust,
  TLS/metadata/asset failures, exact installed reuse and live/dead fetch recovery;
  **12/12 focused first-build acquisition failure/retry** checks.

Four static/shared consumer images pass the macOS binding audit; shared C++
consumers import the selected installed CRT dylibs, not host libc++ as a
substitute. The corrected native fixture passes **922 regression checks**
against that same candidate. Original/copied SDK inventories and release inputs stay unchanged.
The [CLI-owned acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#selection-first-build-and-pinned-https-replay-macosarm64-2026-11-06)
owns exact tested source/binary/tool/trust identities, commands, host-test
isolation, retained failures and regression scope. Raw evidence remains in
ignored `out/first-workflow-macos-20261106/` in the CRT checkout.

No producer code/schema/API/ABI change, SDK rebuild, production pin or artifact
promotion was needed. Windows matcher/workflow/DLL and installer-concurrency
gates remain open, as does Linux's retained pure-C shared provider-policy audit.
Existing Linux evidence retains its tested snapshots; it is not a replay of
the new consumer correction. Defaults, root locks, committed CLI binaries,
public trust and STATUS remain unchanged.


## Windows/arm64 receipt, selection, first build and HTTPS replay (2026-11-06)

**Pass, local/unpromoted consumer replay of an existing producer candidate; Windows/arm64.** Native Windows 11 arm64 VM (10.0.26300, 4 vCPUs, no GPU), LLVM 22.1.8, Windows SDK imports `10.0.28000.0/um/arm64`, native arm64 shell and Python.
The producer input is the **existing** clean-commit Development SDK of the Stage 2 record: CRT `0631131`, `v0.0.1-rehearsal.7` (archive `976150ba98d3e5d6dc609a6218a5d4e4ad9ab0f4746e1447694765fdbd316294`, toolchain contract
`08ae2c51559704ea6791dab7e4555448d9fbeeef29f7f92bdd66bfa8a6301d68`, manifest `167f326094539ec92cd86a7aed723cc8a23b191d7fdcf38a0ee8f7de43232b4e`, content identity `c4b352012b426c60e2d4d666febf565e17239f8f534714e7b67d89430f051233`).
**It was not rebuilt.** The producer delta from `0631131` to the reviewed CRT `a00c6be` is `tools/create_host_tools.py` (a macOS-only private dylib closure), `tools/replay_host_private_macos.py`, `tools/test_host_tools.py` and `AGENTS.md`; none of
these is shipped in the Development SDK, whose wrappers, libc, libc++ and ports are unchanged, so the earlier producer gate (`crt-development-sdk-test`, `crt-c-test` 169/169, `crt-dev-release`) is not repeated and no producer claim is made for `a00c6be`.

The consumer is the CLI self-built from this SDK alone at crt-cli `160a4a6` (executable `d855f092e27bfab01047902ec14ae9747447e82d866702ea71d580d59ef80582`, 32,403,968 bytes, PE machine `0xAA64`, imports only `kernel32.dll` and
`api-ms-win-core-synch-l1-2-0.dll`; cold self-build 17 executed / 0 reused, warm 0 / 17; 985 native, 151 archive and 113 SDK-store checks; 531 recorded processes; unchanged SDK inventory). The drivers are the CLI repository's existing
Windows drivers, unchanged, under the restricted token of `crt_windows_trace`:

| Suite | Result |
| --- | --- |
| Installer, installation receipts and explicit repair, stored contract, project selection (`replay_sdk_install_windows.py`) | **101/101** (includes the three concurrent-install cases) |
| Installed-SDK selection and search-path collisions, full-identity matcher (`replay_sdk_selection_windows.py --cases selection`) | **68/68** |
| First build without `configure` (`--cases first-build`) | **86/86** |
| Pinned HTTPS acquisition with a disposable test CA (`replay_sdk_acquire_windows.py`) | **74/74** |

All four suites install or acquire from the original producer manifest, which has no top-level `format` (the release-envelope correction) and `install.format` 1. A junk `libc.dll` planted in the CLI's working directory shadows the shared runtime on
Windows; that limit is documented by a passing case, not defended. No decoy executable was started in the selection, first-build and acquisition suites.

**Concurrent-install note (superseded by the [root-cause fix](#windowsx64-concurrent-install-sigsegv-fix-2026-11-06)).** At the time of this replay the 3.1 item about the native SIGSEGV seen once in a Windows/x64 concurrent install stayed open: it was neither reproduced nor explained here. For supporting evidence only, the same scenario (two simultaneous
`sdk install --lock-wait 300` of this SDK into a fresh store) was repeated 15 times with the self-built CLI run directly; in 15 of 15 rounds both processes exit 0, one reports `installed:` and the other `already installed:` with the same identity,
the store holds one SDK and nothing is left in `.staging` or `.locks`. The throwaway script labeled every round `FAIL` because it parsed text output as JSON; the rounds were judged from the retained raw output.

No CRT, producer or SDK defect and no CLI product defect was found. Details, hashes and the record of the stress script's defect are in the CLI's
[acceptance record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#selection-first-build-and-pinned-https-replay-windowsarm64-2026-11-06). Raw evidence is in the CLI checkout's ignored
`out/windows-arm64-s3-20261106/`. Not run: Full-stage metadata, a published release asset, official trust, promotion and the Windows/x64 crash closure. No tag, release, production lock or STATUS change.

## Windows/x64 concurrent-install SIGSEGV fix (2026-11-06)

**Root cause fixed in the pinned libunwind recipe; verified by the CLI-owned replay.** CRT `05b5678`
(`libstdc++/third_party/libunwind/recipe.json`) patches `Registers_x86_64(const void*)` to copy the `_xmm` array under
`_WIN64` and to zero it in the default constructor. Win64 makes XMM6-XMM15 callee-saved; the unpatched cursor resumed a
catch handler with indeterminate vector registers, which corrupted a freshly zero-initialised `std::wstring` in the
`--lock-wait` retry loop. Producer-side evidence is the minimal reproducer `libstdc++/tests/imported_libcxx_eh_registers_test.cc`
(run for both linkages by `tools/test_libcxx_runtime.py`, effective on Windows/x86_64 only) and the design note in
[cxx_runtime.md](../design/cxx_runtime.md#win64-callee-saved-vector-registers-across-a-catch). The CLI-owned
[replay](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#windowsx64-concurrent-install-sigsegv-fix-2026-11-06)
passes the installer driver 101/101 three times, selection 68/68, first build 86/86 and HTTPS 74/74 with a CLI self-built
against the SDK rebuilt from `05b5678`.

Linux check done for this review: the patch's `find` text occurs exactly once in the pristine pinned `Registers.hpp`
(so the recipe still applies on every host) and the added code is `_WIN64`-only, so Linux libunwind code is unchanged.
A Linux rebuild of the patched recipe was not re-run here. macOS, Windows/arm64 and Linux/aarch64 are not re-verified
against the fix (their own libunwind save/restore paths are separate).

## macOS/arm64 D3 public-SDK handoff (2026-11-09)

Producer: clean `2f4103d97880d5a51c474ef7b65fe810186c49de`, retained detached worktree `../crt-d3-2f4103d`,
build `out/d3-macos-20261109/build`. Local Dev SDK version `v0.0.1-rehearsal.7`, content identity
`02e67f3ef8e8eef48f4497c1f5f5ad938e2fdc98ff143fd6cdf849b46fec999b`; portable compiler provider SHA-256
`912094d0016e8d1a8a6c116e1b257867cfc2f284b148de8d5b419f4d6c97e189` (exported with `tools/export_cli_toolchain.py --portable`).
The producer logs `dev-build.log`, `release-build.log`, `result.txt` retain both commands' exit 0. Dev verification passes,
including its static/shared HTTPS+inflate and trust-negative consumer. These are local artifacts, not publication.

The consumer copied that public SDK and independently pinned host-tools bundle outside both repositories and the
producer worktree. Seatbelt denies original-tree reads/writes, undeclared execution, outside-scratch writes and
network access for the native jobs. The CLI needs no producer-private headers, sources or build outputs. It builds
unmodified zlib 1.3.1 into a separate port store and proves static/shared C/C++ consumption and SDK-provider controls;
the SDK copy and original SDK inventory stay unchanged. The packaged Python oracle is test-only and has a separately
labeled host-interpreter/SDK-tool exception. No CRT source, public ABI, recipe bytes or wrapper changed here.

CLI owns the exact executor/audit/store/key policy and technical evidence, committed as
`4936df5e2abae90e84cd3a52d452b7a9bb612329`, in
[its D3 macOS record](https://github.com/webos21/crt-cli/blob/4936df5e2abae90e84cd3a52d452b7a9bb612329/docs/acceptance/sdk_install_acceptance.md#source-port-d3-replay-macosarm64-2026-11-09).
CRT's ignored `out/d3-macos-20261109/` retains per-suite stdout/stderr/commands and input inventories, actual loader
provider paths and the nine-image Mach-O binding audit. The native build uses declared ar/ranlib; the oracle's Darwin
configure chooses `libtool` and its temporary absolute install name. The differential keeps all 15 static member
objects identical and explicitly checks the intended install-name/RPATH difference, rather than patching upstream.

Scope: macOS/arm64 only. This does not qualify the new delta on Linux or Windows, approve the pending adapter table,
move a production SDK/CLI lock or publish a release. Earlier Linux/Windows SDK and CLI baselines retain their own
records. Formal table approval (including the macOS layout/tool policy) and native Windows CRT-shell/make
static/shared acceptance remain open in Tranche 4. STATUS is not changed by this replay.

## Windows/x64 D3 public-SDK replay boundary (2026-11-09)

Reviewed producer `a62d5e2d3a17ed0984e659552413a6d044330f6c` and CLI
`4936df5e2abae90e84cd3a52d452b7a9bb612329`. The producer diff since
`05b56788501b02f71a8b8084cc4041effa1475a9` contains four documentation files only.
The consumer therefore reuses that clean, local `v0.0.1-rehearsal.10` public Dev SDK,
with its original provenance, without rebuilding or promoting it. No CRT runtime,
recipe, SDK payload, compiler policy, ABI or release identity changed.

The current CLI product inventory matches the macOS D3 replay. Native Windows/x64
common regressions, cold/warm self-build, SDK-free commands and direct PE imports
pass with source/SDK/provider visibility probes and process accounting. Original
and copied SDK inventories stay unchanged. This is not a fresh producer CTest,
Development SDK production/trust run or Windows/arm64 acceptance.

The CLI's five input-enabled D3 suites all compile after consumer-owned test
fixture path/home corrections, then fail at the unsupported Windows boundary:
no Windows adapter layout, executor admission or PE port audit. No native CLI
static/shared source-port item, consumer proof or oracle equivalence is accepted.
The producer recipe's existing Windows support is not changed to a failure status:
these are CLI adapter gaps, not a rerun/failure of CRT's recipe engine. Formal
adapter-table approval and Tranche 4.2 remain open.

The producer-side `replay_cli_contract_windows.py` needed two guard corrections:
scan C/C++ source/header suffixes rather than recipe JSON descriptions containing
`_WIN32`, and permit D3's private `src/main/store_lock.h`. It still rejects consumer
OS macros and unexpected private headers. The script hash under test is
`3bf505ded4a323aaf57b2a4019ca5ee35de3df57f495ab913cb2e1d43d58bb81`.
Consumer fstream fixtures use explicit UTF-8 strings with the current SDK's narrow
file APIs; the unavailable wide-path overload is not qualified by that adjustment.

[CLI-owned evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#source-port-d3-replay-windowsx64-2026-11-09) records exact SDK/policy/tool/binary identities, per-suite
outcomes, all failed attempts and input-checked test-only continuations. Raw logs
are in the CLI checkout's ignored `out/windows-d3-20261109/`. No committed binary,
consumer root lock, default trust, STATUS, public tag or release was changed.

## Windows/x64 CLI source-port implementation handoff (2026-11-09)

The CLI implementation at `2ae74d4d55bd1d7210c96ae596a48b2a934931bf` closes the missing Windows executor/layout/PE
boundary recorded above for native Windows/x64. It consumes the same clean public
rehearsal.10 SDK from CRT `05b56788501b02f71a8b8084cc4041effa1475a9` and the same
independently pinned CRT host-tools bundle. Reviewed producer baseline is `ff6b9e2`;
no producer runtime, SDK, recipe, wrapper, ABI or artifact identity changes are needed.
The shared recipe schema, native compiler policy, host-tools and project-lock
formats retain their existing versions; Windows layout/audit/key inputs belong to
the consumer's experimental digest-keyed adapter.

[CLI-owned acceptance](https://github.com/webos21/crt-cli/blob/2ae74d4d55bd1d7210c96ae596a48b2a934931bf/docs/acceptance/sdk_install_acceptance.md#source-port-d3-windowsx64-implementation-and-replay-2026-11-09) records final source/binary/receipt hashes, the fresh
isolated contract replay, all input-enabled source-port suites and the actual JSON
port-build command. Both static/shared recipe roundtrips and C/C++ store consumers
pass with static-link and actual shared-provider evidence, lock negatives, PE/store
failure/recovery checks and the packaged producer-driver oracle. The CLI executable
is refreshed from the qualified image. Earlier unsupported/failed receipts retain
their original outcomes; unchanged SDK and host-tools inventories are checked.

This is a consumer acceptance handoff, not new producer CTest/SDK production or
release qualification. Formal adapter-table approval stays pending, including
Windows tool-script policy, import-library build-key inputs, PE audit scope and the
oracle's explicit extra `z.lib` copy. Affected common changes still need their
Linux/macOS replay; Windows/arm64 and promotion remain separate gates. No consumer
root lock, STATUS or public release is changed. The CLI compatibility ledger owns
the exercised combination; this document does not duplicate its adoption ledger.

## D3 closure review and affected-host plan (2026-11-09)

Owner-requested baseline: CRT `21b9d0e` and CLI `2ae74d4`. Producer recipe
`dde944984d799197c0af1553ac7cf38139edc7b2e4e673ef9b4c17dd8c1a349b`
and test `3d7460fb51e215424ef4fa36593cd7662bdcf83bbd1e986eb0ed746b07aebe62`
match the SDK copy and CLI adapter. The adapter classifies make as a host tool,
no target-port dependencies, static/shared variants and a minimum C SDK stage;
the exercised CLI installation profile remains native Dev at `02-cxx`.

Review covers the accepted field/placeholder subset and generated-Makefile
workaround; independent CRT shell/make and declared LLVM; Linux ELF, macOS
install-name/ar/OS-SDK and Windows DLL/import-library/audit policy; separate
port-store/provider/lock identities; and bounded oracle differences. Current
macOS executor environment/make overrides are included in key inputs; review
metadata is deliberately excluded. Do not change recipe or SDK bytes simply
to mark a review. Do not rewrite existing store items or root locks.

This is a review candidate, not final approval. Latest common-change Linux
proof is run here and the owner provides the matching native macOS replay.
Keep `crt_review: pending` and TODO 4.1/4.2 open until both evidence and the
review/adoption decision are recorded. Existing Windows/x64 final evidence
remains valid for its exact unchanged inputs; any new affected policy needs
its own delta. Windows/arm64 D3 is separate from its completed D2 results.
The CLI owns its compatibility ledger and
[native handoff](https://github.com/webos21/crt-cli/blob/main/docs/guides/development.md#d3-common-change-closure-and-native-handoff).

## Linux D3 common-change closure and macOS handoff (2026-11-09)

Baseline CRT `21b9d0e` / CLI `2ae74d4`: the [consumer-owned replay](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#linux-d3-common-change-replay-and-macos-handoff-2026-11-09)
passes the six input-enabled D3 suites plus native project regression, bare CLI/
import/visibility checks and actual port publication/reuse/verify. The copied
public Dev SDK and independent CRT host tools preserve exact original inventories;
no producer runtime, SDK, recipe or wrapper changes are needed for the common
Linux code delta. No GFX/Web/Full rebuild is performed.

The retained Linux SDK is the earlier dirty `b6bf52b` developer fixture, with
its existing compiler contract and local-only host tools. This is intentionally
not clean release qualification or a new latest-CRT-runtime acceptance. Exact
source/binary/SDK/provider/tool/archive pins, results, oracle exception, traces
and data paths stay in CLI acceptance and its compatibility ledger.

The old SDK-provider C shared control maps host `libz.so.1.3.1`. Its historical
negative-separation assertion is not endorsed SDK binding. Positive static SDK
and actual port static/shared evidence remain distinct. TODO 6.4 retains the
compiler-policy/runtime-search decision; do not silently waive or relabel that
mode at contract approval.

Linux's affected-delta gate is recorded. The owner runs the same product/test
fingerprint on native macOS through the CLI guide. Final adapter-table approval,
D3 closure and any release promotion remain open. Existing final Windows/x64
evidence is reused for unchanged inputs; Windows/arm64 D3 remains separate.

## macOS D3 affected-change closure replay (2026-11-09)

Reviewed producer documentation baseline `2fdce21d687936c313a3a5990d7acdc3ab824c54` and consumer
`938518c4b3bb2c887e921af602082b46ac9be5b4`. CLI's product/test fingerprint is exactly the Linux handoff's
`22720f7d9e223af06e41887ec0bed9ede55568ecdd6b9b5ef3499769893bee16`, including the Windows implementation's common
changes. The clean macOS public Dev SDK from `2f4103d` (rehearsal.7, content `02e67f3e...c999b`) and its separately
pinned provider/independent host tools are reused with their original identities; Linux's dirty developer provenance
is not adopted. The producer delta is documentation plus the Windows-only replay guard, absent from this SDK;
therefore no runtime/recipe/wrapper/ABI or SDK/GFX/Web/Full rebuild is needed.

Fresh native macOS/arm64 source-hidden CLI build and the six input-enabled D3 suites plus full native regression
pass. The improved macOS host-only driver passes **29/29 gates**, including copied-SDK/source/provider/bundle-free
CLI checks, actual port publication/reuse/verify, warm cache and unchanged original/copied inputs. Port-provider and
SDK-provider runtime maps discriminate correctly on this SDK; the existing Mach-O binding audit passes on nine images.
This does not endorse the earlier Linux C shared SDK control's host-libz mapping (TODO 6.4 remains open).

Consumer-owned [acceptance](https://github.com/webos21/crt-cli/blob/8ad11dd95d09c416c62ddf4ea6d393e4f7c5279f/docs/acceptance/sdk_install_acceptance.md#macos-d3-common-change-closure-replay-2026-11-09)
at CLI `8ad11dd95d09c416c62ddf4ea6d393e4f7c5279f` and its compatibility ledger retain exact SDK/provider/tool/source/CLI pins, suite counts, raw receipts and the oracle-only
exception. Raw evidence is in this checkout's ignored `out/d3-common-macos-20261109/`; `qualification.json` binds the
input/command/result and map/audit hashes. No product or upstream patch was required; no root/default lock, committed
binary, SDK asset or STATUS was changed. The macOS port key remains the earlier `d1cf55d8...5e2797`.

This completes Tranche 4.2's requested macOS affected-source proof, not formal CRT approval, D3 global closure,
artifact promotion or preview release. Tranche 4.1 remains the explicit adapter/field/key review and adoption decision.
Existing Windows/x64 qualification is retained for its unchanged inputs; Windows/arm64 D3 remains separate.


## D3 adapter contract approval (2026-11-09)

**Decision: approve the bounded zlib adapter; close Tranche 4/D3 for its exercised
Linux/x86_64, macOS/arm64 and Windows/x64 combinations.** Review baselines are CRT
`dfb83c4` and CLI `8ad11dd95d09c416c62ddf4ea6d393e4f7c5279f`, plus the CLI's
review-metadata/regression delta identified in its linked acceptance. CRT owns recipe
semantics, SDK/compiler and host-tools contracts; CLI owns adapter execution, store,
provider/project locks and the adoption ledger. Studio has no affected contract.

| Reviewed boundary | Accepted policy |
| --- | --- |
| Identity | zlib 1.3.1; recipe SHA-256 `dde944984d799197c0af1553ac7cf38139edc7b2e4e673ef9b4c17dd8c1a349b`, source `9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23`, roundtrip test `3d7460fb51e215424ef4fa36593cd7662bdcf83bbd1e986eb0ed746b07aebe62`; producer and exercised installed SDK files match |
| Roles/capabilities | `make` is an explicitly pinned host tool; no target-port dependencies; static/shared variants; `01-c` minimum is a requirement, not qualification of every `01-c` SDK; the exercised CLI inputs are Dev `02-cxx` |
| Recipe semantics | Fail-closed configure subset and declared placeholders/environment merge; unknown fields/digests refused; generated-Makefile prefix escaping retains its documented removal condition; no upstream source patch |
| Native execution | Explicit CRT shell/tools and public SDK wrappers; native Clang/linker/ar/ranlib and declared OS-SDK boundary; Python reference driver is a separately labeled oracle only |
| OS policy | ELF SONAME/layout; macOS relocatable install-name and declared archiver arguments; Windows PE audit despite POSIX file names, real symlinks and declared import libraries; oracle differences retain their per-host explanations |
| Keys/lifetime | Source/recipe/test/adapter, SDK, target/tool/bundle and declared executor environment/make inputs remain pinned; OS metadata/import-library identities retain their recorded bounded scope; review metadata and local paths are excluded; equal keys do not prove byte reproducibility |
| Providers/locks | One SDK or port provider per closure; separate immutable port store; SDK unchanged; locks pin item manifest digests and SDK identity; approval never rewrites old pending manifests |

The [Linux common-change replay](#linux-d3-common-change-closure-and-macos-handoff-2026-11-09),
[matching macOS replay](#macos-d3-affected-change-closure-replay-2026-11-09) and
[Windows/x64 implementation](#windowsx64-cli-source-port-implementation-handoff-2026-11-09)
provide native evidence at their exact retained inputs. The review changes only
CLI review metadata and focused tests; executor, key serialization, audits, recipe,
SDK and provider bytes are unchanged. Focused Linux evidence and the actual reuse
of a retained pending item/project pin are owned by
[CLI acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#d3-review-metadata-and-existing-item-reuse-2026-11-09).
Unchanged macOS/Windows execution gates are reused rather than represented as fresh
runs of the new metadata-only binary.

Limits remain explicit: Linux's retained C shared SDK-provider control resolved
host libz and is **not endorsed** (Tranche 6.4); Windows SDK shared-z absence is a
refusal, not positive evidence. Windows/arm64 D3, general recipes/dependency graphs,
locked/offline semantics, binary-cache trust, exhaustive loader auditing and Full
installation are outside this closure. The Windows oracle's extra byte-identical
`z.lib` remains a D4 layout review before dependent ports. No schema/version,
producer artifact, root lock, committed CLI binary or trust/default URL is changed;
this decision grants no public release or production pin.

The next bounded gate is Linux Dev SDK + CLI Preview input qualification. Existing
dirty Linux developer SDK/bundle provenance is useful replay evidence, not a clean
release candidate. Freeze a clean identified producer candidate, verify its actual
archive/portable contract and source/notices, then build the CLI against that
immutable handoff. Keep C shared SDK mode excluded until 6.4 is qualified. Final
package/trust/first-user replay applies to each advertised host; promote producer
assets before consumer pin adoption. D4 and Full delivery remain independent.


## Linux Dev Preview candidate rehearsal (2026-11-10)

**Clean local producer candidate, unpromoted.** Select CRT
`0ab8f8cfbbde19e14dd5861ba176fb8881e06182`, version `v0.0.1-preview.20261110.3`, native
Linux/x86_64, Release, LLVM21. Producer and compile provenance are both known
and clean; the SDK keeps Dev profile 1, manifest stage `02-cxx`, existing native
policy 1 and recipe formats. The CLI source is independently pinned at
`024de89d2a3e6dfbfe8b01db77262d091e2c13a7`; consumer behavior/resources and final
binary hashes are owned by [CLI Preview acceptance](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md#linux-local-candidate-2026-11-10).

A detached local source clone preserves earlier work and records its canonical
upstream origin. Fresh Ninja output is
`/home/syszone/GitLocal/crt-preview-20261110/producer/out/preview-linux-20261110-r4`.
Configuration uses explicit `CRT_TARGET_OS=linux`, `CRT_TARGET_ARCH=x86_64`,
`CRT_USE_IMPORTED_LIBCXX=ON`, `CMAKE_BUILD_TYPE=Release` and the version above;
Skia/GPU/FFmpeg/media-curl/LVGL options are off. Dev's own network/JSON/xz ports
still build through their normal producer path. Only verified-on-use raw Git/
download caches are reused; target outputs and source assets are fresh.

`cmake --build <fresh-build> --target crt-c-test crt-cxx-test crt-dev-release crt-host-tools-test --parallel 6`
exits zero. C tests pass **147/147**; C++ runtime, string-ABI and iostream tests
pass static/shared. The Win64-only vector-register branch is not applicable to
Linux and is not claimed as a native Windows replay. Host-tools inventory/native
images pass. Dev's static/shared HTTPS+inflate and trust negatives pass, followed
by actual archive extraction/profile verification and portable policy export.
An additional external validation of the extracted SDK passes with producer,
original SDK and CLI trees hidden. GFX/Web/Full SDKs are not rebuilt.

| Collected asset | SHA-256 |
| --- | --- |
| `crt-v0.0.1-preview.20261110.3-linux-x86_64-dev.tar.xz` | `e858c1b8b22697e11551de44e1b5c3aba3cbdc93679787e33d7ae6241d6f2217` |
| `crt-v0.0.1-preview.20261110.3-linux-x86_64-dev-toolchain.json` | `90b3bb59aaac7df481becbad4e2b87473e2fefcb803123332689d7f1135e4e30` |
| `crt-v0.0.1-preview.20261110.3-linux-02-cxx-source.tar.xz` | `cb937e55acd86af008d2695fa826156f9c1defb300fdeb928168b953692f91d9` |
| `crt-v0.0.1-preview.20261110.3-linux-03-gfx-simple-source.tar.xz` | `20479ccb00deffbbd9d35bddb5d0fafddf297c01ccc51bf4c9adff0c345d8270` |

The release-manifest SHA-256 is `d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60`;
SDK content is `36623957fbdd5a384cc173991e20253ff83148028ab8072298dcd434a55a2b3f`. All four declared
asset sizes/digests match the actual copied files. `handoff-r4/` is an immutable
public handoff outside both repositories/build trees. `source-asset-r4.json`
passes **3/3** checks: the two corrected C++ integration files equal this clean
producer, and 02's nested 03 recipe references the actual collected 03 asset.
The host-tools bundle has its own version `v0.0.1-hosttools.20261110.3` and
manifest digest `922c76155957a55d095f902077cb73bbc120ae2c2453e9c1c000f5e58acdefa9`; it remains local-only.

The retained failed attempts explain the new baseline: a nonconforming extra-
hyphen version was rejected; an optimized ucontext fixture looped because a
nonvolatile local counter was restored with registers (`-O0` passed, `-O3`
timed out, volatile `-O3` passed on the same runtime). Commit `ca38216` fixes only
the test and adds a 15-second CTest bound. Commit `0ab8f8c` makes the C++ smoke
accept explicit native architecture, reject foreign execution before output,
and give the EH fixture entry C linkage. Both C++ files ship in source asset 02,
so earlier `.2` assets are preserved and excluded from final selection. No
runtime implementation, public header, ABI or upstream source patch changes.

Raw `build-r4.log`, `handoff-r4.json`, source checks, external result and original
failed attempts stay under `/home/syszone/GitLocal/crt-preview-20261110/`.
Consumer `qualification-r4.json` binds the producer evidence and final CLI
receipts. Local fixture/tool changes do not relabel old dirty SDKs as clean.

This completes the bounded input-rehearsal part of TODO 7.3. Producer source
publication, trust/release metadata, actual SDK promotion and subsequent consumer
pin adoption remain open. The two producer fixes are local commits until pushed;
this source offer is not advertised as publicly available. Every advertised
host needs its final package replay. Linux C shared SDK policy stays unqualified
under 6.4; positive source-port shared evidence does not endorse that SDK mode.
D4 and Full delivery remain independent, and STATUS/default locks are unchanged.


## Linux Dev SDK public promotion (2026-11-10)

Review decision: promote the unchanged qualified `v0.0.1-preview.20261110.3`
Linux/x86_64 Dev artifacts from clean source `0ab8f8cfbbde19e14dd5861ba176fb8881e06182`
under the [bounded manual-pin trust policy](../design/dev_preview_trust.md).
The owner authorized source publication, trust review and SDK promotion.
The [release notes](../releases/release_notes_v0.0.1-preview.20261110.3.md)
freeze the exact manifest/archive/policy/source hashes before publication.
Manifest pin: `d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60`. No asset is rebuilt or
replaced, no publisher signature/default channel is claimed, and the tag must
resolve to the original producer. Source publication now includes the pushed
producer fixes and the public CLI packaging/acceptance checkpoint `3b9a8ed`.

The current CLI refuses GitHub's HTTPS asset redirect. The accepted public path
is a verified manual download followed by native pinned local installation and
C/C++ first build/test/run; redirect-aware acquisition is a separate CLI change.
Publication is staged as a prerelease and public-byte/consumer receipts are
recorded below after download verification. Full and CLI binary releases are
outside this producer promotion.

### Published result and consumer proof

**Promoted:** [v0.0.1-preview.20261110.3](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.3), public prerelease
published `2026-11-10T00:37:33Z`, source tag `0ab8f8cfbbde19e14dd5861ba176fb8881e06182`.
The tag resolves to that exact producer, and all six original collector outputs
remain unchanged. Draft upload metadata and an authenticated draft re-download
match the frozen names, sizes and digests before publication.

An additional **unauthenticated** public download verifies **6/6 files** against
the pre-publication pins. TLS peer/hostname verification is enabled with explicit
CA PEM `/etc/ssl/certs/ca-certificates.crt` (SHA-256
`a9bdea6ecf3eea678a2a25ec45b6167abd5fe18c2caef26380186bceff2a5fdd`), TLS >=1.2 and proxies disabled. The
host verifier accepts one HTTPS hop only to GitHub asset-storage hosts and omits
temporary signed redirect URLs from receipts. Public raw source copies of the
three corrected files match the source commit (**3/3**).

The qualified CLI `6ba046e5baeae24b86e407ad04353ec9d98b989c21921160413408ca0f71dab2` consumes those actual
public-download bytes with producer/repository/build/local-SDK trees hidden,
private HOME/tmp, empty PATH and no consumer network access. **12/12 checks pass**:
pinned native SDK install/verify; C and C++ source-only new, installed-SDK first
build without configure, test/run, zero-command warm build and unchanged exact
project locks; unchanged installed SDK and all download inventories.
The stored-policy selector is `--sdk`; the first guide dry-run's legacy
`--sdk-id` refused a missing explicit contract without binding the project.
The corrected guide at commit `13320f8` passes; no SDK or CLI product change was
required. Failed dry-run evidence is retained separately.

Receipts stay under `/home/syszone/GitLocal/crt-preview-20261110/promotion-20261110/`:
`draft-verification.json`, `public-verification.json`,
`native-public-consumer-r2.json` and `promotion.json`. Promotion receipt SHA-256:
`dd2ed35ce279a1d7b9117890dc1a8ab8095c2e7b425633d0b4ad35127aee055f`. It binds the public files, source/tag/trust-review
commit, explicit CA, exact CLI and every consumer command. Manifest remains
`d4211d3298a764ff22f6e262073f834bbb36cb5d07c22e1ecb10f7f83f8fca60`, SDK content
`36623957fbdd5a384cc173991e20253ff83148028ab8072298dcd434a55a2b3f`, portable policy
`90b3bb59aaac7df481becbad4e2b87473e2fefcb803123332689d7f1135e4e30`.

This closes the **producer publication/manual-trust/promotion** part of 7.3.
The [CLI ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#published-linux-dev-sdk-adoption-2026-11-10)
records the exercised public-byte combination. CLI binary publication, automatic
GitHub HTTPS acquisition/redirect policy and production/default-lock adoption
remain separate. Publisher authentication is still `not_verified`; a new signer
or automatic channel is not invented. Existing Full/other-host qualifications
retain their original scope; no new runtime/ABI or format change is introduced.


## macOS Dev Preview candidate (2026-11-10)

**Clean local producer candidate, unpromoted.** Select CRT `445f127dc2e1ef4ab538166aa2e374f25773a803`,
version `v0.0.1-preview.20261110.macos.1`, native macOS/arm64 (macOS 27.2, build 26B123), Release, Apple clang 21.0.0 with
Xcode `ld` (ld-27106.1). The SDK keeps Dev profile 1, manifest stage `02-cxx`, native policy 1 and the existing recipe formats;
producer and compile provenance are both known and clean. No source, public header, runtime or ABI change was made for this
candidate; the Linux Preview's assets and pins are untouched.

A detached worktree at the commit and a fresh Ninja output directory were configured with explicit `CRT_TARGET_OS=macos`,
`CRT_TARGET_ARCH=aarch64`, `CRT_USE_IMPORTED_LIBCXX=ON`, `CMAKE_BUILD_TYPE=Release` and the version above; Skia, FFmpeg, media curl and
LVGL are off, and Dev's own network/JSON/xz ports build through their normal path. `cmake --build <fresh-build> --target crt-c-test
crt-cxx-test crt-dev-release crt-host-tools-test --parallel 8` exits zero. C tests pass **146/146** (the Linux count is 147; one
test is host-conditional); the imported-libc++ runtime, string-ABI and iostream smokes pass static and shared. Dev's static/shared
HTTPS+inflate and trust negatives pass, followed by actual archive extraction/profile verification and the portable policy export.

| Collected asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev.tar.xz` | 6136776 | `41a1afe701514496a912de83e46481455c9f2c0526d2eadd5b058813c26b2146` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-aarch64-dev-toolchain.json` | 402864 | `d5a100fdd0d38f940fe1c398996f013d57ccc7c4743af2b9ab4988c42b8de464` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-02-cxx-source.tar.xz` | 9005764 | `8a1beeaf48f9e9f25af60ed50f067754b4f6b674b8b43589b4c1f4e7c355fc3a` |
| `crt-v0.0.1-preview.20261110.macos.1-macos-03-gfx-simple-source.tar.xz` | 134120 | `e2d4fc76ca3c5ff378e883ea29565a2a5ba3a5b70d6dc9899c23724fbbf02378` |

The release-manifest SHA-256 is `0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de` (1728 bytes); the archive's install
metadata (format 1) carries SDK content identity `e3dc7fb1810a3f78ab8ef33882225ccb562d21f6f5977a2037ab92819326bac8`, and the portable policy
names the OS SDK kind `macos-xcode-sdk` without a path. All four declared sizes and digests match the copied files and the original
`SHA256SUMS-dev-macos-aarch64`.

### Consumer qualification before publication

A `crt-cli` built from its sources (`1e821d1` plus the move of the private `store_lock.h` into `include/crtcli/`) by this SDK alone, with the
previously committed macOS executable as the bootstrap seed, passes the producer's Seatbelt contract replay: **21 sources**, cold 21
executed / 0 reused, unchanged rebuild 0 / 21, bootstrap loop (code and data identical, only `LC_UUID` and `__LINKEDIT` differ), native
**922/922**, archive **221/221**, SDK-store **114/114** and recipe **141/141**, SDK-hidden help/version/doctor, and only `libSystem` imported.
Executable SHA-256 `558c13c15ffdf0394fb66e85d3a11c4f9c21c6606aae0e3f39aade353e3c7e53` (4368360 bytes). The replay needed two driver corrections that
concern its own guards, not the product: it scanned a JSON fixture's documented `_WIN32` as code, and it rejected the private lock helper by
location, which the repository's header rule (headers live under `include/`) resolves by moving it. Failed attempts are retained.

The same executable then consumes the **local handoff** (the unchanged collector outputs) under Seatbelt with the pre-publication pins:
`tools/replay_preview_public_macos.py` passes **30/30** (pin and asset digests, pinned install and verify, idempotent reinstall, wrong-pin
refusal with nothing published, C and C++ source-only `new`, first build without configure through the installed SDK, test, run, zero-command
warm build, unchanged project locks and downloads). Public-byte results are recorded after publication.

Raw outputs are under the CRT checkout's ignored `out/preview-macos/` and the producer worktree's `out/preview-macos-20261110/`. Not covered:
Full or other-host packages, a published CLI executable, Developer ID signing or notarization, the unqualified pure C shared SDK-provider
mode, redirect-aware GitHub acquisition, and the macOS independent host-tools bundle (local-only).


## macOS Dev SDK public promotion (2026-11-10)

Review decision: promote the unchanged qualified `v0.0.1-preview.20261110.macos.1` macOS/arm64 Dev artifacts from clean source
`445f127dc2e1ef4ab538166aa2e374f25773a803` under the [bounded manual-pin trust policy](../design/dev_preview_trust.md#macosarm64-bounded-policy-adopted-2026-11-10).
The owner authorized the GitHub prerelease promotion. The [release notes](../releases/release_notes_v0.0.1-preview.20261110.macos.1.md), the policy and the guide
section were frozen in the reviewed commit `0d7632d` before publication, so the manifest pin
`0981110c84e5d24a903dbd3965dbe92eb2fe405edfa89804e3086dfeb75983de` precedes the bytes it vouches for. No asset was rebuilt or replaced, no
publisher signature or default channel is claimed, and the Linux Preview is untouched.

### Published result and consumer proof

**Promoted:** [v0.0.1-preview.20261110.macos.1](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.macos.1), public prerelease (not marked latest) published
`2026-11-10T01:29:10Z`. The source tag resolves to `445f127dc2e1ef4ab538166aa2e374f25773a803`, the clean build commit, which is an ancestor of the reviewed pin
commit on `main`. Six assets were uploaded from the unchanged collector output through `gh` into a **draft**: the SDK archive, the portable compiler policy, the
02 and 03 source assets, the release manifest and `SHA256SUMS-dev-macos-aarch64`. Before publishing, the draft's asset names, sizes and
`sha256` digests matched the frozen pins and an authenticated re-download verified against `SHA256SUMS`; the downloaded manifest was byte-identical to the collector's.

An additional **unauthenticated** public download of the six files (`curl`, HTTPS only, TLS >= 1.2, proxies disabled) followed exactly one redirect per file,
to `release-assets.githubusercontent.com` (the host was checked before following and temporary signed URLs are not recorded), and verifies **6/6** against the
pins: all four assets against `SHA256SUMS`, the manifest `0981110c...83de` (1728 bytes) and the checksum file itself.

The exact CLI `558c13c15ffdf0394fb66e85d3a11c4f9c21c6606aae0e3f39aade353e3c7e53` (4368360 bytes; `bin/macos/crt-cli` of
`crt-cli` `dbb0913`) consumes those actual public-download bytes under Seatbelt: no network, writes confined to a scratch HOME and a private temporary directory, the
producer, CLI and worktree checkouts unreadable, a cleared environment with an empty `PATH`, Xcode's `clang`, `clang++`, `ld` and `ar` as the only toolchain.
`tools/replay_preview_public_macos.py` passes **30/30**: pin and asset digests, pinned native install and verify, idempotent reinstall, refusal of a wrong pin with
nothing published, C and C++ source-only `new`, first build **without configure** through the installed SDK selected by `--sdk`, test, run, zero-command warm build,
unchanged project locks (each selects SDK `e3dc7fb1810a3f78ab8ef33882225ccb562d21f6f5977a2037ab92819326bac8`, stored policy `d5a100fdd0d38f940fe1c398996f013d57ccc7c4743af2b9ab4988c42b8de464`, clean
producer provenance), an unchanged installed SDK and unchanged downloads. The result file's SHA-256 is
`8eaee9b5b76659ff0f6fd208d2effa600856cd2c256d1e8b2f327746e7c40f13`.

Receipts stay under the CRT checkout's ignored `out/preview-macos/`: `draft-dl/`, `public-dl/` and `public-consumer/` (`result.json`, per-call output). The
guide commands use the same flags as the proof.

### Limits

Current CLI follows no HTTPS redirects, so the accepted path is manual download plus pinned local installation; GitHub `sdk acquire --source` is not advertised. The
images are not Developer ID signed or notarized. Full, other-host and independent host-tools packages, a published CLI executable, the macOS port/provider replay against
this exact SDK, the unqualified pure C shared SDK-provider mode, updates and Studio remain in their owning queues. STATUS and default locks are unchanged.

## Windows Dev Preview candidate (2026-11-10)

**Clean local producer candidate, unpromoted.** Select CRT `b31276a6acccf0a89731f7bb0aecbdcdc9b45cda`, version
`v0.0.1-preview.20261110.windows.1`, native Windows/x86_64 (Windows 11 Pro 10.0.26100), Release, LLVM/Clang/LLD 22.1.8, Windows SDK
`10.0.28000.0` import libraries. The SDK keeps Dev profile 1, manifest stage `02-cxx`, native policy 1 and the existing recipe formats; producer
and compile provenance are both known and clean. No source, public header, runtime or ABI change was made for this candidate; the Linux and macOS
Previews' assets and pins are untouched. The Win64 callee-saved vector-register unwind fix (`05b5678`) and the Windows D3 adapter are ancestors of the build commit.

A detached worktree at the commit and a fresh Ninja output directory were configured from preset `windows-host-ninja-debug` with explicit
`CMAKE_BUILD_TYPE=Release`, `CRT_TARGET_OS=windows`, `CRT_TARGET_ARCH=x86_64`, `CRT_USE_IMPORTED_LIBCXX=ON` and the version above; Skia, FFmpeg,
media curl and LVGL are off, and Dev's own network/JSON/xz ports build through their normal path. No network access was needed: the verified-on-use
download caches (cJSON, curl, nlohmann json, mbedTLS, make, xz, zlib sources) and the three pinned LLVM source clones were copied from an earlier
build and are re-checked against their pins on use.
`cmake --build <fresh-build> --target crt-development-sdk-test crt-c-test crt-cxx-test crt-dev-release crt-host-tools-test --parallel 8` exits
zero. CTest passes **169/169**; the imported-libc++ runtime, string-ABI, iostream and exception-register smokes pass static and shared, and Dev's
static/shared HTTPS+inflate and trust negatives pass, followed by actual archive extraction/profile verification and the portable policy export.

| Collected asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev.zip` | 73323880 | `c71bb91e9f2c623d44e8154c7798a446b850781bdc1cf515eb9675dc04137be1` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-x86_64-dev-toolchain.json` | 440297 | `7762f572b4614b3068e1ea99694ce19360e213d31c5bd01019e4ad6ce005b726` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-02-cxx-source.tar.xz` | 9115212 | `ef301c5e3cc084766ede7a1372c89071f0a3597863a549f6a2dff9610d0e83ab` |
| `crt-v0.0.1-preview.20261110.windows.1-windows-03-gfx-simple-source.tar.xz` | 133720 | `cf3ba715cf764eb374199ded7da0ee0c61277c1d414f0d0cdb538a5246fd660c` |

The release-manifest SHA-256 is `b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976` (1800 bytes); `SHA256SUMS-dev-windows-x86_64`
(542 bytes) is `08e49d83a8ca8b32809c4b411de70c78bc1a02c6b8299231bfcad0bb284c0b6e`. The archive's install metadata (format 1: 2917 entries, 2736 files,
181 directories, **0 symlinks**, 184989818 bytes) carries SDK content identity `5bb55372ec10e3cfa24c84de981a7130f56aeb57ef0d43d5da22a65036dec7ef`, and the
portable policy names the OS SDK kind `windows-um-import-libraries` without a path. All four declared sizes and digests match the copied files. The
unchanged collector output is kept as an immutable copy outside both repositories.

### Consumer qualification before publication

A `crt-cli` built from `crt-cli` `9a5272d12d80a74389edbc724fd4bd602d618064` by this SDK alone passes the producer's restricted-token contract replay: **21 sources**,
cold 21 executed / 0 reused, unchanged rebuild 0 / 21, native **972/972**, archive **214/214**, SDK-store **113/113** and recipe **141/141**, SDK-hidden
help/version/doctor, and only `kernel32.dll` and `api-ms-win-core-synch-l1-2-0.dll` imported; 709 recorded processes. Executable SHA-256
`3e8c0491d2e385bbaba5c0e8bd63eeac3896f498fed762ecab4370ecf277bd90` (44920832 bytes).

The same executable then consumes the **local handoff** (the unchanged collector outputs) with the pre-publication pins:
`tools/replay_preview_public_windows.py` passes **31/31** (hidden producer and CLI trees unreadable under the run SID, pin and asset digests, pinned install and
verify, idempotent reinstall, wrong-pin refusal with nothing published, C and C++ source-only `new`, first build without configure through the installed SDK,
test, run, zero-command warm build, unchanged project locks, installed SDK and downloads). The documented guide commands (with `--os-sdk`) were also run as written
and pass. Isolation is a restricted token with an empty `PATH` and job-object process accounting: no network or executable denial is claimed. Public-byte results are
recorded after publication.

Raw outputs are under the producer worktree's ignored `out/preview-windows-20261110/` and the local `s9` evidence directory. Not covered: Full or other-host packages
(including Windows/arm64), a published CLI executable, Authenticode signing, the unqualified pure C shared SDK-provider mode, redirect-aware GitHub acquisition, and the Windows
independent host-tools bundle (local-only).

## Windows Dev SDK public promotion (2026-11-10)

Review decision: promote the unchanged qualified `v0.0.1-preview.20261110.windows.1` Windows/x86_64 Dev artifacts from clean source
`b31276a6acccf0a89731f7bb0aecbdcdc9b45cda` under the [bounded manual-pin trust policy](../design/dev_preview_trust.md#windowsx86_64-bounded-policy-adopted-2026-11-10).
The owner requested the GitHub prerelease promotion. The [release notes](../releases/release_notes_v0.0.1-preview.20261110.windows.1.md), the policy, the guide section
and the candidate record were frozen in the reviewed commit `a9c813b` (on `main` as part of `2117b1b`) before publication, so the manifest pin
`b0fca4c81d29b8eee28e0d9fb7d6e26950b094a8be4f2c091e43922366a46976` precedes the bytes it vouches for. No asset was rebuilt or replaced, no publisher signature or default
channel is claimed, and the Linux and macOS Previews are untouched.

### Published result and consumer proof

**Promoted:** [v0.0.1-preview.20261110.windows.1](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.windows.1), public prerelease (not marked latest) published
`2026-11-10T02:38:15Z`. The lightweight source tag resolves to `b31276a6acccf0a89731f7bb0aecbdcdc9b45cda`, the clean build commit, which is an ancestor of the reviewed pin commit on
`main`. Six assets were uploaded with `gh` from the unchanged collector output into a **draft**: the SDK archive, the portable compiler policy, the 02 and 03 source assets, the
release manifest and `SHA256SUMS-dev-windows-x86_64`. Before publishing, the draft's asset names, sizes and `sha256` digests matched the frozen pins and an authenticated
re-download was byte-identical to the collector's files (6/6).

An additional **unauthenticated** public download of the six files (Python `urllib`, HTTPS only, TLS >= 1.2 with peer and hostname verification against the Windows certificate store,
proxies disabled, no credentials) followed exactly one redirect per file, to `release-assets.githubusercontent.com` (the host was checked before following and temporary signed URLs
are not recorded), and verifies **6/6** against the pins: the four assets, the manifest `b0fca4c8...6976` (1800 bytes) and the checksum file itself. Receipt SHA-256
`91efd072ea7ff93fa99235789f1d4b9ee84469d3d6ed4786c3f45ce14bcf9ca9`.

The exact CLI `3e8c0491d2e385bbaba5c0e8bd63eeac3896f498fed762ecab4370ecf277bd90` (44920832 bytes; `bin/win-x64/crt-cli.exe` of `crt-cli` `f0537ef`) consumes those actual public-download
bytes under a restricted token: a per-run SID with the producer, CLI and CRT checkouts proven unreadable by a native probe, a private work directory and temporary directory, an empty
`PATH`, and job-object process accounting; LLVM 22.1.8 `clang`, `clang++`, `ld.lld` and `llvm-ar` are the only toolchain, with the Windows SDK `10.0.28000.0` import libraries.
`tools/replay_preview_public_windows.py` passes **31/31**: pin and asset digests, pinned native install and verify, idempotent reinstall, refusal of a wrong pin with nothing published, C and
C++ source-only `new`, first build **without configure** through the installed SDK selected by `--sdk`, test, run, zero-command warm build, unchanged project locks (each selects SDK
`5bb55372ec10e3cfa24c84de981a7130f56aeb57ef0d43d5da22a65036dec7ef`, stored policy `7762f572b4614b3068e1ea99694ce19360e213d31c5bd01019e4ad6ce005b726`, clean producer provenance), an unchanged
installed SDK and unchanged downloads. The result file's SHA-256 is `5784f28a815be4edb4a881fdabfff63d37885441ba802487d7398f47c6acfd20`.

Receipts stay in the local evidence directory `s9` next to the producer worktree's ignored `out/preview-windows-20261110/`: `draft-dl/`, `public-dl/`, `public-dl-receipt.json` and
`public-consumer/` (`result.json`, per-call output). The guide commands use the same flags as the proof.

### Limits

Current CLI follows no HTTPS redirects, so the accepted path is manual download plus pinned local installation; GitHub `sdk acquire --source` is not advertised. Windows process accounting is
not executable or network denial. The images are not Authenticode-signed. `SHA256SUMS-dev-windows-x86_64` has Windows (CRLF) line endings as produced, so a POSIX `sha256sum -c` needs them
stripped; the manifest is the pinned anchor. Full, other-host (including Windows/arm64) and independent host-tools packages, a published CLI executable, the Windows port/provider replay against
this exact SDK, the unqualified pure C shared SDK-provider mode, updates and Studio remain in their owning queues. STATUS and default locks are unchanged.

## Windows CLI Preview publication handoff (2026-11-10)

The consumer published [`v0.1.0-preview.20261110.windows-x86_64.1`](https://github.com/webos21/crt-cli/releases/tag/v0.1.0-preview.20261110.windows-x86_64.1) from clean CLI product/test source `a55c0d0`;
the consumer documentation/driver checkpoint is `0cae8c8bc1fee02a31baf230aba703460fe1afa3`. Reviewed CRT baseline
`8e89ee1` and the public Windows Dev SDK `v0.0.1-preview.20261110.windows.1` from
`b31276a` are unchanged. No producer rebuild, SDK promotion, API/ABI or shared format
change is needed. The original SDK publication and manual-install evidence above retain
their exact bytes and scope.

[CLI-owned release acceptance](https://github.com/webos21/crt-cli/blob/0cae8c8bc1fee02a31baf230aba703460fe1afa3/docs/acceptance/preview_acceptance.md#windowsx64-cli-preview-release-2026-11-10) records the fresh public-SDK-only
restricted-token build/self-build/native regression, Windows redirect replay, deterministic
package/SDK-free checks, real GitHub acquisition and C/C++ workflow, publication and
unauthenticated public-byte verification. The CLI's compatibility ledger owns adoption;
this producer handoff links that evidence without duplicating its pin/count ledger.

Windows now has the same explicit GitHub redirect-aware acquisition path as Linux/macOS,
while the default refuses redirects. Caller-selected CA/digest pins provide integrity;
publisher authentication remains `not_verified`, with no Authenticode or default trust
channel. Windows process accounting is not network/executable denial. Full profiles,
Windows/arm64, source-port replay from this exact CLI package and updates/self-upgrade
remain separate. The owner explicitly requested README/STATUS consistency in both repos;
the snapshot refresh does not relabel unrelated runtime/device acceptance.

## Linux loader compatibility repair (2026-11-10)

**Promoted:** [v0.0.1-preview.20261110.4](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.4), public non-latest prerelease. Owner scope: repair Linux Dev
SDK compatibility and publish new immutable `v0.0.1-preview.20261110.4`. Review baselines:
CRT `a339924`, CLI `65b20c4`; selected clean producer
`448fa32349ae697f589f0ad8fc60cfdf7a7e195a`. CRT owns TLS/allocator/link policy and source assets;
CLI owns native installation/build/testing and its later adoption ledger. SDK
manifest/profile/native-policy and recipe formats are unchanged. Producer
promotion precedes any consumer CI pin update.

The same original `.3` consumer fails with `calloc@GLIBC_2.2.5` under Ubuntu's
`2.39-0ubuntu8.8` loader and passes under `2.43-2ubuntu2.4`. The former forced
`DT_NEEDED` on ld.so activates glibc 2.39's GNU allocator/mutex handover.
[Upstream's later guard](https://sourceware.org/pipermail/glibc-cvs/2025q1/087042.html)
explains why newer loaders skip it without GNU libc. This is pre-main symbol
resolution, not ENOMEM. [Owned policy](../design/cxx_runtime.md#linux-shared-runtime-loader-imports)
uses TLSDESC, preserves `CRT_1.0`, removes the stub and invalidates old runtime
caches. No upstream source or pin changes. The existing Linux libc++abi weak
GNU destructor probe also captured CRT-thread cleanup when Wayland loaded GNU
libc; the failure reproduces with `.3`. The owned standalone CMake adapter
keeps upstream's pthread-key fallback in CRT's thread domain.

Fresh Release output is `out/preview-linux-20261110-r8`; raw verified source
fetches are reused, target outputs/source assets are rebuilt. The collector ran
at the clean producer above. C 147/147, C++ static/shared runtime/string/iostream,
new integrated descriptor/exception/thread/destructor regression, host-tools and
Dev static/shared HTTPS+inflate with CA negatives pass. External relocated Dev
checks hide producer/SDK/CLI inputs. The same regression ELF passes both exact
loaders (old loader SHA-256 `cd4df4f3c7b83673d61189bf2eaebd33ca4f2853ab9772b8a25e025ef99b1e81`,
new loader `bda779ea9e9ad234f60315477d7cfd0bdde861cd8ddd9df8cdaf62f18c4cec14`),
including a separate host GNU-libc/Wayland coexistence run. ELF checks reject
loader dependencies, GD imports and accidental host destructor registration;
unrelated unresolved imports still fail. Source/policy checks pass 7/7: owned
files match source, nested 03 recipe matches the actual common source archive,
all native modes capture descriptor flags, and no dummy loader DSO remains.

The old runtime comes from official `ubuntu-base-24.04.5-base-amd64.tar.gz`,
SHA-256 `e77b6f10c2590cef872b33ee9f635a0e3fd1f57fb074c0e52b5c7f56147a0c86`.
Local loader substitution is not a full Ubuntu 24.04 hosted-suite replay.
No native aarch64/other-OS, Full/GFX/Web or arbitrary glibc range is qualified.
Old GD objects need rebuilding; fallback destructor-order/DSO-unload and pure-C
shared SDK mode limits remain. No existing project lock is migrated.

[Reviewed release notes](../releases/release_notes_v0.0.1-preview.20261110.4.md)
freeze manifest `983dbcf143b1fe227bf519f07ec153ce5baf50e190382222995770affe43b2c3`, SDK content `22107183e5127d44e979f328fa78aaf2a436db802aa9f8821a864a39d492ca81`
and the six original collector files. Evidence stays under
`/home/syszone/GitLocal/crt-preview-20261110-tlsdesc/` and the fresh build output;
`loader-regression/result.json`, `host-glibc239-result.json`,
`source-policy-review.json` and `external-validation.json/` retain technical
results. Earlier incomplete candidates remain unpromoted and excluded.
Final CLI/public/hosted receipts follow here after validation.

### Local CLI candidate result

CLI `65b20c4` is exported outside both repositories and bootstrapped by the
unchanged published Linux CLI `v0.1.0-preview.20261110.linux-x86_64.1`.
Pinned native install of the copied collector outputs and stage-1 self-build
pass (21 executed/0 reused); native **913**, archive **221**, SDK-store **114**
and port-recipe **142** checks pass with no skips. Warm build is 0/21;
ELF import audit and SDK-free version/help/bootstrap-doctor pass. The local
wrapper initially misread `Ci.run_tests`'s empty failure dictionary as a tuple
after all four suites had passed. Preserve that harness failure; the corrected
postcheck validates the recorded suite exits, warm reuse and standalone checks
without rerunning or relabeling product failures. Final local receipt:
`consumer-postcheck/artifact/ci-summary.json`, CLI SHA-256
`80dddf03f746e6b50caea596ff0201983cf180cfb8d15abea870c2c019df117a`.
The public-byte consumer and actual Ubuntu 24.04 hosted suite are still pending.

### Public promotion and downloaded-byte proof

The source tag resolves to clean `448fa32349ae697f589f0ad8fc60cfdf7a7e195a`.
Reviewed pins precede publication (`2793fc0`, expanded local acceptance
`e6a94ab`). The draft contains exactly the six unchanged collector files;
authenticated re-download matches every name/size/digest before publication.
Unauthenticated verified-TLS public downloads then match **6/6** frozen files,
and four public source files match the producer commit. `draft-verification.json`
and `public-verification.json` retain checks and CA/redirect policy.

The unchanged published Linux CLI, executable SHA-256
`a8fb76a4b257c52345d79560312e0a94f52aaa9251f67524823a0fab8476a43a`, consumes
those public bytes under bubblewrap: read-only root, hidden producer/CLI/local
SDK trees, no network, empty PATH, private HOME/tmp. **14/14** checks pass:
native pinned install/verify, C/C++ new and first build/test/run without
configure, zero-command warm reuse and unchanged exact project locks, SDK and
download inventories. An additional C++ shared-library project passes on the
host 2.43 loader; its identical `greeting-test` also passes the pinned 2.39
loader. This is an actual public SDK/native policy replay, not just wrappers.
Receipt: `native-public-consumer.json`; no existing project is silently updated.

CLI CI adopts these producer-promoted pins in `5d148ba`, retaining the old
failure and adding required Ubuntu 24.04 beside Ubuntu 26.04. The
[CLI ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#linux-loader-repair-sdk-adoption-2026-11-10)
owns completed hosted results. `.3` remains available with a compatibility
erratum; macOS/Windows assets, Full delivery, publisher signing, default/project
locks and STATUS are unchanged. Linux producer CI now provisions LLVM 21 and
compiler-rt explicitly; earlier CI provisioning failures are not SDK/runtime
failures or passing compatibility evidence.

### Required hosted replay closure

The same published `.4` SDK passes both required native Ubuntu 24.04/glibc
`2.39-0ubuntu8.9` and Ubuntu 26.04/glibc `2.43-2ubuntu2.4` CLI jobs in
[run `42805951580`](https://github.com/webos21/crt-cli/actions/runs/42805951580),
consumer `5d148ba`. This includes the formerly failing shared-C++ variant,
both self-build stages, four full suites, unchanged rebuild, stage byte equality
and SDK-free/import gates. The [CLI record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/ci_acceptance.md#completed-linux-hosted-qualification)
owns exact consumer binary/compiler/test/artifact identities; it is not just
local interpreter substitution. The producer defect and this release item close
for those combinations; prior `.3` evidence is retained and wider support is not inferred.

Producer entry-point [CI `42805529392`](https://github.com/webos21/crt/actions/runs/42805529392)
at `422b45a` also passes all five existing core-platform legs (Linux amd64/arm64,
macOS aarch64 and Windows x64/arm64). Relative to the artifact's source `448fa32`,
only CI provisioning/release documentation changed. The immutable source-tag CI
still used its former unversioned LLVM provisioning and failed on x86's older
Clang; main's explicit LLVM 21/compiler-rt provisioning resolves that CI issue.
It is neither a runtime failure nor new native aarch64/other-host Dev SDK qualification.

## Linux/aarch64 Dev Preview candidate (2026-11-10)

**Clean local producer candidate, unpromoted.** Select CRT `e6a94ab9e69b05497b3804328971d24d6071a53d`, version `v0.0.1-preview.20261110.linux.arm64.2`, native
Linux/aarch64 (Ubuntu 26.04.1, Linux 7.0.0-43, glibc 2.43, 4 CPU QEMU guest), Release, Ubuntu LLVM 21.1.8
(`clang`, `ld.lld`, `llvm-ar-21`). The SDK keeps Dev profile 1, manifest stage `02-cxx`, native policy 1 and the existing
recipe formats; producer and compile provenance are both known and clean. This candidate includes the Linux loader/TLS
repair (`d33678b`, `cddade2`, `448fa32`): the first aarch64 candidate `.1` was built from `a339924` before it and is excluded
and never published. The two later commits (`422b45a`, `ce99845`) change only CI configuration and documentation.

A detached worktree and a fresh Ninja output were configured with explicit `CRT_TARGET_OS=linux`, `CRT_TARGET_ARCH=aarch64`,
`CRT_USE_IMPORTED_LIBCXX=ON`, `CMAKE_BUILD_TYPE=Release` and the version above; Skia, FFmpeg, media curl and LVGL are off.
`cmake --build <fresh-build> --target crt-c-test crt-cxx-test crt-dev-release crt-host-tools-test --parallel 3` exits zero.
C tests pass **147/147**; the imported-libc++ runtime, string-ABI, iostream and EH-register smokes pass static and shared; the
shared-loader regression (calloc/free, TLS descriptors in a real DSO and a second thread, thread-local destruction, a cross-DSO C++
exception, rejection of an unrelated missing import) passes. AArch64 already generates TLS descriptors by default, so the x86_64-only
`-mtls-dialect=gnu2` flag is not applied; the removal of the link-only loader import stub shrinks the SDK by one entry (2,796 vs 2,797
in the earlier candidate). Dev's static/shared HTTPS+inflate and trust negatives pass, followed by actual archive extraction/profile
verification (`verify_dist.py --stage 02-cxx --profile dev` on a path with spaces) and the portable policy export. The extracted SDK also
passes the external relocated validation with producer and SDK trees hidden.

| Collected asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-aarch64-dev.tar.xz` | 7632648 | `5c99dc59b747cb31cb623fa057264c3a4e7d5715f960203073dc1b34f664a1c4` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-aarch64-dev-toolchain.json` | 403294 | `a3df2fbd6b4d14d316b691a1d2831d0692aaba133e3b168076ee0ec7cb6434d4` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-02-cxx-source.tar.xz` | 9114820 | `39f7de2ace7154fde45b187fafeae9d10f8d7772f70ecab438d7591f1c29ccbe` |
| `crt-v0.0.1-preview.20261110.linux.arm64.2-linux-03-gfx-simple-source.tar.xz` | 838332 | `1fec38dbc1ff2e7fa886723b1e9327ebd55b65fffd714b1d2b3d9c619a3b746c` |

The release-manifest SHA-256 is `ba77b26edfa0413df5e8c17309506d7ae6ecf134ae7e9908e3742bc562067be3` (1772 bytes); the archive's install
metadata (format 1) is `tar.xz`, root `dev`, 2,796 entries (2,601 files, 178 directories, 17 symlinks), 112,632,062 bytes, SDK content
identity `cd2ab657615c0fa52a1804f301a57dfa21b1067e3e5ed5a9f9a07d86ae050cc6`; the Linux portable policy has no OS SDK entry. `SHA256SUMS-dev-linux-aarch64`
(SHA-256 `0be55a0fb39d9b5fc351e849012fd1cf22d6b779be95580a19f5b3071eedb35a`) verifies all four assets, and all declared sizes and digests match the
copied files. The development contract records producer and base commit `e6a94ab` with `producer_working_tree_dirty: false`; both stage-source
assets carry that commit and `working_tree_dirty: false`.

### Consumer qualification before publication

The CLI's contract replay (`tools/replay_cli_contract_linux.py`) builds `crt-cli` `5d148ba` from this SDK alone with the host LLVM driven through the SDK's exported
toolchain policy (bubblewrap, no network, producer and CLI trees hidden, execve audit): cold self-build **21 executed / 0 reused**, unchanged rebuild 0 / 21, native
**913**, archive **221**, SDK-store **114** and port-recipe **142** checks with every suite exiting 0, SDK-hidden help/version/doctor, 32 distinct executed
paths (none forbidden), no `DT_NEEDED`, interpreter `/lib/ld-linux-aarch64.so.1`, and an unchanged SDK inventory. Executable SHA-256
`ae5f2b212be4351dcb8fc699785cf5c6822aa0b0dcd5183a23e94e56529ec543` (23,162,944 bytes). The replay driver's source guard scanned a JSON fixture's
documented `_WIN32` as code; it now checks compiled source suffixes only (the macOS driver carries the same guard and was not changed).

That executable consumes the **local handoff** (the unchanged collector outputs, kept outside both repositories) with the pre-publication pins:
`tools/replay_preview_public_linux.py` in `crt-cli` passes **30/30** (pin and asset digests, pinned install and verify, idempotent reinstall, wrong-pin refusal
with nothing published, C and C++ source-only `new`, first build without configure through the installed SDK, test, run, zero-command warm build, unchanged project
locks and downloads). Public-byte results are recorded after publication.

Not covered: the older glibc 2.39 loader on aarch64 (the x86_64 repair used an official Ubuntu 24.04 userland for it), a published Linux/aarch64 CLI executable,
Full or other-host packages, the independent host-tools bundle, the unqualified pure C shared SDK-provider mode and redirect-aware acquisition from this URL.

## Linux/aarch64 Dev SDK public promotion (2026-11-10)

Review decision: promote the unchanged qualified `v0.0.1-preview.20261110.linux.arm64.2` Linux/aarch64 Dev artifacts from clean source
`e6a94ab9e69b05497b3804328971d24d6071a53d` under the [bounded manual-pin trust policy](../design/dev_preview_trust.md#linuxaarch64-bounded-policy-adopted-2026-11-10).
The owner authorized the GitHub prerelease promotion. The [release notes](../releases/release_notes_v0.0.1-preview.20261110.linux.arm64.2.md), the policy and the guide
section were frozen in the reviewed commit `d5c54bb` before publication (merged as `ab72aeb`), so the manifest pin
`ba77b26edfa0413df5e8c17309506d7ae6ecf134ae7e9908e3742bc562067be3` precedes the bytes it vouches for. No asset was rebuilt or replaced, no
publisher signature or default channel is claimed, and the other Previews are untouched.

### Published result and consumer proof

**Promoted:** [v0.0.1-preview.20261110.linux.arm64.2](https://github.com/webos21/crt/releases/tag/v0.0.1-preview.20261110.linux.arm64.2), public prerelease (not marked latest) published
`2026-11-10T23:27:22Z`. The annotated source tag resolves to `e6a94ab9e69b05497b3804328971d24d6071a53d`, the clean build commit, which is an ancestor of the
reviewed pin commit on `main`. Six assets were uploaded from the unchanged collector output through `gh` into a **draft**: the SDK archive, the portable
compiler policy, the 02 and 03 source assets, the release manifest and `SHA256SUMS-dev-linux-aarch64`. Before publishing, the draft's asset names and sizes matched the
frozen pins and an authenticated re-download was byte-identical to the collector outputs and verified against `SHA256SUMS`.

An additional **unauthenticated** public download of the six files (`curl`, HTTPS only, TLS >= 1.2, proxies disabled, explicit CA file `/etc/ssl/certs/ca-certificates.crt`)
followed exactly one redirect per file to `release-assets.githubusercontent.com` (the host was checked before following and the temporary signed URLs are not recorded) and
verifies **6/6**: all four assets against `SHA256SUMS`, the manifest `ba77b26e...67be3` (1772 bytes) and the checksum file itself, and every file is byte-identical to the
handoff.

The exact CLI `ae5f2b212be4351dcb8fc699785cf5c6822aa0b0dcd5183a23e94e56529ec543` (23,162,944 bytes; built from `crt-cli` `5d148ba` by this SDK alone) consumes those
actual public-download bytes in bubblewrap (no network, read-only root, producer and CLI source trees hidden, cleared environment, empty `PATH`, the host LLVM 21.1.8 as the only
toolchain). `tools/replay_preview_public_linux.py` passes **30/30**: pin and asset digests, pinned native install and verify, idempotent reinstall, refusal of a wrong pin with
nothing published, C and C++ source-only `new`, first build **without configure** through the installed SDK selected by `--sdk`, test, run, zero-command warm build, unchanged
project locks (each selects SDK `cd2ab657615c0fa52a1804f301a57dfa21b1067e3e5ed5a9f9a07d86ae050cc6` with its stored policy), an unchanged installed SDK and unchanged downloads. The
result file's SHA-256 is `2c2495bba6002d7e5669de6974b2a73babe9ecd7fe1428b6d625f2213ae01974`.

Receipts stay under the CLI checkout's ignored `out/linux-arm64-preview-20261110/` (`external/`, `replay/`, `handoff-consumer/`, `public-consumer/`) and the handoff root
`/home/cmjo/preview-arm64-20261110/` (`handoff/`, `draft-dl/`, `public-dl/`). The guide commands use the same flags as the proof.

### Source-tag CI result and the configure-time guard

The push CI of the source tag ([run `42923428386`](https://github.com/webos21/crt/actions/runs/42923428386)) is red on **linux-arm64 only**; the other four legs pass. It is the
tag's own workflow at `e6a94ab`, which installed LLVM 21 with `--no-install-recommends` and therefore without the compiler-rt package (`libclang-rt-21-dev`, only a suggested
dependency). Linux/aarch64 long double is software binary128, so `printf` and `gdtoa` need compiler-rt's `__multf3`/`__netf2`/`__extenddftf2` family; with no archive the configure
still succeeded and `shell/sh` and `shell/mksh` failed to link much later. Linux/amd64 uses x87 long double natively and never needed the archive. `main`'s `422b45a` installs the
package, and `main` at `ab72aeb` (the same SDK sources) passes all five legs including linux-arm64 ([run `42923382106`](https://github.com/webos21/crt/actions/runs/42923382106)).
The tag is not moved, as with the `.4` tag run; a re-run would use the same old workflow. The published assets are the local qualified collector outputs, not CI products.

The root `CMakeLists.txt` now fails the configure on a **native Linux/aarch64 host** that finds no compiler-rt builtins, naming the package and
`CRT_COMPILER_RT_BUILTINS_OVERRIDE`; an x86_64-host cross configuration only warns, because it may legitimately be configured without being linked.
`tools/test_compiler_rt_configure.py` (native aarch64 hosts only) uses a compiler wrapper that finds no archive: the configure fails with the remedy, an explicit archive still configures,
and the former silent success makes the test fail.

### Limits

Qualification used glibc 2.43; the older glibc 2.39 loader was not exercised on this architecture. No Linux/aarch64 `crt-cli` executable is published (the exact one above is
not committed). Redirect-aware `sdk acquire` from this URL, Full, other-host and independent host-tools packages, the unqualified pure C shared SDK-provider mode, updates and Studio
remain in their owning queues. STATUS and default locks are unchanged.
