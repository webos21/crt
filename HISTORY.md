# HISTORY: CRT Resolved Work Log

Reverse-chronological record of completed work on the CRT shell/rootfs/porting
loop and PAL, one dated entry per resolved item (grouped by the commit(s) that
landed it). Current/open work lives in `TODO.md`. This file retains October
2026; earlier months are summarized in [docs/history](docs/history/README.md),
with a pinned Git source and recovery instructions for the complete original log. Dates are the git author date of the commit that introduced or last
substantively updated each entry, so an entry whose investigation spanned
multiple days is dated by its span (`start..resolved`) or by its last
substantive update.

## 2026-10-29

### Consolidate the native CLI producer contract and handoff documentation

Recorded the native JSON/LLVM app-build boundary, experimental producer exporter,
Linux regression fix and linked CLI-owned acceptance in README/TODO/design docs.
The runtime fix remains `64b447b`, the clean build baseline of the local Dev SDK;
this later exporter/documentation commit does not relabel that SDK or promote
an artifact. The CLI repository records its own implementation commit and
unchanged source-input comparison with the original 2026-10-28 receipts.
Other-host replay and final artifact/lock adoption remain open. Documentation
links, source/exporter hashes and recorded evidence were rechecked; no runtime
source changed in this consolidation. STATUS remains unchanged.

## 2026-10-28

### Native CLI compiler policy and Linux directory-type regression

The owner replaced CLI CMake/Python application builds with native `crt.json`
and internal compiler-driver modules, using `src/main/` and C/C++ `src/test/`.
CRT retains ABI/startup/link-policy ownership. The new
`tools/export_cli_toolchain.py` captures installed wrapper argv into a separately
hashed experimental Linux/x86_64 provider without modifying the immutable SDK.
This producer migration tool still uses Python/shell; CLI build/test/run and
control commands invoke native LLVM directly. Existing SDK consumers keep their
wrapper/CMake entry points; no CRT -> CLI build dependency is introduced.

Native SDK inventory exposed an existing Linux `readdir` defect: the PAL uses
getdents64 on both architectures, but read `d_type` from legacy record-tail
padding instead of byte 18. Following Bionic's getdents64 behavior, fix commit
`64b447ba40ef339a65ca009adeda80530a83689c` corrects the offset without public ABI
changes. The strengthened file-path regression fails against the older SDK and
passes after the fix. The CLI scanner is unchanged; no host/upstream workaround
or old SDK mutation is used.

A clean worktree at that commit builds local Dev SDK `v0.4.0-cli.20261028`
from source-only caches, with newly compiled core, imported C++ and TLS/network
ports. All 144 C tests and the static/shared Dev HTTPS+inflate/trust consumer
pass. Local producer logs live under
`/home/syszone/GitLocal/crt-cli-producer-20261028`; CLI source/SDK-hidden native
self-build, generated C/C++ workflows and 429 assertions pass under
`/home/syszone/GitLocal/crt-cli-replay/tranche1-native-linux-20261028-r2`.
The native CLI has zero ELF DT_NEEDED/RPATH and bare runtime maps contain only
itself and the declared OS loader. The CLI repository's bootstrap acceptance and
compatibility ledger own the exact hashes/commands, failed attempts and bounds.

This is an unpromoted Linux D0 experiment. Other-host replay, CLI HTTPS,
authenticated self-integrity, automatic SDK/store installation, self-upgrade and
stable/general SDK contracts remain open. Root consumer lock adoption and any
public SDK/CLI release are separate gates. STATUS is unchanged.

### Coordinate the first CRT-native CLI consumer increment

Applied the owner's selected CLI name, shared immutable SDK/project references,
first-build acquisition and one-executable/helper boundary. Tranche 1 records
the completed initial layout/input/fixture work and leaves macOS/Windows native
replay and coherent final artifact adoption open. The existing clean producer
Dev SDK feeds a separately identified CLI local bootstrap; no CRT runtime or
upstream source changed. CLI owns commands, formats, execution receipts and
adoption in its acceptance/compatibility records. Linux results are not copied
into a new producer ABI/release qualification claim, and STATUS is unchanged.

### Align the SDK/CLI bootstrap and lifecycle plan

Moved the full SDK -> CLI -> Studio initiative into TODO In Progress and
numbered its D0-D7 producer/joint tranches. The first tranche now reviews
product/initial-layout decisions before minimal CLI implementation, then SDK
handoff, identity fixtures, one-executable feasibility and native consumption.
Clarified local unreleased SDK experimentation and producer-first final pin
adoption without requiring simultaneous public releases.

Updated the producer contract from a default dynamic CLI bundle to a
single-executable goal requiring native proof; retained existing SDK evidence
at its original scope. CLI owns self-upgrade, scoped doctor, evolving templates
and per-project SDK storage/acquisition. Command prefix, precise file/helper/data
policy and storage/acquisition defaults remain open decisions. The CLI-owned
design/acceptance and compatibility record own details and future results;
this is planning/documentation work, not a CLI build, runtime pass or promotion.

### Close the release qualification and local assembly rehearsal

Completed the remaining Linux/x86_64-host -> Linux/aarch64 Full `06-web`
cross build from the same clean `3741ea5` and rehearsal version as the native
records. Fresh target core/C++, Skia, ports and standalone predecessors feed
the official isolated Web builder; host generators, target binaries/archives,
fontconfig proof, ordinary distribution verification and inherited-runtime
equality pass. Actual strict cross collection and space-path extraction also
pass, with the corresponding-source bundle identical to the three native Full
collections. Preparation failures and bounded procedural recoveries are retained;
no committed runtime or upstream source changed.

Reviewed the pulled macOS/Windows/native ARM evidence together with the original
Linux receipts and current focused tooling tests. Closed the per-host local
rehearsal in TODO and synchronized STATUS, root README and owning guide/acceptance
links. The imported Toybox README remains unchanged. Native ARM Dev and cross
Full execution/pairing limits remain explicit. Same-name Linux source-asset
variants require coherent selection/rebuild before future promotion under P0;
no tag, upload, artifact promotion or CLI adoption occurred. Exact commands,
artifact hashes, receipts and final scope:
[release qualification](docs/acceptance/release_qualification.md#final-combined-assembly-review-2026-10-28).

### Pass the native Linux/aarch64 Development assembly at the WebKit 2.54.2 pin

Ran the Development profile natively on the aarch64 guest from the same clean `3741ea5` (detached worktree) and version
parameter as the other hosts: `crt-c-test` 144/144, imported libc++ smokes, the Dev SDK consumer, real `crt-dev-release`,
extracted-archive equality (2,579 paths), 23 runtime digests equal to the plain `02-cxx`, clean source identities and the
source-hidden relocated external consumer. The Full `06-web` was not run: Linux/aarch64 is only a cross target of that stage
(x86_64 host), so that packaging and S5 stay open for an x86_64 host. Evidence and artifact hashes:
[release qualification](docs/acceptance/release_qualification.md#linuxaarch64-native-development-assembly-2026-10-28).

### Pass the Windows/x64 release assembly rehearsal at the WebKit 2.54.2 pin

Replayed the Linux rehearsal natively on Windows/x64 from the same clean `3741ea5`
(detached worktree) and version parameter, with a WebKit 2.54.2 source cache prepared
and validated on the host (archive hash and size, commit/tree/listing, 34,445 identical
files and zero content differences against the tarball). Core CTest 164/164, the
Development SDK consumer, both release collectors, the new-pin Web build with the
relocated packaged browser (141 mapped files, zero violations), HTTP/TLS 10/10 and the
Host ABI audit pass; a 37-check assembly audit confirms checksums, ZIP/inventory
equality (2,695 and 3,673 paths), 27 runtime digests, clean source identity and the
byte-identical 1,942-path corresponding-source bundle. S5 passes natively with eight
negatives and no promotion or adoption.

Two host preconditions surfaced and are recorded rather than bypassed: a work root
one character past the Win32 directory limit during the EGL-Registry unpack (root
shortened, rerun from configure), and the restricted-token S5 consumer needing a
producer tree that public Users grants cannot read (the one scratch worktree's ACL was
made private with the owner's approval; the test is unchanged). Evidence and hashes are
in [release qualification](docs/acceptance/release_qualification.md#windowsx64-positive-assembly-2026-10-28).
Linux/aarch64 cross packaging and the final combined review remain In Progress.

## 2026-10-27

### Pass the first real Linux Development/Full release assembly rehearsal

Froze clean `3741ea5` after the WebKit 2.54.2 source gate, selected the local
`v0.4.0-rehearsal.20261027` version parameter and built fresh Linux/x86_64
Development and cumulative Full `06-web` SDKs plus matching source assets.
Both actual CMake release collectors pass without a dirty/provenance bypass.
Extracted archives preserve every SDK file/link, and the paired SDKs share all
23 inherited runtime digests. The corresponding-source bundle matches the
baseline's 1,942 source paths, with only the five explicitly declared Git CRLF
conversions; the initial overly strict raw-blob audit and corrected evidence
are retained without changing any collected payload.

Core CTest 144/144, six imported-libc++ static/shared smokes, the new-pin Web
build/API/network/relocated browser replay, and the packaged source-hidden Dev
consumer with all eight S5 negatives pass. Source/release unit suites pass
164/168 with four conditional skips. The existing owner record permits the
fresh Web release decision once the micro pin is current. The run creates no
CRT tag, upload, artifact promotion or CLI adoption. Other-host new-pin replay,
Linux/aarch64 cross packaging and the final combined review remain In Progress.
Artifact hashes, exact commands, receipts and limits belong to
[release qualification](docs/acceptance/release_qualification.md#linuxx86_64-positive-assembly-2026-10-27).

### Prepare the WebKit 2.54.2 source pin for release qualification

Moved release qualification into `TODO.md` In Progress after the five-pair SDK
split closure. Replaced the stale WebKit micro pin with the official 2.54.2
archive and signature-verified annotated tag, regenerated complete source
correspondence and license inventories, and reviewed the seven changed files
covered by carried patches. The 23 patch definitions remain unchanged; refreshed
their 16 affected before/after hashes and verified all three host selections
with zero fuzz and exact output hashes. Focused fetch, licensing, JSC helper
and Web stage tests pass (81 passed, two conditional skips). Native new-pin
build/replay and actual release collection remain active work, not completed
release acceptance. Identities, receipts and local rehearsal scope are in
[release qualification](docs/acceptance/release_qualification.md).

### Complete the Windows/arm64 S5 replay and close the Full / Development SDK split

Produced the Development SDK and the Full `01-c`..`05-ui` SDKs on the Windows/arm64 VM from one clean commit (`3810c7c`; the earlier in-tree
SDKs predated `source_commit` in the `03`-`05` manifests and could not pair). The S5 driver passes natively: restricted-token isolation with
the checkout and `out/` denied, static and shared HTTPS+inflate and explicit-CA cases (eight), ten-image app-local closure with a System32-only PATH,
and eight negatives including a stale Full source identity; the collector's `check_pair` accepts the real pair, all 27 inherited runtime digests
match, and `handoff.json` records not-promoted / not release-qualified / CLI adoption not run. The Full reference is `05-ui`, because the
06-web stage is not supported on Windows/arm64. Evidence:
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#windowsarm64-clean-production-and-s5-replay-2026-10-27).

With this the S5 sub-gates of all five native pairs are checked and **the SDK split tranche (S0-S5) is complete**, so it left `TODO.md`
`In Progress`. Summary of what it delivered: the Full SDK (cumulative `01-c`..`06-web`) is unchanged; the Development SDK is `02-cxx` plus
zlib, Mbed TLS and libcurl (static and shared) with no graphics, media, UI or Web content, expressed as a `dev` profile (manifest stays
`02-cxx`, identity in `share/crt/contracts/development.json`), built by the shared media-stage port implementation (`tools/build_dev_sdk.py`),
verified by `verify_dist.py --profile dev`, and proven with a CLI-shaped C++17 consumer (HTTPS GET with explicit-CA verification plus zlib
inflate) outside the producer tree on each pair. The release collector (`crt_sdk_release.py`) exists with local fixtures; actual clean/tagged
collection is release qualification. CI cost stays bounded (one host smoke in CI, the full matrix at release time). Not goals: signing and
publisher authentication, a static CLI, and any change to the Full SDK stages.

### Complete the Windows/x64 clean Full/Development SDK build and S5 replay

Started from an absent `out/` and clean `b99ac74`; built C/C++, graphics,
Skia/media, UI and isolated Web with the default features enabled, plus the
Development SDK through its ordinary producer. Preserved failed attempts and
recovered the original frozen source/predecessor identities. New Full/Dev
packages pass relocation, payload comparison and native checks; all 27 inherited
runtime digests match. The restricted-token consumer proves producer/source
and descendant read denial before static/shared HTTPS/inflate/trust execution.

All eight S5 negatives and the local candidate handoff pass after normalizing
native diagnostic path separators; unrelated diagnostics still fail. The
existing CI tooling selection passes 148/152, four host-conditional skips, with
the new count/path tests in the same job. Marked only Windows/x64 S5 complete;
Windows/arm64 remains open. Build work and receipts stay under `out/`, except
the required source-hidden consumer copy. The security report keeps public
release blocked pending the WebKit stable micro pin update. Artifact hashes,
commands, preserved failures and source/tooling provenance limits belong to
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#windowsx64-clean-production-and-s5-replay-2026-10-27).

### Fix WebKit checkout file counting on deep Windows paths

Git's pinned WebKit checkout completed, but its Python file-count diagnostic
failed with `WinError 3` while traversing a deep directory. The count now uses
the existing extended-path filesystem helper, leaving Git/build-tool arguments,
pin checks and source selection unchanged. A 350+ character fixture reproduces
the failure before the fix and passes afterwards; it runs in the already
selected CI `ExecutedRecipeTests` class without a workflow change.

The repaired helper also verifies the actual failed checkout: 69,576 files,
the pinned commit/tree/listing and pristine state before and after. Existing CI
tooling passes 147/151 with four host-conditional skips. The frozen clean Web
source asset is preserved; the full build resumes through its normal verified
checkout-cache path, not a relabeled asset. Full/S5 completion is recorded
in [Windows clean replay](docs/acceptance/sdk_split_acceptance.md#windowsx64-clean-production-and-s5-replay-2026-10-27).

### Keep external-consumer failure fixtures independent of the TEMP location

Two mocked launcher-failure fixtures now supply their own synthetic producer
boundary instead of assuming the host temporary directory is outside the CRT
checkout. This preserves the failed-token/sandbox no-fallback and stale-receipt
assertions when ordinary test work is placed under `out/`; actual native
visibility/disjointness checks are unchanged. The existing CI tooling selection
passes 146/150 with four host-conditional skips using TEMP under `out/`.
The initial failures and successful rerun are retained with the
[Windows clean replay](docs/acceptance/sdk_split_acceptance.md#windowsx64-clean-production-and-s5-replay-2026-10-27),
which also records the completed Full/S5 replay.

### Add the S5 native SDK package/negative/handoff replay

Added one reproducible native S5 driver that verifies identified Dev/Full
inputs, requires matching target/runtime bytes, packages and extracts Dev,
checks payload identity and runs the existing producer-denying consumer. It
requires the intended verifier diagnostics for seven disposable Dev mutations
and rejects stale Full source identity through the unchanged collector pair
gate. Source/Full metadata is mutated only in private negative fixtures;
original SDKs remain unchanged. The local handoff receipt retains archive,
producer and consumer identities without promoting artifacts or approving CLI
adoption. The existing Linux/amd64 PR smoke now runs this gate.

Linux/x86_64 package, eight consumer cases and eight negatives pass using the
existing identified Dev/Full candidates; no new runtime or clean release claim
is made. Recorded Linux/aarch64 evidence already covers S5's required native
negatives. macOS/Windows stale-Full replay and candidate receipts remain in
TODO. Exact commands, identities, counts and limits belong to
[S5 acceptance](docs/acceptance/sdk_split_acceptance.md#s5-native-negative-and-candidate-handoff-gate).

### Fix Windows libc++ long-path source copying and close the fetch follow-up

Imported-runtime fetch now reuses the existing extended-path filesystem helper
for primary and shared sibling source copies. Each directory is fully copied
in a private sibling staging area before publication, so a failed copy cannot
leave a partial destination accepted as fetched. A missing shared CMake source
also triggers recovery even when the primary directory remains. Tool arguments,
work roots, pinned upstream bytes and Git/global Windows configuration are
unchanged; the isolated 02 source asset already carries the shared helper.

Preserved the known incomplete libc++ directory under an explicit failed-copy
name, recovered from the existing pinned clone and verified all five originally
missing files against upstream SHA-256. In the original deep CMake tree, with
`CRT_DEV_PREDECESSOR` empty, the normal C++ fetch/configure/build, static/shared
smokes (including string ABI and iostream) and verified `02-cxx` distribution
succeed. The earlier S3 Development SDK and its predecessor are preserved, not
relabeled as using this new runtime.

Focused fetch/predecessor/filesystem regression passes 26/26; the existing CI
tooling selection passes 143/147 with four host-conditional skips. Fixtures
exercise 350+ character paths, real sparse Git primary/sibling copies, failed
copy publication and missing-sibling recovery without network access. Removed
the resolved work from TODO rather than leaving another pending tranche. No
new runtime ABI, recipe pin, CI workflow, tag, upload or consumer lock changed.
Original failures, recovery commands and limits belong to
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#windows-libc-long-path-fetch-recovery-2026-10-27).

### Verify the Windows/x64 Development SDK with producer trees inaccessible

Added a native Windows restricted-token counterpart to the Linux/macOS external
consumer driver. Only new private test-work directories receive synthetic-SID
ACLs; producer/source/SDK permissions and accounts are untouched. A child probe
and fresh descendants prove source, headers, static libc and DLL reads fail
before SDK-only compilation. The inherited job bounds cleanup, processes remain
hidden and there is no unrestricted fallback. The fixture supplies a private
host Python and the bounded validation capsule, not another runtime/toolchain.

Fresh networking ports and current SDK tooling were assembled through the
official external-02-cxx CMake entry point, retaining the predecessor's source
identity. The package and its relocated/external consumers pass profile checks,
all eight static/shared HTTPS/inflate/trust cases and app-local DLL loading with
System32-only PATH. All 27 inherited runtime digests match; seven private-copy
verifier negatives reject their inputs. ZIP materialization of two declared zlib
file aliases is recorded separately from full payload-byte equality and each
tree's unchanged link-aware inventory; no archive behavior was changed.

The old local Dev SDK's missing required tool and a fresh default producer's
deep-path libc++ fetch-copy failure were preserved, not relabeled successes.
The latter is a separate Planned defect; no short-root workaround or upstream
patch was used. Marked only Windows/x64 S3 complete; Windows/aarch64 still needs
native replay. The focused unit suite passes 28/28 and the exact CI tooling
selection passes 136/140 with four existing host-conditional skips. No runtime
ABI/schema, CI workflow, STATUS or consumer lock changed, and no release was
published. Commands, artifact identities, probe receipts and remaining limits
belong to [SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#windowsx64-source-hidden-and-package-replay-2026-10-27).

### Complete macOS Development SDK isolated-consumer verification

Regenerated the native macOS/arm64 Development SDK through the ordinary CMake
entry point after fresh configuration. The package verifies, retains all 18
predecessor runtime digests and extracts with an identical 2,570-entry inventory.
Static/shared SDK-only HTTPS/inflate consumers pass all eight trust cases with
the producer checkout, build tree and original SDK inaccessible under an
inherited macOS sandbox policy. The external driver retains Linux bubblewrap
and fails closed when isolation is unavailable.

Extended the existing Mach-O audit to external consumers; the shared executable
has no static allocator and binds CRT-owned references to CRT libc. An isolated
dyld trace confirms all ten SDK images load from the relocated package. Seven
private-copy profile negatives and the focused unit regressions pass. No runtime
ABI or packaging schema changed. Marked the macOS S3 sub-gate complete, keeping
Windows producer-tree-hidden replay and clean release qualification open.
Commands, package identity, native receipts and limits belong to
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#macosarm64-source-hidden-and-shared-consumer-replay-2026-10-27).

### Complete the Windows/arm64 Development SDK source-hidden replay and close S3

Ran `tools/test_dev_sdk_external.py` natively on the Windows/arm64 VM against the extracted Development SDK of the producer replay below
(restricted token, whole checkout hidden, descendant denial probed, 2,695-entry inventory unchanged, `AA64` binaries): static and shared
HTTPS+inflate and trust negatives pass (eight cases), and the shared closure of ten SDK images resolves at load with a System32-only PATH.
Seven private-copy verifier negatives are rejected. The focused suite is 28/28 after a test fix: one Windows test launched `cmd /c probe.cmd`
by bare name and failed whenever `NODEFAULTCURRENTDIRECTORYINEXEPATH=1` is in the environment; it now uses `.\probe.cmd`. The fixture's
OpenSSL on this host is an emulated x64 binary (build-host only). With this every S3 sub-gate is checked and S3 is complete. Not claimed:
the x64 record's CI tooling selection, the stale-Full-SDK pairing negative, clean/tagged release qualification. Evidence:
[acceptance](docs/acceptance/sdk_split_acceptance.md#windowsarm64-source-hidden-replay-2026-10-27).

### Complete the Windows/arm64 Development SDK producer replay

Ran the ordinary `crt-development-sdk-test` entry point natively on the Windows/arm64 VM from a clean tree (`376f06d`). Fresh static/shared
zlib, Mbed TLS and curl built through the shared producer; the Development SDK verifies, passes the static/shared HTTPS+inflate consumer (eight
cases) and its six recipe tests, archives (138,495,124-byte zip) and verifies again after extraction into a spaced path with an identical
2,695-file inventory. The extracted SDK passes the consumer again; missing static/shared curl and a changed inherited libc byte are rejected;
the 17 Development unit tests pass. No producer or contract change was needed. Not claimed: producer-tree-hidden isolation (the Linux driver
needs bubblewrap, and there is no Windows equivalent) and the stale-Full-SDK pairing negative. A recipe-test run without `--install-prefix`
left a `crt-port-work/` directory in the checkout (deleted). Evidence:
[acceptance](docs/acceptance/sdk_split_acceptance.md#windowsarm64-native-producer-and-package-replay-2026-10-27).

### Complete the Linux/aarch64 Development SDK producer replay

Updated the native C/C++ predecessor incrementally and built fresh static/shared
zlib, Mbed TLS and curl through the shared Development/media producer. The
Development SDK verifies, archives and verifies again after extraction into a
spaced path. Recipe tests and SDK-only HTTPS/inflate consumers pass on this
device, including explicit-CA rejection cases and a replay with the producer
checkout, build outputs and original SDK hidden. Runtime inheritance and
package inventories match; private-copy corruption/missing-port checks and
stale Full SDK pairing reject their invalid inputs.

Marked Linux/aarch64 S2 and its source-hidden S3 sub-gates complete, retaining
Windows/arm64 production, the other native isolation gates and clean release
qualification as open work. Artifact identities, commands, local receipts and
the distinction from release/CLI adoption belong to
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-native-producer-and-package-replay-2026-10-27).

### Prove the Linux Development SDK with producer trees hidden

Built fresh zlib/Mbed TLS/curl ports through the existing Development/media
producer and verified the reduced profile. Added a native Linux external
acceptance driver that relocates the SDK into a fresh spaced path and masks the
producer checkout, build outputs and original SDK with bubblewrap. An external
probe verifies invisibility before profile checks and compilation; the bounded
fixture capsule supplies no CRT headers/runtime objects or Dev/runtime build drivers.
Static/shared C++17 consumers pass HTTPS+inflate and explicit-trust negatives,
with unchanged original and relocated SDK inventories. Runtime bytes match the
existing Full stages; missing static/shared ports, changed runtime bytes and
different producer/Full source identities are rejected by their owning checks.

Added boundary regressions and one Linux/amd64 PR CI producer/external smoke.
Marked the already implemented S4 collector complete, keeping positive tagged
assembly in release qualification; ARM producer and macOS/Windows hidden-tree
gates remain explicit. Artifact identities, import closure, source provenance,
prior native baselines and CI execution limits belong to
[SDK split acceptance](docs/acceptance/sdk_split_acceptance.md).

### Unify public CMake stage target names

Renamed stage 02, 03 and 04 public command prefixes to `crt-cxx`, `crt-gfx` and
`crt-media`, each with config/build/test/dist actions. Stages 01, 05 and 06
retain their names. The old stage-04 partial aggregates move to explicit
`crt-internal-media-gfx-*` and `crt-internal-media-runtime-*` names, so public
Graphics selects window/input and public Media selects the full GPU/media stage.
Updated dependencies, preflight mapping, catalog/classification, CI, diagnostics
and current build/use documentation. SDK stage IDs, archive paths, install
components, cache step identities and Python driver filenames retain their names;
old public prefixes have no aliases. Historical acceptance commands retain their
original source-baseline meaning, with a linked migration procedure.

The renamed Linux entries publish/verify C through UI SDKs and pass their native
tests and imported C++ smokes. Fresh-graph regressions cover all 24 stage targets,
scope separation and existing preflight IDs; the existing CI tooling check passes.
The completed naming item is removed from TODO. The
[acceptance record](docs/acceptance/build_system_acceptance.md#public-stage-target-naming-2026-10-26)
owns commands, counts, producer/consumer review baselines and limits, including
Web graph-only coverage and no new native macOS/Windows replay.

## 2026-10-26

### Complete build-system consolidation (B5)

Finished the internal target-scope review: Wayland/xkbcommon producers are
registered by default for Linux targets, including explicit aarch64 cross builds;
macOS/Windows standalone port experiments use an explicit option. The smoke
driver preserves that option in its shadow build. Production-module registration
and scanner-forwarding regressions run in the existing CI tooling check.

A genuinely empty Linux/amd64 build root now completes the ordinary C through
UI build/test/dist chain. The actual Simple Graphics, Media and UI archives
verify after extraction into paths with spaces, and their SDK-only examples
build and run with bounded frame/pixel/input checks. Final public-target,
configuration-preflight and release-negative review passes. Removed completed
B0-B5 and the cold Wayland proof from TODO; clean/tagged release qualification
and SDK consumer adoption retain their independent scope.

Commands, source identities, measurements and native-versus-fixture limits are
in the [B5 acceptance](docs/acceptance/build_system_acceptance.md#b5-closure-2026-10-26).

### Repair Linux build-system replay blockers

The Linux/amd64 replay exposed three failures beyond the Windows/macOS entry-point
baseline. Ordinary media relink packaging now compares the exact portable ELF
transformations performed by CRT or CMake, retains corruption/archive rejection,
and removes build-only link dependency outputs while preserving `$ORIGIN`.
The published SDK's kit successfully relinks libcrtmedia and runs a C consumer
with the SDK and LLD.

Shared libdl now discovers the main program's `DT_DEBUG` through its kernel
program headers instead of the DSO's own `_DYNAMIC`; the regression rejects the
old SDK and both imported C++ linkage modes pass their exception/string/iostream
smokes. Linux's existing OS-loader TLS prerequisite starts at `02-cxx`, so the
Development SDK's shared consumer passes strict import closure and all trust
cases. The producer/verifier migration and consumer review baselines are recorded
without changing manifest/recipe versions or approving CLI/Studio adoption.

Reproduction, negative cases, native results and limits belong to the
[repair acceptance](docs/acceptance/build_system_acceptance.md#linuxamd64-repair-replay-2026-10-26).
The subsequent cold Wayland subgraph exposed the missing protocol archive in
the ordinary SDK. Its producer now owns explicit Ninja byproducts and backend
dependencies; ordinary publication includes it without a manual reconfigure,
and packaged Simple Graphics/GPU examples rebuild. A fresh Web work-root then
completes native `crt-web-build/test/dist`, its browser/network/ABI gates and
the Development/Full runtime comparison. Original failed inputs are retained;
the [final acceptance](docs/acceptance/build_system_acceptance.md#final-linuxamd64-entry-point-acceptance)
owns measurements, archive identity, relocation and cache evidence.
The remaining B5 scope is completed in the closure above; clean/tagged release
qualification remains separately tracked in TODO.

## 2026-10-23

### Separate release qualification from build-system consolidation

Split B4's completed local SDK producer/release-target implementation and Windows
replay from the unperformed clean/tagged positive release rehearsal. B4 is checked
only for that implementation scope; build-system consolidation still requires B5
native replay and final entry-point review. The independent Planned item
`Release qualification and assembly rehearsal` now owns matching clean source /
versioned Development and Full artifacts, real positive release collection and
final artifact review. The SDK split's S4 links to that single rehearsal rather
than scheduling another copy.

Preserved all provenance and release checks: existing dirty `development` SDKs
are not relabeled by later commits, and runtime byte equality is not matching
clean producer identity. The acceptance document records the new scope and
interprets earlier combined-B4 checkpoints without erasing their evidence.
Documentation-only review; no build, tag, upload, consumer-lock change or public
release. See the [owning acceptance contract](docs/acceptance/build_system_acceptance.md#release-qualification-scope-split-2026-10-23).

### Complete Windows B2 Web production and extracted-archive acceptance

`crt-web-build` now succeeds through the isolated producer and atomic publication;
`crt-web-test` replays the newly produced SDK in a disposable copy. The remaining
dist failure was ZIP's pre-1980 timestamp limit, exposed by epoch-dated pinned
source files. Windows archive assembly now clamps only ZIP metadata via the
standard library, preserves SDK bytes and mtimes/empty directories, and replaces
the final archive atomically only after writing succeeds. POSIX tar assembly is
unchanged. The failed ZIP is retained separately, not shipped as a successful one.

`crt-web-dist` succeeds. The actual ZIP passes CRC and full payload comparison;
all 3,674 files match the SDK. Its extracted copy in a spaces-named relocated
directory passes ordinary verification plus browser observation, HTTP/TLS trust
negatives and host ABI again. All 27 inherited Development runtime files match
Full06 byte-for-byte; the producer SDK remains unchanged by replay. Final tooling
regression: 233 selected, 230 pass/three existing skips; exact CI command: 96
selected, 95 pass/one existing skip. No second CI workflow or full WebKit CI job.

B2's Windows entry-point gate is checked and its completed detail removed from
TODO; B4 clean/tagged matching release rehearsal and B5 Linux/macOS native replay
remain open. This is a local `development` artifact from the explicitly recorded
dirty source asset, not clean release provenance or CLI/Studio adoption. No tag,
upload or public release was created. Artifact hashes, commands and limitations
are in the [completion evidence](docs/acceptance/build_system_acceptance.md#windows-b2-completion-2026-10-23).

### Bound Windows SDK publication contention and guarded recovery

The fresh relink-capable Windows run passed compilation, packaged browser/network
and ABI checks and `verify_dist`, then failed at its final directory rename with
WinError 5. This is not an accepted/published 06 SDK. The precise holder or cause
of the access denial was not established; no browser/compiler process remained
when inspected. Preserved the validated staging tree and failed log.

The common host filesystem helper now retries only Windows errors 5/32/33 up to
eight atomic replace attempts (4.5 seconds of bounded backoff), retaining extended
filesystem paths, with persistent/other errors propagated. No ACL changes,
process killing, destination deletion or non-atomic publication fallback. Both
04 and 05/06 publishers use it. Real Windows directory-handle contention and a
complete SDK publication-copy probe pass, with source and output inventories
equal. Added explicitly scoped run recovery to the in-tree Web adapter, forwarding
the isolated driver's existing source/predecessor/packaging-only checks rather
than bypassing them.

Regression selects 231 tests: 228 pass and three existing skips; the exact CI
tooling command selects 94: 93 pass and one existing skip. A new source asset's
packaging-only retry is accepted by the existing comparison of compiled inputs
and unchanged predecessor; complete publication/test/dist is still pending.
See [acceptance](docs/acceptance/build_system_acceptance.md#windows-atomic-publication-and-guarded-recovery-2026-10-23).

### Bridge ordinary media SDKs to the isolated Web relink contract

The repaired Windows producer compiled WebKit and reached final SDK verification,
then correctly rejected the inherited FFmpeg dependency: ordinary CMake 04/05
developer SDKs did not create/declare the libcrtmedia relink kit required once
06 becomes an isolated artifact. Their legacy developer verification was not a
promise of isolated-Web predecessor completeness. No candidate was published.

Ordinary media packaging now captures the real Ninja link/objects and declares
the kit; UI packaging preserves and validates it. Build-prefix libraries map to
installed SDK files only after byte equality, and the ordinary bootstrap import
descriptor is retained verbatim as checksummed kit support, not another runtime.
Declared kits are checked even for ordinary SDKs; Web preflight rejects absent
declarations or damaged kits before fetching/building dependencies. Existing
isolated-kit and compile/predecessor identity guards were not weakened.

New separate 04/05 SDKs verify, while all 239 inherited bin/lib/libexec/system-bin
payloads match the preserved old 05 SDK byte-for-byte. The actual kit relinks a
Windows DLL, and an SDK-built consumer imports that relinked DLL and runs with
System32-only PATH. Full tooling/distribution regression: 227 selected, 224 pass,
three existing skips; the exact CI tooling command selects 90, 89 pass and one
existing Darwin-only skip. A new frozen Web source asset and fresh work directory
retain the stricter predecessor contract; B2 production/test/dist is still open.
See the [acceptance record](docs/acceptance/build_system_acceptance.md#ordinary-media-relink-predecessor-repair-2026-10-23).

### Fix the Windows carried-patch host-tool boundary

Reproduced the Strawberry patch 2.5.9 `hunk` assertion on disposable pristine
files, independently of the full Web build. Git for Windows applies the exact
same 0003 patch successfully and every output matches the existing manifest.
Windows now explicitly selects Git's apply engine; POSIX build hosts retain
native patch. Preflight checks the selected tool before expensive dependencies,
records its command/version, and CMake fingerprints the resolved host executable.
The invocation isolates parent Git discovery and inherited Git worktree/index
settings, preserves bytes and never updates a repository index. No carried patch,
upstream source, hash requirement or global Git/Windows setting changed.

Native Windows tests also apply all three target patch sets to pinned file
copies: Windows 18 patches/50 files, Linux 17/39 and macOS 22/48, with unchanged
upstream inputs. These are patch-byte checks on a Windows build host, not native
Linux/macOS acceptance. The full regression selects 210 tests: 207 pass and three
existing host-inapplicable skips (`patch-fix-unit.log`). B2's whole-build and
packaged test/dist gates remain open for the new source-asset replay; failed runs
are preserved. Evidence is in the
[build-system acceptance](docs/acceptance/build_system_acceptance.md#b2-host-patch-repair-2026-10-23).

### Record the B2 Windows build-host patch failure

The fresh `run-41yecki4` producer exited unsuccessfully after completing its
target dependency phase and reaching WebKit carried-patch application. The
selected host tool, `C:\Strawberry\c\bin\patch.exe` (its diagnostic identifies
patch 2.5.9), asserted on `hunk` while applying
`0003-coff-assembler-flavor.patch` to `Source/WTF/wtf/InlineASM.h`, exiting 3.
This is the observed immediate failure, not evidence of a WebKit compile/runtime
defect; tool selection and a bounded comparative reproduction remain to do.
The source-copy repair progressed past its former failure point, but B2 remains
open: no new 06-web SDK was published and packaged test/dist did not run.
Preserved the original source identity, predecessor, failed work and log; no
automatic retry or provenance relabeling was performed. Exact evidence is in
the [build-system acceptance](docs/acceptance/build_system_acceptance.md#b2-host-patch-failure-2026-10-23).

### Repair deep Windows Web source materialization and executed-recipe identity

The overnight B2 build completed its target dependencies but stopped before
WebKit compilation while copying the complete pinned checkout into the actual
deep work root. The aggregated Python exception contained 411 failing entries,
not a missing upstream package: 345 files and 66 directories hit Win32 path
limits. Preserve the failed source asset, predecessor, work directory and log.

Added host-only extended-path filesystem copying for the Web cache/composition
steps, retaining hardlink break-before-overlay semantics and all upstream files.
Git's long-path option is per invocation, never a global setting. Copy failures
now report the total and at most three bounded examples instead of serializing
an enormous list. A complete deeper cache copy validates the product pin and
pristine checkout; all formerly failing files match their sources byte-for-byte,
all 66 directories exist, and measured destination paths span 259..302 characters.
No source exclusions, shortened work root, registry change or new upstream patch
was used. Native tooling/Web/distribution regressions select 206 cases: 203 pass,
three existing host-inapplicable skips, zero failures.
The exact existing-CI tooling command also passes all 65 cases; it gains only
small filesystem/recipe fixtures, not another workflow or full Web build.

Also fixed an external-recipe override retaining the predecessor's stale 06-web
pointer in the result SDK. The generic driver emits a verified execution receipt;
the Web producer validates schema/OS/SHA/commit and installs that actual recipe
only into its staged copy. Legacy direct invocation needs an already matching
recipe. This does not change recipe schema 2 or mutate the predecessor/tarball.
Native execution confirms the override is staged while the complete predecessor
inventory stays unchanged. The completed Development port builds are reused by
the guarded metadata-only path to include the new tool helper; verification and
both HTTPS consumer link modes pass again without upgrading compile provenance.

A new source asset and fresh Web run retain the dependency/compile identity
guards; no failed objects are relabeled as reusable. Whole Web build, packaged
browser test/dist and clean tagged release assembly remain open in TODO. Linux/
macOS replay is still deferred. Exact evidence and active log are in the
[build-system acceptance](docs/acceptance/build_system_acceptance.md#deep-windows-work-root-repair-2026-10-23).

## 2026-10-22

### Harden stage reuse and implement the Windows Development SDK reference path

Committed/pushed the prior configuration-target checkpoint as `1033c86`, then
implemented Web and local Dev/Full release entry points. These are not declared
accepted yet: current Windows Web production and a clean tagged
positive release rehearsal remain in TODO, with Linux/macOS replay explicitly
deferred until Windows completes. Old Web SDKs were correctly rejected by the
current SOURCE-OFFER/relink gates, never patched into passing artifacts.

Completed B3 cache hardening: actual output inventories, tarball/archive outputs,
executed-driver/complete configuration/tool/environment/predecessor inputs,
failed-run invalidation and no caching when inputs change during execution.
libc++ fetch/configure boundaries no longer fingerprint a parent mutated by its
later build. Windows core replay passes 162 executed tests plus one existing
COW skip (163 selected, 49.51 seconds), then the same target skips correctly.
Windows tooling passes 114 regression cases including native symlink retargeting,
real archiver filtering and atomic directory publication. The
[cache record](docs/acceptance/build_system_acceptance.md#b3-cache-correctness)
owns commands, observations and the local metadata-cache limitation.

Implemented the reduced SDK producer through the existing media networking
build, not new recipes. Actual Windows zlib/Mbed TLS/curl static/shared builds
complete; packaging checks expose and correct PE/.so suffix assumptions,
Mbed TLS DLL location declarations and absolute text metadata. A metadata-only
retry verifies unchanged predecessor/runtime and recipe build identity, retains
compiled artifacts/provenance and does not grant clean release identity.
The resulting 02-cxx/dev profile verifies and its relocated C++17 reference
consumer passes HTTPS/inflate plus wrong/missing/malformed-CA negatives in both
link modes. Actual shared PE imports produce ten app-local SDK runtime files;
the process runs with System32-only PATH. This is producer evidence, not CLI
adoption or a shipping static CLI decision. The normal CMake producer/consumer
then passes against the current ordinary 02-cxx, including another spaces-path
replay. Its inherited core-runtime digests exactly match the current Full 05-ui
predecessor. Both are still dirty local development artifacts, not a clean
tagged release; the Full 06-web comparison remains pending. The cache correctly
refuses to record producer reuse when source inputs change during compilation.
S0/S1 are checked; multi-host S2-S5 stay open.

Fresh ordinary Windows upper-stage production also exposes missing DLL import-
library BYPRODUCTS and preparation edges incorrectly inside the non-Windows
Web-port-list condition. Corrected the graph; current ordinary 05-ui production
and Web-predecessor verification pass. Full Web production is still pending.
Also eliminated configure-time compiler-rt archive mtime churn without relaxing
the predecessor-byte fingerprint: filter privately, copy only changed bytes.
A no-change native reconfigure/`c_shared` build now does no work. Final fresh
catalog checks find 28 public/380 internal targets; each local release target
separately rejects the default tag without starting a producer. Final review
also separates the CRT source identity from WebKit's upstream source pin when
checking Full/Development pairs.
Windows Web host-tool preflight now exposes/fingerprints the existing POSIX
Perl module overlay and checks prerequisites before fetching target ports.
The initial missing-module failure is retained separately from the configured
retry. Additional distribution/wrapper regressions select 49 tests, with two
existing host-inapplicable skips and no failures.
New release collectors also bind their clean checkout to the SDK producer
commit. Stage-source `stage.json` now records creation-time dirty state
additively; the new Full collector checks the pinned Web source asset and rejects
legacy/dirty creation provenance instead of upgrading it from collector state.
Old source assets remain valid local development/replay inputs. Clean-source
and collector-mismatch fixtures pass; native tagged assembly is still pending.
Exact failed/successful checkpoints and active build logs are in the
[build-system acceptance](docs/acceptance/build_system_acceptance.md).
The [SDK design](docs/design/sdk_development_contract.md#implemented-packaging-profile-boundary)
owns the limited profile fields, trust/notice/SBOM policy and source/runtime
release comparison. Consumer baselines were inspected read-only; no CLI/Studio
lock, STATUS, public tag, release upload or extra CI workflow changed.

### Add bounded stage configuration preflights and tighten target-catalog checks

Completed build-consolidation B1: six opt-in `crt-<stage>-config` targets inspect
the already configured tree without fetching, compiling or recursively running
CMake. They require the complete advertised stage's feature options, report the
effective pair/tools, reject unsupported Web pairs and missing Linux media link
inputs, and remove older success reports on failure. Existing reduced-feature
builds retain their behavior. Local `stage-config/` JSON is diagnostic only;
SDK formats, ABI, recipes and consumer locks are unchanged.

Fresh Windows/x64 preflights pass in a path containing spaces without creating
dependency build trees. LVGL-OFF rejects UI but leaves lower preflights usable;
reapplying the preset restores all six passes. This exposed bare compiler names
in the cache after a preset reconfigure; resolved CMake tools now take priority,
with PATH lookup for direct-script use. Windows/aarch64 cross configuration passes
the lower five configs and rejects Web; no arm64 compile/runtime pass is claimed.
Both target inventories match at 23 public/378 internal names. Fixed the
classifier's OS-specific missing-target exemption and added README/catalog
parity coverage. Thirty tooling tests pass and are wired, with C/C++ preflights,
into the existing CI matrix; no second workflow or upper-runtime rebuild is added.

The [acceptance record](docs/acceptance/build_system_acceptance.md#b1-local-implementation-and-replay-2026-10-22)
owns commands, results and limits. TODO keeps checked B0/B1 prerequisites and
open B2-B5; Web aggregates, cache integrity, Development SDK/release production
and other-host replay remain unfinished. STATUS was not requested or changed.

### Review and restore tracking of the preceding build-system consolidation

Audited `0208bcd` (feature-enabled presets and build-time Skia/ports/LVGL inputs),
`027b5db` (stage input-hash cache), `0443910` (target catalog/classifier/CI) and
`aa81932` (OS-scoped inventory). Those changes existed in source but lacked
matching completed-work entries here and an active TODO execution outline.
Added build-consolidation B0-B5 ahead of the SDK split, preserving its S0-S5
producer dependencies instead of treating a renamed `02-cxx` as Development SDK.

The supplied five-host inventory is retained as prior reported evidence, not a
new five-host replay. Locally, baseline tooling passes 18 tests and a fresh
Windows/x64 preset configures without pre-fetched Skia/LVGL; all 17 original
public targets exist. A real fresh C-stage build, including recipe Make and CRT
shell, then runs 163 selected CTests: 162 executed passes, the existing COW skip,
zero failures, 41.18 seconds. Existing upper-stage/other-host measurements in the
original commits are not repeated or promoted into new acceptance here.

Found and documented limitations: directory-existence checks miss deleted nested
payloads; excluded `test_*.py` inputs can hide changes to executed test drivers;
configuration/predecessor identity needs broader cache coverage. Temporary
fixtures reproduce both false-success skips. C++ smoke currently runs uncached,
and Wayland/xkbcommon targets remain defined on all OSes; corrected the public
description and left fixes in the active outline. Exact findings and local logs
are in the [review](docs/acceptance/build_system_acceptance.md#b0-baseline-review-2026-10-22).

### Fix Windows iostream through complete priority-constructor startup

The first-cout crash was a CRT initialization-range/order defect. The pinned
libc++ places standard-stream setup in `.ctors.65435` (priority 100), beyond
the old plain `.ctors` end marker. Moved GNU end markers after every valid
priority subsection, reversed GNU constructor traversal and used forward GNU
destructor traversal in both executable and DLL startup. MSVC range behavior
is unchanged. No application Init workaround, forced archive load, upstream
patch, public ABI or exception/unwind policy change was introduced.

Added EXE/DLL priority regressions: both fail before the fix with constructor
order 3 instead of 123, then pass with the fix, including reverse lifetime
order and explicit DLL unload notification. A fresh-cache Windows C-stage
replay selects 163 cases: 162 executed passes, zero failures, existing COW skip.
Strict Windows/arm64 compilation passes for changed startup/priority code;
native arm64 and Linux/macOS execution are not claimed.

The corrected objects alone make the static stream consumer pass; an old
shared libc++ DLL still fails, confirming that DLLs must be rebuilt too.
Rebuilt all three imported runtimes in a separate work tree against a corrected
01-c predecessor, created/verified a separate local 02-cxx SDK, and passed
static/shared container/exception/RTTI, string-ABI and real stream-lifetime
smokes. An external consumer using packaged wrapper copies passes as well;
shared PE DLL imports are checked. The initial wrongly selected cumulative
C++ predecessor's header-search failure is retained separately, not counted
as a product regression or successful build.

The smoke now requires exact stdout/stderr through application destruction;
the new source rides the isolated 02-cxx asset with a closure regression.
Expanded predecessor fingerprints to include startup objects, builtins and
Windows runtime DLL/import-library inputs: cached runtime links must not survive
a startup-only fix while C archives stay unchanged. Dedicated mutation cases
verify invalidation of the affected nested builds. Predecessor/fetch/source-
closure tooling passes 19 cases, with no additional CI workflow.
Exact inputs, hashes, limitations and reproduction logs live in
the [diagnostic record](docs/bringup/crt_pal_residual_triage.md#windows-iostream-priority-initialization-fix-2026-10-22);
startup policy lives in [C++ runtime design](docs/design/cxx_runtime.md#windows-constructor-priorities).
Removed the completed Windows Tranche 2 from TODO; macOS binding Tranche 3
remains open. Existing SDKs/higher-stage DLLs were not overwritten or replayed,
and no release or STATUS update is claimed.
Documentation checks pass for four edited Markdown files and 104 local links/
anchors, plus English/fence/conflict checks, Python compilation and
`git diff --check`.

### Fix Windows virtual random/zero descriptor duplication

The Windows PAL now clones HANDLE-less random/zero descriptor state for dup
and dup2 instead of rejecting its placeholder as an invalid native HANDLE.
Valid self-dup preserves descriptor flags, distinct duplicates clear CLOEXEC,
and existing F_DUPFD/F_DUPFD_CLOEXEC wrappers work without public ABI changes.
Occupied dup2 destinations use the existing kind-aware close path, including
when an ordinary file replaces a virtual slot; no placeholder CloseHandle or
leaked replaced pipe writer is needed. Bionic contract references and exact
scope are in the [diagnostic checkpoint](docs/bringup/crt_pal_residual_triage.md#windows-random-device-dup-fix-2026-10-22).

Added portable `virtual_device_dup_test` to routine CTest with a 15-second
bound. A fresh Windows/x64 Debug build fails before the fix at dup2 self with
EBADF, then passes; checks include flags, invalid inputs, minimum fd, close
lifetime, reuse and bidirectional virtual/native replacement. Full C-stage
replay selects 161 cases: 160 executed passes, zero failures and the existing
native-COW skip. Both changed C sources also compile with strict Windows/arm64
flags; no native arm64, Linux or macOS execution is claimed.

Created/verified a separate local 01-c SDK from the fresh runtime/shell (reusing
unchanged Make and successor-recipe inputs), then externally rebuilt/executed
the regression against static libc and the shared-core link lane. DLL imports
and the success marker are checked, not inferred from an environment setting.
Packaged CRT mksh random redirection and duplicate/read/close now pass; zero
dup/close also passes. No frozen SDK was overwritten or release published.
Removed the completed Tranche 1 from TODO; iostream Tranche 2 and macOS binding
Tranche 3 remain open. No upstream patch, new public symbol, CI workflow change
or STATUS update; virtual-fd cross-process transport remains outside this fix.
Markdown validation passes for three edited documents and 100 local links/
anchors, plus English/fence/conflict checks and `git diff --check`.

### Select residual PAL defects with bounded Windows reproducers

Moved random-device duplication, Windows iostream and macOS dylib binding from
Planned into one ordered In Progress increment ahead of SDK Producer Contract
Gate P0, removing the duplicate planned entries. Completed the Windows triage
and prepared the native macOS handoff; outstanding fixes/native evidence stay
unchecked rather than claiming the three reports are resolved.

The identified packaged 02-cxx SDK reproduces random-device dup/dup2 (including
self) and both F_DUPFD variants failing with EBADF while the original fd still
reads/closes; packaged CRT mksh redirection fails before fork. Iostream include-
only and cstdio controls pass static/shared, but actual cout use fails in both
with exit 139 after a main-entry marker. This narrows the old startup report;
cerr is not reached and no root cause or fix is claimed. In-tree libc and
imported libc++ archive/DLL hashes match the tested SDK copies, not an
independent fresh-build comparison.

Exact identity/hashes, bounded commands, observed failures, reproduction cases
and the per-dylib macOS handoff are in the
[diagnostic record](docs/bringup/crt_pal_residual_triage.md). Initial sandboxed
launcher execution stalled; a five-second exit-only control isolated the
environment restriction, and the functional run completed outside the sandbox
without retry loops or visible consoles. No production runtime, upstream source,
SDK content, CI or STATUS was changed; no native macOS pass is claimed.
Documentation validation passes for four changed Markdown files and 121 local
links/anchors, plus English/fence/conflict checks and `git diff --check`.

### Prefer JavaScriptCore reuse over a deferred QuickJS runtime

Replaced TODO's conditional QuickJS entry and the roadmap's deferred-engine
proposal with consumer-driven JavaScriptCore reuse outside WebView. The engine
already belongs to `06-web`; a separate scripting/embedding API, runtime
closure, startup cost and footprint still require scoped validation. This
does not introduce another engine or required SDK stage, promise a lightweight
runtime, or claim standalone embedding has been implemented. The historical
explanation of the retired `05-js` QuickJS stage remains intact.

### Refine Planned work into producer priorities and conditional follow-ups

Reordered TODO with SDK Producer Contract Gate P0 first, bounded PAL defect
revalidation next, and independent hardening/consumer-driven extensions after
it. Retained WebRTC, conditional standalone scripting, WebGPU, EME/DRM, JS-native bindings,
interactive job control, Toybox expansion and the ELF-loader pointer with links
to their owning documents. None is promoted into active work or made a blanket
CLI/Studio prerequisite; the accepted SDK stages remain closed.

Corrected stale queue descriptions against the source: the allocator already
uses indexed bins/local coalescing/a heap tail, positive-pid Windows probes and
SIGKILL already exist, and shared fetching already retries transient failures
and hashes cached archives. Retained only conditional Scudo evaluation,
remaining signal semantics and an optional FreeType mirror. Narrowed Linux
curl-dist work to partial-option revalidation, merged the Wayland missing-
archive report into the clean-build dependency gate, and framed ABI work as
extending existing audits. Older iostream, macOS binding and shell-performance
reports require current evidence rather than assumed fixes or blanket rewrites.

This is queue maintenance only: no runtime implementation, new acceptance pass,
contract version change or CLI/Studio lock promotion. README and STATUS are
unchanged.
Validation: both edited Markdown files pass local link/anchor checks (90 links),
English/fence/conflict checks and `git diff --check`. TODO content before
Planned and the existing inbound anchor targets are preserved; no build was run.

### Close the Windows relink-kit test-portability follow-up

Reproduced both planned failures on Windows/x64, then fixed only the test
fixtures in `tools/test_crt_relink_kit.py`. Command-output parsing now retains
quoted paths and compares absolute resolved paths, not mixed separator
spellings; its working path contains spaces. The ELF stand-in compiler now
uses `sys.executable` with a Python script as the first fixture recipe argument,
so it needs neither a POSIX shebang nor shell/file-association fallback. Script
and output paths contain spaces and the two subprocesses have 30-second
timeouts. The generated runner still executes and removes the fixture ELF's
absolute RUNPATHs, with exactly `$ORIGIN` required afterward.

Verification: `python -m unittest -v tools.test_crt_relink_kit` passes 12/12.
The combined `tools.test_crt_web_release`, `tools.test_crt_source_offer`,
`tools.test_crt_relink_kit`, `tools.test_license_choices`,
`tools.test_crt_runtime_notices`, `tools.test_prepare_release_assets` and
`tools.test_stage_06_web` run 131 cases: 130 executed passes and the existing
Windows skip for direct POSIX-wrapper execution; no failures/errors.
Added the resolved follow-up to [Web acceptance](docs/acceptance/crtweb_acceptance.md#tranche-10-closure-and-owner-release-approval-2026-10-22)
without erasing the failed checkpoint, and removed the completed work from
TODO. The old TODO anchor points to the completed-history entry area so dated
snapshots remain navigable; STATUS itself was not refreshed. SDK Producer
Contract Gate P0 remains planned. Production relink code, SDK binaries and CI
are unchanged; no Linux/macOS runtime replay, commit/push or release upload
was performed by this bounded fixture fix.
Documentation checks pass for 13 Markdown files and 270 local links/anchors,
including the retained explicit anchor, plus English/fences/conflict checks
and `git diff --check`; Python compilation also passes.

### Synchronize the public README and the explicitly requested STATUS snapshot

Updated STATUS from its 2026-10-12 B4 checkpoint to the source/history through
`2d2853d` plus the owner-approved Tranche 10 closure edits. It now distinguishes
native three-host `06-web`, Linux/aarch64 cross-package and additional in-tree
evidence, and Windows/arm64's lower-stage-only replay. Recorded public-release
approval separately from publication, preview withdrawal separately from the
retained source tag, and VM/device/relink/fixture limitations without upgrading
skips or cross compilation into native passes. Corrected the old blanket
`getrusage()` stub claim against `libc/src/resource.c`: Windows self and macOS
self/children have implemented counters, while the remaining selectors/host
cases retain their limits.

README remains a user introduction: removed obsolete first-render/future-WebView
claims and the duplicate completion matrix, linked STATUS/acceptance for scope,
and clarified CPU WebView transport versus GPU composition. SDK Producer
Contract Gate P0 remains planned; no CLI/Studio implementation, new runtime
replay, release upload or commit/push is claimed by this documentation sync.
Validation: 13 changed Markdown files pass 270 local link/anchor checks,
English/fence/conflict checks and `git diff --check`. No new runtime replay was
needed for this documentation-only update; existing test limits are preserved.

### Close Web Tranche 10: owner release approval, regenerated SDKs and withdrawn preview

The owner explicitly approved public Web releases and closure of Tranche 10.
The recorded native Linux/x86_64, macOS/arm64 and Windows/x64 isolated SDKs,
Linux/x86_64 -> Linux/aarch64 cross-package proof, five-pair lower-stage
regeneration and libcrtmedia relink evidence close the accepted scope. No new
build/runtime replay is claimed: the original records retain VM GPU/VA/audio
limits, synthetic-device scope, and Windows/arm64's packaged-FFmpeg-only relink.
Windows/arm64 `06-web` remains unsupported by design.

Read-only GitHub API verification confirms the owner removed the
`v0.4.0-preview.1` binary release (404) and the public releases list is empty;
the public repository and tag still return 200. `git ls-remote` confirms tag
object `88f19a659075546b1d566712488ccf7cfc510cb8` and source commit
`fd01d7c85f3dc6b96aa5019b0edff2121598c070`. Exact endpoints and the closure
decision live in [Web acceptance](docs/acceptance/crtweb_acceptance.md#tranche-10-closure-and-owner-release-approval-2026-10-22).

`third_party/crt-runtime/release.json` now records the explicit owner approval
and UTC timestamp, accepting only the advisory tool's fixed reminder for the
completed native/cross packaging gate. It does not waive failed advisories,
stale pins, expired support grace or unrelated blockers. The regression suite
keeps missing approval denied by default and checks that this actual record
allows the completed gate but still rejects those other conditions.

Marked SDK regeneration and owner review complete in the LGPL guide; retained
the source-offer/notices obligations and historical preview notes with a
withdrawal notice. Removed the completed Web checklist from TODO; SDK Producer
Contract Gate P0 remains the next planned increment, not started. Updated the
acceptance header, public README, roadmap and distribution/release guides to
point to their owning evidence rather than stale B4/development-only status.
STATUS and the consumer repositories were not changed. Existing immutable
development SDKs keep their old decisions; approval takes effect only in newly
generated source assets/packages with current release checks. No GitHub
release or archive was created, deleted, uploaded or republished by this work.

Validation: the six approval/source-offer/license/notices/release/stage suites
run 119 tests (118 executed passes, one expected POSIX-only skip). Adding the
unchanged relink-kit suite gives 131 (128 passes, two failures, one skip): both
failures reproduce from committed `2d2853d` test source on Windows (mixed path
separators and a directly executed POSIX fake-compiler script). They are a
separate Planned test-portability item, not new product relink failures; details
and exact suite names are in Web acceptance. Sandbox-only Git/compiler/symlink
errors were resolved by a permitted unsandboxed rerun, not security-setting
changes. No full SDK/runtime rebuild was needed for the approval/doc edit.
Documentation checks pass for 12 Markdown files and 254 local links/anchors,
English text, fences and conflict markers; `git diff --check`, release JSON
parsing and Python compilation pass. Other than the explicit GitHub withdrawal
queries, external links were not revalidated.

### Tranche 10 on Windows/arm64: option-ON SDKs 01-c..05-ui, isolated 04 -> 05 chain, relink kit, and three defects

Run on a 4-vCPU Windows 11 arm64 VM (no GPU). The in-tree `01-c..05-ui` SDKs were regenerated with FFmpeg, curl, Skia, GPU and LVGL ON
and all five pass `verify_dist.py`; `04-gfx-media` and `05-ui` carry the generated `SOURCE-OFFER.md` (FFmpeg 8.1.2). `ctest` is 220/220
(5 skipped: MF capture, the three `crtweb_*gpu*` tests and `fork_heap_snapshot`; no GPU on a VM). The isolated `03 -> 04 -> 05` chain ran
from an immutable copy of the in-tree `03-gfx-simple` with a clean-tree source asset (`f0217d7`): `04-gfx-media` and `05-ui` publish and
verify, installed `gfx-gpu`, `gfx-skia`, `media-player`, `media-stream` and `ui-basic` pass, and the libcrtmedia relink kit works from a
relocated SDK at a spaced path with a relative `--out` (same 75 exports and section sizes as the SDK's DLL; a program with only the
relinked DLL beside it plays 30 frames). 06-web is not provided on Windows/arm64 by design (`tools/build_stage_06_web.py` accepts native
Linux/x86_64, macOS/aarch64, Windows/x86_64 and the x86_64 -> Linux/aarch64 cross build).

Defects found and fixed, each in CRT code and none in an upstream source:

- `libcrtgfx/src/arch/windows/gpu_win32.c` included `<string.h>` although its unit has no libc header on its include path. Windows/x64
  passed only because a host MinGW header was found by accident (likely; not verified on that host); this machine has none (`0116844`:
  compiler builtins).
- `crtgfx_skia_shared` names `libc++.dll.a` itself while `crt-c++` also appended the static `libc++.a` to the DLL link, so lld stopped with
  duplicate `std::__1` symbols in the isolated 04 stage. The target now links that DLL with `CRT_CXX_STL_LINKAGE=shared`, which leaves the
  predecessor SDK unchanged (`8ffd984`). Why the Windows/x64 stage did not hit it was not investigated.
- `crtmedia_http_lifecycle_test` failed "upload cancel" (handle count +5) in about 3 of 10 runs on this VM: the audit sampled while cancelled
  transfers were still closing. It now resamples for up to 2 s, so a leak still fails (`f0217d7`; 20/20 afterwards, in-tree).

Pitfalls on this host, recorded because each cost a run: (1) cmake must run from a native arm64 shell. A bash from Git for Windows is an x64
process, reports `PROCESSOR_ARCHITECTURE=AMD64`, and configures the x86_64 sources; (2) Strawberry Perl's `c\bin` ships an x64 cmake 3.29
that shadows the installed one, so only `perl\bin` goes on PATH; (3) `make.exe` is linked statically against `libc.a`, so a libc change needs
`port-rebuild-make` (the freetype build died silently with exit 253 on the August binary); (4) Skia's pinned GN has no Windows/arm64 package
(the fetch URL returns 404), so the Windows/x64 `gn.exe` of the same pinned revision runs under emulation as a build-host tool (SHA-256 of
`gn.exe`: `cf28c1ea5c5c6c2ec1c2e4e12b360f276595ceea8de6a35368dfd1f9b39b527d`; not shipped in any SDK) and is found through PATH; (5) isolated stages
need `CRT_WINDOWS_SDK_LIBPATH` (8.3 form, newest installed SDK 10.0.28000) and `CRT_CC/CXX/AR/RANLIB`.

Local-only deviations, not in the repository: the packaged zero-copy media bridge smoke (`crtgfx_skia_media_window_demo`) fails here because
the VM's D3D12 adapters cannot import a hardware-decoded frame (`crtgfx_skia_import_media_frame failed`), so it was skipped in the extracted
work copy of the 04 stage; `CRTMEDIA_PLAYER_DEMO_SOFTWARE_ONLY=1` made the media-player step expect `hardware_decode=no`. Both need a host with
a GPU. Evidence: [acceptance](docs/acceptance/crtweb_acceptance.md#windowsarm64-option-on-sdks-and-isolated-04---05-chain-2026-10-22).

## 2026-10-21

### crtmedia_playback_pipeline_test: it measured the audio server's start-up latency, not CRT pacing

The wall-time floor failed on back-to-back runs (57-62 ms against a 200 ms floor; 275-295 ms when it passed). The test fed the player the timestamp of
the audio it had just written, while `player.h` and `media_player_demo.c` use the sink's real playback position; the Pulse sink's initial one-second credit
then let the 1.04 s clip through at once, and only the server's credit latency set the pace. The test now reports the sink position (loop about 1.1 s;
0 failures in 20 back-to-back, 15 ctest and 5x3 concurrent runs). No product code changed. Linux/x86_64 only; macOS, Windows and a host without any
audio sink were not run.

### Tranche 10: Tranche 8 device gates against the installed Linux/x86_64 06-web SDK

New `tools/test_stage_06_devices.py` builds `crtweb_media_test` and `crtweb_capture_test` (public C API only) with the installed SDK's
wrapper and runs them with the SDK's own processes, reusing the driver's audit and pass criteria. Both gates pass on the isolated
06-web SDK, six more runs on the pristine SDK pass, and they pass with the kit's relinked `libcrtmedia.so` in place of both copies.
Camera: the labelled synthetic camera only; no physical camera was opened. Linux/x86_64 only.

### Tranche 10: Linux/x86_64 isolated 04 -> 05 -> 06 chain on WebKit 2.54.1, and the missing libcurl in isolated libcrtmedia.so

First fully isolated Linux/x86_64 chain since the 2.54.1 re-pin; all three stages pass `verify_dist.py`. The isolated 04 stage built
`libcrtmedia.so` without linking curl (the stage CMake lacked `CRTMEDIA_CURL_SHARED_LIBRARIES`), which the 06 stage only met in its last
step as undefined `curl_*` in the external-consumer link; fixed in the stage CMake, with a stage check and tests. The kit's relinked
library works in place of both `libcrtmedia.so` copies of the 06-web SDK (browser smoke and observation). Linux/x86_64 only.

### Tranche 10: Linux/x86_64 libcrtmedia relink kit, and the VA-API decode race behind four failed stage runs

The Linux kit was proven from a real isolated `04-gfx-media` stage: relink from a relocated copy (path with a space, relative
`--out`) gives the same exports/NEEDED/SONAME and identical code and data sections; a modified `libavutil.a` shows up in the
relinked library; both relinked libraries load and demux the test clip. The first relink exposed that its RUNPATH kept this
machine's SDK path and `/lib/x86_64-linux-gnu`, so `relink.py` now strips absolute entries (ELF kits ship `crt_elf.py`).
The stage itself kept failing because Intel's iHD VA-API driver died in `vaDestroyBuffer` under FFmpeg frame threads (lifecycle
test 7/40 failures, packaged zero-copy demo hang); VA-API decode now uses one thread (0/160 failures after).
`crtmedia_playback_pipeline_test` also failed here regardless of the change; its cause was found the same day (next entry).

### Tranche 10 on Linux/aarch64: relink kit and isolated 04/05 stages

The isolated `03-gfx-simple -> 04-gfx-media -> 05-ui` stage builds ran natively on the aarch64 guest (04: 14 min; 05: 27 s) and the relink kit was
exercised for the first time on Linux: `share/crt/relink/libcrtmedia` is written and passes `check_kit`; `relink.py` links a `libcrtmedia.so` with the
same 2,019 exports as the SDK's and no undefined `__aarch64_*` helper, with and without `--ffmpeg-libdir` (a freshly built FFmpeg). The run found
four defects that were shared by every host and invisible until a stage was built end to end: the stage-source asset did not carry
`tools/crt_web_release.py`, so the last `verify_dist.py` of any isolated stage failed with `ModuleNotFoundError` (asset list fixed; new closure test
over `verify_dist.py`'s module-level imports); isolated 04 and 05 never installed `SOURCE-OFFER.md`, which `verify_dist.py` requires of every stage that
ships FFmpeg and which names its stage (`crt_source_offer.install_stage_offer`, used by both entrypoints; tested); `relink.py` resolved a relative
`--out` against the kit directory (paths are now made absolute first); and `test_crt_relink_kit`'s Windows parse test failed on POSIX hosts
(separators folded). The macOS/arm64 run found the same stage defects the same day (`04c9b83`); the merge kept that implementation (`install_for_stage`) and added the offer test and the `verify_dist.py` module-closure test. Local-only deviation, not in the repository: this guest has no VAAPI driver, so the stage's zero-copy media bridge smoke
(`interop=zero-copy`) was skipped in the extracted work copy to reach the later steps; it needs a host with a VA driver. The SDK also needed
`crtgfx-wayland-build` run once for `libxdg-shell-protocol.a` (the in-tree dist without it is native-Wayland-less and cannot seed stage 04).

### Review SDK/CLI/Studio contracts against all three local repositories

Compared the attached cross-repository review with CRT `a23a173`, CRT-CLI
`a76d8ad` and CRT Studio `d3793e3`. Ownership is consistent; both consumer
repositories still contain documentation only. Added an explicit Planned
SDK Producer Contract Gate P0 without changing the active Web closure. The
CLI-owned `docs/compatibility.md` separates design provenance, existing formats,
proposed contracts and exercised combinations; no runtime combination is
approved by this review. Existing SDK `format: 1`, port `schema_version: 1`
and stage `schema_version: 2` are not one common v1 contract.

Clarified feature-level CLI prerequisites and D7's formal approval role in the
integration baselines; current product designs remain owned by the consumer
repositories. Added SDK-free HTTPS/bootstrap requirements and a common manual
contract-review procedure. Corrected independent host-tools acceptance:
tool execution needs no target SDK, but a zlib build requires its pinned target
SDK while remaining independent of CRT source/build trees. Consumer histories
record their own documentation changes. These are documentation/acceptance
requirements, not implementation, runtime replay, new CI, or release approval.
Documentation validation covers local links/anchors, English additions, fences,
conflict markers and whitespace. It also corrected an existing Windows 9B
acceptance anchor in this history; external URLs were not network-validated.

### Tranche 10 on Linux/aarch64: option-ON SDK regeneration and four aarch64-only defects

The five in-tree stage SDKs (`01-c`..`05-ui`) were regenerated on the Ubuntu 26.04 aarch64 guest with FFmpeg, curl, Skia, GPU and
LVGL ON; manifests carry the `crt-runtime` components, the FFmpeg stages carry `SOURCE-OFFER.md`, and `verify_dist.py` passes. CI builds only
default options, so the option-ON aarch64 build exposed four defects, each fixed at its cause: the FFmpeg recipe now passes
`--disable-linux-perf` (aarch64-only feature vs the `-fcrt-real-linux-sdk` configure sentinel; new `tools/test_ffmpeg_recipe_linux.py`);
`crtmedia_shared` links with `-Wl,-Bsymbolic` on Linux (`libcrtmedia/cmake/crtmedia_targets.cmake`); `crt_compiler_rt_builtins`
adds `-Wl,-u,__stack_chk_fail` on Linux/aarch64 (`CMakeLists.txt`) so the distro builtins' outline-atomics member resolves in CRT's libc, not the
host's `libc.so.6`; the curl recipe disables nghttp2 and libidn2 so host libraries cannot add undeclared dependencies. Verified: 285 tooling
tests, full build, `ctest` 203 tests. Environmental exceptions on this guest: six VAAPI tests hang in Mesa's virtio video driver (`libLLVM` init)
unless `LIBVA_DRIVER_NAME=x`, and the playback-pipeline wall-time test fails with no audio sink. Evidence:
[acceptance](docs/acceptance/crtweb_acceptance.md#inherited-runtime-notices-2026-10-20).

### Tranche 10 on Linux/aarch64: the in-tree WebKit gates run natively

`build_webkit_crt_port.py --webkit --run-tests` ran on the aarch64 guest for the first time past B3 and found two more defects: the
`curl-openssl` recipe linked the host's nghttp2/libidn2 (host glibc inside the NetworkProcess; `file://` loads never finished), and ports built
through `crt-cc`/`crt-c++` used outline atomics that the CRT libc does not provide (`libcrtmedia.so` had undefined `__aarch64_ld*`). Fixed by
`--without-nghttp2 --without-libidn2` and `-mno-outline-atomics` for Linux/aarch64 in both wrappers. All gates pass except `media_playback`, which
fails on this guest's audio stack (second PipeWire-Pulse stream stalls); GPU gates were not run. Details in the acceptance document.

## 2026-10-20

### SDK, CRT-CLI and Studio: consolidate the developer-environment design

Consolidated the evolving discussion into three English-language design documents:
`docs/design/sdk_development_contract.md`, `docs/design/crt_cli.md` and
`docs/design/crt_studio.md`. The adopted direction is separate repositories,
a CRT-native CLI/bootstrap before the IDE, stage-selectable immutable SDKs,
source-first port recipes and project locks. Windows host tools/rootfs,
host-vs-target identity, scoped environments, update/recovery and staged joint
gates are explicit. Existing JSON recipes, wrappers and SDK paths remain the
initial implementation basis; superseded host-native/binary-first proposals
are not treated as current policy. Added the documentation index and a terse
Planned entry without changing the active Web work, STATUS or the root README.
This completes design consolidation only, not CLI/Studio implementation,
repository creation or release acceptance.

### CRT/PAL residual defects, Tranche 3: native macOS binding audit

Audited every Mach-O image of the isolated 04/06 SDKs and the in-tree developer SDKs with a new tool
(`tools/audit_macos_bindings.py`: each CRT-owned libc/libm/libdl symbol an image references, and the library `nm -m` says it
is bound to). One defect reproduced: the in-tree `libcrtgfx_gpu.dylib` linked `libSystem` ahead of `libc.dylib`, binding
`pthread_mutex_*`/`pthread_cond_*` to Darwin's 64-byte mutex on the CRT's 40-byte `pthread_mutex_t`; a shared-linked
`gpu_test` trapped in `os_unfair_lock_lock_slow`. Fixed target-locally (CRT libc/libm first for `crtgfx_gpu_shared`, libm
added for `crtgfx_skia_shared`); the isolated-stage SDK was already correct. Classified, not changed: the window backend's
deliberate host allocator (`libcrtgfx.dylib`), libc's `dlsym(RTLD_NEXT)`, math-only bindings of static executables, and the
`curl` tool's static libc next to shared libcurl (40 clean transfer rounds, latent). Shared-consumer runs of the media (11),
gpu, window, UI (5) and zlib/curl/mbedTLS libraries pass with a shared libc. The audit now runs in `verify_dist.py` on
macOS. Details and limits: [triage record](docs/bringup/crt_pal_residual_triage.md#macos-native-binding-audit-2026-10-22).

### Tranche 10 on macOS/arm64: isolated 04 -> 05 -> 06 chain, relink kit, and six macOS-only stage defects

The first isolated macOS `04-gfx-media -> 05-ui -> 06-web` chain since the Tranche 8/9 macOS work (the 2026-10-20 06-web replay had
used an in-tree `05-ui` predecessor). It passed `verify_dist` at every stage and the 06-web SDK is a development candidate with the
source offer and the libcrtmedia relink kit. Defects found and fixed (evidence: `out/tranche10-macos-relink`, logs `stage0{4,5,6}.log`):

- `libcrtmedia` CMake named the in-tree `c_shared` target, which an isolated stage does not have: it uses the predecessor's `libc.dylib`.
- The 04 source asset lacked `crtmedia_exports_macos.txt`; every stage asset lacked `tools/crt_web_release.py` (`verify_dist` imports it).
- Executables that load CRT dylibs linked the static `libc.a` next to `libc.dylib` (two malloc registries): `crtgfx_skia_raster_smoke`
  faulted in `unindex_free_block`, then the installed `gfx-skia` example. Both now name `libc.dylib` first (relinking with
  `CRT_CXX_RUNTIME_LINKAGE=shared` confirmed the cause).
- The isolated 04 stage never enabled `CRTGFX_ENABLE_GPU_BACKEND` on macOS, so its Skia bridge had no Metal shared-image sources and a
  06-web built on it failed to link `crtgfx_skia_shared_create/_query` in `crtweb_gpu_worker`.
- The isolated 04/05 builders did not install the LGPL source offer (`crt_source_offer.install_for_stage`).
- The relink kit could not rewrite a macOS link line and `relink.py` failed on a relative `--out`; a Windows-path test also failed on macOS.

Relinked `libcrtmedia.dylib` from a relocated SDK keeps the 103 exports and runs the zero-copy media demo (see the LGPL guide).

### Tranche 10: the release tool exports 05-ui and 06-web

- `tools/prepare_release_assets.py` now covers all six stages. `05-ui` is checked for the option-ON build (lvgl/freetype/
  ffmpeg/skia dependencies, `libcrtui.a`, `ui-basic`); `06-web` must come from the same-version `05-ui` and carry SBOM,
  security snapshot, source correspondence, source offer and the recorded release decision. The corresponding-source bundle is
  built whenever a `04`/`05`/`06` stage is exported.
- The Web public-release decision is owner-controlled (`web_public_release` in `third_party/crt-runtime/release.json`,
  `tools/crt_web_release.py`): `build_stage_06_web.py` records it, `verify_dist.py` checks it is coherent, and the release tool
  refuses a development candidate for a final tag while allowing it (flagged in the manifest) for a pre-release tag.
- Tests: `test_crt_web_release.py` (7), new cases in `test_prepare_release_assets.py` (30 total) and `test_stage_06_web.py`.
  Guide: [`release_preview.md`](docs/guides/release_preview.md#05-ui-and-06-web).

### Tranche 10: the LGPL source offer is generated, and the release carries the corresponding source

After the owner's choices (the project's issue tracker as contact, a fixed 3-year period, upstream archives not bundled)
the source offer stopped being a draft with placeholders. `tools/crt_source_offer.py` renders
`share/licenses/SOURCE-OFFER.md` from a template, `third_party/crt-runtime/release.json` and the build's own recipes;
stages that ship FFmpeg get it from `create_dist.py`, `06-web` gets it again with WebKit from `build_stage_06_web.py`,
`verify_dist.py` rejects a missing or unresolved one, and `prepare_release_assets.py` adds
`crt-<tag>-corresponding-source.tar.xz` (tracked tree of the release commit plus a manifest naming each upstream archive by
address and SHA-256). The first isolated build with a changed predecessor refused the packaging-only retry, so `sdk06b`
is a fresh full `05 -> 06` build: offer, `verify_dist.py`, HTTP/TLS 10/10 and the installed/relocated observer pass.
The guide `docs/guides/lgpl_source_offer.md` dropped its draft status; the "legal sign-off" gate is now "owner review"
with external legal review optional. Open: Windows and Linux/aarch64 SDK regeneration, offering `05-ui`/`06-web` in the
release tool and the `public_release_allowed` switch.

### Tranche 10: header decisions for the inherited libc and an LGPL source-offer draft

The five import-manifest/header findings of the inherited-runtime notices were compared with their upstreams: the
four header-less imports (`gdtoa_fltrnds.h`, `s_signgam.c`, `s_llrint.c`, `s_llrintf.c`) are byte-identical to Bionic
main and to OpenBSD/FreeBSD and have no header there either, so each is attributed to its origin (Lucent/gdtoa group,
BSD-2-Clause `s_lrint.c` group, FreeBSD msun) and recorded in `third_party/bionic/header_decisions.json` bound to the
file's SHA-256; `gdtoa_support.c` turned out to be a derivative of Bionic's Apache-2.0 `gdtoa_support.cpp`, so it is
reclassified in the import manifest and carries an Apache-2.0 section 4(b) modification notice. The notice generator
renders the decisions into `share/licenses/crt-libc/NOTICE` and the provenance (no open finding left; a changed file
reopens its decision); the macOS SDK stages were regenerated and verified. The LGPL parts of the Web stage (WebKit
2.54.1, FFmpeg 8.1.2) and what is the corresponding source, the replace/relink path and the offer wording are drafted in
`docs/guides/lgpl_source_offer.md` with placeholders and seven decisions for the owner and legal review. Not legal
advice; legal sign-off, hosting/contact for the offer and Windows/aarch64 SDK regeneration remain open.

### Tranche 10: WebKit support policy adopted (G = 60, S = 30)

The owner adopted the support policy in `docs/design/libcrtweb_api_policy.md`: a pinned series may be released for 60 days
after the next stable series first ships, and a re-pin starts within 30 days. `check_webkit_security.py` now computes the
series' superseded age from the upstream release index (new `series_*` fields in `web-security.json`) and replaces the fixed
blocker "upstream support window unconfirmed" with a blocker that appears only past 60 days; `crt_web_release.py` refuses a
public release past the grace period, and the owner cannot waive that through `accepted_blockers`. Live check: 2.54.1 is the
newest stable, not superseded, within grace. Tests: `test_crt_web_release`, `SecurityTests.test_support_window`.

### Tranche 10: what upstream says about the 2.54 support window

Looked up instead of assumed (WebKit documentation, release schedule, release list, advisories, read 2026-10-20): upstream
gives security fixes only to the latest stable branch, does not generally backport, and publishes no duration or end-of-life
date. In practice a series' last micro came 1, 6 and 28 days before its successor (2.48, 2.50, 2.52) and none after; with the
March/September cadence 2.54 should stop receiving fixes around 2.56.0 (about March 2027, unannounced). So the blocker is
not a fact left to confirm but a policy the owner has to choose (how old a pinned series may be at release, how fast CRT
re-pins); the recorded facts are in `libcrtweb/third_party/webkit/recipe.json` and `docs/acceptance/crtweb_acceptance.md`
(Upstream support window of the WPE WebKit stable series). The blocker in `check_webkit_security.py` is unchanged.

### Tranche 10 on Windows/x64: SDKs regenerated with the LGPL source offer

The Windows/x64 SDK stages were regenerated from the merged tree and the isolated `05 -> 06` build repeated from a new
immutable predecessor: all five stages verify (the FFmpeg-shipping `04-gfx-media` and `05-ui` carry the generated
offer), a fresh 8,301-action WebKit build finished in 4,267 s, and the 06-web SDK's offer names FFmpeg 8.1.2 and
WPE WebKit 2.54.1 and passes the stricter `verify_dist.py`. Installed, spaced-path and externally rebuilt browsers pass the
observer with the work root hidden, HTTP/TLS is 10/10 and the export and PE Host ABI audits are clean; the 3,630-entry
inventory is unchanged across the copy. `prepare_release_assets.py` correctly refuses these development candidates
(a release needs a tagged build). Open: Windows/arm64 (needs an arm64 host), a tagged release rehearsal, the 2.54
support-window confirmation, owner review. Evidence: `docs/acceptance/crtweb_acceptance.md` (Windows/x64 SDK regenerated
with the LGPL source offer).

### Tranche 10 on Windows/x64: WebKit 2.54.1 isolated replay and inherited-runtime notices

The in-tree Windows SDK was regenerated with the inherited-runtime notices (five stages, each `verify_dist.py` pass,
`crt-libc/NOTICE` 139/290 identical to Linux and macOS), then a fresh isolated `05 -> 06` build of the 2.54.1 pin ran
end to end from an immutable 3,378-entry predecessor and a clean-tree source asset: 8,301 WebKit actions with the
Windows patch chain (18 of the 23 patches) unchanged, installed browser smoke, observer (141 mapped files, one CRT
C++ runtime per role), HTTP/TLS 10/10, strict PE Host ABI, SPDX with the runtime components and the security snapshot
against the newest micro. A copy at a spaced path and an externally rebuilt browser pass the same checks with the
work root hidden. Nothing in the engine, port or PAL needed a change; the product cache fetch was the pinned 330
changed Source blobs only, and the port archives were the previous work root's hash-checked cache. Two
Linux-host-only unit tests that failed on a Windows host were fixed (`tools/test_stage_06_web.py`). This SDK predates the header
decisions and the generated LGPL source offer that landed the same day: the Windows SDKs are regenerated again with them
before a release. Still open for the Tranche: Windows/arm64 and Linux/aarch64 SDK regeneration, the 2.54 support-window
confirmation. Evidence: `docs/acceptance/crtweb_acceptance.md`
(WebKit 2.54.1 replay on Windows/x64).

### Tranche 10 on macOS/arm64: WebKit 2.54.1 replay and inherited-runtime notices

The in-tree macOS SDK was regenerated with the inherited-runtime notices (five stages, each verified, 139/290 header
inventory identical to Linux), then a fresh isolated `05 -> 06` build on the 2.54.1 pin ran end to end: 8,302 WebKit
actions with the 23 patches unchanged, installed browser, HTTP/TLS 10/10, 37 exports, strict Host ABI, observer
(Host/Web/Network, one CRT C++ image per role), a relocated space-path copy and an external rebuilt consumer with
the work root hidden, SPDX with the runtime components, security snapshot against the newest micro. Nothing in the
engine, port or PAL needed a change. Still open for the Tranche: Windows replay of 2.54.1, aarch64 and Windows SDK
regeneration with the notices, the five header decisions, the LGPL source offer, legal sign-off and the 2.54
support-window confirmation. Evidence: `docs/acceptance/crtweb_acceptance.md` (WebKit 2.54.1 replay on macOS/arm64).

### crt-ui-dist without LVGL fails with its reason

A default configuration (`CRTUI_ENABLE_LVGL=OFF`) built 04-gfx-media and then stopped in `verify_dist.py` on a missing
`examples/ui-basic/main.c`. `crt-ui-dist` is now a target that fails at once and names `-DCRTUI_ENABLE_LVGL=ON` (and the
`crtui-lvgl-fetch` target) when LVGL is off (`cmake/fail_with_message.cmake`); the LVGL-on target is unchanged.
Checked on a clean Linux default configuration; the build with LVGL on still plans `create_dist.py`.

### Tranche 10: driver gate vs the SDK's shared libcurl

The in-tree SDK's shared mbedTLS `libcurl.so.4` (no FILE protocol) shadowed the Web port's OpenSSL curl through the
wrapper-generated RPATH, which hung every `file:` load. `crt-cc`/`crt-c++` now honour `CRT_LINK_RPATH_FIRST`, the
driver sets it on Linux, and the eleven-entry gate passes against the unmodified SDK (`tools/test_link_rpath_first.py`).
Linux/x86_64 only.

### Tranche 10: WebKit 2.54.1 re-pin and inherited-runtime notices

Re-pinned `libcrtweb/third_party/webkit` from 2.54.0 to 2.54.1 (lightweight upstream tag: archive `.asc` and a
0-differing tarball/commit correspondence are the authenticity evidence). The 23 carried patches apply to all
three OS chains; only four manifest hashes changed. New libc surface for WTF thread scheduling
(`getpriority`/`setpriority`, `PRIO_*`, `id_t`, `SCHED_RESET_ON_FORK`); `HighPriorityThreads.h` rename in
`PlatformCRT.cmake`; the lifecycle test's warm-up is now 20 iterations. All eleven Linux/x86_64 gate entries pass.
The replay also exposed that the current in-tree SDK's shared mbedTLS `libcurl.so.4` shadows the port's OpenSSL
curl (no FILE protocol) -- the replay ran with the SDK's shared curl/zlib removed; the driver fix is open.
`create_dist.py` now installs notices and provenance for the inherited runtime (libc, shell, LLVM C++ runtime)
in every stage, `verify_dist.py` requires them, and the Web SBOM lists them. Not run on macOS or Windows.

### Tranche 10: Linux x86_64 host -> aarch64 target cross build

Built an aarch64 `05-ui` predecessor SDK on this host and ran the isolated
`06-web` stage on it; the 8,305-step WebKit build and `verify_dist.py` pass, the
SDK has 162 AArch64 ELF files and no x86-64 file, and `cross-build-evidence.json`
separates host generators (gperf, ICU tools, Perl/Python/Ruby, fontconfig
generators) from target artifacts. Fixes found on the way: SDK `make` (aarch64)
no longer used as a host tool; libc++ gets the target arch and prefers the built
ABI library; pthread-in-libc declared instead of a false try-compile positive;
Wayland scanner built as a native host tool; zlib shared probe/PIC fix (static-only
fallback broke HarfBuzz); Clang's default `libgcc.a` replaced by an explicit
compiler-rt query on Linux. Evidence and limits (no aarch64 execution) are in
docs/acceptance/crtweb_acceptance.md. Uncommitted at this point.

### macOS CI: host-generator working-directory alias comparison

Reproduced the failure in [Actions job 118328631706](https://github.com/webos21/crt/actions/runs/39330934417/job/118328631706):
the serializer fixture successfully generated its output, but compared the
literal temporary build path under `/var/folders/` with `os.getcwd()`'s
canonical `/private/var/folders/` path. They name the same directory; this was
a host-test assertion failure, not a CRT/PAL or generator defect.

The regression now compares directory identity with `Path.samefile()`. A
second fixture explicitly builds through a directory symlink, retaining the
oversized Unicode/special-character argv, adjacent import, following-command,
no-op rebuild and changed-dependency checks. Only that extra symlink fixture
is skipped on Windows, where creating it needs privileges; the original
generator tests still run. Production code and target ABI are unchanged.

Native macOS validation passes all three host-generator cases and
`cmake --workflow --preset macos-host-ninja-debug` passes **139/139** tests.
An initial sandboxed workflow blocked IPv6/Unix socket operations; both pass
in the full native rerun. This is local verification, not a new hosted CI run;
Linux/Windows were not replayed on this Mac. `STATUS.md` remains unchanged.

### Windows Web Tranche 10: fresh isolated SDK and product-hidden space-path acceptance

The v11 official continuation finishes recompilation, dependency audit, API
runtime replay and clean consumer build/install, then stops on Windows CRLF
notice bytes. All 64 WebKit notices verify against the existing inventory;
22 require only CRLF-to-LF conversion. The notice writer now ships the exact
hash-matching bytes without mutating the checkout or accepting other content
changes. Stage regressions pass 38 cases.

An explicitly unpublished diagnostic first checks all candidate gates without
publishing an SDK. The new SHA-pinned development asset then passes the strict
retry proof (190 identical compile inputs and unchanged compile driver) and
the official installed-predecessor bootstrap. Mandatory recomposition,
reconfiguration, 1,368 recompilation steps, API replay, clean browser/GPU-worker
build/install, provenance/security, HTTP/TLS, Host ABI and distribution checks
complete in 1,799.4 s; only then is the final Windows/x64 SDK published.

Separate ordinary non-debugger replay passes installed, actual space-path and
external browsers. A stricter second replay hides product work before external
browser/API configure/build as well as execution. All three process-role
observations, ten HTTP/TLS cases, both export/Host ABI audits and both
distribution verifications pass. The 3,603 SDK byte/alias entries and immutable
3,353-entry predecessor are unchanged; product work is restored and no observed
browser/debugger processes remain. Stage tests pass 38 cases, host-generator
tests 2, and both registered tests in the fresh CMake/CTest directory pass 2/2.
Detailed identities, commands, failed checkpoints and evidence stay in the
[Windows replay record](docs/acceptance/crtweb_acceptance.md#fresh-windows-full-stage-replay-after-pal-fixes).
This closes native Windows development acceptance only: the asset is a local
uncommitted snapshot, not a clean f03626c/public release; cross-target and
release notice/source/SBOM/security gates remain in Tranche 10.
The direct API diagnostic's generated root-level data is moved into out for
recoverable evidence retention, not left as untracked project input.

## 2026-10-19

### Windows Web Tranche 10: renewed input and host-tool checkpoint

The intermediate checkpoints below are superseded by the accepted 2026-10-20
development SDK above; their failed identities remain separate evidence.

Verified the PAL-fixed immutable predecessor's complete inventory/runtime
identity and reviewed media exports, and revalidated the pristine source-only
WebKit cache. Created a new clean-f03626c source asset and started the official
installed-SDK bootstrap in a fresh work root. Native build-host gperf and ICU
generators now build afresh outside the target SDK; their versions, paths,
host-only Perl module inputs and runtime DLL hashes are recorded. Target ports,
WebKit, packaged validation and product-hidden space-path replay remain in
progress, not accepted from this checkpoint. The owning evidence is the
[fresh Windows replay](docs/acceptance/crtweb_acceptance.md#fresh-windows-full-stage-replay-after-pal-fixes).

The first attempt additionally isolates an external diagnostic-tool false
stop: fontconfig's CRT xargs PATH probe triggers a native WinRT debugger
notification. The same probe passes without debugging, fails with the old
debugger and passes with corrected notification handling. Only the local
diagnostic tool changes; identical pinned target inputs are resumed through
the official work-root reuse contract. This is not full-stage acceptance.

The resumed clean-work-root pass completes all selected target ports and
their recipe-declared runtime probes. Windows OpenSSL/curl remain explicitly
static-only. WebKit product compilation, publication and installed/relocated
consumer acceptance are still open; measured evidence stays in the linked
acceptance section.

Fresh WebKit compilation exposes a separate Windows build-host command limit:
the serializer batch invocation exceeds 35K characters. The CRT-owned CMake
hook now dispatches only that generator via UTF-8 JSON argv and in-process
Python, without changing pinned upstream sources or the target ABI. Oversized
and special-character arguments, dependencies, unchanged/rebuilt outputs and
ordinary custom commands pass the regression; the real 309-argument WebKit
generator emits all 20 expected files in a separate spaced directory. This
regression joins the existing CTest run. Because build inputs changed, a new
development source asset and entirely empty work root start the full replay
again; the former object tree remains failed evidence, never accepted cache.

The new full run completes all 8,300 product compile/link steps, and its
separate build-tree C API test passes. A subsequent audit false rejection
of CRT /c drive spelling is repaired without weakening containment: 175
actual discovery records and 37 stage regressions pass. The existing
packaging-retry verifier proves all 190 compile inputs and the compile-driver
AST unchanged before the audit-only source-asset continuation. SDK publication
and product-hidden relocation remain open, not inferred from compilation.

### Windows nested fork: copy the actual committed stack, not stale TEB limits

Fresh libwebp configure exposed a reproducible PC=0 execute fault after the
HANDLE/RNG/DIR fixes. A full dump, a normal corrected debugger replay and
bounded before-ResumeThread/NtContinue observations establish the omitted
return slot: the memory-copy child retains its initial native TEB StackLimit
(0x15f000), above the nested fork's live SP (0x15db90). Its next child never
receives the return address at 0x15ebb8; the native loader commits zero-filled
pages before resuming the otherwise valid saved PC.

Both x64 and arm64 fork backends now walk the parent stack reservation's
VirtualQueryEx regions to locate the contiguous committed readable extent
containing the saved SP. They copy that extent, including existing loader
headroom, and reject child-reservation clamping that would drop live frames.
No arbitrary stack enlargement, native loader redesign or upstream patch is
used. Guard-page preservation/automatic future stack growth remains a
separate, documented limit.

The nested-fork regression preserves a 16 KiB live marker with already
committed headroom so it tests copy completeness, not the guard-growth limit.
The old stack implementation fails; the final implementation passes 100
repetitions together with the HANDLE/RNG/DIR regressions. Full C stage passes
156 tests with one expected skip. The final ARM64 backend and test compile
with the strict CI-shaped flags; native arm64 execution remains pending.
The final option-ON predecessor matches freshly built libc/mksh/make and its
3,353-entry inventory remains unchanged. Fresh libwebp configure (87.9 s),
make -j4 (92.3 s), install (14.7 s) and both static/shared round trips pass
in a separate execution SDK/work root. The corrected native debugger observes
26,782 processes without access violations or invalid-handle events.
Full isolated Windows 06-web/relocation replay and the distinct historical
RtlAllocateHeap caller remain separate acceptance gates; evidence,
discarded diagnostic attempts and limitations are in
[the investigation](docs/bringup/windows_mksh_tranche10_crash.md).

### Windows fork: rebind process-owned directory search state

A minimal old-SDK DIR consumer faults in FindNextFileA/RtlEnterCriticalSection
after fork because the copied DIR references the parent's opaque native
search state. Windows directory streams now track a private low-level fork
epoch, reopen on child read and replay the copied logical entry count.
Child rewind creates owned search state; child close never FindCloses the
borrowed pointer. Raw _Fork is covered without depending on atfork hooks.
No upstream patch or public header/ABI change is made.

The permanent Windows fork-runtime regression covers opendir/fdopendir,
fork/_Fork, copied position, direct rewind/close, repeated EOF and unchanged
parent position in a private three-file fixture. The same regression faults
with the old dirent implementation and passes 100 consecutive repetitions
with the fix; the minimal reproducer also passes. C stage: 156 passed, one
expected native-COW skip. PAL, dirent and test strict Windows/arm64 source
compilation passes; native arm64 execution remains for CI/device replay.

Replay is linear in entries already read, not a native directory snapshot:
mutation/order changes are not guaranteed, and a renamed/unavailable saved
path yields EIO. The historical RtlAllocateHeap fault is still a separate
unproven causal connection. Fresh package replay remains open; see
[the investigation](docs/bringup/windows_mksh_tranche10_crash.md).

### Windows fork: rebind the native RNG cache in the child process

Isolated an additional, deterministic unknown-module access violation:
after the parent initializes `/dev/urandom`, the copied SystemFunction036
pointer targets cryptbase.dll, which is absent from the fork child. A full
dump shows an execute fault in an unmapped region from `__crt_sys_read`.
The minimal fixture has no unreaped sibling registry, separating this
defect from the borrowed wait-HANDLE cleanup bug below.

The RNG cache now publishes a PID and function pointer atomically, owns a
native DLL reference in each process, and rebinds before inherited random
descriptor reads as well as new opens. Extended `fork_runtime_reset_test`
covers inherited reads and getrandom after both fork and raw _Fork; all
100 repetitions pass. The same minimal old-SDK binary faults, whereas a
recompile against the fixed runtime loads cryptbase in the child and exits
normally. Windows core remains 156 passed / one expected skip, and both
the PAL source and regression compile for Windows/arm64. No upstream
source, public API, CI or Linux/macOS backend is changed. Native arm64
runtime execution remains for CI/device replay.

This does not prove that the separate historical RtlAllocateHeap read
fault has the same cause. Its bounded direct native-heap fork probe also
passes on the old SDK, so that probe is not claimed as a regression for
the heap incident. Further package replay and limitations belong to
[the investigation](docs/bringup/windows_mksh_tranche10_crash.md).

### Windows memory-copy fork: preserve child-owned HANDLEs during fd reset

Native mksh debugging exposed a definite ownership defect: copied parent
process-wait HANDLE numbers were closed in the memory-copy child, although
CreateProcess never inherited those private handles. Values can already name
unrelated child loader/signal objects. The child now clears the registry
without closing borrowed numbers; the legacy NT clone keeps its inherited
handle cleanup. No public ABI, upstream source or Linux/macOS backend changed.

Added a borrowed-close guard to `fork_runtime_reset_test`:
the test fails against the original implementation, passes after the fix,
and passes 100 consecutive repetitions. Test-only CloseHandle IAT
interposition observes the forbidden operation without depending on Windows
reusing a particular handle number; other closes delegate to kernel32.
The earlier event-collision fixture was replaced after a final repeat
exposed that allocation-policy assumption. Relinked C-stage executables pass
156 tests with one expected native-COW skip; eight focused shell/fork/process
cases pass. Windows/arm64 source compilation passes, not a native execution
claim. Fixed mksh also passes a 1,772-process parallel install-sh diagnostic
and the retained libwebp make install (1,361 observed processes), without
observed invalid-handle/access-violation exceptions.

The original SDK additionally reproduced `RtlAllocateHeap + 0x27`
(`ntdll + 0x31577`) accessing `0x20010`. Its causal connection to the HANDLE defect,
and the unknown-module/zero-offset event class, remain unproven; the limited
minidump lacks a decisive native caller stack. Windows Tranche 10 remains
open pending a new immutable predecessor and fresh isolated/relocation
replay. Detailed source reasoning, commands and diagnostic limitations are
in [the investigation](docs/bringup/windows_mksh_tranche10_crash.md).

### Web Tranche 10: Windows SDK port-tool fixes and runtime blocker evidence

Fixed recipe PATH prepending for Windows CRT mksh (POSIX drive spelling and
separator), bound fontconfig's bare/configured gperf calls to the native host
generator through an owned relay, and selected external LLVM AR for SDK-mode
CMake ports rather than a repository-only wrapper. Fontconfig's real shared
font match, HarfBuzz shared shaping, libjpeg static/shared round trips and
libpsl shared suffix smokes pass in the retained diagnostic tree. Pinned LF
bytes for ICU's owned options/relay files so evidence hashes match Windows
file bytes. All 83 focused regression cases pass.

The full Windows SDK replay is **not** complete: libwebp header installation
repeats libpng's earlier `Error 5`, and task-specific Windows error records
confirm mksh `0xc0000005` access violations. Fork/exec/PAL restoration is a
candidate, not a stack-proven root cause. Recorded the failed checkpoints,
preserved source-stage identities and revoked reuse of out-of-band diagnostic
trees. No Windows `06-web` SDK is published and no active build is left running.
The immutable predecessor is retained; no core runtime or upstream source was
changed. Native runtime triage must precede a fresh final stage build. Evidence
and limits are in
[Windows blocker](docs/acceptance/crtweb_acceptance.md#current-windows-blocker-2026-10-19).
`TODO.md` keeps that gate open; `STATUS.md` is unchanged.

### Web Tranche 10: Windows host-generator / target ICU boundary

Separated WebKit's native MinGW gperf/ICU generators from CRT target ports, with
host-only compiler-runtime DLL adjacency and the configured Git shell. The
owned pkgdata bridge preserves generated options, calls only CRT target
wrappers and completes MinGW pkgdata's omitted shared-data installation.
The missing install had left a small stub DLL in the prefix: replacing it
with the actual generated data DLL made the same static/shared Unicode
consumers pass. Fresh official source-stage v4 now passes both consumers too.
No host compiler/runtime is used for the target data and no upstream source
or core runtime change was needed.

Fresh SDK-only browser/GPU-worker compile/link preflight also passes after
CMake path spelling and Skia consumer-header configuration fixes; execution
is still pending. Focused helper tests pass 77 cases. Detailed checkpoints,
source identities and logs are in
[Windows T10 acceptance](docs/acceptance/crtweb_acceptance.md#windowsx64-isolated-build-and-relocation-replay-in-progress-2026-10-18).
This is not completion of the isolated Windows SDK or Tranche 10. `STATUS.md`
is unchanged.

### Web Tranche 10: native host Perl / CRT OpenSSL recipe replay

Fixed packaged-SDK configure-root substitution for native build-host
interpreters. OpenSSL's Unix CRT target additionally needs POSIX-path Perl,
its build-host modules and the normal `crt-native-tool` generator boundary;
direct native Perl invocation from CRT make lost output redirections.
With explicit host-tool inputs the original recipe configures, builds,
installs and passes its TLS 1.3 static smoke in a fresh diagnostic work
directory. The documented Windows shared-build exclusion remains unchanged.
Recipe status and the porting matrix now reflect this native static proof.

This resolves the OpenSSL diagnostic, not Windows source-stage acceptance:
these changed compile inputs require a fresh official source asset/work-root.
Detailed failure checkpoints, host-tool prerequisites and the successful
`out/web-windows-tranche10-openssl-diagnostic-v5.log` are recorded in
[Windows T10 acceptance](docs/acceptance/crtweb_acceptance.md#windowsx64-isolated-build-and-relocation-replay-in-progress-2026-10-18).
No upstream or CRT runtime source was patched; `STATUS.md` is unchanged.

## 2026-10-18

### Web Tranche 10: Windows native media DLL export boundary

The existing Windows configuration had CURL disabled and was correctly rejected
as an incomplete isolated Web predecessor. Built the original CRT curl/mbedTLS
recipes with static/shared HTTP/HTTPS smokes, enabled the complete predecessor
configuration and republished/verified `01-c`..`05-ui`. Its immutable input copy
matches the original inventory. The generated media `.def` now has native proof:
exactly 75 public exports, a real DLL-import consumer resolves every name and
passes 100 frame geometry/ownership-callback/release cycles. Cross-generated
PE fixtures are no longer the only evidence for this boundary.

This closes only the Windows media export sub-gate. Isolated Windows `06-web`
build, package/relocation and browser/network/Host ABI replay remain in progress;
cross-target, inherited notices and stable-pin/support release gates remain open.
Logs, host-tool setup findings and current build entry are in
[Windows T10 acceptance](docs/acceptance/crtweb_acceptance.md#windowsx64-isolated-build-and-relocation-replay-in-progress-2026-10-18).
No core runtime/public ABI/upstream source patch was changed. `STATUS.md` is unchanged.

### Web Tranche 10: macOS/arm64 isolated development SDK replay

Implemented the native macOS `05-ui -> 06-web` source-stage path: SDK-only
application/Metal-worker link interfaces, private versioned dylib families,
ordered portable install names/RPATHs and ad-hoc signing. Preserved
`libWebKit.dylib.SOVERSION` and its relative alias; separated RPATH deletion
and addition for Apple's tool. WebKit's static OpenSSL curl remains private,
and the inherited media curl/runtime images are unchanged. Added native
checkpointed ps/vmmap observation, using the existing strictly PID-scoped,
unlinked/non-executable CRT IPC classification. Incomplete or non-pristine
source caches now fail before the expensive dependency phase.

The official final asset produces `out/tranche10-macos-20261018/sdk06` from an
immutable installed option-ON predecessor. Dependencies pass 21 native smokes;
a fresh 8,301-action WebKit build and final 1,369-action incremental rebuild
pass C API execution. Installed/space-path-relocated SDKs and an SDK-only
external consumer pass load/script/exact pixels, three-role mapping/cleanup,
Host ABI and distribution verification with the entire original work tree
hidden. HTTP/TLS passes 10/10 before publication and again after relocation.
The 3,532-entry SDK inventory is unchanged by copying/runtime checks; input
SDK and compiler wrappers are unchanged. Public exports are exactly 37, with
no private curl export/shared-curl dependency. C-stage 138/138 and 113 focused
tests (112 pass, one expected Darwin-nm/Windows-export fixture skip) pass.

Failed cache/version-alias/RPATH/IPC-observer checkpoints and diagnostic SDKs
remain separate from official acceptance. No new upstream patch, public ABI,
runtime/backend policy or acceptance bound changed. This closes the macOS
development-package slice, not Tranche 10/public release: Windows packaging,
cross-target proof, inherited-runtime notices and stable-pin/support closure
remain open. Detailed inputs, hashes, commands and limits are in
[macOS T10 acceptance](docs/acceptance/crtweb_acceptance.md#macosarm64-isolated-build-and-relocation-replay-2026-10-18).
`STATUS.md` is unchanged.

### Web Tranche 10: Linux-first isolated development SDK

Implemented the installed `05-ui -> 06-web` source stage, complete predecessor
link interface, separate-process browser example and packaged GPU worker.
Fixed the shared-CRT GNU linker TLS import with a narrow link-only loader stub
and positive/negative runtime tests. Preserved LLVM runtime aliases through
stage copying/publication, isolated browser VA stubs and private static OpenSSL
curl, and kept the application/GPU worker on its accepted static CRT runtime.
Windows media now generates exports from the 75 public API names; both PE
driver fixtures pass, but native Windows acceptance remains open.

The official Linux entrypoint produces `out/tranche10/sdk06-v10`. Installed and
space-path-relocated SDKs pass browser load/script/pixels, three-role mapping
and cleanup, HTTP/TLS 10/10 and distribution/Host ABI audits. An external sample
rebuilt solely with the installed SDK also passes with the original build tree
hidden. Input SDK/compiler wrappers stay unchanged; the 3,583-entry output
inventory is identical after copying. C-stage 138/138, 75 focused distribution
cases (one expected skip), 15 licensing and six Web helper tests pass.

The candidate includes checked Web notices, source correspondence, SPDX 2.3
composition and a fail-closed advisory snapshot. This closes the Linux
development-package slice, **not Tranche 10 or public release**: other-host
packaging implementation/replay, aarch64 cross proof, inherited-runtime notice
closure and current stable pin/support decisions remain. Detailed inputs,
checkpoint separation, commands, hashes and limits belong to
[Linux T10 acceptance](docs/acceptance/crtweb_acceptance.md#linuxx86_64-isolated-build-and-relocation-replay-2026-10-18).
`STATUS.md` remains unchanged.

### Web Tranche 9: Windows/x64 native replay and three-host closure

Replayed `9b76335` on Windows/x64 with the current CRT SDK, native D3D12
worker/consumers and a 2,152-step incremental browser rebuild. The initial
unread-transfer fixture counted cold Winsock initialization as a handle leak.
A GPU-free socketpair/close probe grows once and stays flat; the Windows-only
test now initializes and closes its transport before the resource baseline.
The original +2 handle/thread bounds remain, with explicit before/after
diagnostics. No runtime implementation, public ABI, upstream patch or GPU
fallback/loss policy changed. Failed checkpoints remain separate from passes.

The official 9C entry passes. Final immutable-artifact replay passes strict
9C **3/3** (84 complete worker sessions), 9A and 9B each **offscreen 3/3 +
native-window 3/3**, CPU WebView recovery/twenty lifetimes and B7 **100/100**.
9C RSS is **-8/+120/+196 KiB**, threads **+2** and handles **+0** per run,
within the unchanged bounds. Static/runtime Host ABI passes, all fourteen
selected artifact hashes stay unchanged and no test/browser/worker remains.
The refreshed C-stage has **156 passes / 1 expected native-COW skip**; native
PAL/wire/SurfaceView **5/5** and focused helpers **68/68** pass. Default
no-browser GPU CTests remain skips, not acceptance.

Together with the accepted Linux and macOS replays, this closes **9C and
Tranche 9**. The boundary is the CRT-owned GPU worker and controlled context/
logical-device owner recovery, not upstream GPUProcess, accelerated WebCore,
physical GPU reset or arbitrary hung-driver recovery. Tranche 10 isolated
`06-web` packaging/security remains open. Commands, measurements and retained
evidence belong to [Windows 9C acceptance](docs/acceptance/crtweb_acceptance.md#9c-windowsx64-native-replay-2026-10-18).
`STATUS.md` is unchanged; no Linux/macOS execution or physical audio/camera
test is claimed by this Windows replay.

### Web Tranche 9C: macOS/arm64 Metal native replay

Native replay ran on October 17; this record was finalized on October 18.
Replayed `8154c8c` on an Apple M1 Pro with macOS 27.0.1. No runtime,
upstream patch, public ABI, fallback policy or acceptance limit needed changing.
The official `--run-gpu-lifecycle-tests` entry reconfigured and rebuilt 2,158
browser/JSC/Skia steps, linked WebKit, Web/Network processes and `crtweb_host`,
and passed the native 9C gate. B7's executable was also refreshed; the earlier
reused-browser checkpoint remains separate from final acceptance.

On those rebuilt artifacts, strict Metal 9C passes **3/3**, with twenty
measured complete lifetimes, seven policy warm-ups and final fresh pixels per
run. RSS is **+816/+752/+656 KiB**, threads and descriptors **+0**; all observed
browser descendants and workers are reclaimed. Native mapping/Host ABI audits
pass. This is controlled context abandonment plus owner teardown/recreation,
not physical GPU reset or arbitrary hung-driver recovery.

Final 9A and 9B each pass offscreen **3/3** and native-window **3/3** (5/5 frames
each). CPU WebView recovery/twenty lifetimes are flat; refreshed B7 passes
**100/100**, **+32 KiB/+0 threads/+0 descriptors**. C-stage CTests **138/138**,
native PAL/wire/SurfaceView **5/5**, shared Metal PAL, 59 focused unit tests,
SDK `01-c`..`05-ui` verification and the unchanged patch manifest pass.
Missing-prerequisite CTest entries remain **two skips**, not GPU passes.

Detailed bounds, immutable artifact hashes, native observations, reproduction
and checkpoint separation belong to
[macOS 9C acceptance](docs/acceptance/crtweb_acceptance.md#9c-macosarm64-metal-native-replay-2026-10-17).
At that checkpoint only Windows/x64 9C remained; its completion is recorded above.
No Linux/Windows execution, `06-web` packaging or physical camera/audio
validation is claimed here. `STATUS.md` is unchanged.

## 2026-10-17

### Web Tranche 9C: Linux owned-worker loss/recovery and complete lifetimes

Confirmed the separate CRT GPU worker as the product GPU-process boundary;
upstream GPUProcess/accelerated WebCore is not claimed. Added lost-context
image/ACK rejection, BYE plus bounded transfer-FD draining without new imports,
and explicit fresh-session recovery. Worker EOF retires its pool; browser
ownership is discharged before GPU drains, and Web-process loss reloads with a
retry/deadline bound. Device teardown stays after borrowed-resource/context
destruction, not under live native handles.

The independent strict 9C mode passes Linux **3/3**, each with twenty measured
complete lifetimes, seven fault-policy warm-ups and final fresh pixels. RSS is
**+20/+20/+20 KiB**, thread/fd deltas zero; descendant cleanup and native Host
ABI/mapping audits pass with unchanged tested artifacts. Added deterministic
unread-native-transfer cleanup and fail-closed evidence tests. CPU WebView,
9A/9B native-window, PAL/wire/SurfaceView and SDK verification regressions pass.
The initial interleaved map-log failure is preserved, not counted as acceptance.
Detailed bounds, the controlled-loss (not physical-reset) distinction and
native replay entry belong to
[Linux 9C acceptance](docs/acceptance/crtweb_acceptance.md#9c-owned-worker-lifecycle-linux-implementationreplay-2026-10-17).
At that checkpoint Windows/macOS still needed the new 9C mode; macOS's
completed replay and the later Windows/global 9C closure are recorded above.
`STATUS.md` and the public C/v1 wire contracts are unchanged.

### Linux native fork/spawn heap-snapshot replay

Replayed the synchronized native-COW allocator guard from `71f79c9` on
Linux/x86_64. Fresh configure/build, the deterministic real-allocator regression
**10/10**, and the full C-stage **138/138** pass. No Linux fix, public ABI or
Windows memory-copy fork change was needed. This closes the separate queued
Linux replay, not the three-host GPU-worker lifecycle gate. Evidence and
reproduction belong to
[Linux fork replay](docs/acceptance/crtweb_acceptance.md#linux-native-forkspawn-replay-2026-10-17).

### macOS B7 follow-up: stabilize the native fork/spawn heap snapshot

The retained October 16 spawn-child crash points at the allocator free-list
while exporting the framed-socket environment before exec. Inspection found
native fork taking an unguarded heap snapshot and native spawn additionally
skipping CRT child reset. A new fixture that includes the real allocator holds
its private lock in another thread: the pre-fix binary fails because fork
creates a child inside that critical section. Resetting the child's lock alone
does not stabilize copied heap metadata.

Linux/macOS now lock the heap around native fork, release it in the parent
on success/error and reset it in the child. Application atfork callbacks stay
outside that lock; spawn uses runtime-only child reset without application
callbacks. `_Fork()`, the public ABI and Windows memory-copy fork stay unchanged.
The direction follows Bionic's fork boundary and Android Scudo's allocator
snapshot locking; references and scope limits are in
[Process Fork Model](docs/design/process_fork.md).

macOS/arm64: C-stage CTests **138/138**, fresh-CMake regression **10/10**, and
shared-CRT B7 **3 x 100/100** lifetimes pass. RSS is **+32/+48/+32 KiB**, threads
and descriptors **+0**, at most two children. Every run passes native
UI/Web/Network mapping and static Host ABI audits. SDK `01-c`..`05-ui`
regeneration/verification pass; WebKit's malloc/free/realloc/calloc/spawn
bindings point to CRT libc. Linux/Windows x86_64/aarch64 syntax checks pass,
not native execution. Linux runtime replay was still queued at that checkpoint;
the completed replay is recorded above.

The deterministic fixture exposes the missing synchronization, not the exact
historical invalid-pointer instruction; the 300 clean B7 lifetimes are bounded
replay evidence, not proof against every allocator defect. Original failures
are retained. Detailed evidence and reproduction are in
[B7 allocator follow-up](docs/acceptance/crtweb_acceptance.md#b7-native-forkspawn-allocator-follow-up-2026-10-17).
9C remains open; `STATUS.md` is unchanged.

### Web Tranche 9B: Metal shared resources and macOS native acceptance

Implemented and replayed on macOS/arm64 on October 16; static/shared PAL and
99 unit tests passed again on October 17, with no remaining native test/browser
processes. Final results and open work are reconciled. Metal now uses IOSurface BGRA8 textures and
two native MTLSharedEvents, with monotonic ready/release epochs on the Ganesh
queue. Two CRT capability/lease descriptors redeem independent Mach references;
private native SDK code is separated from the C99 Bionic descriptor/thread
transport. No WebKit patch, upstream pin, frozen v1 packet or public C API changed.

Strict browser offscreen 3/3 and native-window 3/3 (5/5 frames each), static
and shared PAL twenty-epoch/retained-resource tests, wire/CPU CTests 4/4,
six negative controls, 99 unit tests and all five SDK verifications pass.
Browser Host ABI and native process/mapping audits pass in all six strict
runs. Worker -> application is shared GPU memory; WebCore remains CPU-rendered.
Detailed bounds, evidence, reproduction and retained failures are in
[Metal 9B acceptance](docs/acceptance/crtweb_acceptance.md#9b-macosarm64-metal-shared-resource-implementation-2026-10-16).

The shared PAL initially bound Bionic socket/pthread calls to libSystem;
CRT-first linking fixes this, verified with native Mach-O fixups. SDK
publication now declares IOSurface, normalizes Darwin archive indexes with
`ranlib -D`, and orders libc++ configure after the packaged C publisher, without
weakening predecessor hashes. Stage-source closure and generated link-order
tests cover the isolated build path; a full isolated product build is not claimed.

The exporter must remain alive until import completes. Imported references
survive transfer-FD closure, but producer/device loss recovery remains 9C.
At that checkpoint, the intermittent B7 spawn/allocator failure and `06-web`
packaging remained open; the focused B7 follow-up is recorded above.
Linux/Windows were not re-executed here; `STATUS.md` is unchanged.

## 2026-10-16

### Web Tranche 9B: macOS availability replay, not native resource acceptance

Rebuilt the pulled `436e7d9` worker/consumer, shared Skia objects and SDKs on
macOS/arm64. This confirms a missing implementation, not a completed Metal
resource replay: the native factories are unsupported, the descriptor has no
Metal handle form, the resource test has no native Mac observer and the strict
driver refuses the host. Native probes return 77 ordinarily / 1 strictly;
explicit worker fallback returns CPU mode with zero resources. Neither skip
nor fallback was counted as 9B acceptance.

Shared wire/CPU surface CTests pass 4/4; Metal 9A offscreen 3/3 and real window
3/3 (5/5 frames each) pass with strict browser mapping/Host ABI evidence.
CPU WebView recovery and twenty lifetimes pass. B7 first fails after 82
lifetimes, with an OS crash report identifying a spawn child's allocator
failure while exporting the framed-socket environment; an unmodified repeat
passes 100 lifetimes. The initial failure is retained, not relabelled green,
and a focused spawn/allocator investigation remains in `TODO.md`.

SDK `01-c`..`05-ui` verification, 53 unit tests and the unchanged patch manifest
pass. Detailed bounds, native mapping counts, retained failures, evidence and
the four prerequisites for Metal resource acceptance belong to
[macOS 9B availability replay](docs/acceptance/crtweb_acceptance.md#9b-macosarm64-availability-replay-2026-10-16).
No runtime code/ABI or acceptance limit was changed at that checkpoint;
`STATUS.md` was untouched. Metal 9B, 9C and `06-web` packaging remained open;
the subsequent Metal implementation is recorded above.

### Web Tranche 9B on Windows/x64: D3D12 shared texture and fences, strict replay

Windows had no resource backend (`skia_shared.cc` was the unsupported stub), so this implements one behind the
unchanged `crtgfx_skia_shared_image` interface and the v2 wire, then replays the strict 9B gate. A producer
creates a committed shared texture (`ALLOW_SIMULTANEOUS_ACCESS`) and two shared `ID3D12Fence` objects; the
consumer opens them on its own device with `OpenSharedHandle`. A fence is both the GPU wait and the CPU wait and
only counts up, so an epoch is a value and a stale signal cannot satisfy a later epoch; each side records an
explicit barrier to COMMON on its direct queue before it signals. The three NT handles ride the CRT's
`SCM_RIGHTS` as descriptors through a new PAL hook, `__crt_windows_fd_adopt_handle()`; the descriptor and
BUFFER_ADD carry a backend-specific handle count (five Vulkan, three D3D12) and an adapter identity.

Accepted: offscreen 3/3 and native D3D12 window 3/3 (5/5 frames each) through the checked-in strict driver on an
Intel UHD 630, 24 worker uploads, 23 reuses, 0 GPU readbacks, 6 imports / 3 live / generation 2, UI +4 KiB RSS
and -1 handle, worker +116..+204 KiB and +0 handles, browser Host ABI and module audits clean. Negative controls:
exit 77/1/2, and a real mismatch (application on the software adapter) negotiates `mode=cpu-fallback` with zero
resources that the strict run refuses. The 9A gate, the B3-C8/Tranche 5 gate and `ctest` (208/208) pass on the same build.

Found by the run: the first identity was all zeros (a 4-byte `wchar_t` translation unit reads a real
`DXGI_ADAPTER_DESC1` at the wrong offsets, so the mismatch tests passed vacuously; now read in `gpu_win32.c`);
`GrD3DTextureResourceInfo`'s pointer constructor adopts without `AddRef`, so the wrapped shared resource was
released early (SIGSEGV); `WebView::launch()` used `/tmp` when `TMPDIR` was unset; the Skia object libraries raced
the `dist/02-cxx` publisher. Details and limits are in
[Windows 9B acceptance](docs/acceptance/crtweb_acceptance.md#9b-windowsx64-d3d12-shared-resource-implementation-and-replay-2026-10-16).
At this checkpoint, macOS Metal and 9C process/device-loss closure remained open;
Linux and macOS had not re-run the shared descriptor/wire/test edits. The macOS
availability replay is recorded above; no `06-web` packaging or global Tranche 9 completion is claimed.

### Web Tranche 9B: Linux shared GPU resources and owned producer

Implemented the Linux resource slice over a separate v2 channel, preserving
the frozen CPU v1 wire/public C API. A statically CRT/SDK-Skia-linked GPU worker
owns an ordinary CPU WebView and separate browser processes. It uploads the
browser pixels once into an exportable Vulkan image; the application imports
native memory and composes it without a CPU upload. Only worker -> app is
shared GPU memory, not GPU WebCore or upstream WebKit GPUProcess execution.
Fixed-width metadata and five owned descriptors carry memory plus native
ready/release semaphore/fence references. UUID/allocation validation, consumed
fence epochs, exact serial/generation ownership and bounded pools prevent CPU
ACKs or stale completion signals from permitting overwrite.

Offscreen 3/3 and native-window 3/3 (5/5 frames each), twenty alternating
updates, stalled-consumer immutability, resize retirement, descendant teardown,
Host ABI/browser mappings, real unsupported/fallback negative controls and
two-device PAL/wire tests pass. CPU WebView and 9A regressions pass. Fresh GPU-ON
build verifies all five SDKs; GPU-OFF object compilation verifies unsupported
stubs. Added isolated-source closure and strict resource-evidence guards.
Detailed measurements, failed fixture checkpoints, reuse of unchanged browser
binaries, reproduction and limits belong to
[Linux 9B acceptance](docs/acceptance/crtweb_acceptance.md#9b-linux-shared-resource-implementation-and-replay-2026-10-16).
Windows/macOS resource backends and 9C process/device-loss/GPUProcess closure
remain open; no `06-web` packaging or global Tranche 9 completion is claimed.

### Web Tranche 9A: macOS Metal final-compositor replay

Replayed `3b09d45` plus acceptance-observer/build-environment fixes on macOS/arm64.
Metal/Ganesh offscreen **3/3** and native presentation **3/3** pass, each live run
presenting **5/5** frames. The public strict CLI passes independently. Pixels,
clip/opacity/ordering, input, viewport resize, retained old CPU source, twenty
updates, producer replacement and cleanup pass with unchanged bounds. This is
CPU-shared-memory upload, not GPU WebCore rendering or zero-copy/GPU-buffer IPC.
Detailed measurements and reproduction belong to
[macOS 9A acceptance](docs/acceptance/crtweb_acceptance.md#9a-macosarm64-replay-2026-10-16).

Added an acceptance-only external vmmap checkpoint, including native PID/name/
parent validation for every browser role. Missing observations fail closed.
Real region rows exclude report metadata; only the CRT's PID-scoped, unlinked,
non-executable temporary memfd data is classified separately. Foreign images
remain violations. Apple's shared-cache inventory stays separate from the
static Mach-O dependency firewall; it is not proof of GPU browser rendering.

The initial public entry failed before GPU execution: consumer shared-runtime
environment hints contaminated the nested libc++ build with an absolute
predecessor RPATH. The runtime builder now removes those hints and invalidates
old environment-dependent build trees once. A second checkpoint passes GPU
rendering but catches observer metadata/memfd misclassification; both failed
checkpoints are retained. No runtime implementation, public ABI, source pin,
WebKit patch or acceptance threshold changed. Refreshed SDKs `01-c` through
`05-ui` all verify after the build fix.

CPU WebView passes three recoveries and twenty lifetimes (+0 KiB/+0 threads/+0
descriptors); B7 passes one hundred lifetimes (+32 KiB/+0/+0). Browser static/
runtime audits and the public header/export firewall pass. Wire/CPU surface
CTest passes 3/3; GPU evidence/observer tests 17/17, backend boundary 7/7,
device evidence 18/18, libc++ predecessor/environment 6/6 and fetch 1/1,
driver/JSC helpers 2/2 and 9/9, source closure 7/7 and licensing 15/15 pass;
the manifest remains 23 patches/63 modified/24 CRT-owned files. No remaining
test/browser process, audio output or physical camera use. This closes the
three-host 9A baseline, not 9B/9C or `06-web` packaging. Linux/Windows were not
natively rerun here for the host-neutral runtime-build environment fix.
`STATUS.md` is intentionally untouched.

### Web Tranche 9A: Windows D3D12 final-compositor replay

Replayed `332e904` on Windows/x64: offscreen **3/3** and native D3D12/Ganesh
presentation **3/3**, each live execution displaying **5/5** frames. The real
browser's CPU-shared-memory image remains a CPU-upload source, not GPU-buffer
IPC, zero-copy or GPU WebCore rendering. Pixels, UI ordering/ancestor clipping/
opacity, input, resize/old-image retention, twenty updates, producer loss/
replacement and teardown pass with the original bounds. No audio or physical
camera was opened. Measurements and log locations belong to
[Windows 9A acceptance](docs/acceptance/crtweb_acceptance.md#9a-windowsx64-replay-2026-10-1516).

The initial fresh combined SDK/consumer build exposed a real publisher/reader
race: `crt-libcxx-dist` replaced `02-cxx` while Web consumers read libc++ headers.
Order all three consumers after that publisher. A second fresh configure and
the same parallel `-j 8` aggregate build pass; cumulative SDKs `01-c` through
`05-ui` verify. The failed build is retained, not counted as acceptance.

Added acceptance-only Windows Toolhelp module observations and strict parser
coverage: all three browser roles are required; unknown libraries and native
final-compositor GPU clients fail, while CRT GL link stubs and OS-transitive
DLLs remain distinct from direct imports checked by the static PE audit.
Failure-only child PID/name diagnostics identified an artificial
`CREATE_NO_WINDOW` repetition launcher creating `conhost`. Corrected that local
launcher to match the checked-in driver; preserved both failed checkpoints
without changing cleanup assertions, deadlines or resource bounds.

The existing CPU WebView's three recoveries/twenty lifetimes, B7's hundred
lifetimes and full static/runtime Host ABI audits pass. Wire/CPU-surface CTest
passes 3/3, GPU evidence/maps 11/11, backend boundary 7/7, existing device
evidence 18/18, source closure 7/7, licensing 15/15, driver/JSC helpers 2/2 and
6/6, and the unchanged 23-patch manifest pass. GPU-OFF still registers CPU
consumers only; no-host GPU CTest skips and strict/invalid-argument negative
controls fail as required. At that checkpoint this closes the Windows replay
slice, not the then-pending macOS 9A replay or the 9B/9C resource/GPUProcess gates.
The macOS replay is recorded above. No public ABI or WebKit patch changed;
`STATUS.md` is intentionally untouched.

## 2026-10-15

### Web Tranche 9A: Linux GPU final composition (CPU-upload path)

Added the optional `crtweb/skia.h` C++ companion and a real browser/WebView GPU
consumer. The application uses its own SDK Ganesh context; WebKit retains its
different private Skia in the separate browser process. The companion validates
GPU target/context identity, flushes and checks synchronous GPU submission before
source retirement, and leaves presentation/ACK to the caller. The v1 shared-memory
wire contract and public C ABI are unchanged. This is explicitly CPU BGRA upload
and GPU final composition, not GPU WebCore rendering or zero-copy IPC.

The new strict driver gate (`--webkit --run-gpu-tests`, optionally
`--gpu-window-frames 5`) requires actual GPU execution rather than a skipped
prerequisite. It writes separate GPU evidence and audits the browser host plus
Linux Host/Web/Network mappings, distinguishing CRT EGL/GLES stubs and anonymous
shared data from native GPU client libraries. Fresh CMake configuration and
consumer/host builds succeed. The test proves GPU output pixels, UI ordering,
ancestor clipping/opacity, input-driven pixel change, viewport/target resize,
retained old-frame immutability, twenty fresh updates, producer termination/
replacement and child cleanup. Detailed repetition/resource measurements,
negative controls and reproduction belong to
[Web acceptance](docs/acceptance/crtweb_acceptance.md#9a-linux-implementation-and-replay-2026-10-15).
The final checked-submit implementation passes offscreen **3/3** and live
Wayland/Vulkan **3/3**, each live run presenting 5/5 frames. Every twenty-update
sample is +40 KiB host RSS/+0 threads/+0 descriptors, with clean browser
static/runtime audits. These counters do not measure device memory.

Preserved the first title-before-paint fixture race and the initial map-observer
classification failure; neither was relabeled as a pass. The existing CPU
WebView regression passes three process recoveries and twenty host lifetimes
with +0 KiB RSS/+0 threads/+0 descriptors, and wire/surface CTest passes 3/3.
GPU evidence/map tests pass 8/8; existing device evidence, driver/JSC helpers,
source closure, licensing and patch-manifest checks remain green. A separate
GPU-OFF configure keeps CPU consumers but registers no GPU consumer; ordinary
CTest records missing prerequisites as skipped, while strict negative controls
fail. No audio or physical camera capture was requested by this GPU gate.

Split the remaining Tranche 9 boundary into resource/GPU synchronization and
device/GPUProcess lifecycle/host replay gates. At that Linux checkpoint,
macOS/Windows replays still awaited execution (recorded above). The resource/
GPUProcess gates remain open; the Linux final-compositor slice does not close Tranche 9.
`STATUS.md`, root README and the completed Tranche 8 evidence are unchanged.

### Web Tranche 8 closed: Linux output drain and native allocator replay

Refreshed the current Linux/x86_64 runtime from `ef081d2` with a fresh CMake
configuration, verified cumulative SDKs `01-c` through `05-ui`, and rebuilt
WebKit's common player, WebProcess, NetworkProcess and media test. The initial
focused replay exposed a real Pulse tail stall: decoder EOF now correctly waits
for output, but PipeWire retained less than one processing quantum and never
advanced its read cursor far enough to finish playback. A one-frame-prebuffer
experiment did not fix it and was reverted; both failed checkpoints are retained.

The Linux-private Pulse position path now uses native DRAIN when a stopped
stream still has accepted PCM queued, waits for its acknowledgement and
re-queries the server cursor. It never substitutes elapsed time or written
frames for device position. Failed drain/query stays an error. Seven fake-peer
protocol scenarios exercise the actual framing/ACK path without devices; the
strict evidence predicate now also requires the complete fixture's position to
reach its accepted 14,400 frames (parser tests 18/18, with a failing negative
control retained). No public ABI, shared WebKit player or other host backend
changes in this follow-up.

The final verified SDK passes strict focused 8B **3/3**: complete 48kHz stereo
PCM, 14,400 frames/28,320 nonzero samples, native ALSA playback/clock evidence,
zero fallback/write failures, playback EOF and twelve measured lifetimes per
run within the unchanged resource bound. The final measurements, failed
checkpoints, runtime audits and reproduction live in
[Web acceptance](docs/acceptance/crtweb_acceptance.md#linux-focused-8b-replay-and-tranche-8-closure-2026-10-15).
These are device delivery/clock observations, not human listening. Camera
hardware was not reopened; the accepted three-host 8A/8C evidence is preserved.

Native core CTest passes **136/136**, allocator padding sweep **130/130**,
seed-42 baseline tiers **4/4** and contention cases **8/8**. Detailed correctness
scope and performance caveats belong to
[allocator acceptance](docs/acceptance/allocator_baseline.md#linux-native-indexed-allocator-replay-2026-10-15).
Base-media tests pass 7/7 (including the portable MF queue regression and Linux
Pulse protocol cases), stage-source include closure 7/7, licensing 15/15 and the
23-patch manifest gate. The 04 source archive contains the private MF helper.
Updated the port mapping and library overview to remove obsolete implementation
claims; `STATUS.md` is intentionally unchanged. Together with the recorded
macOS/Windows output-drain and allocator replays, this closes 8B and Tranche 8.
GPU integration (9) and isolated distribution/security closure (10) remain open.

### macOS replay of the Windows Web 8B output-drain correction

Rebuilt `8c989ac` on macOS/arm64: refreshed the cumulative `01-c` through `05-ui`
SDKs (all verified), then rebuilt the composed WebKit tree, including the common
player, WebProcess, NetworkProcess and media test. Focused strict 8B replay
passes **3/3** without further runtime changes. Every run delivers the complete
48kHz stereo fixture (14,400 frames/28,320 nonzero samples), with sink-close
positions 18,479/18,629/18,048 and 48/48/47 applied clock updates, 44/44/43 advances.
No sink fallback or write failure. Default output is the built-in MacBook Pro
speakers; this is CoreAudio delivery/clock evidence, not a listening test.

Pause/seek/EOF, paused PCM preroll, local/HTTP playback and bad/missing-source
handling pass. Each run measures twelve playback lifetimes; Web threads are
28 -> 25, 28 -> 25 and 27 -> 25 within the unchanged +3 bound. Runtime mapping
audits pass with one CRT libc++/libc++abi image pair, recording Apple's framework
pair separately, and no remaining work-tree Web/Network process after teardown.
Base-media CTest passes 7/7, including the new device-free MF queue ownership
regression; stage-source include closure passes 7/7 and the generated macOS 04
source asset contains its private helper. Evidence/parser tests pass 17/17,
licensing 15/15 and the 23-patch manifest check.

Evidence is under `out/macos-host-ninja-debug/tranche8b-drain-replay-20261015/`:
SDK/WebKit build logs, `base-media.log`, default-device observation, and three
per-run gate logs/JSON plus `summary.json`. Detailed measurements and replay
scope belong to `docs/acceptance/crtweb_acceptance.md`. This closes macOS focused
replay of the common 8B correction, not the outstanding Linux playback/allocator
replay. Camera hardware was not reopened; prior macOS 8C acceptance is unchanged,
and the Windows MF backend itself was not exercised on macOS.

### Windows CI malloc coalescing fixture: remove startup-heap dependence

Windows/x64 CI run `38697153498`, job `116296326734`, fails only
`malloc_test_runs` (154/155). Its detailed step log was not accessible through
the unauthenticated log endpoint; local reproduction on the same `8c989ac`
binary reports `malloc_test: adjacent coalescing reuse` when a child-only
environment variable contains 22,560 padding bytes. The old test assumes
twelve process-heap allocations are physically consecutive despite Windows
startup importing a variable-sized environment into that same heap. That is
not a public malloc address-order guarantee; it can invalidate the fixture
without a coalescing defect.

Both existing normal/diagnostic targets now reuse the independently renamed
production allocator in `malloc_fault_instance_b.c` for this white-box fixture.
The fixture checks one OS region and consecutive addresses before requiring
reuse, explicitly exercises ascending and descending release, checks live
neighbours and shrink preservation, and rejects fresh-region fallback. The
remaining public malloc-family tests still exercise the process allocator.
No runtime implementation, public ABI, workflow or skip-CI policy changes.

A fresh `out/windows-malloc-ci` configuration and `crt-c-build` complete;
core CTest passes **155/155**. Both layouts pass all 65 padding sizes
0..30,720 by 480 bytes (**130/130**). Negative controls temporarily disabling
backward or forward merging fail both layouts with the corresponding reuse
message; the production source was restored before the final rebuild.
Reproduction commands, log paths and the pending remote CI confirmation are
in [allocator acceptance](docs/acceptance/allocator_baseline.md#windows-ci-coalescing-fixture-2026-10-15).

Merged `c0cf7a9` with the macOS 8B replay record, preserving both histories.
A separate fresh macOS/arm64 configuration passes the ordinary/diagnostic
malloc tests 2/2 and the same 65-size environment sweep 130/130. This is focused
test-fixture replay, not another full core or indexed-allocator acceptance run;
logs and reproduction are recorded in the allocator acceptance section.

### Windows Web replay fixes: output EOF and MF frame-ring ownership

The first current shared-code replay passes 10/12; its failed checkpoint is
retained in `docs/acceptance/crtweb_acceptance.md`. The complete 300ms PCM clip
fit in WASAPI's buffer before any position advance, yet the player posted
decoder EOF as playback end. The CRT player now keeps applying the device clock
until accepted PCM drains. Focused strict output delivers 14,400 frames/28,320
nonzero samples, 28 clock updates/20 advances and zero errors/fallbacks; media
threads 17 -> 19 remain within the existing bound. At this Windows checkpoint,
the common player change still awaited Linux/macOS focused replay (macOS results above).

Windows physical-camera stress exposed a pre-existing MF ring ownership bug:
dequeue left the transferred storage address in its old slot and teardown
freed all capacity slots, including frames already freed/reused by consumers.
The private ring clears transferred slots and drains only its active range.
The existing device-free frame test covers wraparound, caller-held frames,
inactive stale slots and repeated drain. Its portable helper is included in
all stage-source assets; include-closure checks pass 7/7. Temporary capture
tracing was removed. The corrected focused physical gate passes three ordinary
probes plus twenty same-PID lifetimes after four warmups, rotating all four
release modes under unchanged limits. Peak UI delta is +32 KiB/+0 threads/+0
handles; peak Web delta +1,212 KiB/+1/+2; final Web +1,196 KiB/+0/+0; all settling
is 0ms.

Final combined Windows replay passes **12/12** with both device options enabled:
all 14,400 fixture frames and 28,320 nonzero samples reach Remote Audio, position
14,400, 28 applied clock updates/18 advances and zero errors/fallbacks. Three
ordinary camera runs pass with twenty distinct pictures each; twenty measured
lifetimes use one WebProcess, four warmups and all four release modes. Peak
Web RSS delta is +876 KiB with no thread/handle growth; final +876 KiB/-2 threads/
-4 handles, settling 0ms. Media lifetimes keep one PID (25 -> 19 threads).
Public API/B7/WebView lifecycle and fifteen network trust cases pass; eighteen
static binaries and 140 runtime mapped files have no audit violation. No
remaining work-tree browser process. Final evidence:
`C:/crtw/b3w/acceptance-b3.json`, `out/web-windows-current-final.log` and the
work-root media/camera logs; detailed measurements and reproduction belong to
`docs/acceptance/crtweb_acceptance.md`. Final SDK/source-asset refresh verifies
`01-c` through `05-ui` and the generated 04 source archive contains the private
queue header; base-media tests pass 5/5. Evidence/parser tests pass 17/17,
stage-source closure 7/7, CMake paths 2/2, ABI policy 6/6, TLS fixture 4/4,
licensing 15/15 and the 23-patch manifest gate. Artifact/software logs are
`out/web-windows-current-artifacts-final.log` and
`out/web-windows-current-media-final.log`. This closes Windows replay, not the
new common output-drain fix's then-pending Linux/macOS replay or Linux native allocator
regressions. Remote Audio is endpoint/clock evidence, not local-speaker listening.

### Windows native replay of the indexed allocator

Rebuilt the current runtime and cumulative SDK from `d3687a9` after a fresh
Windows preset configuration preserving the existing CRT options; `01-c`
through `05-ui` verification passes. Core CTest passes 155/155, including
ordinary/diagnostic allocator, fault/canary, contention and native Windows
fork/runtime-reset/many-region regressions. Seed-42 baseline correctness passes
all four tiers through 1M operations (12 OS regions at 1M); all eight
private/shared 1/8/16/32-thread contention cases pass. This is native x64
execution, not a cross-target compile or replay of pre-change executables.

Evidence: `out/web-windows-current-fresh-configure.log`,
`out/web-windows-current-sdk-build.log`, `out/web-windows-current-core-ctest.log`,
`out/web-windows-current-allocator-{baseline,contention}.log` and reports under
`out/web-windows-current-allocator/`. These one-off runs overlap SDK work and
are correctness evidence, not controlled throughput comparisons; host-RSS
provenance across Windows startup relaunch was not independently revalidated,
so the raw runner RSS fields are not used for memory-envelope acceptance.
Linux native replay of this allocator revision remains separate work.

### macOS Web 8C: allocator latency and native lifetime scoping

The final combined gate passes **12/12**, closing macOS/arm64 Tranche 8C. It includes three ordinary
physical-camera runs and twenty measured lifetimes in one WebProcess, plus a separate forty-cycle
stress run. All original resource/deadline bounds remain in force. Failed intermediate checkpoints
are retained beside the final acceptance evidence rather than relabelled as successes.

The native comparison led to a CRT/PAL-only reproducer, without WebKit. Sampling shows Apple's
Portrait/Espresso initialization calling CRT new/delete and waiting on the global allocator lock:
malloc traversed live blocks and each free swept the entire heap. Repeated capture grew from
roughly 1.6s to 21.7s per lifetime; forward-only coalescing was insufficient and eventually missed
the first-frame budget. The bootstrap allocator now indexes free blocks by size, coalesces physical
neighbours locally and appends via a tail. Its private header and released-payload bookkeeping change,
not Bionic public allocation types/symbols/error policy. Minimum/header alignment stays at least 16
bytes, including Windows' native 8-byte long double case. Scudo/host malloc were not substituted.

Direct CRT/PAL capture then completes twenty measured sessions with increasing owned-frame timestamps
and 360-1,491ms lifetimes, RSS +2,448 KiB maximum and no thread/fd growth. Allocator-only browser runs
pass twice but a combined replay still catches native thread +3; that failed checkpoint is preserved.
All AVFoundation start/stop/release calls now own Cocoa autorelease pools, as open already did. The
combined allocator/PAL path passes focused twenty- and forty-cycle stop/revoke/navigation/clone runs
without increasing the +24 MiB/+2 thread/+2 fd or two-second limits. No separate stream-clock change
was needed for these accepted runs. This does not attribute all native cached-thread variation to a
single autorelease defect. Detailed evidence and the final combined gate belong to
`docs/acceptance/crtweb_acceptance.md`.

Faster execution also exposes a B7 observer race: its 100-cycle test passes, but external `vmmap`
sometimes reaches a WebProcess after it has exited. A macOS-only atomic ready/release checkpoint now
holds the first painted process pair alive for observation before the measured warmup; B7 again
observes UI/Web/Network with no violations. The checkpoint parser rejects missing, duplicate and
wrong-process identities. The unit suite passes 17 cases. CTest passes 190/190 (the two physical
camera-to-file tests were deliberately excluded); allocator ordinary/diagnostic/fault tests, seed-42
1M baseline and all eight 1/8/16/32-thread contention cases pass. Native Linux/Windows runtime replay
of the common allocator change is still separate from the successful cross-target compile checks.

### macOS Web Tranche 8 shared-code replay: complete PCM output accepted

Rebuilt and verified the cumulative `05-ui` SDK, recomposed the pinned PlatformCRT tree (22 applicable
patches from the 23-entry manifest), and replayed `ae1dac2` on macOS/arm64. The combined result is
**11/12**, with the newly strengthened physical-camera lifetime gate still red. Playback delivers the
complete 48kHz stereo fixture (14,400 frames, 28,320 nonzero samples) through CoreAudio, without write
errors or sink-open fallbacks; paused preroll, backpressure, seek and twelve media lifetimes pass.
Network trust, synthetic permission/stream-clock regressions, public/internal/WebView lifetimes and
Host ABI audits pass. Three ordinary physical-camera executions pass too. Detailed measurements and
the unresolved same-process camera failures live in the acceptance document and `TODO.md`.

The camera investigation retained the initial failed checkpoint. Sampling the CRT WebProcess finds Apple's
Portrait/Espresso model initialization and AppleDepth model-file descriptors surviving the immediate
post-stop snapshot; an idle observation sees the native counts decline again. A PAL autorelease-pool
experiment did not resolve the result and was reverted. The test now warms every release mode (four
cycles), records up to two seconds of post-stop settling against the unchanged resource bounds, and
prints DOM/track state on a video-clock timeout. Fourteen evidence-parser tests pass. Focused hardware
retests still fail (resource settling and video-clock startup), so **macOS 8C remained open at this checkpoint** and no
runtime/PAL implementation fix is claimed. Basic media CTests pass 6/6 and the capture-conversion
CTest passes; no work-tree processes remain. Evidence and reproduction:
`docs/acceptance/crtweb_acceptance.md`, macOS current shared-code replay (2026-10-15).

**Native lifecycle comparison.** Added an opt-in Apple-SDK/libSystem-only diagnostic under
`libcrtmedia/tests/native/`, deliberately outside CRT targets and acceptance. Both 2,000ms and 300ms
post-release cadences complete four warmups plus twenty fresh AVFoundation sessions, each receiving
real camera frames. Measured maxima are respectively RSS +2,080/+2,528 KiB, threads +1/+0 and fds
+0/+0. Thus repeated session ownership is supported on this Mac; the browser failure cannot be
attributed solely to unsupported macOS lifecycle behavior. `stopRunning` guarantees the session has
stopped, not that framework caches or worker pools have vanished. The fixed resource deadline is
our acceptance policy, not an Apple promise, and remains unchanged. This control does not exercise
CRT allocation, PAL conversion/queues or DOM/navigation/clone semantics; it neither identifies the
remaining defect nor closes 8C. See the acceptance document's native comparison and the diagnostic
README for the separate host-only build/run procedure. No camera images were saved.

### Linux Web capture clocks and same-process device lifetimes

Linux replay of the strengthened permission-token regression passes three
consecutive synthetic runs, including delayed completion after view destruction,
allocation churn, replacement-request identity and an unknown opaque handle.
Physical-camera stress exposed live video `currentTime` stuck at zero: file
players have a decode worker to maintain their clock, but streams do not.
The stream player now owns monotonic play/pause anchors and freezes its clock
when its last track ends. Synthetic regressions check advance, pause, resume
and a clone retaining the source after the original track stops.

The opt-in camera probe now exercises twenty measured lifetimes after three
warm-ups in one UI/WebProcess pair, rotating explicit stop, revoke, active
navigation and last-clone release. Each cycle requires fresh pictures and the
unchanged +24 MiB/+2 thread/+2 descriptor limits in both processes. Linux also
observes zero native camera descriptors after every release; image data is not
saved. The driver rejects incomplete traces, process replacement, unknown
resource observations and separate-process reopen evidence as substitutes for
this lifetime gate. macOS/Windows replay of the new stream-clock/resource gate
remains open in `TODO.md`; detailed Linux measurements and reproduction are in
`docs/acceptance/crtweb_acceptance.md`.

### Linux ALSA playback selection and transferred-frame accounting

After the user granted the audio group and logged in again, the first browser
audio replay exposed a pre-existing direction-selection bug. `sscanf` counted
the card/device assignments even when its trailing `p` literal did not match,
allowing a capture node ending in `c`. This invalid replay is retained, not
accepted as output evidence; no captured PCM was saved to a file. Selection now
requires the complete playback-node name. `WRITEI_FRAMES` also now returns the
actual `snd_xferi.result`, not the input `frames` field, preserving short writes.
The private UAPI shape was checked against Bionic's
[cleaned asound header](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/kernel/uapi/sound/asound.h)
and Linux's `snd_pcm_xferi_frames_ioctl`; no public ABI or host client-library
dependency was added.

The native-format fixture also exposed paused preroll consuming and discarding
decoded audio before `play()`: only the last 2,112 of 14,400 frames reached the
sink. The worker now retains preroll audio until playback and releases it on
seek/teardown. Audio-only preroll stops after the first valid buffer; paused
video seeking uses a bounded (4 MiB/256-buffer) PCM queue so interleaved audio
cannot block demux before the target video frame. Overflow fails loading
instead of dropping samples or growing without limit.
Audio-only readiness requires a retained decoded buffer rather than demux
progress. A second loss path released compressed samples on decoder
`CRTMEDIA_WOULD_BLOCK`; the worker now retains and retries that same sample,
as required by the installed codec contract, and releases it on seek/teardown.
The regression waits paused after readiness; strict output requires all 14,400
fixture frames and nonzero PCM, not just a tail fragment. Linux additionally
requires that complete fixture to use a native playback node.

A deterministic CRT-linked Linux PAL test replaces only OS boundaries and
checks capture/suffix rejection before opening anything, lowest playback-node
selection, partial/zero/full transfer counts and invalid transfer results.
The strict browser evidence joins PAL route diagnostics to nonzero PCM and a
progressing device clock by sink identity; Linux requires an exact playback
node, not a Pulse endpoint that may be Dummy Output. The acceptance document
owns the hardware replay and its format/listening limits.

Two consecutive focused strict-output replays pass: the entire generated
stereo S16/48kHz fixture (14,400 frames, 28,320 nonzero samples) is delivered to
`/dev/snd/pcmC0D0p`, with four progressing device-clock updates and no write
errors or sink-open fallbacks in each run. Seek events pass too. System-call
tracing observes only that playback PCM node, not capture nodes. Basic media
CTests pass 6/6, evidence checks 12/12, licensing 15/15, path tests 2/2, JSC tool
tests 6/6 and TLS-fixture tests 4/4; the 23-patch manifest check passes.

The final combined Linux replay passes 12/12, including fifteen network-trust
cases and all physical-device gates. Camera stress retains the same process
for all twenty measured stop/revoke/navigation/clone cycles: final UI +44 KiB
RSS/+0 threads/+0 descriptors, WebProcess -76 KiB/-1/+0, with zero native
camera descriptors after every cycle. Public API/internal B7/WebView lifetimes
are 100/100/20 cycles with +52/+240/+0 KiB RSS and no thread/descriptor growth.
Static audit covers eighteen binaries and runtime audit 132 mapped files
without violations; no browser test process remains. Final evidence:
`out/web-crt-webcore/acceptance-b3.json` and `tranche8-linux-hardware-final.log`.
The OS PipeWire profile is still Off/Dummy Output; the native stereo fixture is
the physical-output proof, not mono MP3/Pulse output or a listening test.
Current shared-code replay on macOS/Windows remains open in `TODO.md`.

### Windows Web physical-camera replay and WASAPI PCM-format correction

The explicitly requested browser camera probe passes three consecutive ordinary
executions: QuickCam for Notebooks Deluxe, 640x480 at 30 fps, twenty samples and
twenty distinct picture hashes in each run, one application permission request
each, `readyState=ended` after stop, and reopening in the next process. Images
are not saved. This is browser permission/capture evidence, not a claim of
same-process camera resource bounds or independent device-busy checking.

The previously missing default render endpoint is now available as Windows
"Remote Audio". Initial strict-output playback still fails: stereo S16 44100 Hz
opens, but mono float 44100/8000 Hz and mono S16 8000 Hz are rejected at WASAPI
`Initialize` with `0x88890008`. The backend now requests the documented shared-mode
PCM channel/rate conversion and default-quality resampler, leaving the public
format contract and native-type boundary unchanged. All four format probes open;
the existing audio-sink CTest now also writes/drains the three converted formats
when a Windows endpoint is available and passes. No-device CI retains its skip.
The focused strict-output browser replay passes after the corrected cumulative
SDK is refreshed and verified through `05-ui`: HTML MP3 delivers 5,184 accepted
nonzero PCM frames at 8000 Hz mono, position reaches 5,184 frames, nine clock
queries re-anchor playback with five advancing updates, and zero write errors
or sink-open fallbacks. The same WebProcess PID is retained across twenty-four
measured lifetimes; threads 25 -> 27 remain within the unchanged +3 bound.
Source of the PAL flags:
[Microsoft WASAPI stream flags](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants).

The initial failed strict-output run is retained in
`out/web-windows-tranche8-devices.json` and `.log`; it must not be presented as
an audio pass. Generated format/build evidence: `out/web-windows-wasapi-conversion-build.log`.
Focused strict-output evidence: `out/web-windows-audio-required.json` and `.log`;
SDK verification: `out/web-windows-wasapi-sdk-refresh.log`. The final combined
gate with `--require-audio-output --allow-physical-camera` passes 12/12, including
three more physical-camera executions and strict output with the same MP3/clock
metrics and 25 -> 27 media threads. Public API 100 cycles: -28 KiB RSS/-3 threads/
+0 handles; internal B7: -64 KiB/-3/+0; WebView 20 cycles: -16 KiB/-2/+0.
Network trust passes 15/15; static audit covers 18 binaries and runtime audit
140 files without violations. No remaining work-tree browser test process.
Existing base-media CTests pass 5/5, path tests 2/2, ABI policy 6/6, device evidence
4/4, licensing 15/15 and the 23-patch manifest check passes. Final evidence:
`C:/crtw/b3w/acceptance-b3.json` and `out/web-windows-tranche8-hardware-final.log`;
the acceptance document owns detailed reproduction and limits.
Remote Audio acceptance means PCM/device-clock delivery, not a listening test
or evidence of this host's local physical speakers.

### Web media permission tokens survive view teardown without stale grants

Windows synthetic-capture repeats reproduced a common-code use-after-free:
`crtweb_view_destroy()` freed a resolved permission token, but the public contract
required a delayed completion to return `CRTWEB_ERROR_INVALID_ARGUMENT` safely.
Resolved tokens now move to inert runtime-owned storage on view release, without
retaining WebKit request/device references, and are freed at runtime destruction.
Completion validates ownership before dereferencing; reserved token addresses
prevent stale grants from aliasing a new request within that runtime. The public
header and API policy clarify application ownership and the runtime lifetime limit.

The strengthened capture regression adds allocation churn, completion after
view destruction, replacement-request identity and rejection of an unknown
opaque handle. Five consecutive Windows synthetic runs pass; three subsequent
combined media/capture repetitions and the final full replay pass. This is not
Linux/macOS validation of the fix: those replays remain in `TODO.md`.

### Windows Web Tranche 8 automatic replay complete; hardware gates remain explicit

The final combined Windows/x64 gate passes 11/11 with the FFmpeg-enabled `05-ui`
predecessor: public API/process lifecycle, B5/B6, B7 Host ABI, WebView,
CPU playback, synthetic permission/capture and all fifteen browser/WebView
network trust cases. Windows playback uses twelve representative warm-ups and
twenty-four measured lifetimes with the unchanged +3 thread bound, positive
Toolhelp observations and the same WebProcess PID. Each cycle waits for decoded
media readiness and requires playback-clock progress; early-play/loading-race
coverage is not inferred. Final threads are 23 -> 15; focused repeats are
23 -> 22, 23 -> 15 and 22 -> 15. Earlier intermittent failures and their causes
remain in `docs/bringup/crtweb_windows_tranche8.md` rather than being erased.

The PKI fixture owns an OpenSSL configuration in its subprocess environment,
fixing the host executable's missing compiled-in config path without changing
the caller's environment. Fixture tests pass 4/4, CMake path tests 2/2, ABI
policy 6/6, device evidence 4/4, licensing 15/15 and the 23-patch manifest
check passes. Static audit covers 17 binaries; runtime audit covers 140 mapped
files across UI/Web/Network processes with no violations. No work-tree browser
test processes remain after the gate. Detailed measurements, reproduction and
limits are in `docs/acceptance/crtweb_acceptance.md`, Windows Tranche 8 replay;
generated evidence is `C:/crtw/b3w/acceptance-b3.json` and
`out/web-windows-tranche8-ready-final.log`.

No browser physical-camera run was requested/performed. Real audio is not
verified: all default render endpoint roles return WASAPI `0x80070490`, and
the automatic run records 39 sink-open fallbacks and no PCM output. The opt-in
PAL diagnostic names the failed API/HRESULT without exposing native types.
Forty-eight fresh-thread open/close probes stay at four threads. Real Linux/
Windows output, explicit Windows camera evidence, common-fix replay and
same-process physical-camera resource stress remain open; Tranche 8 is not
closed. Windows media DLL export hygiene remains separately queued in Tranche 10.

## 2026-10-14

### Windows Web Tranche 8 predecessor: CRT-tool FFmpeg build and SDK install dependency

The installed `05-ui` media DLL had been built with FFmpeg disabled, so the
Tranche 8 playback link lacked `crtmedia_extractor_create` and codec APIs.
FFmpeg 8.1.2 now configures, builds (`-j8`) and installs with CRT mksh and
CRT-built GNU Make, not Git Bash/MSYS Make. Debug build-tool timings are
1744.6 s configure, 473.4 s build and 237.0 s install; the recipe has no
standalone tests and these timings are not an optimized performance baseline.

Refreshing the cumulative SDK exposed an independent install-order gap:
`crt-media` installs `crtmedia_player_demo` when FFmpeg is enabled, but
`crt-media-build` did not build it. The aggregate now conditionally depends
on that executable. A fresh Windows preset configuration preserving the CRT
options and `crt-ui-dist` pass, including `verify_dist` for `01-c` through
`05-ui`. The new import library exports extractor/codec APIs; twelve CPU/base
media CTests pass (frame, GPU-frame ownership, format, player, audio sink,
WAV/video/MP3/malformed demux, extractor/codec, software encode/mux and timing).

Generated evidence: `out/web-windows-ffmpeg-crt.log`,
`out/web-windows-sdk-fresh-configure.log`, `out/web-windows-sdk-build-fixed.log`
and `out/web-windows-media-cpu-regression.log`. The interrupted restricted-launcher
run and the mistakenly selected hardware-dependent media test are documented in
`docs/bringup/crtweb_windows_tranche8.md`. Neither closes browser device acceptance.
The Windows core test preset also passes 155/155 (`out/web-windows-core-regression.log`).
WebKit's fresh configure links the extractor prerequisite successfully; driver
CMake paths now use forward slashes to avoid an invalid `\U` escape under source-list
evaluation (two path regression tests pass). ABI policy tests pass 6/6, device
evidence parser 4/4, licensing 15/15 and the 23-patch manifest check passes.
Windows WebKit 8A-8C replay remains open pending its combined runtime/ABI gate.

### Web Tranche 8 macOS device evidence: physical camera and CoreAudio output

The explicitly opted-in `crtweb_camera_probe` passes three consecutive ordinary executions on
macOS/arm64: FaceTime HD camera, 640x480 at 30 fps, one application permission request per run,
20 frame samples with 19/18/19 distinct pictures in the final combined replay, `readyState=ended` after `track.stop()`, then
successful reopening in the next process. No images are saved. The probe now requires changing
pictures and cleans up the runtime even when load/capture fails. No new OS permission prompt was
required on this machine; other hosts may require one.

The HTML MP3 test delivers nonzero PCM through the installed `libcrtmedia` CoreAudio sink to the
default built-in MacBook Pro speakers: 8000 Hz mono, 5184 accepted frames/nonzero samples, position
4693 frames, seven progressing sink-clock updates, zero write failures or sink-open fallbacks in
the run. This is API/device-clock evidence, not microphone loopback or a human listening test.
The player now honors sink short writes and records actual accepted frames, nonzero samples,
position and clock updates under `CRTWEB_MEDIA_AUDIO_TRACE`. The test's global `getenv()` ran
before CRT startup and initialized an empty environment; moving it into `main()` lets the diagnostic
setting reach the Web process. A global `DYLD_LIBRARY_PATH` caused a typed-operator-new initialization
abort when entering CoreAudio (and in the camera probe); the device tests use linked RPATHs instead,
as the existing WebView replay already does. The mapping audit still requires one CRT C++ image
pair and separately records Apple's framework runtime.

The driver records this evidence in `acceptance-b3.json`; `--require-audio-output` makes real output
mandatory, and the explicit `--allow-physical-camera` adds three hardware runs. Normal automated
gates still use the labelled synthetic camera. The final combined gate passes 12/12: public API
100 cycles +16 KiB/+1 thread/+0 fd within bounds, internal B7 +80 KiB/+0/+0, WebView 20 cycles
+0/+0/+0, media Web process 28 -> 24 threads, static/runtime audits clean and no leftover work-tree
process. Four evidence-parser tests, fifteen licensing tests and the manifest check pass; the probe
also refuses execution without opt-in. Reproduction and limits are in
`docs/acceptance/crtweb_acceptance.md`, 8C device follow-up.
Windows 8A-8C replay, Linux audio-output evidence and same-process camera resource-delta stress remain open.

### Web Tranche 8 replayed on macOS/arm64: libcrtmedia exported its internals and bound libc to the host

8A (network trust), 8B (playback engine over crtmedia) and 8C (camera permission lifecycle, synthetic source) pass on
macOS in the combined gate, eleven entries, with the public API lifecycle (+0/+0/+0), WebView lifecycle and B7 flat. The
first run stalled every `file:` load; the cause was found by bisecting the configuration (player off passes, player on
stalls) and then reading the Mach-O bindings of libWebKit (`dyld_info -fixups`: `_curl_multi_init` bound to
`libcrtmedia`, not libcurl). Three defects in the CRT's own libraries followed: libcrtmedia.dylib exported its
FFmpeg/libcurl/mbedTLS (now `crtmedia_*` only), bound its libc symbols to libSystem because libc.dylib came after the
frameworks on the link line (FFmpeg's `pthread_once` bus error in the Web process; libc.dylib first now), and the SDK
carried `libc++.dylib` as a second regular file of the same install name (the dist copier dereferenced symlinks; they
are kept). Audit policy now names the OS media frameworks as libcrtmedia's PAL. Physical-camera and audio-output
evidence were completed in the device follow-up above. Details: `docs/acceptance/crtweb_acceptance.md`, Tranche 8C.

### Web Tranche 8, gate 8C: camera permission lifecycle and capture (Linux/x86_64, synthetic source)

MediaStream is compiled in (no WebRTC/microphone/display). `crtweb` gained `on_media_permission`,
`crtweb_media_permission_complete` and `crtweb_view_revoke_media`, carried over the WebView control channel;
the default is denial. `CRTVideoCaptureSource` (a `RealtimeVideoCaptureSource` over `crtmedia` capture)
opens a device only after a grant; frames reach `<video>` through `MediaPlayerPrivateCRT::load(MediaStream)`.
Enabling MediaStream on this port needed patch 0023 (missing `VideoFrame.h`), an exclusion of the stale
generic mock capture sources and a `UserMediaCaptureManager.h`. `crtweb_capture_test` (labelled synthetic
camera, device never physical) and the WebView camera case pass; the gate has eleven entries. Not done: a
physical-camera run (explicit opt-in via `crtweb_camera_probe`) and the macOS/Windows replays of 8A-8C.

### Web Tranche 8, gate 8B: the CRT playback engine over crtmedia (Linux/x86_64)

`MediaPlayerPrivateCRT` (WebCore overlay, no WebKit patch) registers once per Web process and plays
H.264/AAC MP4, MP3 and WAV through the installed `crtmedia` C API: a worker thread owns demux/decode/audio
write, the main thread owns notifications and painting; frames are copied RGBA into WebKit's Skia.
`http(s):` goes through WebKit's own resource loader into a temporary file (no second HTTP stack in the
Web process; playback starts when the transfer completes). Packaging defects found and handled in the web
stage: `libcrtmedia.so` needs host VA-API (replaced by failing `libva` stubs so no host library is
mapped) and records `libc++.so` (a distinct file from `libc++.so.1`; aliased by a symlink so one runtime
is mapped). `crtweb_media_test` and the whole gate pass (`media_playback`: 36 mapped files, no
violation); the Linux host has no sound card, so audio output is unverified. Open: audio on a physical
host, progressive HTTP, macOS/Windows replay of 8A/8B, 8C capture.

### Web Tranche 8A: CRT network trust adopted and accepted on Linux/x86_64

The actual PlatformCRT product already uses WebKit's Curl request scheduler over CRT-owned
OpenSSL/libcurl ports and CRT PAL sockets, not WPE's libsoup/GLib backend. The porting map and
Tranche 8 contract now distinguish that product from the WPE reference rather than planning to
remove libraries the product never linked. Browser request semantics remain in WebKit; the media
HTTP range/GET transport is not substituted for arbitrary browser methods, headers or cookies.

`crtweb_network_configure()` copies explicit PEM trust anchors, defaults to rejecting HTTPS when
there are none, and exposes only a loudly named development-only verification opt-out. Trust is
process-wide and frozen once WebKit initializes because NetworkProcess can be shared between
runtime data stores. Ambient host roots/CA variables do not grant trust; invalid PEM, unrelated CA,
expired certificates and wrong SAN fail the page load. The portable launch bundle budget is
16 KiB, with explicit rejection rather than truncation. Public C API version remains 1; no existing
runtime config layout changes. Separate `crtui` WebView hosts receive a CA-file option or explicit
insecure option, read/copy the file before initialization, and fail startup on invalid files.

The new public-header-only `crtweb_network_test` and build-host-only loopback Python/OpenSSL PKI
fixture test ten API cases (including default-reset, copying, configuration errors and immutability)
and five real separate-host WebView cases. Accepted pages follow a 302, load an external script,
POST through a 307 preserving the body/header/method and round-trip an HTTP-only cookie. No
Internet or checked-in private keys are used; no capture device is opened. The fixture initially
used negative `openssl x509 -days` for expiry, which the host CLI rejects; it now signs the fresh
expired fixture through `openssl ca` with explicit relative start/end dates.

The final combined driver passes **all nine entries**, including all fifteen network cases,
B3-C8, rendering/input, public API/WebView lifecycle and Host ABI checks. Public API 100 cycles:
**+128 KiB RSS, +0 threads, +0 descriptors**; WebView 20 cycles: **0/0/0**. The static audit covers
15 binaries and the runtime audit 123 mapped files across UIProcess/WebProcess/NetworkProcess,
with no violation. The public header/export firewall, three fixture unit tests, fifteen licensing
tests, three driver tests and the 22-patch/18-overlay-file manifest gate also pass.

Reproduction and the generated `out/web-crt-webcore/acceptance-b3.json` evidence are owned by
[Tranche 8A acceptance](docs/acceptance/crtweb_acceptance.md#8a-network-ownership-and-explicit-trust).
macOS/Windows network replay, `crtmedia` playback (8B) and capture (8C) remain open; **Tranche 8
is not closed**. The user subsequently approved video camera and explicit permissions for 8C
(2026-10-14); its target contract is in the API policy/acceptance documents, not an implemented
capture claim. Microphone remains deferred. `STATUS.md` is intentionally unchanged.

### Web Tranche 6 closed on Windows/x64: lifecycle recovery, handle ownership and PE audit

The Windows/x64 replay of `tools/build_webkit_crt_port.py --webkit --run-tests`
passes all eight combined B3-C8/Tranche 5 entries. The public API test recovers
the same view from three forced WebProcess deaths, covers the launch-before-first-
pump, busy-runtime and sibling-runtime cases, and finishes 100 lifecycle cycles
at **+4 KiB RSS, -1 thread, +0 handles** after warm-up. The `crtui` WebView
recovers and repaints three times with exactly one termination callback per active
process death, then finishes twenty host cycles at **+4 KiB RSS, -2 threads,
+0 handles** without needing its bounded settling interval. Internal B7 finishes
100 cycles at **-32 KiB RSS, -3 threads, +0 handles**, with at most two children.
The runtime PE audit maps 108 files across UIProcess, WebProcess and NetworkProcess
with no violation; the static audit covers 14 executables/DLLs with no foreign
dependency, and the WebView host/library audit is also clean.

The replay found and fixed Windows-specific ownership and acceptance defects:

- fd snapshots always contain `DuplicateHandle` copies, including socket entries.
  Disposing those copies with `closesocket` failed and leaked a handle on every
  spawn; both parent and child snapshot cleanup now use `CloseHandle`, guarded by
  a 32-spawn CLOEXEC AF_UNIX regression in `unix_socket_test`.
- WebView shutdown used to discard unread surface messages after sending BYE.
  On Windows, handles carried by those already-framed SCM_RIGHTS records had
  already been duplicated into the consumer. Shutdown now disables application
  callbacks but drains the surface channel through host exit, importing and
  closing each queued handle before releasing the client.
- A late crash notification from a retired process could be mistaken for the
  replacement's death. The per-view termination latch is reset only when the new
  navigation commits, not when reload is merely requested.
- Cross-site history leaves cached/prewarmed WebProcesses that a public consumer
  cannot identify. The WebView recovery test now probes observed candidates one
  at a time and stops when the active page's callback arrives, instead of racing
  WebKit by killing the whole process pool simultaneously. It still requires a
  new PID, a finished load, a fresh frame and exact pixels; cached process
  ownership remains covered by runtime/host shutdown.
- The acceptance driver now always asks Ninja to refresh `crtweb_webview_test`,
  recognizes `gperf.exe` in the dependency prefix and disables auto-detected
  native ccache for `.cmd` compiler wrappers. A bounded Windows replacement-
  process `ERROR_ACCESS_DENIED` retry is logged just like the existing transient
  file-not-found retry.

Reproduction uses the Windows `05-ui` SDK and port prefix with
`--work-root C:\crtw\b3w --reuse-tree --webkit --run-tests -j 8`; the accepted
machine-readable result is `C:\crtw\b3w\acceptance-b3.json`. Together with the
macOS/arm64 replay below, both host replay tranches are closed; Tranche 8 is next.

## 2026-10-13

### Web Tranche 7: Tranche 5 lifecycle replayed on macOS/arm64 (nothing had to change)

After pulling the Linux closure of Tranche 5, the combined gate (`build_webkit_crt_port.py --webkit --run-tests
--recompose`) passes on macOS with all 22 patches applied from a clean checkout: the public API lifecycle test (three
forced WebProcess deaths recover into new PIDs; 100 cycles, +16 KiB RSS, +0 threads, +0 descriptors), the WebView
gate (termination events, reload and recomposition three times, 20 host cycles +0/+0/+0), internal B7 (+64 KiB, +0,
+0), the static and runtime Host ABI audits (364 mapped files, 15 binaries) and the export/header firewall. No code,
patch or recipe changed on this host; the Linux author had already written the libproc branch of the shared
observation helper (`crtweb_test_process.h`) and `tools/build_webkit_crt_port.py` already enabled Tranche 4/5 on macOS.
A first run was against a tree composed before the pull and so lacked patch 0022; it printed three
`Error receiving IPC message on socket -1` lines (the race patch 0022 fixes), and a local edit that fixed them in patch
0019 would have collided with 0022's expected hash. It was dropped for the original patches; the recomposed run prints
none. The SDK `libc.dylib` in `dist/05-ui` was refreshed from the build tree again (delete, copy, re-sign).
Windows/x64 subsequently closed Tranche 6 on 2026-10-14. Open: Tranche 8 onward.

### Web Tranche 5 closed on Linux/x86_64: public API recovery and bounded process teardown

The combined `tools/build_webkit_crt_port.py --webkit --run-tests` gate now runs
`crtweb_api_lifecycle_test`, built against the public header and `libcrtweb` alone,
alongside the accepted B3-C8, internal B7 and `crtui` WebView gates. Three forced
WebProcess deaths recover by reloading the same view into a replacement PID,
with exact canvas/background pixels at the new size. Twenty runtimes destroyed
before their first pump leave no live child or zombie. A busy infinite script
is retired without a late callback; destroying that runtime preserves its sibling.
One hundred create/load/resize/paint/destroy cycles, alternating explicit view
destruction with runtime-owned destruction and with scripts still pending at close,
leave no child. Every destruction is bounded below five seconds. The public API
run's post-warm-up growth is **+28 KiB RSS, +0 threads, +0 descriptors**.

The `crtui` WebView gate also terminates the active and cached/prewarmed WebProcess
children of its owned host after the cookie/download/popup/IME tests, receives
exactly one termination event and reloads/composes a fresh frame three times. Its
twenty host launch/load/resize/shutdown cycles grow **+0 KiB RSS, +0 threads,
+0 descriptors**. Internal B7 still passes 100 cycles (**+228 KiB RSS, +0 threads,
+0 descriptors**); static and runtime Host ABI audits, header/export privacy, B5
render, B6 input and the previous public API gate all pass. The wire-layout,
transport and surface CTests pass 3/3; JSC/licensing driver tests pass 18/18, and
the patch manifest still validates 22 carried patches/62 modified upstream
files/18 CRT-owned overlay files. No upstream patch or PAL ABI was added.

The new tests found and resolved these ownership/lifetime defects:

- A dead view still advertised its previous backing-store picture as a valid
  frame. Crash handling now invalidates paint/damage/composition state; frame
  copy returns `CRTWEB_WOULD_BLOCK` until a new paint. `crtweb_host` also retires
  its pending-frame flag when it reports the termination event.
- Runtime destruction slept for 300 ms and returned while an unresponsive WebProcess
  remained. The CRT launcher now registers unique WebKit launch tokens and retires
  each only after its reaper's `waitpid` completes. The runtime tracks the page and
  whole process pool, including cached/prewarmed children, closes each page explicitly,
  waits two seconds for cooperative shutdown, then terminates unresponsive owned
  launches and waits up to two more seconds for reaping. Other runtimes' launches,
  including a shared NetworkProcess, are excluded.
- Launch-in-flight teardown exposed a subtler case: deferred session cleanup can
  start a NetworkProcess after context/store release. The shutdown loop refreshes
  the launcher registry, includes these new launches and excludes launches which
  predated teardown or belong to a surviving runtime. Observing only the view's
  PID or its store before release missed that short-lived child.
- `SurfaceClient` released its images/mappings but never closed its owned connection
  socket. Its destructor now closes the descriptor after image/buffer retirement,
  so repeated WebView host shutdown does not accumulate consumer connections.

Reproduction and the frozen bounds are in
[`docs/acceptance/crtweb_acceptance.md`](docs/acceptance/crtweb_acceptance.md), Tranche 5;
the run writes `out/web-crt-webcore/acceptance-b3.json` and the local log is
`out/web-crt-webcore/tranche5-gate.log`. macOS/arm64 replayed these gates in
Tranche 7 on 2026-10-13 and Windows/x64 in Tranche 6 on 2026-10-14. Their
already-completed B4-B7/Tranche 4 evidence is preserved. GPUProcess remains
deferred until Tranche 9 enables GPU rendering.

### Web Tranche 4 replayed on Windows/x64: DLL constructors, POSIX record locks and two boundary spellings; Tranche 4 closed

`tools/build_webkit_crt_port.py --webkit --run-tests` on Windows builds `libcrtweb.dll`, `crtweb_host.exe` and
`crtweb_api_test.exe`, runs the in-tree `crtweb_webview_test` against the host, and the whole B3-C8 gate passes
3/3 consecutive runs (`crtweb_api_test` also 15/15 alone, no IPC error on stderr; lifecycle +0 handles; no child left);
`ctest` 186/186. Nothing in the engine changed except a one-hunk carried patch; the defects were in the PAL, the
toolchain wrappers and two path spellings.

- *Static constructors of a DLL never ran.* `crtDllMainCRTStartup` (dllcrt.c) had no equivalent of crt1's
  `__crt_run_init_array`, and the `-shared` links of `tools/crt-cc`/`tools/crt-c++` did not bracket the DLL's
  `.ctors`/`.CRT$XCU` regions. A namespace-scope `std::map` in `libcrtweb.dll` was zero-filled memory, and the first
  `crtweb_view_destroy` iterated it (null node, access violation); WebKit hid the same gap because it mostly uses
  function-local statics. The shared link now places `crt1_ctors_begin.o`/`crt1_ctors_walker.o` first and
  `crt1_ctors_end.o` last (their symbols excluded from the export table), and `dllcrt.c` walks both regions on
  `DLL_PROCESS_ATTACH` (destructors on `FreeLibrary`, not at process exit). Weak references: a DLL linked without
  the markers (libc.dll itself) behaves as before. This is probably also the cause of the open "std::cout crashes
  with shared libc++.dll" note; not re-measured.
- *POSIX record locks over LockFileEx.* SQLite (the cookie database, local storage, IndexedDB) reported "database
  is locked": `fcntl(F_SETLK)` mapped 1:1 to `LockFileEx`, which is per handle, cannot convert a lock and cannot
  unlock part of or a span over several locks. `__crt_sys_fcntl` now keeps the process's view per file (volume +
  index): exact physical ranges through one owner handle, conversion and splitting done by unlock-and-relock,
  `F_GETLK` over the part not our own, `F_SETLKW` by polling, the entry dropped when the owner closes.
  `libc/tests/fcntl_record_lock_test.c` runs SQLite's SHARED/RESERVED/EXCLUSIVE sequence on two descriptors and
  against a second process. Limit: the conversion window is not atomic against other processes.
- *Windows host paths.* `getcwd()` outside a rootfs is `C:\dir`; a `file://` URL built from it reaches the
  POSIX-persona libcurl as `file:///C:/dir` and is rejected as a bad URL, and SQLite's unix VFS takes a path
  that does not start with `/` for a relative one (`/C:\dir/C:\dir/cookies.db`, unable to open). `libcrtweb`
  rewrites `file:` URLs and the data directory to the CRT's `/c/dir` spelling at the API boundary
  (`docs/design/libcrtweb_api_policy.md`).
- *Patch 0022.* `Connection::readyReadHandler()` returns when the descriptor was released under it
  (about one run in three printed `Error receiving IPC message on socket -1` at teardown and reported a second
  `didClose`; 15/15 clean after). Stacked on 0019; the manifest checker passes (22 patches).
- *Harness.* The new tests removed their scratch directories with `system("rm -rf ...")`, which finds no `rm` for a
  CRT program whose PATH is a host PATH; they use `libcrtweb/tests/common/crtweb_test_fs.h`. The driver's webview
  gate reads the PE export table (`llvm-readobj --coff-exports`) and the `.exe`/`.dll` names; `curl-openssl` had to
  be rebuilt for `file:` (`port-rebuild-curl-openssl`), as on macOS.

Open: Linux/aarch64 only if assembly changes (policy). The Linux/macOS hosts have not re-run `fcntl_record_lock_test`,
the `unix_socket_test` forwarding case, the DLL-constructor change (Windows only) or patch 0022.

### Web Tranche 4 replayed on macOS/arm64: `realpath` lost path tails, `remove` of a directory link, and a loader-path trap

The whole gate passes (`--webkit --run-tests`): B3-B7, `crtweb_api_test` (5/5), the in-tree `crtweb_webview_test`
(8/8) against `crtweb_host`, the `libcrtweb.dylib` export/header firewall, and the host ABI audit; `ctest` 191/191.
Found on the way, in order: the web prefix's curl was built before `file:` was enabled (rebuild; `crtweb_api_test` failed
at the first page); the launcher inside `crtweb_host` fell back to `./WebProcess` only when the host was started
through `/tmp`, which was **`realpath()` dropping every component after a symlink**: `resolve_symlink_path` copied the
tail from the buffer it was already overwriting (any path through `/tmp`, `/var`, `/etc` on macOS; Linux has the same
code); `remove()` used `stat` and so called `rmdir` on a symlink to a directory (now `lstat`); and a crash at exit
in the in-tree test that looked like a heap bug: the test links AppKit/Metal, `DYLD_LIBRARY_PATH` names the SDK, dyld
matches `/usr/lib/libc++.1.dylib` by leaf name and loaded the SDK's libc++ (and its libc) under the frameworks. The
gate now runs that test with no loader path. Tests: `file_path_test` (relative and absolute directory links with a tail,
`remove` of both). Open: Windows/x64 replay of Tranche 4. Not analysed: `Error receiving IPC message on socket N` notices
(one with `socket -1`, one `Function not implemented`) appeared in runs with the SDK on `DYLD_LIBRARY_PATH`; none in the
clean runs recorded here (driver log, 8 webview and 5 API runs).

### Web Tranche 4, milestones C4-C8: several runtimes, website data, downloads, popups/dialogs, IME (Linux/x86_64)

`crtweb` gained runtimes with their own website data store (ephemeral, or a data directory with a
cookie file, cache and storage; cookie accept policy; `crtweb_runtime_clear_data`), downloads (the
application chooses the destination, sees progress and the result), popups (`on_popup_request`/
`on_popup_created`, `window.close`), JavaScript dialogs answered asynchronously, and IME
(`crtweb_view_ime_*`: composition, commit of any UTF-8, cancel). Several runtimes share the one web
event loop; a pump services all of them. The WebView carries the same through the control extension
(IME, dialog, download decision and clear-data commands; download, popup, dialog and data-cleared events);
the host reports popups and the application opens another WebView for them, which is also the
multiple-runtime case. Patch 0021 adds the three Web-process messages the non-Cocoa port lacks for
composition and insertion. `crtweb_api_test` (now with a loopback HTTP server shared with
`crtweb_webview_test`) and the whole gate pass. Open: macOS/Windows replay (C3), local-storage clearing,
IME caret rectangle for candidate windows, authentication/permission prompts, file pickers, context
menus.

### Web Tranche 4: `crtweb` runtime/view API and the crtui WebView (Linux/x86_64)

The public API is plain C (`libcrtweb/include/crtweb/crtweb.h`): one runtime per process, views with
navigation, history, script, frame copy, input and callbacks, all on the owner thread and delivered from
`crtweb_runtime_pump()`; `libcrtweb.so` (`src/crtweb.cpp`) is the only code that sees WebKit and exports
`crtweb_*` only (policy: `docs/design/libcrtweb_api_policy.md`). The crtui WebView is *not* an in-process
call: an application linked against the static CRT/Skia/window stack cannot share an address space with
`libWebKit.so` (shared libc/libc++, a different Skia), so `crtweb::WebView` launches `crtweb_host`
(built from the public API alone) and speaks the 3A protocol plus a control extension (`crtweb_surface_
control.h`: chunked 128-byte COMMAND/EVENT messages for navigation, load/URL/history/script results).
Gates: `crtweb_api_test` (2.3 s, public header only), the in-tree `crtweb_webview_test` (a crtui window
composing and driving the page, 1.5 s), header and export firewall, host ABI audit of the host and
library, all earlier B3-B7 gates from a clean recompose. Found: web curl built without FILE (now
enabled), a stale SDK `libc.so` whose `inet_pton(AF_INET6)` returned -1 and made WebCore abort on `file:`
URLs non-deterministically, PlayStation's `platformUserAgent` ignoring the application name (patch 0020),
and a manifest checker that could not express stacked patches. Open: macOS/Windows replay, IME,
downloads/popups/dialogs, cookies, GPU frames, packaged `06-web`.

### Web Tranche 3B B5-B7 replayed on macOS/arm64: one PlatformCRT defect (patch 0019)

`build_webkit_crt_port.py --webkit --run-tests` runs B3-B7 on macOS: render through the surface protocol, the scripted
input run, 100 create/destroy cycles with the Host ABI audit in its Mach-O form (static recursive audit of every binary;
`vmmap` of the three live processes sampled by the driver). All pass; the lifecycle is flat (threads +0, descriptors +0,
RSS +96 KiB). What it took: the driver and `crtweb_lifecycle_test` learned macOS (loader path, libproc for children,
threads and RSS, `fcntl(F_GETFD)` descriptor count, host-side `vmmap`); a first design that ran `ps`/`vmmap` through
`popen` inside the test process was dropped because the observer is itself a child that changes the lifecycle under
test, and `proc_listchildpids` returns a count of pids, which first made the test believe it had no children and skip
the wait for the Web process (a measurement bug that looked like a leak of two threads per cycle). The leak that
remained was real: the PlayStation socket-monitor thread sits in `select()` holding its `Connection`, and on macOS a
`close()` from another thread does not wake it, so each cycle kept a monitor thread and a work queue. Patch 0019 bounds
that `select()` at 100 ms. Merged with the same-day Windows replay of B5-B7 (the driver's `gate_environment` and the
lifecycle test's per-host branches). The native WPE reference frame is not available on this host, so the B5 pixel diff
is not computed here. Open: Linux/aarch64 only on assembly changes (policy); B8 onward.

### Web Tranche 3B milestones B5-B7 replayed on Windows/x64: two kernel-handle leaks and a wrong peer found in the PAL

`build_webkit_crt_port.py --webkit --run-tests` on Windows now runs the same B5/B6/B7 gates as Linux
(`render_through_surface_protocol`, `input_through_surface_protocol`, `lifecycle_and_host_abi`) and passes 3/3
consecutive runs, no child left, `ctest` 184/184. No WebKit source needed a new patch; every defect was in the PAL.

- *B5/B6 passed first time:* `crtweb_render_test` (proof string identical, 678 colours, frame to `surface_probe`) and the
  full scripted 3A input run (pointer, key, wheel, text, resize, stalled consumer) work over the CRT's AF_UNIX
  SEQPACKET and memfd mappings, built with the CRT toolchain as `surface_probe.exe`. The diff against the native WPE
  reference is not computed (there is no WPE reference build on this host); the gate does not require it.
- *B7 failed: +7 kernel handles per create/destroy cycle* (RSS and threads were flat). `NtQueryObject` types of the
  UI process's handles (`CRTWEB_LIFECYCLE_HANDLES=1`, a diagnostic in the test) named them:
  1. **Thread + event per thread.** The signal backend duplicates a thread handle and creates a sigsuspend() event for
     every thread that touches it; `__crt_windows_signal_detach_current()` never closed them (3 threads per cycle).
     They are closed at thread end, under the sender's `injecting` lock so a concurrent `pthread_kill` cannot use a
     closed handle.
  2. **A socket per cycle, and a descriptor that did not arrive.** A process that made both ends of a `socketpair()` and
     handed one end to a child asks Winsock for the peer's pid and gets its *own*: the passed descriptor was
     duplicated into the sender itself (leak) and the receiver was given a handle value that is not a socket in its
     process (`ENOTSOCK`). WebKit's UI process forwards the Network process's sockets this way. The record now says
     the handle lives in the sender (`CRT_SEQ_FLAG_SOURCE_PROCESS`, sender pid) and the receiver takes it with
     `DuplicateHandle(..., CLOSE_SOURCE)`. `unix_socket_test: test_descriptor_sent_to_inheriting_child` fails on the old
     code and passes on the new. Limit: a sender that exits before the receiver receives takes the descriptor with it.
- *Host ABI audit, Windows form.* Static: the `llvm-objdump -p` import closure of every produced `.exe`/`.dll` (CRT
  SDK, port prefix, build tree, OS runtime contract). Runtime: the Toolhelp module list of the UI, Web and Network
  processes (111 files) is the CRT SDK, the port prefix, the build tree and System32 DLLs. `msvcrt.dll` and
  `ucrtbase.dll` appear in every Windows process (advapi32/RPCRT4 import them), so the runtime list only reports OS
  DLLs; a CRT binary importing the host C runtime is what the static half catches.
- *Test.* `crtweb_lifecycle_test` has a Windows branch (Toolhelp children, `K32GetProcessMemoryInfo`,
  `GetProcessHandleCount`, thread count); its "descriptors" are kernel handles. Result: 100/100 cycles,
  +0 handles, -36..-56 KiB RSS (13.6 MiB), +0 threads, about 85 s.

Open: macOS and Linux/aarch64 replays of B5-B7; the Linux/macOS hosts have not re-run `unix_socket_test` with the new
case; forced Web process termination and recovery (Tranche 5).

### Web Tranche 3B milestone B7: Linux/x86_64 prototype closed (100x create/destroy, Host ABI audit, rechecks)

`crtweb_lifecycle_test` creates a context, page configuration and 320x240 view 100 times, loads a canvas
page, checks the painted pixels, then releases everything and waits for the Web process to exit.
Result: 100/100 iterations, after warm-up +0 descriptors, +0 threads, +264..332 KiB RSS (65.9 MiB, flat),
no orphaned child after the run, about 80 s. The Network process is deliberately *not* required to
exit: it belongs to upstream's default website data store and stays one reused process (the test checks
it never exceeds one). The test's first run aborted in `WebProcessProxy` teardown
(`crashDueToCheckedPtrToDeadObject` under `RunLoop::TimerBase::~TimerBase`). Root cause, upstream:
`RedBlackTree::remove` leaves the removed node's `CheckedPtr` `m_left`/`m_right` pointing into the tree;
the generic RunLoop's `ScheduledTask` is such a node, so when a former child timer task was destroyed
first (zeroed by `WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR`), the removed node's destructor decremented a
zero count. Carried patch 0018 clears the two links in `remove()` (BSD-3-Clause file; PlayStation's
generic RunLoop should meet the same crash, not confirmed). Gates added to
`build_webkit_crt_port.py` (`lifecycle_and_host_abi`): the Host ABI audit now covers every produced
binary statically (ldd closure of `libWebKit`, `WebProcess`, `NetworkProcess`, the tests; `surface_probe`
is statically linked) *and* at runtime: the UI, Web and Network processes' `/proc/<pid>/maps` (123 files)
contain only the CRT SDK, the port prefix, the build tree, the bundled font, the fontconfig cache and the
host dynamic loader. License scan re-run on the pinned tree: identical to
`libcrtweb/third_party/webkit/license-scan.json`; patch manifest (18 patches, 56 modified files) and
`test_webkit_licensing.py` pass. Open: macOS/Windows/aarch64 replays of B5-B7, forced Web process
termination and recovery (Tranche 5).

## 2026-10-12

### Web Tranche 3B milestone B6: input through the web surface protocol

`crtweb_surface_host` (the CRT port's frame producer) is run against the unchanged, complete 3A
consumer `surface_probe`: pointer click -> page report and pixel change, key code and modifiers, wheel,
committed text into a focused field, resize, and a stalled consumer all pass (3/3 consecutive runs,
4-5 s). Input enters through `WKViewCRTHandlePointer/Wheel/Key/Text` (libwpe events built from the
contract's keysym + XKB keycode + modifiers). The run found five things: the fontconfig port installs
no configuration, so generic and default family names matched nothing and an empty `<input>` collapsed
to 6 px (aliases now in the driver's configuration); PlayStation's key-to-editing path is a stub
(patch 0017); `WKViewSetSize` does not reach the drawing area (`WKViewCRTSetSize`); a shrink leaves
the old backing store looking current (patch 0016); and the first frame went out before the first
paint. The legacy loader client rejects a title callback, so the host polls `WKPageCopyTitle`. Open:
IME, non-US layouts, touch/pen, drag, scale factors, B7 (100x create/destroy, leak audit, closing
rechecks), macOS/Windows/aarch64 replays of B5 and B6.

### Web Tranche 3B milestone B5: first render through the web surface protocol

`DrawingAreaCRT` paints on the CPU (dirty rectangles into a `ShareableBitmap`, `DrawingAreaProxy::Update`,
`DisplayDidRefresh` pacing: the protocol of upstream's non-composited drawing area, re-implemented
against today's `DrawingArea`), the UI process composes into upstream's Skia `BackingStore`, and
`WKViewCRTCopyPixels` copies it out. `libcrtweb/platform/crtweb_surface_producer.h` is a header-only C
implementation of the 3A producer contract (HELLO/CONFIG, a pool of three memfd buffers, ACK pacing,
BUFFER_RELEASE ownership). `crtweb_render_test` renders the Tranche 2 fixture in a 640x480 view, checks
the proof string against the WPE reference's, and sends the frame to `surface_probe --reference` (a new
mode of the 3A consumer, built with the CRT toolchain); the driver diffs the frame against the native
WPE reference. Result: proof identical, 678 colours incl. all five fixture colours, solid regions
identical to the pixel, 8.3 % of pixels differ, all text (the CRT font set differs from the WPE
host's). 15/15 runs. Open: input (B6), 100x create/destroy (B7), animation/scrolling/resize, damage.

### Web Tranche 3B milestone B4 replayed on Windows/x64: process kill, handle lifetime and AF_UNIX close fixed in the PAL

`build_webkit_crt_port.py --webkit --run-tests` (Windows host, persona clang/lld, imported libc++) builds a static
`libWebKit.a`, `WebProcess`, `NetworkProcess` and `crtweb_process_test`; the gate passes (`crtweb_process_test` 10/10
consecutive runs, no `WebProcess`/`NetworkProcess` left in the process list, `crtweb_webcore_graph_test` unchanged).
Nothing in upstream WebKit's IPC changed: the unix backend, `posix_spawn` launcher and generic run loop run over the PAL.

- *Build.* Framework header copies are symlinks when the host can create them (probe in `OptionsCRT.cmake`; needs
  Developer Mode on Windows). `libWebKit` is static on Windows (a DLL would export 206,769 symbols, the PE limit is
  65,535); `dllcrt.o` has a weak `__dso_handle` for the DLLs that remain. `-D_LIBCPP_WIN32API` is added to the WebKit
  flags: without it WebKit's `std::filesystem` calls disagree with the imported libc++ DLL about `path::value_type`
  (`wchar_t` vs `char`), which was a SIGSEGV in `recursive_directory_iterator`; the resulting `std::string`
  conversion in `FileSystem.cpp` is carried patch 0015. `--recompose` reconfigure passes every cache define.
- *`readlink("/proc/self/exe")`* returns the executable path (forward slashes), so the launcher's sibling lookup works.
- *Cross-process `kill()`.* There are no signals: `kill(pid, 0)` probes with `OpenProcess`, `SIGKILL` is
  `TerminateProcess` with an exit code tagged `0x4B110000|sig`, which `waitpid` reports as `WIFSIGNALED`; other signals
  are `ENOSYS` (`spawn_kill_test`).
- *Descriptors in flight.* A socket passed by `SCM_RIGHTS` or inherited by `posix_spawn` was `WSADuplicateSocket`ed;
  that copy dies with the sender, and the Web process lost its connection when the UI process's Network-process end
  closed. Sockets are now `DuplicateHandle`d into the target (`WSAStartup` is run on import) so the connection
  outlives the sender.
- *Graceful AF_UNIX close.* An abortive last close discards unread data and the peer sees a reset, not EOF. When a
  close or process exit drops the last handle of a socket (`NtQueryObject` `HandleCount == 1`) it is `shutdown(SD_SEND)`
  first, so the peer reads what was sent and then EOF.
- *Handle inheritance.* CRT sockets are created non-inheritable (`SetHandleInformation`); an inheritable handle in a
  later-spawned child kept the NetworkProcess's connection alive after the UI process exited.
- Test: `crtweb_process_test` lists children with Toolhelp32 (no `/proc`) and skips processes that have exited but
  are still held by a handle, and gives the reaper up to 5 s to retire the killed process (it took ~100 ms).

Dropped: `unix_select_test` and `unix_fd_inflight_test` (their results depended on the fork-capable relaunch hop test
executables use, not on the behaviour under test). Open: Windows B5 onward; the Linux/macOS hosts have not re-run the
shared changes (`spawn_kill_test`, `OptionsCRT.cmake`, patch 0015, `crtweb_process_test`).

### Web Tranche 3B milestone B4 replayed on macOS/arm64: three libc defects found and fixed

`build_webkit_crt_port.py --webkit --run-tests` builds `libWebKit.dylib`, `WebProcess` and `NetworkProcess` on macOS
and `crtweb_process_test` passes (10/10 consecutive runs, no child left, no IPC error on stderr, `crtweb_webcore_graph_test`
unchanged). Two compile errors and three runtime defects stood between the Linux result and that.

- *Compile.* `WebPasteboardProxyLibWPE.cpp` defines `readStringFromPasteboard(..., size_t index, ...)` for a declaration
  with `uint64_t`: the same type on LP64 Linux, distinct on Darwin (`unsigned long` against `unsigned long long`); carried
  patch 0014 spells the definition with the declaration's type. `WKPage.cpp` includes `<Block.h>` under `__BLOCKS__`,
  which clang defines for Darwin targets; `crt-cc`/`crt-c++` pass `-fno-blocks` outside the real-SDK compiles (the persona
  has no Blocks runtime).
- *`readlink("/proc/self/exe")`* mapped the name to the executable and then read that regular file as a link (`EINVAL`), so
  WebKit's launcher fell back to `./WebProcess`. It now returns the canonical executable path, as Linux does.
- *`posix_spawn` of a missing executable returned 0.* The fork/exec implementation (Linux and macOS) exited 127 in the
  child without telling the parent; the launcher believed it had a child that never answered. A close-on-exec status pipe now
  carries the failing `errno` of the file actions, the attributes or the exec back to `posix_spawn()`, which reaps the child
  and returns it (`spawn_status_test`).
- *A framed SEQPACKET socket sent as `SCM_RIGHTS` arrived as a plain stream.* The framing flag is per process, so the
  Web process wrote raw bytes to its end of the pair the Network process made, and the Network process (framed) read
  `00 00 01 68` as a 1.7 GB record length: `Error receiving IPC message ... Unknown error 71` and a connection that never
  worked, hidden by the test passing on the UI-process pair alone. The length word now has a flag bit announcing an 8-byte
  mask of the record's descriptors that are framed in the sender; the receiver frames exactly those on delivery
  (`unix_socket_test`: `test_passed_seqpacket_socket`; it fails with the delivery step removed).
- `strerror` had no text for `EPROTO` and 18 other errno values Bionic defines (`Unknown error 71`).
- `crtweb_process_test` finds children with `ps(1)` on macOS (no `/proc`).

SDK note: the installed `05-ui` wrappers and `libc` were older than the tree; they were refreshed for this replay (the
`libc.dylib` must be deleted before it is copied: overwriting a signed dylib in place gets the process `SIGKILL`ed).
Open: Windows B4 (the `win` launcher), Linux/aarch64 only if assembly changes (policy), B5 onward.

### Web Tranche 3B milestone B4: the WebKit multi-process model on CRT

`libcrtweb/port/Source/WebKit/PlatformCRT.cmake` builds `libWebKit.so`, `WebProcess` and
`NetworkProcess` from the pinned commit (patches 0010-0013, five new CRT files). The gate,
`libcrtweb/tests/webkit/crtweb_process_test`, is a UI process on WebKit's C API: creating a view
launches a Web and a Network process; a loaded HTML string round-trips (`didFinishNavigation`);
`kill -9` of the Web process is detected (`webProcessDidCrash`). 20/20 runs, 0.7 s each, no child
left behind; the driver runs it after the B3 program (`--webkit --run-tests`). Reading the pinned tree
showed that upstream's PlayStation WebKit list is stale (a mis-cased `WKRunloop.cpp`, a compositor
that no longer builds, an SDK-only bundle loader, `PLATFORM(PLAYSTATION)` members), and that the
maintained compositing stack is GLib/EGL: the Web process therefore draws through a CRT
`DrawingAreaCRT` (a no-op in B4, CPU painting in B5). Clang needed the framework header copies to be
symlinks (a `#pragma once` is per file). The launcher is `posix_spawn` + a SOCK_SEQPACKET pair
(argv: process identifier, descriptor), one reaper thread per child. Open: macOS and Windows replays
(`/proc/self/exe`, the `win` launcher), painting and input (B5, B6), 100x create/destroy and the leak
audit (B7).

### Web Tranche 3 replayed on Linux/aarch64: B1, B2, B3 and the 3A consumer contracts are green

Ubuntu 26.04 aarch64 QEMU guest (4 CPUs, 7.4 GB RAM), Clang 21.1.8, at `be9df3a`, the in-tree SDK rebuilt with
`cmake --fresh` (clean build, 0 warnings; `crt-ui-dist` verified; `ctest` 157/157). Nothing in CRT, the carried patches
or the harnesses had to change.

- **B1.** `build_webkit_jsc.py --port CRT --mode baseline-jit` from an empty work root: 44 acceptance steps pass
  (compose 42 s, configure 9 s, build 348 s, acceptance 63 s) with a clean host-ABI audit; `--mode interpreter` passes
  too (build 292 s).
- **B2.** `port-build-web`/`port-test-web`: all 16 run tests pass (OpenSSL TLS 1.3 handshake in process; the
  OpenSSL-backed libcurl over HTTPS on IPv4 and `::1` with the `CURLOPT_SSL_CTX_FUNCTION` hook; HarfBuzz, libjpeg-turbo,
  libwebp, libxml2, libpsl, fontconfig, libwpe), static and shared. `check_web_dependencies.py` resolves all eleven
  `find_package()` calls inside the prefix.
- **B3.** `build_webkit_crt_port.py --run-tests -j3`: 7,857 steps in 26 min 30 s on four cores (Release, 3.2 GB build
  tree). `crtweb_webcore_graph_test` prints element `<div/>`, text width 86.70 and 216 dark pixels, the Linux/x86_64,
  macOS and Windows numbers. The audit finds no GLib symbol and 26 `DT_NEEDED` entries, all SDK, port or loader.
- **3A.** `crtweb_wire_layout_test` and `crtweb_wire_transport_test` pass in the plain tree; with Skia on (pinned
  Skia m148 from `crtgfx-skia-smoke`, 11 min) `crtweb_web_surface_test` prints `ok` and passed 20 of 20 runs, the Skia
  smokes pass and the full `ctest` is 164/164. The WPE `crt` display module and the native end-to-end run are **not**
  replayed here by decision: Tranche 2's native WPE reference is the Linux/amd64 comparison baseline only, and the
  module and its end-to-end run build against it. (The aarch64 host was prepared for it and the reference build was
  started, then stopped; the build tool stays x86_64-only and no aarch64 change was kept.)

What the replay found:

1. *A repository defect, not a host one.* `dbff12d` added `CurlSSLHandleCRT.cpp` and used it from
   `PlatformCRT.cmake` without declaring it in `libcrtweb/patches/manifest.json`, so
   `test_webkit_licensing.test_the_committed_manifest_passes` failed on every host at `be9df3a` (119 of 120 tooling
   tests). The B4 commit on `main` (`d8c6686`) declares the same file, with its purpose updated for the CA bundle, so
   the merged tree has one entry and the replay's identical one-line fix was dropped in favour of it; with it tooling
   is 120/120 and `check_webkit_patch_manifest.py --tree` passed against the composed checkout.
2. *The dependency check needs more than `port-build-web`.* WebKit's `find_package(PNG)` and `find_package(SQLite3)` are
   among its eleven, but libpng and SQLite are not in the `web` group, so a prefix built only with `port-build-web`
   fails `check_web_dependencies.py` with `Could NOT find PNG`; building `port-build-libpng` and
   `port-build-sqlite-amalgamation` fixes it.
3. *Scratch space.* On this host `/tmp` is a 3.7 GB tmpfs (`usrquota`), which is memory and swap. A WebKit work root
   there fills it (`EDQUOT`, and tool output is lost); work roots belong under `out/`.

Also: the first-run fontconfig scan of the bundled fonts took 73 s here (about two minutes on Linux/x86_64).
Open: B4-B7; the CA bundle and the SIMD/asm paths stay off, as on the other hosts. The Tranche 2 reference and the WPE
`crt` module are not part of the aarch64 matrix.

### macOS AF_UNIX SOCK_SEQPACKET layer re-examined and hardened after review

A review of the first CRT-framed implementation (`6152300`) was checked against the source; the findings that held
were fixed: non-blocking/`MSG_DONTWAIT` receive now keeps a partly received record per socket (it previously read the
payload with blocking reads), descriptors that arrive with a record are owned by the state until delivery and closed
when the record fails or does not fit the caller's control buffer, the framing flag is handed across `exec` through
`CRT_SEQPACKET_FDS` (the answer to B4's launcher question), `readv`/`writev` are one record, sends are serialized
and `dup`s share state, and a framed `dup` beyond the 4096-entry table is `EMFILE`. Regression tests for each
(`unix_socket_test`); each was confirmed to fail with its fix removed. Kept by decision: a sender that already wrote
part of a record completes it (`EAGAIN` only when nothing was written).

### Tranche 3B replayed on Windows/x64: B1, B2 and B3 (JavaScriptCore, the web ports, WebCore with Skia)

`build_webkit_jsc.py --port CRT --mode baseline-jit` passes the whole acceptance (every Baseline/DFG/FTL/Wasm step, host ABI audit clean; one
earlier run hit the known ~1% JSC `Invalid value for lock: 0` abort in a 4-thread context-cycle process and passed on
the repeat, as recorded for the JSCOnly build). Seven web ports build and pass their run tests with
`crt-port-build.py --use-crt-shell --test` (libjpeg-turbo, libwebp, libxml2, libpsl, HarfBuzz static+shared; OpenSSL
and the OpenSSL-backed libcurl **static only**, including curl HTTPS over `::1` through the Windows IPv6 layer), plus
SQLite, fontconfig, libwpe and the `crt-egl` no-GPU libraries; `check_web_dependencies.py` resolves all eleven
`find_package()` calls inside the prefix; and `build_webkit_crt_port.py --run-tests` builds WTF, bmalloc, JavaScriptCore,
PAL, WebCore and the bundled Skia from the pinned commit and passes the B3 gate with the Linux and macOS numbers
(text width 86.70, 216 dark pixels, no GLib symbol, imports only CRT DLLs, port DLLs and KERNEL32).

What the replay found, all fixed in the CRT, the tooling or the carried files:

- *Link-time symbol resolution differs.* lld's COFF driver reports an undefined symbol from any object a link pulls
  in; an ELF link with `--gc-sections` drops it silently when the referencing function is unreferenced. The WebCore link
  therefore exposed four references that Linux and macOS have always carried unresolved: `__dso_handle` (a weak
  definition in `crt1.c`; libc++abi.a's shim still wins where it is linked), `__cxa_thread_atexit` (libc++abi builds
  it for UNIX only: `libc/src/arch/windows/common/cxa_thread_atexit.c`, on the pthread key CRT already runs at thread
  exit, test `cxa_thread_atexit_test`), `CurlSSLHandle::platformInitialize()` (a CRT-owned empty hook,
  `libcrtweb/port/.../CurlSSLHandleCRT.cpp`; the CA bundle stays unset) and `SkStrikeRef.cpp`, which WebKit's own Skia
  CMake list omits (added to the `Skia` target from `PlatformCRT.cmake`).
- *Command lines.* Windows passes at most 8,191 characters to a `.cmd` compiler wrapper (a WebCore compile line has
  several hundred `-I`/`-D`): `CMAKE_NINJA_FORCE_RESPONSE_FILE=ON` moves defines, includes and flags into response files.
  And a CRT-to-CRT spawn whose argument vector exceeds CreateProcess's 32,767 characters (OpenSSL's `ar qc libcrypto.a
  <1,500 objects>`) failed with `E2BIG`: `__crt_sys_posix_spawn` now writes such a vector to a private temp file named by
  `CRT_ARGV_FILE`, which the child's startup (`crt1.c`) reads and deletes; a native Windows program keeps the limit.
  Test `spawn_long_argv_test` (4,000 arguments, 160 KB, plus a short grandchild).
- *Native programs and redirection.* A native (non-CRT) Windows tool run by a CRT shell only receives the shell's
  redirections when spawned in "native" mode (`CRT_SPAWN_NATIVE_WINDOWS`); `pkg-config` and Python now go through
  `tools/crt-native-tool` like `ar`, and OpenSSL's generators run as `CRT_SPAWN_NATIVE_WINDOWS=1 perl` (the output file
  was silently empty and the build then died on "did not return a true value").
- *`crt-port-build.py` on Windows:* the `cmake` build system (compilers are the `.cmd` wrappers, native CMake
  detection, `-I<prefix>/include` because CMake drops an implicit directory), a space-free `CONFIG_SHELL`
  (`Program Files`), `PKG_CONFIG`/`PYTHON` for configure, `configure_interpreter` (OpenSSL's `./Configure` is Perl),
  `@HOST_PERL@`, `@ROOT_POSIX@` (`/c/Users/...`, the spelling fontconfig accepts as absolute), test `skip`, DLL+import
  library output for `crt_shared_libraries`, forward-slash `--prefix` for `install_subdir`, and a `fetch_ports.py` that
  skips archive members whose path exceeds the 259 characters a classic Windows path allows (HarfBuzz test data).
- *OpenSSL.* Windows' ABI is LLP64: the `linux-x86_64` target says `SIXTY_FOUR_BIT_LONG`, so `BN_ULONG` was a 32-bit
  `unsigned long` counted as 64 bits ("bignum too long" at the first EC key). `porting/shims/win32/openssl-crt-windows.conf`
  defines `crt-windows-{x86_64,aarch64}` (the Linux targets with `SIXTY_FOUR_BIT`). Shared OpenSSL is not built (the
  Unix target's version script and soname have no PE counterpart), so libcurl is static as well.
- *Toolchain.* `crt-cc` drops `-lpthread`/`-lrt` on Windows as on macOS; the builtins archive no longer contains
  compiler-rt's `eprintf.c.obj`, whose `fprintf` shadowed libc.dll's (it broke linking Skia into `libcrtgfx_skia.dll`);
  `build_webkit_jsc.py` finds `kernel32.lib` for the wrappers; the WebKit driver sets `PYTHONUTF8` (the CSS value
  generator read a UTF-8 file as cp949), uses a native Windows Perl (MSYS Perl compares `/c/...` with `C:/...` in the
  bindings generator) and `NVALGRIND` for bmalloc (valgrind.h casts pointers to `unsigned long`).

Host tools the Windows replay needs beyond CMake/Ninja/Python/Ruby: `pkgconf` (libxml2, libpsl and fontconfig configure
scripts, WebKit's Find modules) and a Perl that has `ExtUtils::MakeMaker`, `IPC::Cmd` and friends (OpenSSL's
`Configure`; Git for Windows' Perl lacks them, so the run used MSYS Perl with a small overlay of pure-Perl core
modules, and Strawberry Perl for WebKit's generators). Windows CTest 182/182.

### Tranche 3 replayed on macOS/arm64: 3A contracts and B3 (WebCore, PAL, Skia)

`crtweb_wire_layout_test`, `crtweb_wire_transport_test` and `crtweb_web_surface_test` pass on macOS, and
`build_webkit_crt_port.py --run-tests` (now macOS-aware) builds WebCore, PAL and the bundled Skia from an empty work
root and passes the B3 gate with the Linux numbers (text width 86.70, 216 dark pixels, no GLib symbol, only CRT
libraries and `libSystem`). Found and fixed: Darwin has no AF_UNIX `SOCK_SEQPACKET` (CRT-framed stream sockets with
`SCM_RIGHTS` in libc; this settles the macOS transport decision); `MSG_*` flags were Linux numbers on Darwin
(`MSG_DONTWAIT` arrived as `MSG_WAITALL`) and `cmsg_len` after `MSG_CTRUNC` was not clamped; the in-tree macOS C++ test
saw `std::string` size 0 through SDK libc++ headers over the imported libc++ archive; fontconfig/libwpe/crt-egl
ELF-isms (gperf probe on `PATH`, `statfs` branch, `eglplatform.h`, `.so` stub libraries); patch 0009 (WTF persistence
coders for `size_t` under Darwin integer types); and two `libcurl` images in one program through `PKG_CONFIG_PATH`
order. The WPE `crt` module stays Linux-only. Linux/aarch64 and Windows B3 remain open.

### Windows/x64: AF_UNIX streams, socketpair(), SOCK_SEQPACKET and SCM_RIGHTS in the CRT PAL (Gates A and B)

The Tranche 3A transport (one `AF_UNIX`/`SOCK_SEQPACKET` connection per view, a memfd descriptor per buffer) needs three
things Windows' own AF_UNIX (stream sockets bound to a path, Windows 10 1803+) does not have. They are now CRT library
functionality, not a WebKit adapter (`libc/src/arch/windows/common/syscall.c`, "AF_UNIX on Windows"):

- **Stream sockets.** `sockaddr_un` is rebuilt for Winsock with its path mapped onto the host (`translate_path_for_host`);
  a name that does not fit 108 bytes after the mapping is `ENAMETOOLONG`, an abstract name is `ENOTSUP`. A stream socket
  is never framed or altered: a native AF_UNIX peer sees exactly the bytes written.
- **`socketpair()`** (AF_UNIX, stream or seqpacket, `SOCK_CLOEXEC`): a throw-away path in the temp directory, one
  connect, one accept, and the path is removed before the call returns; both fds know their peer is this process.
- **`SOCK_SEQPACKET`**: a Winsock stream socket the CRT frames (16-byte header with magic, payload length and
  descriptor count; descriptor records; payload). `send`/`recv`/`write`/`read`/`sendmsg`/`recvmsg` keep record
  boundaries, a short buffer truncates and sets `MSG_TRUNC` (and `MSG_TRUNC` in `flags` returns the real length), a
  zero-length record is a record, end of stream is 0, `MSG_DONTWAIT` and `O_NONBLOCK` work, `SO_TYPE` answers
  `SOCK_SEQPACKET`, an accepted socket inherits the type and a spawned child keeps it (snapshot flag). Both ends must be
  CRT programs. `MSG_PEEK` is `EOPNOTSUPP` (the receive path never reads ahead, so `poll()` stays truthful).
- **`SCM_RIGHTS`** (SOCK_SEQPACKET only; on a stream socket it stays `ENOTSUP`): the sender asks the stream for the
  peer's process id (`SIO_AF_UNIX_GETPEERPID`; the pair's own id for a `socketpair()`), then makes the object available
  to that process the way a kernel would: `DuplicateHandle` into it for files, pipes and memfds, `WSADuplicateSocket` for
  sockets, and the record carries the receiver-side handle (or the protocol info). The receiver builds a new CRT fd per
  record (`MSG_CMSG_CLOEXEC` honoured, a control buffer that is too small drops and closes the rest and sets
  `MSG_CTRUNC`). A message is delivered whole or not at all; on a failed send the duplicates are closed again.

Gate C (same day): Skia m148 built for Windows (`crtgfx-skia-fetch/configure/build`, `CRTGFX_ENABLE_SKIA=ON`) and
`crtweb_web_surface_test` enabled for Windows in `libcrtweb/CMakeLists.txt`; it prints `ok` and passes 8/8 runs, with
`crtgfx_skia_raster_smoke` and `crtgfx_skia_cpu_coverage` also passing. The `SurfaceClient` consumer path now has a
Windows replay, over the CRT's own AF_UNIX transport, with the wire v1 unchanged.

Evidence: `unix_socket_test` (new; Gate A stream path/half-close/EOF/EADDRINUSE/non-blocking/poll/socketpair, Gate B
records/truncation/SO_TYPE/path listen-accept, and descriptor passing of a file, a memfd mapping and a TCP socket in one
process, three at once with CLOEXEC, a small control buffer, and a file plus a memfd between two processes with the
child's write visible in the parent's mapping); `socketpair_test` now runs its Linux body on Windows (it asserted
`ENOSYS`); `crtweb_wire_transport_test` (new; the frozen 3A wire helpers `crtweb_wire_send/recv` over the real
transport, HELLO/CONFIG/BUFFER_ADD with a memfd/FRAME/TITLE/ACK/RELEASE between a consumer and a producer process, plus
the EPROTO case). Two mutations (the `MSG_TRUNC` flag, the handle duplication) fail the tests. Full CTest 170/170, the AF_UNIX
and IPv6 socket tests 25 consecutive runs.


### Windows/x64: IPv6 sockets, socket option numbering, and the Tranche 3A replay

Asked whether the macOS IPv6 finding also holds on Windows: it did. The Windows socket layer handed Winsock the Bionic
numbers unchanged, so `socket(AF_INET6)` (Bionic 10, Winsock 23) failed with `EAFNOSUPPORT`, a `sockaddr_in6` carried
the wrong family, and `getaddrinfo()` kept `EAI_FAMILY`; `SOL_SOCKET` options other than `SO_REUSEADDR`/`SO_ERROR`/`SO_TYPE`
went through with Linux numbers (`SO_KEEPALIVE` 9, `SO_BROADCAST` 6, `SO_LINGER` 13, `SO_SNDBUF` 7, `SO_RCVBUF` 8 are other
options or nothing in Winsock; `SO_PASSCRED` 16 even landed on `SO_DONTROUTE`). The pulled `socket_ipv6_test` also did not
build on Windows (`-Werror` unused functions behind its Windows branch). Done in the Windows PAL
(`libc/src/arch/windows/common/syscall.c`): the AF_INET6 domain, the `sockaddr_in6` family field on bind, connect,
sendto/recvfrom, sendmsg/recvmsg, accept, getsockname and getpeername (the layout is the same, only the family value
differs), a `SOL_SOCKET` table (reuse, type, error, broadcast, send/receive buffer, keepalive, linger with its
`{int,int}` to `{u_short,u_short}` conversion) and an `IPPROTO_IPV6` table (unicast hops, multicast if/hops/loop,
`IPV6_V6ONLY`); an option without an equivalent answers `ENOPROTOOPT`. `getaddrinfo()` returns IPv6 results on Windows
(it keeps no AAAA resolver, so a real name is still answered from A records) and answers `localhost`/`*.localhost`
as loopback itself, `::1` then `127.0.0.1` (RFC 6761). `socket_ipv6_test` runs in full on Windows and gained options,
`sendmsg`/`recvmsg` with a `sockaddr_in6` name and a dual-stack case; mutating the domain, the linger conversion or
the hop-limit number each makes it fail. Full CTest 167/167 before the test additions.

Tranche 3A on Windows, what replays and what does not. The wire contract (`crtweb_surface_wire.h`) is checked by the new
`crtweb_wire_layout_test` (every message size, field offset and message number; passes under LLP64). `socketpair_test`
keeps asserting `ENOSYS` for `socketpair()` on Windows. Not replayed: the consumer-side `crtweb_web_surface_test`
(`SurfaceClient` over `SOCK_SEQPACKET`, `SCM_RIGHTS` and a memfd, with Skia) and the WPE `crt` display (Linux-only by
construction); both need a Windows transport decision and a Skia build of this tree (`CRTGFX_ENABLE_SKIA` is off here).

## 2026-10-11

### Web Tranche 3B milestone B3: WebCore and PAL build and link for the CRT platform with the bundled Skia

`tools/build_webkit_crt_port.py` (new) composes the tree (pinned commit + `libcrtweb/port` + carried
patches), configures `-DPORT=CRT -DCRT_ENABLE_WEBCORE=ON` against the B2 prefix, builds, and with
`--run-tests` runs and audits `libcrtweb/tests/webcore/crtweb_webcore_graph_test`. `OptionsCRT.cmake`
gained the WebCore configuration (PlayStation-shaped, Skia, curl + OpenSSL, no GPU process, WebGL,
media or WebRTC) and `WebCore/PlatformCRT.cmake` plus `PAL/pal/PlatformCRT.cmake` list the files:
PlayStation's plain stubs unchanged, libwpe's key/pasteboard/editor files, the EGL context files
against a "no GPU" library. The bundled Skia m154 builds with CRT's clang and libc++ (782 sources),
and WebCore (about 6,600 units) compiles and links with it. The test program starts WTF and JSC,
calls `ProcessWarming::initializeNames()`, creates a `Document` and an element without a Page,
paints two rectangles into a Skia `ImageBuffer` and checks the pixels, and draws text through
fontconfig -> FreeType -> HarfBuzz -> Skia (86.70 px for nine DejaVu Sans Mono glyphs at 16 px, 216
dark pixels); the program references no GLib symbol and needs only SDK, port-prefix and loader
objects.

What building found that the plan did not have: fontconfig is required (the bundled Skia and
WebKit's Skia font backend use it off Android/Windows), so decision D6 is revised and fontconfig 2.17.1
is a port; WebCore links EGL and GLES unconditionally and libwpe's CMake needs an EGL library, so a
`crt-egl` port provides the Khronos headers and CRT-built `libEGL.so`/`libGLESv2.so` that answer
every call with failure (replaced in Tranche 9); libwpe 1.16.3 is a static port as in PlayStation
(its shared loader uses `dlopen()`, which the CRT Linux libdl does not implement); WebKit's OpenSSL
crypto is written for OpenSSL 1.1 (a force-included shim for `EVP_PKEY_get0_RSA`, one file);
`PLATFORM(PLAYSTATION)` selects types in nine WebCore files (patch 0007) and `Page.cpp` calls a
setting that exists only for GTK/WPE (patch 0008), because upstream's PlayStation port does not build
with Skia at this commit. `tools/crt-port-build.py` gained `build.system headers` and
`crt_shared_libraries`. In CRT: the BSD `timer*` macros, `initstate()`/`setstate()`,
`shm_open()`/`shm_unlink()`, and the Bionic liblog functions (declared, never defined); in
`tools/crt-c++`, `__dso_handle` for shared-runtime executables and the loader as a needed object so
`__tls_get_addr` (libc++abi, ICU) resolves for `thread_local` destructors.

Verified: see `docs/acceptance/crtweb_acceptance.md` ("B3 result"). Not exercised: node insertion into
a Page-less document (crashes in `Element::insertionSteps`; needs a window), networking and TLS, the
process model. Open: macOS, Linux/aarch64, Windows; the CA bundle; CRT-owned user agent and display.

### IPv6 on macOS through CRT libc, curl HTTPS over ::1 without Apple frameworks

CRT's macOS socket layer was IPv4-only. Added: `AF_INET6` socket/sockaddr_in6/IPV6_* translation to Darwin, IPv6
`inet_pton`/`inet_ntop`/`in6addr_*`, a list-returning `getaddrinfo()` with IPv6 literals and host-resolver answers
translated into Bionic structs (Windows unchanged), a wider `SOL_SOCKET` option table. curl (both recipes) keeps IPv6
on and drops the framework link through a fail-closed `TargetConditionals.h` shim on the include path. `socket_ipv6_test`
and `curl_openssl_https_ipv6_test` (IPv6 literal and `localhost`, shared and static) pass; ctest 180/180. Linux/Windows
effects of the shared `inet_*`/`getaddrinfo` changes are not re-run here.

### Web Tranche 3B B1 and B2 replayed on macOS/arm64

`--port CRT` (interpreter and Baseline/DFG/FTL/Wasm, 43 steps, clean Mach-O audit) and all seven web-group ports
(static and shared run tests from a clean state, OpenSSL TLS 1.3 handshake and curl-openssl HTTPS included) pass, and
WebKit's own `find_package()` resolves all eleven dependencies; every image loads only CRT libraries and
`libSystem`. Found and fixed: patch 0006 (the pinned commit's top-level CMake selects an Apple SDK on a Darwin
host); `-lpthread`/`-lrt` bound libc symbols to the host `libSystem` through `libpthread.tbd` (SIGBUS in a static
OpenSSL test); a static `libc.a` in a test executable next to the ports' `libc.dylib` (two libcs, intermittent heap
corruption with a server thread; `crt-cc` now honours `CRT_CXX_RUNTIME_LINKAGE=shared` on macOS and tests can carry
an `env`); ELF-only flags and `.so` names in recipes; OpenSSL's Darwin `Configure` target; curl-openssl linking
Apple frameworks unless `--disable-ipv6`. Open then: IPv6 on macOS in both curl recipes (closed the same day, next entry), Linux/aarch64 and Windows for B2.

## 2026-10-10

### Web Tranche 3B milestone B2: the WebCore dependency ports

Seven ports on Linux/x86_64, all run-tested, in a new `group: web` that gets `port-build-<name>`,
`port-test-<name>`, `port-build-web` and `port-test-web` but stays out of the automated all-ports sets
(Windows and macOS replays are open): libjpeg-turbo 3.2.0 and HarfBuzz 14.6.0 (the first `cmake` recipes),
libwebp 1.6.0 (with demux), libxml2 2.15.4, libpsl 0.23.3 (IDNA through the ICU port), OpenSSL 3.5.9
(LTS; libraries only) and `curl-openssl` (curl 8.21.0 with the OpenSSL backend, installed to
`<prefix>/curl-openssl` because it shares `libcurl.so.4` with the mbedTLS curl). Tests exercise the
parts WebKit uses: the BGRA JPEG extension, WebP demux, XPath, `psl_registrable_domain` with an IDNA
suffix, HarfBuzz shaping through FreeType and ICU, a TLS 1.3 handshake over a BIO pair, and HTTPS from
an in-process server with `CURLOPT_SSL_CTX_FUNCTION` called with a real `SSL_CTX`.
`tools/check_web_dependencies.py` then runs WebKit's own `find_package()` calls (the Win and
PlayStation ports' eleven) against the prefix: all resolve under it. Fetching and checksums: the
libxml2 and OpenSSL archives were compared with the SHA-256 their projects publish beside them; the
others carry the digest of the HTTPS download (no signature check done here).

Found and fixed in the loop: `tools/crt-port-build.py` gained `build.system cmake` (generated cross
toolchain file with find roots limited to prefix and sysroot; cmake/ninja run without the prefix on
`LD_LIBRARY_PATH`, which had made the host cmake load the CRT's libcurl), `configure_script`,
`install_subdir`, `make_targets`/`install_targets`, `@CRT_ARCH@` and per-test `library_dirs`. In CRT
libc, from OpenSSL's socket code: `getpeername()`, `SO_TYPE`, `struct linger`, `getnameinfo()` with
`NI_*` (numeric host only), `getservbyname()`/`getservbyport()`, `inet_ntoa()`; the macOS x86_64
`socketpair` stub had its error sign wrong (fixed before it ran anywhere). OpenSSL is built with
`-D__ANDROID__=1`: its `__linux && !__ANDROID__` branch is the glibc one (SysV shm, ucontext async).
Verified: full serial `ctest`, tooling unit tests (120), the seven recipe tests, the dependency check.
Open: macOS, Linux/aarch64, Windows for each port; SIMD (libjpeg-turbo, OpenSSL asm); a CA bundle.

### Web Tranche 3B plan and milestone B1: `-DPORT=CRT` builds JavaScriptCore from the pinned commit

Plan (`docs/acceptance/crtweb_acceptance.md`, "3B plan"). Reading the pinned tree settled the shape
of the port before writing any of it: WebKit selects a port by *file name*
(`Options${PORT}.cmake`, per-module `Platform${PORT}.cmake`) and by `-DWTF_PLATFORM_<PORT>=1`, so a
port is mostly new files; the only existing file that must change is the `ALL_PORTS` list. Of the
non-GLib ports, PlayStation has 59 port-specific files (WTF 4, WebCore 16, WebKit 36, JSC shell 2,
WebDriver 1), uses the generic run loop, curl, Skia and a C view API, and is the structural model;
Win (182 files) is the model for Windows-only parts (IPC, launch) at Tranche 6; WPE/GTK stay the
reference. The curl backend and WebCrypto need OpenSSL and an OpenSSL-backed libcurl, which CRT's
mbedTLS curl is not: a new port pair for the web stage. Milestones B1-B7 each have a measurable
gate.

B1. `libcrtweb/port/` holds the CRT-owned files (`Source/cmake/OptionsCRT.cmake`, and
`PlatformCRT.cmake` for WTF, bmalloc and JavaScriptCore, which for now include the JSCOnly lists);
patch 0005 registers the name (one line); the manifest declares the four files under `new_files`.
`tools/build_webkit_jsc.py --port CRT` composes the tree (the pinned commit checked out sparsely
and verified, hard-linked into a work tree, overlay copied, patches applied; the checkout is never
modified) and builds as before. Linux/x86_64 from the installed `05-ui` SDK: configure 11 s, build
about 5 min, the 1B interpreter acceptance and the 1C/1D baseline-jit acceptance (DFG, FTL, Wasm)
pass with the host-ABI audit clean; the composed tree and the manifest check
(`tools/check_webkit_patch_manifest.py --tree`) agree. No functional change relative to JSCOnly,
which is the point: the port mechanism is proven before WebCore is added.

### Web Tranche 3B gates: the full WebKit commit pinned, the tarball proven a subset of it, per-file license scan

Source gate. `recipe.json` gained `product_source` (role *PlatformCRT product source*): the commit
behind the signed tag, its tree id and the SHA-256 and count (461,979) of its recursive listing, which
together pin every byte of the tree. `tools/fetch_webkit_commit.py` fetches exactly that commit (depth 1,
trees only, about 14 MB, seconds), verifies the ids and listing and that the tag still peels to it, and
checks out `Source/` sparsely (69,574 files, 30 s). Its `--correspondence` mode hashed every file of the
release tarball as a git blob against the tree: 34,417 identical, 130 identical after CRLF->LF (ANGLE
Windows headers, `.gitattributes`), 0 content differences, 4,295 tarball-only (generated Documentation,
NEWS). The Tranche 0 caveat "not proven equal beyond six samples" is closed; the 35,810 `Source/` files
only the commit has are the Windows/Mac/iOS/PlayStation ports the tarball omits (`PlatformWin.cmake`,
`OptionsWin.cmake`, ... are present in the checkout).

Licensing gate. `tools/scan_webkit_headers.py` classified the header of all 69,573 files under
`Source/`: BSD-2-Clause 25,041, BSD-3-Clause 19,534, LGPL variants 3,138, no header 13,036, the rest
bundled components; 23 GPL (+14 GPL-with-Bison-exception) files, none of them WebKit code (ANGLE's
flex/bison tooling, yasm, bison output); 85 unclassified files in the core components, listed in
`libcrtweb/third_party/webkit/license-scan.json`. The classifier's first version silently missed
`version 2.1` and ICU texts (punctuation was left in the normalised text); the tests pin both. The patch
manifest is now schema 2: each modified file records its license class (17 files, all BSD-2-Clause;
`JavaScriptCore/CMakeLists.txt` has no header and records its directory-level `COPYING.LIB`), the
policy is in the file, and `new_files` is where CRT-owned platform files are declared.
`tools/check_webkit_patch_manifest.py` enforces it and `tools/test_webkit_licensing.py` (15 tests)
covers the classifier, the recipe pin, the correspondence classes and the manifest rules.
Not done: classifying the 85 core files by hand and the bundled components' notices (Tranche 10).

### Web Tranche 3A on Linux/x86_64: frame-producer and input contracts frozen, WPEPlatform `crt` display

A WPE display can be added from outside the WebKit tree: `WPEDisplay` is a GIO extension point
(`wpe-platform-display`, modules scanned from `WPE_PLATFORMS_PATH`). `libcrtweb/platform/wpe-crt` is
such a module; with `WPE_DISPLAY=crt` the unmodified WPE 2.54.0 reference build renders into it, and
no WebKit file changed. Each committed shared-memory buffer is copied into a producer-owned pool of
three memfds and announced over an `AF_UNIX` `SOCK_SEQPACKET` connection defined by
`libcrtweb/platform/crtweb_surface_wire.h` (contract v1: pixel format, ownership, serials, damage,
acknowledgement-paced single frame in flight, latest-wins coalescing with a 250 ms bounded wait,
resize, shutdown; pointer/wheel/key/text/focus with XKB keysym + keycode). The consumer adapter
`libcrtweb/platform/crtweb_surface_client.h` exposes the newest frame as a Skia image for a `crtui`
SurfaceView and maps `crtui` input to the wire.

Verification: `tools/build_webkit_wpe_crt.py` runs the native host against a CRT-built probe (frames
and pixels, serials, ack/release, pointer -> click and its pixel effect, key code/modifiers, wheel,
committed text, resize, and a stalled consumer that holds all buffers) 4/4 passes; the hermetic
`crtweb_web_surface_test` (CRT-built, Skia raster target, fake producer) and the new
`socketpair_test` pass in the full serial `ctest` (172/172, sound-card test excluded).

Findings, all fixed in CRT rather than worked around: `wpe_buffer_import_to_pixels` is
(transfer none), so unreffing it crashed after a few frames (our bug); WebKit never pushes the
document title to the toplevel (the embedder does); WebKit takes XKB keycodes (evdev + 8) and drops
key events with keycode 0, so the wire carries the XKB keycode and committed text maps ASCII through a
US layout; and the CRT libc had no `SOCK_SEQPACKET`/`MSG_TRUNC`/`MSG_CTRUNC`/`SOCK_RDM` and no
`socketpair()` (Linux and macOS syscalls added, ENOSYS on Windows). `crtweb_` joins the test-name
prefixes the C-stage workflows exclude. Open: 3B gates, the Windows/macOS transport, a live
crtgfx presentation of a web SurfaceView, GPU buffers. Not built here: the new `socketpair` syscall stubs for
Linux/aarch64, macOS (both) and the Windows ENOSYS stub (written from the neighbouring `socket` stubs; the Linux/x86_64
build and tests are the evidence).

### Documentation consolidation: adopted policy replaces exploratory drafts

Removed 13 marketing/study/restructuring drafts after comparing them with the
current contracts. Their source-portability and embedded-development direction
is owned by `docs/design/project_meanings.md`; stage separation and SDK usage
by `runtime_roadmap.md` and `docs/guides/distribution.md`; graphics references and
native host boundaries by `libcrtgfx_wayland_plan.md`, the graphics API policy,
and the Host ABI firewall. The English/Korean marketing copies no longer form
a parallel product definition.

Superseded QuickJS/Ozone/desktop plans and unsupported claims about foreign
binary reuse, certification, exact board simulation, driver-crash isolation,
resource limits, and pixel identity were not promoted into policy. The draft
`crt-clang`/`.cfg` launcher proposal was not an implemented SDK interface and
was not retained as a promised feature. Existing SDK wrappers and package
contracts remain authoritative.

Merged the separate loader plan into `docs/design/dynamic_loading.md`, retaining
compatibility levels, module ownership, selection criteria, and the staged ELF
loader proposal. Shared-artifact policy remains separate and now acknowledges
the existing Linux `CRT_1.0` version scripts. Replaced roadmap completion diaries
with acceptance links, removed obsolete bootstrap/Android-target wording from
project scope, corrected the FAQ's widget description, and consolidated the
shell guides' installed-layout explanation. Initial hello and Windows fork
investigation records retain their distinct reproduction value.

The removed drafts and original loader plan remain recoverable from commit
`4e5eead68048723c37e46c22d80bca43915ac093` using `git show <commit>:<path>`;
their original paths are under `docs/marketing`, `docs/study`, `docs/refine`,
and `docs/linker_loader.md`. This is archival access, not active documentation.
Validation checked repository Markdown destinations and section anchors, deleted
path references, and whitespace. No runtime behavior changed.

### Web Tranche 2 closed: native Linux WPE reference baseline

Built the verified, pristine WPE WebKit 2.54.0 release on the physical Ubuntu
26.04.1/x86_64 host with GCC 15.2.0, CMake 4.2.3 and Ninja 1.13.2. The new
`tools/build_webkit_wpe_reference.py` verifies the archive and every upstream
`sha256_before` named by the CRT patch manifest, configures the WPE port with
legacy/DRM-display/Wayland backends off and the built-in headless backend on,
builds MiniBrowser plus its process/resources/bundle closure, and installs it
under a private work prefix. Native development packages were unpacked only
under `out/web-reference/sysroot`; the host installation was not changed, and
this is deliberately not a CRT SDK build.

The repository-owned C11 harness and local HTML fixture explicitly create and
connect the headless display, construct a WebKit web view for it, resize to
640x480, and exercise HTML, CSS, DOM, JavaScript and canvas. The real
multi-process run returned the exact proof
`CRT WPE reference|local-html-ok|42|192x96`, a 640x480/stride-2560 BGRA8888 image with
multiple colors, and FNV-1a `da2864e59e05c36f`. The resulting PPM SHA-256 is
`efef2b53b70d6882cda4ad00d4bccfa12c2c11d1cc65bfd98ebb250d980a51c3`.
The pixel hash is recorded as local evidence, not made a portable gate.

The bring-up also resolved three reference-configuration traps without patching
upstream. Disabling Video while leaving WebCodecs on produces an incomplete
`VideoFrame`; disabling both still leaves the WPE build's generated
`JSHTMLMediaElement` custom binding inconsistent. The accepted build therefore
keeps their supported upstream relationship. Disabling `USE_LIBDRM` also hides
`drm_fourcc.h` while the snapshot backing-store path still uses
`DRM_FORMAT_XRGB8888`, so libdrm stays enabled even though the DRM display backend
is off. Finally, Release builds ignore `WEBKIT_EXEC_PATH`; using a private CMake
install prefix supplies the compiled-in Web/Network process paths without a
system install. No WebKit source file, CRT runtime code or carried patch changed.

## 2026-10-09

### Web Tranche 1 closed with the Linux/aarch64 1D-B/C/D replay

Regenerated and verified the cumulative `05-ui` SDK from `a8a20fa`, then built
fresh Baseline-JIT and sampling-profiler JSCOnly trees from the verified WPE
WebKit 2.54.0 pin on Ubuntu 26.04/aarch64 with Clang 21.1.8. The ordinary
Baseline-JIT tree passed every 1B..1D-C gate. FTL/B3 produced 158 reports at the
reduced threshold, 108 at default thresholds and 252 with serial compilation;
the FTL-off negative control produced zero. WebAssembly passed interpreter-only,
BBQ, BBQ+OMG and software-bounds configurations with 21 BBQ and 7 OMG reports,
and its C API signal probe measured 50 fast-memory signal deliveries versus zero
with explicit bounds checks. The 0/1/4/8-thread context cycles, JS and Wasm
watchdogs, RSS bounds and ELF host-ABI audit all passed.

The separate `ENABLE_SAMPLING_PROFILER=ON` build reran the complete 1B..1D-C
sequence before its profiler gate. It again proved FTL (157/108/252 reports),
Wasm BBQ/OMG (21/8 reports) and the 50/0 fast-memory signal distinction. The
official profiler run sampled the main VM and all four worker VMs with 5/5
profiles, 297 traces and 296 named hot frames. Twenty focused repetitions passed
20/20 with every VM sampled; the minima were 274 traces and 271 named hot
frames. The final host-ABI audit was clean, full Linux/aarch64 CTest passed
148/148, the JSC harness tests passed 3/3, and distribution verification passed
for the cumulative 03/04/05 stages. No runtime, WebKit patch or harness change
was required. This was the last native 1D-B/C/D replay, so JavaScriptCore
Tranche 1 is closed on all four acceptance hosts; W^X, SIMD, Wasm threads and
Windows/arm64 JSC remain separate non-gating follow-ups.

### Windows emulated-TLS churn is bounded and Tranche 1 follow-ups are classified

Added `emutls_thread_churn_test`, a bounded Windows/x64 regression that runs a
warm-up and two sequential batches of 128 short-lived pthreads, each touching a
256 KiB compiler-lowered `__thread` object, while recording process private
bytes. The pre-fix implementation retained every terminated thread's emutls
array and objects: the second batch added 42,024,960 bytes and total post-warm-up
growth was 84,049,920 bytes. `libc.dll` now keeps its process-wide emutls key but
frees only the exiting CRT pthread's object slots and array, after pthread-key
destructors, on both normal return and explicit `pthread_exit()`. This does not
restore the unsafe process-exit teardown. After the fix, both batch and total
growth are zero; the churn regression passes 20/20 repetitions, the churn,
process-exit-race and multi-DLL emutls tests pass 3/3, and the full Windows/x64
CTest passes 165/165.

Closed the non-implementation Tranche 1 follow-ups at the same boundary. Host-ABI
audits are complete on Linux/x86_64, macOS/arm64 and Windows/x64. The tested 1 MiB
pthread default is now explicit policy: CRT keeps the default and an embedder
selects a larger stack for workloads that require it. The isolated `06-web`
stage chain and the full build-host/target split are Tranche 10 work. The fixed
roles are host gperf/Perl/Python/Ruby/ICU generators versus target ICU/libc++/JSC/
WebKit, with an x86_64-host -> Linux/aarch64-target proof deferred to that stage.

### Web Tranche 1D-B/C/D accepted on macOS/arm64

Completed the missing native macOS replay with fresh Baseline-JIT and sampling-profiler
JSCOnly builds against the regenerated, verified `05-ui` SDK. FTL/B3 passed at reduced,
default and serial thresholds (153, 107 and 252 FTL reports; zero in the negative control).
WebAssembly passed interpreter, BBQ, OMG and software-bounds modes (21 BBQ and 5 OMG reports),
and its C API probe measured 50 fast-memory signal deliveries versus zero with explicit bounds
checks. Context cycles on 0/1/4/8 threads, JS/Wasm watchdog termination and the Mach-O host-ABI
audit were green. The separate profiler build reran all earlier gates, then sampled the main VM
and four worker VMs with 5/5 profiles and 1,167 named traces/hot frames. Twenty focused profiler
repetitions passed 20/20, with a minimum of 1,156 named frames.

The FTL replay exposed a persona-versus-object-ABI mismatch: CRT intentionally presents macOS
to WebKit as Linux-shaped source, but Darwin arm64 reserves x18 while WebKit's Linux register
table allocates it. Under B3/Air pressure generated code reused x18 across a runtime call and
faulted near `operationStringFromCharCode`. Carried patch
`0004-darwin-arm64-reserved-x18`, with verified before/after hashes and a removal condition,
selects WebKit's existing reserved-x18 table only for macOS/arm64; it does not enable Apple SDK
source paths or change Linux/aarch64.

The profiler replay exposed a macOS `pthread_create()` publication race analogous to the
Windows/x64 finding. A target wrapper learns its kernel-allocated stack from Apple's pthread API,
but the creator could return a Bionic-shaped `pthread_t` before those bounds were stored. An
immediate WebKit `pthread_getattr_np()` then derived stack origin `0x100000` and crashed in
`sanitizeStackForVM`. The target now release-publishes the bounds and wakes the creator before
the thread id is returned; the detached lifetime handshake is shared by macOS and Windows.
The three focused pthread regressions each pass 100/100, the JSC harness unit tests pass 3/3,
and full macOS CTest passes 125/125. Linux/aarch64 is now the only remaining 1D-B/C/D native
replay.

## 2026-10-08

### Web Tranche 1D-D: JavaScriptCore sampling profiler accepted on Windows/x64

Rebuilt the pinned JSCOnly source against the installed Windows/x64 `05-ui` SDK with
`ENABLE_SAMPLING_PROFILER=ON`. The separate profiler build passed every 1B..1D-C gate
before its profiler-specific gate, including DFG/FTL, Wasm fast-memory faults and
watchdog VM traps. Its official profiler step sampled the main VM and all four
`$262.agent` worker VMs: **5/5 profiles, 153 traces and 153 named hot-function frames**.
The aggregate acceptance and PE host-ABI audit were clean. Twenty further isolated
profiler runs passed 20/20, always sampled all five VMs, and produced at least 152
traces and 152 named hot frames.

The first worker-profiler run exposed a real libc/PAL prerequisite rather than a JSC
profiler defect. On Windows, `pthread_create()` could return before its target wrapper
had called `GetCurrentThreadStackLimits()` and recorded the stack in the pthread control
block. WebKit immediately calls `pthread_getattr_np()` from the creator; it could
therefore observe `{ base = NULL, size = 1 MiB }`, manufacture `0x100000` as the stack
origin, and crash the worker in `sanitizeStackForVMImpl`. The Windows start wrapper now
release-publishes the real bounds and wakes the creator, while `pthread_create()` waits
for that publication before returning the `pthread_t`. A second creator-publication
handshake keeps a detached thread's control block alive until the creator has finished
publishing its id. `pthread_stack_bounds_test`
locks in the contract by querying a live worker immediately after creation and requiring
a non-null, plausible stack range. The final source passes the full Windows CTest 164/164,
the focused pthread/signal/TLS subset 7/7, the stack-bound regression 100/100 and the JSC
harness unit tests 2/2; the detached-thread regression also passes 100/100. The cumulative
`05-ui` distribution was regenerated from that source and every stage passed `verify_dist`;
its direct profiler replay passed again with 5/5 VMs, 138 traces and 138 named hot frames.

One redundant post-package full-suite replay was stopped after its second Wasm acceptance
process exceeded the normal duration before reaching the profiler. The official complete
1B..1D-D run, the 20/20 profiler repetitions and the final packaged-SDK direct profiler run
remain green; treat this as a 1D-C observation and reopen that gate only if it reproduces.
This closes 1D-D on Windows/x64; Linux/aarch64 and macOS/arm64 native replays remain.

### Web Tranche 1D-D: JavaScriptCore sampling profiler accepted on Linux/x86_64

Added a distinct `sampling-profiler` mode to `tools/build_webkit_jsc.py`. It builds
the pinned JSCOnly source from the installed and verified `05-ui` SDK with
`ENABLE_SAMPLING_PROFILER=ON`, while retaining Baseline JIT, DFG, FTL and WebAssembly,
and reruns the complete 1B..1D-C acceptance before its profiler gate. Interpreter and
ordinary Baseline-JIT modes continue to compile the profiler out, so their existing
configuration and evidence remain isolated.

The new `jsc_sampling_profiler_acceptance.js` validates JSC's platform-support result,
profile interval, non-empty traces and named hot-function frames. It exercises the main
VM plus four concurrent `$262.agent` worker VMs, each with an independently started
profiler. The official run produced 5/5 valid profiles, 1,320 traces and 1,320 named hot
frames; its host-ABI audit was clean. Twenty further runs passed 20/20 with all five VMs
sampled every time and minima of 1,288 traces and 1,288 hot frames. The relevant rebuilt
signal/TLS CTest subset passes 9/9 and the tool suite passes 104/104. Tranche 1D-D is
closed on Linux/x86_64. Full in-tree CTest is 169/170; the sole failure is the known,
pre-existing no-sound-card `crtmedia_playback_pipeline_test_runs` pacing check, unrelated
to Web/JSC. This run also closes the installed-`05-ui` SDK half of the packaging follow-up.
ICU/libc++abi's loader-provided global-dynamic TLS is recorded as the accepted current loader
boundary rather than an active Tranche 1 gap; it becomes work when CRT owns the loader. Native
Linux/aarch64, macOS/arm64 and Windows/x64 replays remain.

### Windows CI: architecture-correct VM-trap coverage and scheduler-safe sleep assertion

Repaired the two Windows failures in GitHub Actions run 37707937441. The
`signal_vmtrap_test` source included the x86_64-only `hlt` scenario in its
Windows/arm64 build: its handler advanced `uc_mcontext.gregs[REG_RIP]` and its
inline assembly emitted the one-byte x86 instruction. The complete `hlt`
scenario and its default test-list entry are now x86_64-only. Windows/arm64
continues to exercise its actual JSC trap shape through the existing AArch64
`brk #0xf000` test, including Linux-layout `pc` rewriting, as well as the common
worker, initial-thread, concurrent and blocked-wait signal tests. The source
compiles cleanly with the CI target and flags (`aarch64-w64-mingw32`,
`-femulated-tls`, `-ffixed-x18`, `-Werror`).

The Windows/x64 job's lone `bionic_surface_test` failure was not reproducible
in 200 isolated runs. Its sleep check nevertheless imposed a false two-second
upper bound on `usleep(20000)`: POSIX guarantees that the call does not return
before the requested interval, but an oversubscribed CI runner may resume it
arbitrarily later. The regression now checks the real contract (at least 19
ms, retaining the existing one-millisecond timer tolerance) instead of treating
scheduler delay as a libc failure. The rebuilt focused Windows/x64 signal and
Bionic-surface tests pass, the latter passes 200/200 isolated repetitions, and
a fresh configure/build/test workflow passes all 143 Windows/x64 CI tests.

### Windows emulated TLS is owned once by libc across DLL boundaries

Closed the follow-up exposed by the Windows JSC 1D-C exit-race fix. The first
project-owned `__emutls_get_address` stopped compiler-rt's unsafe process-exit
teardown, but JSC, graphics and tests still compiled that source into each
consumer. PE/COFF does not coalesce the runtime's counter, Win32 TLS key or
per-thread slot array, so a control object shared by two DLLs could have one
index but two different values.

`libc/src/arch/windows/common/emutls.c` is now a normal source of both `libc.a`
and `libc.dll`; it retains the no-process-exit-teardown rule and advances the
global high-water mark when it observes an already-numbered control. Windows
shared-library links through `crt-cc` now import libc/libm/libdl instead of
embedding their static archives, and both wrappers plus the common CMake DLL
policy prevent a consumer from auto-exporting libc's emutls import thunk. The
Windows compiler-rt archive staged by the project is a copied archive with only
`emutls.c.obj` removed, so archive ordering cannot silently restore a private
runtime while all other builtins remain available. JSC, in-tree/isolated Skia,
the 05-ui predecessor contract and the standalone gfx-skia example no longer
carry an emutls source/object/archive or the old duplicate-definition bypass.

The new `emutls_multi_module_test` builds DLL A and DLL B against `libc.dll`.
A exports a control object; writes through either DLL are visible through the
other on the same thread, a new pthread starts from the template and cannot
overwrite the main thread, two compiler-lowered `__thread` values are isolated,
and the shared plus four private controls receive five distinct nonzero indexes.
PE inspection confirms `libc.dll` exports `__emutls_get_address`, both fixture
DLLs import it from `libc.dll` without re-exporting it, and the staged filtered
builtins archive no longer defines it. `pthread_native_tls_test`, the 100-child
`emutls_exit_race_test` and the new test pass; the stage closure/dependency unit
tests pass 14/14; the full Windows build and CTest pass 143/143. Per-thread
allocation reclamation under high thread churn remains a separately measured
follow-up; process-exit teardown must not return.

## 2026-10-07

### Windows/x64: silent exit 134 at process exit was compiler-rt's emulated-TLS teardown (2026-10-08)

The last Windows 1D-C symptom: 4-8% of runs of even a 0.3 s Wasm script (the first group of the acceptance script, dump options on or
off, only with concurrent JIT) ended with exit status 134, output cut in mid-line, or the `Invalid value for lock: 0` message. Found by
bisecting options (concurrent JIT off: 0/80; Wasm off: still failing; Baseline-only: 0/80; DFG on: 9/80), a 0.3 s reproducer, and
instrumenting `abort()`/`_exit()` (their callers): 14 of 15 failures happened after the script had printed its last line, and
`abort()` was called from `__emutls_get_address` -- compiler-rt's win_abort() after `TlsGetValue` failed with
ERROR_INVALID_PARAMETER ("The parameter is incorrect"; the CRT's link stubs for `__acrt_iob_func`/`__stdio_common_vfprintf` abort silently
before emutls can print that). compiler-rt's emutls registers an `atexit()` handler that frees its TLS index and mutex. `exit()` runs
atexit handlers while other threads still run (only the final `ExitProcess` stops them), so a compiler thread touching a thread_local in
that window used a freed index. The project now owns `__emutls_get_address` (`libc/src/arch/windows/common/emutls_link_stubs.c`, compiled into
JavaScriptCore and the graphics consumers): same control-object layout and per-thread array, state initialised on first use whatever
control object comes first (compiler-rt's fast path skipped that for an already numbered one) and no teardown at exit. New test
`emutls_exit_race_test` (spawns itself 100 times; threads spin on a thread_local while the main thread calls `exit(0)`; plus per-thread
semantics and a pre-numbered control object). After the change: 300/300 and 100/100 runs of the reproducer (about 7% before), the
60-run BBQ acceptance script with the dump options, 40 BBQ+OMG, 40 more BBQ, the official `baseline-jit` acceptance (131 s,
passed) and full CTest 163/163. Not changed: libc/libc++ and other modules that link compiler-rt's own emutls still have its atexit
teardown, and a control object shared between two modules is numbered by each module's own counter (per-module arrays); moving
`__emutls_get_address` into libc so there is one state per process is the follow-up in `TODO.md`.

### Windows/x64: signal-state leaks behind the `instance-lifecycle` hang (2026-10-07)

Two more defects of the redirect protocol, found by chasing the test-order dependence and the 120 s hang under the BBQ
dump options (about 7% of runs).
1. **Signal mask leak.** The sender wrote the handler's mask into the stopped target before redirecting it. A target that
   was taking a fault at that moment ran the fault handler with the changed mask, saved it as its own and restored it, so
   the mask (SIGUSR1) stayed blocked for good. Showed up as `fault,hlt`/`fault,worker` failing 40-60% when the fault-storm test ran
   first (and as the "leaked main-thread mask 512" seen earlier). The target now sets and restores its own mask after it has
   claimed the interruption; the sender no longer touches it.
2. **Lost acknowledgement.** The claim word lived in the frame on the target's stack. A thread whose redirect did not take
   effect carried on using that stack and overwrote the frame (the pump's captured claim was `746351308`), so the sender's
   cancel CAS failed and it waited for an acknowledgement for ever while holding the registry lock: exiting threads piled up
   in `registry_remove` and `WTF::Thread::suspend` waited for its answer (all-thread stack captured with lldb). The handshake is
   now one word in `crt_signal_state` (`inject_word` = serial * 4 + phase: waiting, claimed, cancelled, running) that
   survives the stack being reused; a stale stub finds a different serial and only resumes the interrupted context.
`signal_vmtrap_test` can now run tests in a chosen order (`SIGNAL_VMTRAP_ONLY=fault,worker`) and re-run them; the fault storm
runs first by default, with a mask postcondition. After the fix: all order pairs pass 12/12, 0 hangs in 150 runs
(BBQ-only 110, BBQ+OMG 40; 3-4 of 40 before), full CTest 158/158. Still open: a JSC `Invalid value for lock: 0` abort
(`WTF::LockAlgorithm::unlockSlow` on `JITThunks::m_lock`, in a Baseline-JIT compiler thread, first group of the script) in
about 4% of runs with the dump options and none without them. `signal_sync_stress_test` (mutex/cond under signals) and a flags-survival
check (`lock cmpxchg` + `jne` interrupted) both pass, so the CRT's own synchronisation is not the evident cause.

### Windows/x64: BBQ-only SIGSEGV at a JIT address root-caused and fixed

Reproduced with `jsc_wasm_acceptance.js` under the BBQ-only options (about 10% of runs, always in `memory-and-traps`, 0.5 s in).
A temporary log in JSC's Wasm `trapHandler` showed it returning `NotHandled` because the ucontext it received had its pc at
`__crt_windows_signal_entry`, not at the faulting load. A thread suspend request had redirected the thread (SetThreadContext)
after the kernel had built the exception record (pc = JIT address) but before it captured the context, so the VEH saw a
record with the JIT pc and a context already pointing at the stub. `exception_handler` only recognised the record carrying
the stub address; it now also checks the context pc, runs the injected handler, restores the saved context and lets the
faulting instruction fault again (`libc/src/arch/windows/common/signal_backend.c`). A new regression test (`test_fault_while_signalled` in `signal_vmtrap_test`: 200000
`hlt` faults on one thread while another floods it with SIGUSR1) showed a second defect of the same class: the injected frame sat
128 bytes below the target's stack pointer, where the kernel builds the exception CONTEXT/record of a fault in flight, so the frame
(its claim word) could be overwritten and the sender waited for ever or the handler saw a wrong pc. The frame now sits 16 KiB
below the stack pointer (falling back to the old margin on a shallow stack). Before: the test hung or crashed every run; after: it
passes. After both fixes: 0 SIGSEGV in 70 BBQ-only runs (about 10% before), the signal tests pass 20 consecutive runs, full CTest
157/157, `jsc_watchdog_test` 5/5. The test runs last in `main` because, run first, a later worker-interruption round of
the same test intermittently never gets its handler to start with the 16 KiB placement; that order dependence is not understood
yet and is tracked in `TODO.md`. The temporary JSC logging was removed.
Not fixed here: JSC `Invalid value for lock: 0` aborts (about 5% of runs) and a rare `instance-lifecycle` hang under the dump options.


- **Web Tranche 1D-B (FTL) accepted on Windows/x64 and the signal/runtime part of the still-open 1D-C (WebAssembly)
  replay hardened.** The installed/verified `05-ui` SDK's JSCOnly binary passes FTL at
  reduced/default thresholds and with serial compilation plus the FTL-off negative control. A near-final full run also
  passed the Wasm interpreter/BBQ/OMG and software-bounds matrix, watchdog termination of JS and Wasm loops, context cycles
  and the host-ABI audit, but the final stress below prevents accepting 1D-C. Because Windows
  has no `strace`, new `jsc_wasm_signal_probe.cpp` runs the OOB probe through JavaScriptCore's C API and brackets it with
  CRT's signal-delivery generation: fast memory handles 50 hardware faults, bounds checks handle zero.
  The work mapped JSC's `hlt` VM trap (`STATUS_PRIVILEGED_INSTRUCTION`) to Linux-compatible `SIGSEGV`/`SI_KERNEL` with a
  null address, delivers signals held by a fault handler after its mask is restored, adds a pump for pending delivery when
  the target is repeatedly inside Windows code, and serializes injection/delivery per target. The final duplicate-delivery
  race was subtler: the sender polled a claim stored below the target's original stack pointer; after context restoration
  the target reused that memory, so the sender could see zero again, restore the pending bit and run one signal twice.
  A stable acknowledgement now lives on the sender's stack, while the target-frame claim is only cancellation arbitration.
  Restored strict upper bounds in `signal_vmtrap_test` caught the old bug in 19-20/20 runs; after the fix
  `signal_vmtrap_test`, `signal_threads_test` and `memcpy_atomicity_test` each pass 20 consecutive runs, full Windows CTest
  passes 157/157, and `crt-ui-dist`/`verify_dist` complete. One near-final aggregate JSC run had all FTL/Wasm steps green but a
  single earlier DFG 4-thread context-cycle process hit JSC's `Invalid value for lock: 0`; the same focused configuration
  then passed 10/10 and a second full official harness passed every step plus the aggregate/host-ABI gates (131 s).
  Final review additionally kept the sender stack alive until a just-claimed handler's acknowledgement store. The SDK
  rebuilt from that exact source exposed the remaining non-signal-closure result: its dump-based BBQ run hung, while
  bounded no-dump repetitions were BBQ-only 9/10 (one controlled JIT-address SIGSEGV, exit 139) and BBQ+OMG 10/10.
  Therefore Windows 1D-C remains in `TODO.md`; 1D-B and the resolved PAL work belong here. Temporary mask/ring tracing was
  removed. Open: isolate the Windows BBQ failure; replay 1D-B/C on macOS/arm64 and Linux/aarch64; then profiler 1D-D.

## 2026-10-06

- **Web Tranche 1D-B (FTL) and 1D-C (WebAssembly) on Linux/x86_64; two CRT bugs fixed that only concurrent Wasm tier-up
  exposed.** `tools/build_webkit_jsc.py` gained step 4 (`run_ftl_acceptance`) and step 5 (`run_wasm_acceptance`) on the same
  `baseline-jit` binary, each requiring evidence of its tier: FTL compile reports (108-252 per script run; zero with FTL off),
  Wasm interpreter-only / BBQ / BBQ+OMG separated by `useBBQJIT`/`useOMGJIT` and proven by the tier's disassembly dump, fast
  memory proven by `strace` counting the SIGSEGVs JSC's fault handler handled (50 vs 0 with software bounds checks), watchdog
  termination of a Wasm loop on main and worker threads. New `jsc_ftl_acceptance.js`, `jsc_wasm_acceptance.js` (raw-byte
  modules, nine trap kinds, 400 instances, async), `jsc_wasm_oob_probe.js`; `jsc_watchdog_test wasm`.
  The Wasm script first hung (~1 in 10 runs under 4-way CPU contention) and then crashed (~1 in 100). A hang's backtrace
  (gdb as the parent; ptrace attach is blocked) showed every thread in `crt_spin_lock(thread_lock)` of `libc/src/tls.c`: the
  registry lookup behind errno/`pthread_self` held a global lock, WTF parked the holder in its SIGUSR1 suspend handler and
  the resumer needed `pthread_self` for `pthread_kill`. The registry is now a lock-free-reader table keyed by tid (writers
  only take the lock). Crashes in JIT code (SIGSEGV/SIGILL) remained until the second bug: CRT's OpenBSD `memcpy` copied 4
  bytes as four byte stores, and WTF patches call displacements with `memcpy` while another thread executes them (BBQ->OMG
  call-site patching); glibc builds compile the same copy to one `mov`, the CRT wrapper's `-fno-builtin` does not. Copies up to
  16 bytes are now single overlapping 2/4/8-byte accesses, as in Bionic. A native glibc build of the same source ran 1,200
  runs without failure and the CRT runtime 0 in 800 after both fixes (before: ~1% crashes plus the hangs). New tests:
  `signal_threads_test` park/resume in a thread hot in errno (fails every run on the old registry) and
  `memcpy_atomicity_test` (over a million torn 4-byte values before, none after). Verified: full `ctest` 163/163 serial
  (the sound-card test excluded), tooling 102/102, `crt-ui-dist`/`verify_dist`, interpreter and baseline-jit harness passes
  from the installed `05-ui` SDK, stress FTL/Wasm/watchdog 100/100 each. Details and limits: `docs/acceptance/crtweb_acceptance.md`
  ("1D-B and 1D-C result"). Open: replays of 1D-B/C on the other hosts (needs the libc fixes), the sampling profiler (1D-D),
  SIMD and Wasm threads, and a Wasm loop with no call is not interruptible by the watchdog (upstream).

- **Windows/arm64: thread-directed signals, per-thread masks and fault-to-signal mapping (same gate as Windows/x64).**
  180e43c had written `libc/src/arch/windows/common/signal_backend.c` against the x64 `CONTEXT` and the Linux x86_64
  `ucontext_t`, which broke the windows-arm64 CI build (`greg_t`/`REG_R8` undeclared); first guarded to keep masks, waits and
  `sigsuspend` only, then completed. The file now carries both layouts: the ARM64 `CONTEXT` (912 bytes, x0-x30/sp/pc/v0-v31),
  an arm64 entry stub (`bl __crt_windows_signal_run`), and a Linux-aarch64 `ucontext_t` frame for the handler
  (`private/crt_linux_ucontext_aarch64.h`, with the FP/SIMD record), as the macOS backend does; the header's own
  `ucontext_t` stays private on Windows/arm64. `brk` -> SIGTRAP leaves the pc on the instruction (Linux behaviour, the handler
  steps over it); a thread parked in a CRT wait gets x19-x30/sp in its context. `signal_vmtrap_test` gained an arm64 path
  (`b .` loop, `brk #0xf000`) and `signal_threads_test` now runs on all Windows. Evidence: windows-arm64 CI at `41326fd`,
  135/135 tests, `signal_vmtrap_test` 4.88 s and `signal_threads_test` 1.80 s (really executed, not skipped). The JavaScriptCore
  replay on Windows/arm64 is not done.
- **Web Tranche 1D-A replayed on Windows/x64: DFG with concurrent compiler threads is green on the first run.** Same
  Windows Baseline-JIT binary as 1C with `JSC_useDFGJIT`/`JSC_useConcurrentJIT` and four compiler threads, polling traps
  off, so the Windows signal VM-trap gate now has its DFG consumer. The harness's step 3 passes in full (DFG script at
  jitPolicyScale 0.01, at default thresholds and with serial compilation, the 1B and JIT scripts under DFG, thread
  cycles, watchdog on a DFG-compiled loop, host-ABI audit, no FTL code); the DFG script and the watchdog ran 100/100 and the
  4/8-thread cycles 40/40 without a failure; the compiler threads are visible (15 threads against 12 with concurrent
  JIT off). Nothing in CRT, the patches or the harness had to change. Evidence: `docs/acceptance/crtweb_acceptance.md`.
- **Web Tranche 1D-A replayed on Linux/aarch64: DFG with concurrent compiler threads is green on the first run.**
  Ubuntu 26.04 aarch64 QEMU guest at `8e30763`: `cmake --fresh` of the old tree (24 commits had changed shared libc,
  `crt-c++` and the dist prerequisites), clean build with 0 warnings, `crt-ui-dist`/`verify_dist` for 03/04/05, tooling
  102/102, `ctest` 140/140 (the new `signal_vmtrap_test` skips on aarch64 and is not counted). `--mode baseline-jit` from an
  empty work root passes every 1B/1C step and all of step 3 (DFG at 0.01/default thresholds/serial with 123/108/137
  `using DFG` reports, 1B and JIT scripts under DFG, cycles on 0/1/4/8 threads, watchdog on a DFG-compiled `spin`, no FTL
  report, host-ABI audit clean); stress: DFG script 100/100, watchdog 100/100, 4/8-thread cycles 40/40; `useDFGJIT=false`
  gives zero reports; `JITWorker` threads exist in `/proc/<pid>/task` only with concurrent JIT on; interpreter mode
  unchanged and passing. No CRT or patch change was needed. Windows/x64 was the remaining 1D-A replay (done, see above).
- **Web Tranche 1D-A replayed on macOS/arm64: DFG with concurrent compiler threads is green on the first run.**
  Fresh `crt-ui-dist` and an empty-tree `--mode baseline-jit` build; all 1B/1C steps and step 3 (DFG at 0.01/default
  thresholds/serial, 1B and JIT scripts under DFG, context cycles on 0/1/4/8 threads, watchdog on DFG code) pass,
  host-ABI audit clean; stress DFG 100/100, watchdog 100/100, cycles 40/40; DFG-off gives zero reports and FTL none;
  compiler threads confirmed by `sample` (no `/proc` on macOS). No CRT or patch change was needed.

## 2026-10-05

- **Web Tranche 1D-A on Linux/x86_64: JavaScriptCore's DFG tier with concurrent compiler threads runs on the CRT runtime;
  the `libatomic.so.1` prerequisite is removed.** The 1C binary already contains DFG, so 1D-A is a run-time step in
  `tools/build_webkit_jsc.py` (`--mode baseline-jit`, "step 3"): `DFG_ON_OPTIONS` (DFG and concurrent JIT on, four compiler
  threads, FTL/WebAssembly off, polling traps off), the same at default thresholds and with serial compilation, the 1B/1C
  scripts under DFG, context cycles on 0/1/4/8 threads and the watchdog with a DFG-compiled `spin`; each step requires DFG
  compile reports and no FTL report. New `libcrtweb/tests/jsc/jsc_dfg_acceptance.js` (7 groups). Everything passed on the
  first run with no CRT change (DFG script 100/100, watchdog 100/100, 4/8-thread cycles 40/40; 108-137 DFG reports per
  script; the report count is zero with DFG off; `JITWorker` threads present with concurrent JIT on and absent without it).
  Evidence and limits: `docs/acceptance/crtweb_acceptance.md` ("1D-A result"). Replays on aarch64/macOS/Windows are open.
  Also: `libatomic.so.1` dropped from `tools/crt_dist_prerequisites.py`, `docs/guides/distribution.md` and `docs/guides/release_preview.md`
  (a fresh `02-cxx`..`05-ui` SDK has no libatomic `DT_NEEDED` on Linux/x86_64 and the aarch64 replay's host-ABI audit
  was clean; `crt-ui-dist`, `verify_dist` and the 102 tooling tests pass). `TODO.md` lost its stale Windows
  software-signal-mask text and now points 1D-A's replays at Linux/aarch64, macOS/arm64, then Windows/x64.


- **Windows/x64: thread-directed signals, per-thread masks and a real `sigsuspend`; JavaScriptCore's Baseline JIT now runs
  with `JSC_usePollingTraps=false` (Web Tranche 1, signal VM-trap gate).** Windows has no kernel signal delivery, so the CRT
  builds it in the PAL (`libc/src/arch/windows/common/signal_backend.c`), not in WebKit. Each thread owns a mask/pending
  set/wake event (`crt_signal_state` inside `crt_thread_context`); `pthread_kill` to a CRT thread (worker or initial)
  suspends the target, captures its CONTEXT, builds a signal frame (saved CONTEXT, Linux-x86_64-layout `ucontext_t`,
  `siginfo_t`) on its stack, puts the handler's mask in place while it cannot run, redirects it to a stub that runs the
  handler and then restores the (possibly rewritten) context with `RtlRestoreContext`. Hardware faults become signals through a
  vectored handler (`int3`->SIGTRAP with RIP after the instruction, AV->SIGSEGV, illegal instruction->SIGILL, divide->SIGFPE).
  `sigaction` honours `sa_mask`, `SA_NODEFER`, `SA_RESETHAND`; `pthread_sigmask`/`sigprocmask` are per-thread and a new
  thread inherits its creator's; `sigsuspend` blocks atomically and returns EINTR after the handler. `ucontext_t` is the
  Linux x86_64 layout on Windows too (JSC reads and rewrites `REG_RIP`); `getcontext`/`makecontext` keep the Windows-only
  state (xmm6-15, TEB stack bounds) in unused slots. A thread parked in operating-system code is never interrupted (loader/heap
  locks, mid-syscall contexts), so the CRT's own blocking waits (`__crt_wait32` behind mutex/cond/sem/once, `pthread_join`,
  `nanosleep`) are interruptible instead: the sender wakes them (`WakeByAddressAll`/the thread's event) and the handler runs
  on the waiting thread with a `getcontext` snapshot (what WTF's thread suspension and JSC's conservative scan need). Bugs found
  by running it: a signal that coalesced while a handler ran was later delivered with a null `ucontext_t`; the new-thread mask
  was wiped by the context init; `sigsuspend` did not deliver what its temporary mask had held back; a hang that turned out
  to be a test teardown race; and, only in JSC, a VMTraps sender blocked forever in `sem_wait` because the target sat in
  `pthread_cond_wait` (8 hangs in 60 runs before the interruptible waits). Verification: `signal_vmtrap_test` (int3, loop
  interruption of a worker and of the initial thread, 8 threads x 300 rounds, blocked waits) 100/100 and `signal_threads_test`
  green, full Windows ctest 135/135, JSC Baseline-JIT acceptance with polling traps off (compile proof 2,340 reports,
  `jsc_watchdog_test` main x3 + worker x3) and the 1B interpreter acceptance green; `jsc_watchdog_test` 100/100 runs, no hang
  or crash. Not covered (by design): `kill(pid)`/process groups/SIGCHLD from other processes, `poll`/`select`/file I/O waits
  (a signal there stays pending until the call returns), and threads not created by `pthread_create`.

- **Web Tranche 1 replayed on Linux/aarch64: native thread TLS implemented for aarch64, and the JavaScriptCore
  interpreter and Baseline JIT run on the CRT runtime.** First verification of the shared libc/headers/wrappers/
  recipes/harness work on an aarch64 Linux host (Ubuntu 26.04 aarch64 QEMU guest, Clang 21.1.8, from a deleted
  `out/`): clean build exit 0 with no warning from CRT sources; C-stage ctest 113/114. The one failure,
  `pthread_native_tls_test`, was the documented gap (every thread reported one TLS address), and the same gap crashed
  JSC's 4- and 8-thread context cycles with `SIGTRAP`, while the script and the 0/1-thread cycles passed.

  `libc/src/arch/linux/common/thread_tls.c` now supports aarch64 (TLS variant I, `tpidr_el0`): the 16-byte header
  and a fresh dtv at the new thread pointer, static blocks above it at the offsets read from the initial thread's dtv
  (never recomputed), the thread pointer aligned to the strictest `p_align`, and the loader's thread descriptor below it
  copied at the same thread-pointer-relative offsets. Its size (glibc's `struct pthread`, 0x720 here) is not exported by
  `ld.so` (no `_thread_db_*`; the TLS functions are `GLIBC_PRIVATE`), so the copy window is a fixed upper bound
  (0x1000) clamped to pages that `mincore()` proves are mapped, and setup declines below 0x100 bytes. Found by
  disassembling `ld.so` that it reads fixed offsets below `tp` (`tp-40`, `tp-0x720`, `tp-0x71c`, `tp-0x600`).
  `__crt_sys_clone_thread` already passed the TLS argument in the aarch64 `clone` order, so no assembly changed;
  x86_64 behaviour is unchanged (both targets syntax-check clean).

  Verified: `pthread_native_tls_test` passes and fails when the setup is made to decline (mutation, restored); full
  `ctest` 136/136 (was 135/136); tooling unittests 102/102; `crt-ui-dist` and `verify_dist` pass. JSC interpreter
  acceptance passes (cycles on 0/1/4/8 threads, 150 each, 0 failures; host-ABI audit clean) and so does the
  Baseline JIT (2,340 compile reports, the same count as Linux/x86_64; compiled loop terminated by the watchdog); 150/150
  watchdog runs, and 20/20 four- and eight-thread cycles in each mode. Three shared-library TLS modules
  (`libJavaScriptCore`, `libc++abi`, `libicuuc`) are in the process, so the multi-module path is exercised.

  A fresh host needed more than the checklist said, now recorded in `docs/acceptance/crtweb_acceptance.md`: `libc++-21-dev`,
  `libc++abi-21-dev`, `libunwind-21-dev` (the build forces `-stdlib=libc++`) and `ruby`; the ICU port built into the
  harness's `--deps-prefix` (`port-build-icu`); and `-DCRTUI_ENABLE_LVGL=ON` for `crt-ui-dist`, whose
  `crtui-lvgl-fetch` target cannot run while configure fails (an existing "Small UI follow-up"). Open on aarch64:
  DFG/FTL/WebAssembly and W^X (1D), and a shared-library TLS case inside `pthread_native_tls_test` itself.

### Windows: thread_local in the in-tree build (`pthread_native_tls_test`)

`pthread_native_tls_test` did not link on Windows (`undefined symbol: _tls_index`): the freestanding
startup has no PE TLS directory, and `tools/crt-cc` already lowers `thread_local`/`__thread` to
`__emutls_get_address()` (`-femulated-tls`, compiler-rt on Win32 TLS slots) for every port, but the
in-tree CMake build did not. `crt_build_flags` now adds `-femulated-tls` on Windows, and the test
links `emutls_link_stubs.c`, `uuid.lib` and `--allow-multiple-definition` (the accommodations
`libcrtgfx` documents for Skia's `thread_local`; a duplicate-`fprintf` warning in the archive member
is expected). The test now passes (6 threads: distinct addresses, `.tdata`/`.tbss` initial values,
isolation, creator untouched); the 28 pthread/tls/fork/thread/signal tests and 151 of 152 in the
full Windows CTest pass. No native PE TLS was implemented: the emulated scheme is the existing
decision (`docs/guides/import_bionic.md`, Errno TLS Tranche). `bionic_surface_test` also failed on
Windows (`gettid()` on the initial thread != `getpid()`): `GetCurrentThreadId()` is not the pid, so
the macOS mapping (initial thread's id recorded at startup returns the pid, other threads keep
their own) in `gettid()` (`libc/src/process.c`) now applies to Windows too. Full Windows CTest 152/152
(two timeouts in the first parallel run right after a libc relink passed on rerun). A forked child's
only thread is a new native thread, so `__crt_atfork_child()` re-records it as the initial thread
(Windows only); `bionic_surface_test` checks `gettid() == getpid()` in a child (mutation: removing
the re-record fails exactly that check). The sweep test timed out in parallel runs right after a
libc relink and passes alone; the cause was not investigated.

### Windows/x64: JavaScriptCore 1C (Baseline JIT) green with polling traps (Web Tranche 1 Windows replay)

The Baseline JIT passes its acceptance on Windows/x64: the assembly interpreter alone compiles nothing, the JIT script produces
2,340 compile reports (the Linux/x86_64 number), 1B passes again with the JIT, 150-cycle contexts on 0/1/4/8 threads, the
watchdog terminates a compiled loop on the main thread and on workers, and the PE audit is clean. It needed one carried patch
(`libcrtweb/patches/0003-coff-assembler-flavor`: COFF assembler directives, and the System V argument-register/`sysv_abi`
conventions JavaScriptCore selects with `OS(WINDOWS)`, plus offlineasm's `--platform=Windows`), `-mno-ms-bitfields`, and a CRT
fix: `getrlimit(RLIMIT_STACK)` reported 8 MiB for a 1 MiB main stack, so the assembly interpreter overflowed the real stack
instead of throwing a RangeError. A libc++ recipe patch gives Windows consumers that hide `_WIN32` the library's own iostream
configuration (so JavaScriptCore.dll links the shared libc++.dll). Signal-based VM traps, the sampling profiler, DFG/FTL and
ARM64 are not covered; the first run uses `JSC_usePollingTraps=true` by decision.

### Windows/x64: JavaScriptCore 1B (interpreter) green, and eight CRT gaps it exposed (Web Tranche 1 Windows replay)

With Ruby installed on the build host, the pinned WPE WebKit 2.54.0 builds `jsc` on Windows/x64 from the installed `05-ui` SDK
and passes the 1B acceptance: `jsc_acceptance.js` (8 groups, peak 74 MB) and `jsc_context_cycle` (150 cycles on 0/1/4/8 threads,
0 failures, about 60 KiB growth), with a clean PE import audit. The persona is Linux-shaped, as on macOS, because the pinned
tarball has no WIN32 sources. Getting there fixed eight CRT gaps, each with a test where the host allows it: libm `atanf`;
`libc.dll` exporting `environ`; a weak `_fltused` for DLLs; one libc per process (executables link `libc.dll` under shared
linkage, `CRT_CXX_STL_LINKAGE` for libc++); `syscall(SYS_gettid/SYS_getpid)`; partial `munmap`, reserve-only `PROT_NONE` and
commit-on-`mprotect`; `size_t` instead of 32-bit `unsigned long` lengths in the memory-map entry points (a 16 GiB mapping was
truncated to 0); and the real stack in `pthread_getattr_np`. New tests: `long_argv_test` (earlier), `mmap_partial_test`,
`pthread_stack_bounds_test`, and the extended `bionic_surface_test`; the new ones fail on the old behaviour (mutation-checked).
libc++.dll's missing `<sstream>` exports turned out to be a configuration mismatch: upstream disables those explicit
instantiations on Windows (LLVM PR41018) and a consumer that hides `_WIN32` took the other branch; a libc++ recipe patch fixes
it and the lane now links the shared libc++.dll (one libc and one libc++ per process). Not done: 1C, the watchdog, the
isolated stage chain, ARM64.

### Windows/x64: getrusage, the JSC harness's Windows lane (Web Tranche 1 Windows replay)

`getrusage(RUSAGE_SELF)` on Windows returned zeros; it now reports the user/kernel CPU time (`GetProcessTimes`) and the
peak working set as `ru_maxrss` in KiB (`K32GetProcessMemoryInfo`), with `RUSAGE_CHILDREN` and `RUSAGE_THREAD` still zero
(Windows keeps no such accounting). `bionic_surface_test` runs its `getrusage` case on Windows, and
`jsc_context_cycle.cpp` reads the peak through it instead of `/proc/self/statm`. `tools/build_webkit_jsc.py` gained a
Windows lane: PATH-based DLL lookup, `JavaScriptCore.dll`, no rpath, a polled peak-working-set measurement (Python has no
`resource` module on Windows), and a PE import audit via `llvm-objdump -p` with the distribution's own Windows contract
(`crt_dist_prerequisites.py`) as the OS-DLL allowlist. Checked on real data only: the audit passes the CRT-built ICU tools
and rejects `python.exe`; the measurement reports 214 MB for a 200 MB allocation. JavaScriptCore itself has not been
configured on Windows: the host has no Ruby. `jit_memory_test` already passes in full on Windows/x64.

### Windows/x64: gperf port (Web Tranche 1 Windows replay)

`gperf` (needed for WebCore's build, not JavaScriptCore's) builds on Windows and its `generate-hash` recipe test
passes. It needed only a recipe override: `--build=x86_64-unknown-linux-android` instead of the usual MinGW
triple. gperf bundles gnulib, which chooses its replacement layer from the configured host OS; for a mingw
host it substitutes Windows headers (`io.h`), `struct stat` -> `_stati64` and `rpl_fstat`, none of which this
POSIX/Bionic-shaped sysroot has, and undefining `_WIN32`/`__MINGW32__` afterwards does not undo them (tried first:
the build failed in `stat-time.h` and `fstat.c`). The Bionic host is the CRT's own identity and also makes
configure's runtime probes run. No CRT defect was found by this port.

### Windows/x64: ICU port and three PAL defects it exposed (Web Tranche 1 Windows replay)

The first step of the JavaScriptCore replay on Windows is the ICU port. It builds and both recipe tests pass
(`unicode-static`, `unicode-shared`; ICU 78.3 with its embedded data). Getting there found three real CRT
defects, fixed in the PAL rather than worked around in the recipe:

- **Silent argument truncation.** The Windows startup (`libc/src/arch/windows/common/crt1.c`) kept at most 255
  arguments and 8191 command-line characters (and the fork relaunch and spawn paths had the same 8192 limit).
  ICU's `libicuin` link has about 280 arguments, so the tail -- the libraries -- vanished and the link failed with
  undefined symbols far from the cause. Limits are now the OS's (32767 characters, at most 16385 arguments); spawn
  already failed explicitly with `E2BIG`. New `long_argv_test` spawns itself with 700 arguments (mutation: the old
  limits give `argc=241`).
- **Programs without `.exe`.** ICU's data Makefile runs `../bin/icupkg`, a tool its own build produced as
  `icupkg.exe`. `stat()`, `access(X_OK)` and spawn now find `name.exe` when `name` does not exist and has no
  extension (MSYS/Cygwin behaviour; mksh checks with `stat`/`access` before it runs a command). Covered by
  `long_argv_test` (mutation fails it). The widened `stat` is a semantic change on Windows: only ICU and zlib/libpng
  port tests were re-run, not every Windows port.
- **Port test `PATH`.** `tools/crt-port-build.py` did not put the SDK `bin` directory (libc.dll) on the `PATH` of a
  test that loads a port DLL.

Recipe-only accommodations (none patch ICU): see `porting/recipes/icu.json` notes -- the POSIX platform path, the
64-bit MinGW configure fragment (`icu_cv_host_frag=mh-mingw64`; the `-U__MINGW64__` the POSIX path needs made
configure pick the 32-bit one, giving an underscore-prefixed data symbol and a mis-named data DLL), and `;`
`PATH` in ICU's `INVOKE`. A first diagnosis blamed literal quotes in `ICULIBS_*`; that was wrong, the arguments
were being truncated. Full Windows CTest 153/153; zlib and libpng port tests pass. Not done: expat/pcre2 and the
other ports were not re-run, gperf on Windows, Windows/ARM64.

## 2026-10-04

- **macOS CI: `bionic_surface_test` failed (`gettid()` in a forked child != `getpid()`).** The macOS
  `gettid()` maps the initial thread to the pid by comparing with the thread id recorded at startup, and a forked
  child's thread has an Apple thread id of its own, so the child fell through to the raw id. The Windows fix
  (child re-records its thread as initial in `__crt_atfork_child`) now applies to macOS too. The failure was not
  seen locally because the test binary was stale; with the fix removed the rebuilt test fails here as in CI.

- **Web Tranche 1C replayed on macOS/arm64: the JavaScriptCore Baseline JIT runs on the CRT runtime.** Clean
  `--mode baseline-jit` build 153 s; assembly interpreter (no code compiled), JIT script (2,340 compile reports),
  1B script under the JIT, VM cycles on 0/1/4/8 threads, a watchdog that kills compiled code on main and worker
  threads, and the Mach-O host-ABI audit all pass, repeatedly. No WebKit patch for W^X: `mmap()` promotes anonymous
  RWX to `MAP_JIT`, `__crt_jit_write_protect` wraps `pthread_jit_write_protect_np`, and JSC's
  `OS_THREAD_SELF_RESTRICT` extension point is bound to it by a force-included header. Second carried patch
  (`0002-macho-assembler-flavor`): Mach-O spellings of the inline/offlineasm assembler keyed on
  `WTF_CRT_MACHO_ASM`; its last hunk drops the bare GDB debug labels the Linux persona emitted, which split the
  `.alt_entry` atoms and left zero-filled gaps in the linked LLInt (found by comparing the `.o` with the dylib at
  the symbol; it looked like a `-dead_strip` problem at first and the `CRT_MACOS_NO_DEAD_STRIP` workaround was
  removed). CRT changes: real macOS signals (libSystem forwarding, Darwin/arm64 to Bionic/Linux aarch64
  `siginfo`/context conversion with register write-back), real stack bounds for threads created with only a
  size, `getauxval`/`<asm/hwcap.h>`, `jit_memory_test` and `signal_threads_test` extended for Apple Silicon.
  Open: macOS/x86_64 (no signal conversion, no JIT permissions), 1D.

- **Web Tranche 1A/1B replayed on macOS/arm64: the JavaScriptCore interpreter runs on the CRT runtime.**
  Decision (A): WebKit sees macOS as a Linux-shaped POSIX platform (`libcrtweb/cmake/crt_webkit_platform.cmake`,
  `-U__APPLE__ -D__linux__=1`, explicit CRT compilers) with one hash-checked carried patch (`WTF::RawHex`,
  `libcrtweb/patches/manifest.json`). `jsc_acceptance.js` (8 groups, 69 MB peak) and 150 context cycles on 0/1/4/8
  threads pass and the new Mach-O host-ABI audit is clean. It found and led to: libc++.dylib linking the host
  libc++abi, ICU's bare install names, `gettid()`/`syscall(SYS_gettid)` on macOS, initial-thread stack bounds,
  a real `getrusage()` (it was a stub on every host), `memset_explicit()`, UTC `tzname`/`timezone`, `-lm`/`-ldl`
  handling in the wrappers, ELF-only option filtering, and fixes to two flaky tests. `jit_memory_test` can only
  check RW to RX on Apple Silicon (no `MAP_JIT`). The Baseline JIT (1C) needs a Mach-O assembler-flavor patch, W^X
  through `OS_THREAD_SELF_RESTRICT` and a real macOS signal backend (`docs/acceptance/crtweb_acceptance.md`).

- **Web Tranche 1C closed on Linux/x86_64: the JavaScriptCore Baseline JIT runs on CRT, and the work
  found five more signal/thread defects.** The pre-JIT gate (harness `--mode`, the CRT
  `jit_memory_test`, 1B rerun from the installed `05-ui` SDK, `libatomic` and gperf cleanups) is closed;
  the JIT build compiles every tier in (a Baseline-only build does not compile: upstream header-order and
  B3 dependencies) and isolates the Baseline JIT with JSC options. Results: the 1B script with the JIT
  off compiles nothing; the JIT script, 1B script, VM cycles on 1/4/8 threads and a watchdog that kills a
  compiled infinite loop on main and worker threads all pass, with 2,340 `using Baseline ... into N bytes`
  compile lines as proof; 10/10 harness runs and 150/150 watchdog runs. Details and the W^X/DFG/FTL
  boundaries are in `docs/acceptance/crtweb_acceptance.md`.

  An intermittent watchdog crash (8 in 100) led to the fixes: `pthread_kill` dereferenced the initial
  thread's tid-valued `pthread_t` (my own code; now validated against live CRT threads); `sigsuspend` was a
  stub, so a thread WTF believed suspended kept running; `sigaction` discarded `sa_mask` and the flags;
  the signal mask was process-wide instead of per thread (all four now real on Linux); and `pause()` was
  missing. New tests `signal_threads_test`, an initial-thread case in `bionic_surface_test`, and
  `jit_memory_test`; mutations of `sigsuspend` and `pthread_kill` fail them. ctest 158/159 serially (the
  sound-card test; two capture tests contend for the now-attached webcam only under `-j4`), tooling 102/102.

- **Web Tranche 1C plan refined (documentation).** Checked against the pinned WebKit: the JIT conflicts
  with `C_LOOP`, DFG defaults on with the JIT and FTL depends on DFG, `ENABLE_MPROTECT_RX_TO_RWX` is 0
  and Linux executable memory is created RWX at once without `MAP_JIT`, and the JIT enables
  signal-based VM traps. 1C is now the Baseline JIT only (DFG/FTL/WebAssembly/profiler are 1D and
  later), W^X is a hardening follow-up rather than part of first green, proof of compiled code is
  required, and a pre-JIT gate (harness build modes, a CRT `jit_memory_test`, rerunning 1B from the
  installed `05-ui` SDK) precedes it. Two review items were already done (gperf dropped from the JSCOnly
  checks; the host `libatomic` dependency removed at its root in the libc++ build).

- **Second review of the `06-web` plan folded in, and its host-ABI firewall found a real
  leak.** Checked against the pinned `WebKitCommon.cmake`: gperf is required only when WebCore
  is enabled (so a WebCore prerequisite, not a JSCOnly one), while Perl (with `English`,
  `FindBin`, `JSON::PP`), Python and Ruby are required for every port. `docs/acceptance/crtweb_acceptance.md`
  now separates build-host tools from target dependencies (and notes that a cross build will
  need that split for gperf/ICU's generators), adds the configure fingerprint
  (`webkit-jsc-config.json`), the Host ABI firewall for every JSC acceptance, a mandatory gate
  before Tranche 3B (pin the full WebKit commit behind the signed tag as the `PlatformCRT`
  product source; the WPE tarball stays the reference, and lacks the Windows port), and a
  per-file license scan plus patch manifest before the first carried WebKit patch. The harness
  gained the fingerprint, the host-tool/Perl-module check, and `host_abi_audit()`; it no longer
  demands gperf. `libcrtweb/README.md` and several stale README lines (`06-web` "no
  implementation", "Tranche 1 next") were brought up to date.

  The new audit failed at once: the SDK's `libc++.so.1` recorded `NEEDED libatomic.so.1` (the
  libc++ build linked the host libatomic), which pulled glibc's `libc.so.6` into every
  JavaScriptCore process beside CRT's `libc.so`. `LIBCXX_HAS_ATOMIC_LIB=OFF` is now set on every
  target (it was Windows-only); `libc++.so.1` no longer needs libatomic and the audit passes
  with no exceptions. The harness result (script, VM cycles on 1/4/8 threads) is unchanged.
- **Web Tranche 1 on Linux/x86_64: JavaScriptCore (JSCOnly) builds with the CRT toolchain
  and passes the interpreter acceptance (1A and 1B); the exercise found and fixed several
  fundamental CRT/PAL defects.** `tools/build_webkit_jsc.py` builds the pinned WPE WebKit
  2.54.0 against an installed SDK (C_LOOP interpreter, JIT/FTL/WebAssembly off, Generic
  event loop, CRT ICU/gperf ports, host Ruby) and runs `libcrtweb/tests/jsc/`: an eight-group
  script (arithmetic, objects, JSON, RegExp, ICU-backed Unicode/Intl, exceptions, GC stress,
  microtasks; peak RSS 198 MB) and a C API program that creates/uses/releases a whole VM 150
  times and 20 times on each of 1/4/8 concurrent threads with flat memory. A clean run takes
  about six minutes. Details in `docs/acceptance/crtweb_acceptance.md` (Tranche 1).

  The defects, each found by a failure in this work and fixed in CRT, not in WebKit:
  `thread_local`/`__thread` was shared by all CRT threads on Linux (clone without
  `CLONE_SETTLS`) -- native per-thread TLS blocks built from `PT_TLS` images and the
  initial thread's dtv (x86_64; aarch64 not yet); an executable linked `libc.a` while the
  shared libraries used `libc.so`, giving the process two libcs with separate thread
  registries and allocators (`CRT_CXX_RUNTIME_LINKAGE=shared` now links libc.so into the
  executable); `siginfo_t` lacked `si_addr`, `ucontext_t` was private and `SA_SIGINFO`
  handlers got a synthesized record and a null context (Bionic/kernel layouts, real
  forwarding on Linux); `open(O_CLOEXEC)` silently ignored the flag; GNU ld.bfd produced a
  PIE with a stray `.rela.plt` entry when `PT_TLS` was present (CTest executables now use
  LLD); and a long list of missing Bionic surface (`pthread_kill`, `pthread_getattr_np` for the
  initial thread, `sched_*`, `memmem`, `usleep`, `mkostemp`, `fallocate`, `sysinfo`,
  `sendfile`, `getprogname`, the extra clock ids, `sys/ucontext.h`, `SYS_gettid`/`__NR_*`,
  `<cxxabi.h>`, ELF32/Nhdr in `<elf.h>`). New tests: `pthread_native_tls_test` (mutation:
  disabling the TLS setup fails it) and `bionic_surface_test`. Verified: full build, ctest
  156/157 (sound-card test only), tooling 102/102, `crt-c-dist`; the changed libc sources
  also syntax-check for aarch64 Linux, macOS and Windows (not run there).

- **Review of the `06-web` plan folded into the docs.** Checked against the pinned 2.54.0
  source: `PlatformCRT` is a new WebKit port (WPEPlatform only the Linux prototype
  boundary); `WPEProcessManager` is built only for Android; the tarball has no Windows IPC
  backend; Tranche 1 split 1A/1B/1C; the frame-producer and input contracts are to be frozen
  before Tranche 3A, with IME deferred past v1.

## 2026-10-03

- **Web Tranche 1A (in progress): `gperf` and `icu` ported as CRT recipes on
  Linux -- the first C++ ports -- and the review of the `06-web` plan folded into
  the docs.** JSCOnly needs ICU >= 70.1 (`data`, `uc`, `i18n`; verified in
  `OptionsJSCOnly.cmake`, default `Generic` event loop so no GLib) and WebKit's
  CMake needs Ruby, Perl and Python as host tools, and gperf once WebCore is on; gperf 3.3 (GPG-verified) and ICU 78.3 (SHA-256 and
  SHA-512 checked against the release) are CRT ports, Ruby is a host tool the
  maintainer installs. Both pass recipe tests (`gperf` runs and emits a hash;
  `icu` static and shared exercise case mapping, NFD and Swedish collation).
  Building them exposed CRT gaps, all fixed rather than worked around:
  - The port builder always used the C-only `01-c` sysroot; a recipe can now say
    `build.sysroot_stage: 02-cxx` (and `make_subdir`, and C++ recipe tests with
    `"language": "c++"`).
  - `tools/crt-c++` compiles with `-ffreestanding`, so an unmodified `int main`
    was mangled (`_Z4mainv`) and crt1 could not link it; `CRT_CXX_HOSTED=1` adds
    `-fhosted` (opt-in; the default is unchanged).
  - `-lm`/`-ldl` from an upstream makefile linked the shared libm into a
    static-runtime executable (`NEEDED libm.so`, no rpath, so the host loader hit
    glibc's `/lib/.../libm.so` linker script: "invalid ELF header", which also
    broke ICU's configure `sizeof(wchar_t)` probe); both wrappers drop them in
    that configuration.
  - Shared libraries linked through the wrapper had no `__dso_handle`
    (`crtbegin_so.o` added to libc and the Linux `-shared` link).
  - CRT headers formed an include ring (`<sys/types.h>` -> `<stdint.h>` ->
    `<wchar.h>` -> `<locale.h>` -> `<xlocale.h>` -> `<stdio.h>`) that gnulib's
    wrapper headers broke at any entry point: `bits/crt_types.h` and the new
    `bits/crt_wtypes.h` are leaf headers, and `<xlocale.h>` no longer includes
    the heavy headers.
  - libc gained Bionic's `getprogname()`/`setprogname()`/`__progname` (gnulib
    requires it), and `<elf.h>` the ELF32 types ICU's `pkg_genc` writes.
  Verified: full build, ctest 154/155 (only the sound-card test; the camera test
  skips without a webcam), tooling 102/102, `crt-c-dist` verifies with the new
  object. Windows and macOS are not attempted for either port. No WebKit code is
  built yet.
  Review changes to `docs/acceptance/crtweb_acceptance.md` / `crtweb_porting.md` (checked
  against the pinned 2.54.0 source): `PlatformCRT` is a new WebKit port with
  WPEPlatform only as the Linux prototype boundary; `WPEProcessManager` is
  compiled only for Android and is not a generic hook; the tarball has no Windows
  IPC backend or `PlatformWin.cmake`, so the Windows transport must be written;
  Tranche 1 split into 1A/1B/1C with a build harness against the installed SDK;
  the frame-producer and input contracts are frozen before 3A (separate `crtweb`
  input path, IME deferred past v1); stale candidate wording fixed.

- **Windows replay of the isolated-SDK text-file relocation found and fixed a
  Windows-only defect.** The macOS/Linux fix for staging paths in `.pc`, `.la` and
  `*-config` files had never run on Windows, and its first Windows run failed in the
  final `verify_dist.py` of the isolated 04 stage (after a 4575 s build): the CRT
  shell passes the install prefix as `/c/crtw/...`, a spelling `tools/crt_text_
  relocate.py` did not know. `root_spellings()` now also yields `/c/...` and `/C/...`;
  two new tests in `tools/test_text_relocate.py` fail without it. The chain was rebuilt
  clean from a regenerated 03 SDK: isolated 04 with the 03 SDK's own tool (4560 s,
  17/17, verified, published; `--reuse-work-root` now leaves its dependency layers
  cached), isolated 05-ui with the 04 SDK's own tool and embedded recipe (165.7 s,
  8/8, sample and packaged demo `presented=30 pixel_check=pass input_check=pass`,
  verified, published). No `.la` remains, the `.pc` files use `${pcfiledir}`, and a
  `curl-config` moved to a path containing a space reports the new location. The
  relocation fix is now verified on Windows, macOS and Linux; evidence in
  `docs/acceptance/crtui_acceptance.md`.

- **Application UI (`05-ui`) closed; documents reconciled; Web Runtime (`06-web`)
  Tranche 0 closed.** With the macOS/arm64 and Linux/x86_64 replays of Tranche 7
  accepted, `crtui` Tranches 0-7 are closed on all three hosts (per-tranche evidence
  in the entries below and `docs/acceptance/crtui_acceptance.md`). `README.md`, `STATUS.md`
  (updated at the owner's request, synchronized to 2026-10-03), `TODO.md`,
  `docs/design/runtime_roadmap.md`, `docs/guides/distribution.md`, `docs/guides/faq.md`,
  `docs/design/project_meanings.md`, `libcrtui/README.md` and the acceptance documents now
  describe `05-ui` as accepted and `06-web` as the current stage; the completed
  Application UI section left `TODO.md`, and the stale "not verified"/"replay
  remains" statements in `docs/acceptance/crtui_acceptance.md` were replaced by the replay
  records. `STATUS.md` gained a `libcrtui` baseline, its known limitations (no IME,
  selection, touch or pointer-drag scrolling; scripted input only; `05-ui` not a
  release asset; `crt-ui-dist` needs `CRTUI_ENABLE_LVGL=ON`) and a UI check table.

  `AGENTS.md` was brought up to date as well: its project structure now lists every
  top-level folder (the upper-runtime libraries, `distribution/`, `examples/`,
  `cmake/`, `benchmark/`, `libcrtweb/`) and the real state of `libm`, `libstdc++`,
  `shell/awk`, `porting`, `tools` and the reserved, unbuilt `linker/`; Android is
  stated to be the source of Bionic libc, not a target OS; WebKit is named as the
  `06-web` goal; the stack, layer 4/5 and reference-document sections were
  corrected, and the document-management rules were pointed to. Stale statements
  in `libcrtmedia/README.md` (encode/capture "active", "no network yet") and
  `libcrtgfx/README.md` (no libwayland-client, no GPU backend) were corrected, and
  the root `README.md` Build section now says `crt-ui-*` needs `CRTUI_ENABLE_LVGL=ON`.

  **Web Tranche 0 closed.** WPE WebKit **2.54.0** (2026-09-16, the current stable
  series) is pinned in `libcrtweb/third_party/webkit/recipe.json` and verified. The signed tag
  `wpewebkit-2.54.0`, its commit `73f39d84ea9d...` and the archive size (46,202,080
  bytes) were confirmed from GitHub and HTTP; the archive was then downloaded and its
  SHA-256 recomputed (`efa9bcc3cb89...eb452`, equal to the release page); the tag's
  signature verifies with `gpg` against the signer's key (Adrian Perez de Castro,
  `5AA3BC33...123B`, DSA-1024 expiring 2027-03-16, not web-of-trust certified); and
  six sampled source files are byte-identical to the tagged commit (the top-level
  `NEWS` exists only in the tarball). The release notes (WPEPlatform stable and
  default, libwpe API deprecated, Skia-only 2D with the compositor in the web process,
  Ninja required, `WPEProcessManager`) were checked against the raw pages, not only a
  summary. The archive holds 38,843 files; `tools/scan_webkit_licenses.py` inventoried
  its 64 license/notice files (LGPL-2/2.1, an Apple BSD-style notice, and bundled
  Skia, ANGLE, pdf.js, gtest and others) in `libcrtweb/third_party/webkit/license-inventory.json`.
  One design-relevant finding: WebKit bundles its **own Skia, milestone 154**, while
  CRT uses m148, so the two stay separate copies. `tools/fetch_webkit.py` and
  `tools/scan_webkit_licenses.py` (with `tools/test_fetch_webkit.py`, 6 tests) make
  this reproducible; `docs/porting/crtweb_porting.md` holds the upstream mapping and
  `docs/acceptance/crtweb_acceptance.md` the frozen scope and the security/SBOM policy, which the
  owner confirmed. Left explicit in the recipe: per-file license headers were not
  scanned, the tarball is not proven equal to the commit tree beyond the sampled
  files, and upstream's security support window is undocumented on the pages read.

- **`crtui` Tranche 7 replayed on Linux/x86_64: the isolated `05-ui` stage passes
  -- Tranche 7 closed.** One fix: `tools/build_stage_05_ui.py` could not find
  Debian's versioned `llvm-nm-21`; it now tries `/usr/bin/llvm-nm-N` and
  `/usr/lib/llvm-*/bin`. Chain from empty work/cache dirs: 03 SDK -> isolated 04
  (1102 s, verified) -> isolated 05-ui (23.5 s, also from space-containing paths):
  ctest 8/8 including the real-clip media view test, installed `ui-basic` rebuilt
  externally and the prebuilt demo `presented=30 pixel_check=pass input_check=pass`,
  verify and atomic publish pass; package has the Skia/media companions, the LVGL
  notice/recipe and no LVGL header; `libcrtui.so` exports only `crtui_*`. The
  text-file relocation fix also holds on Linux (no `.la`, no staging paths,
  relocated SDK's `pkg-config`/`curl-config` follow the move). Shortcut: the 05-ui
  run used the freshly regenerated recipe/asset rather than the one embedded in the
  04 SDK, which predated the fix. ctest 154/155 (sound-card test only), tooling
  94/94.

- **Isolated SDKs no longer carry the staging path in text files.** The 04
  stage installs ports into a temporary prefix that autotools/pkg-config baked
  into `lib/pkgconfig/*.pc`, `*.la` and `bin/curl-config`; Mach-O/ELF had been
  relocated but text had not, and `verify_dist.py` never looked
  (`docs/guides/distribution.md` criterion 8). New `tools/crt_text_relocate.py` (drops
  `.la`, `prefix=${pcfiledir}/../..`, location-relative `*-config`) runs in
  `build_stage_04_gfx_media.py`; `verify_dist.validate_text_paths()` rejects
  leftovers; `tools/test_text_relocate.py` adds 5 tests (tooling 94/94). Rebuilt
  the clean macOS chain (isolated 04 607 s, 05-ui 47 s, 8/8): no stale path in the
  SDK, `pkg-config`/`curl-config` follow a moved copy. Windows/Linux unverified.

- **Isolated `04-gfx-media -> 05-ui` stage replayed on macOS/arm64 (`crtui`
  Tranche 7), after one fix.** The stage builder could not find `llvm-nm`
  behind Apple's clang shim; `tools/build_stage_05_ui.py` now uses `xcrun`.
  Rebuilt the whole chain clean afterwards: 03 SDK -> isolated 04 with the 03
  SDK's own tool (615 s, 17/17) -> isolated 05-ui with the 04 SDK's own tool
  (48.8 s, ctest 8/8, installed sample and packaged demo `presented=30
  pixel_check=pass input_check=pass`, verify and publish pass), and again from a
  path containing spaces. The isolated `libcrtui.dylib` binds libc to `libc.dylib`
  and the dylib-linked sample runs cleanly. Full ctest 162/162, tooling 89/89,
  `crt-ui-dist` verified. Inherited absolute paths in 04-layer text files
  (`.la`, `curl-config`, `.pc`) were recorded as a follow-up and fixed the same day
  (next entry). Linux replay is next.

- **Isolated `04-gfx-media -> 05-ui` stage accepted on Windows/x64
  (`crtui` Tranche 7).** `distribution/stages/05-ui/CMakeLists.txt` and
  `tools/build_stage_05_ui.py` build crtui (private LVGL v9.6.0, fetched live and
  re-verified against its pinned SHA-256), the Skia/MediaView companions, eight
  tests and the installed `ui-basic` sample from a ~118 KB source asset against the
  isolated 04 SDK alone. The entrypoint refuses the ordinary default-OFF 04
  package, declares LVGL as a private static dependency with its MIT notice and
  pinned recipe (the in-tree package never shipped the notice), rebuilds the sample
  externally from the SDK, runs the packaged demo, verifies and publishes
  atomically. `libcrtui/cmake/crtui_sources.cmake` is now the single source list
  for the in-tree and stage builds. `STAGE_SUCCESSORS` gained `04 -> 05`;
  `crt-stage-05-source` exists, `crt-gfx-media-dist` and the 04 source asset embed
  `05-ui.json`, and the 04 entrypoint copies it into its SDK. `tools/crt_dist_
  prerequisites.py` needed no entry (05-ui equals 04 on every OS).

  From the real isolated 04 SDK (4594 s, built from the packaged 03 SDK), the 05
  stage passes in 123-158 s: ctest 8/8 including the compositor test and the
  media-view test (`interop=gpu-copy gpu_frames=5 cpu_frames=0 releases=5`), the
  rebuilt sample and the packaged `crtui_window_demo` both report `presented=30
  pixel_check=pass input_check=pass`, and `verify_dist.py` passes; a work/output
  path containing a space passes too. In-tree `crt-ui-dist` still verifies the whole
  01..05 chain, full CTest is 177/177 (one no-camera skip), and the stage-source
  closure test (7 tests, now covering 05-ui) passes. Removing the LVGL notice makes
  `verify_dist.py` fail.

  A first draft of the `verify_dist.py` check required the companion libraries of
  every in-tree package, which would have rejected the legitimate Skia-OFF package
  (headers install unconditionally); it now applies only to the isolated stage.

  **Clean-build verification.** With `out/` and all scratch directories deleted,
  the whole chain was rebuilt and passed: `cmake --fresh`, default CTest 127/127,
  `crt-gfx-simple-dist` (01-c..03), an isolated 04 built from the 03 SDK with that
  SDK's own `crt-stage-build.py` and no cache (4556 s, 17/17), then the isolated
  05-ui built with the 04 SDK's own tool and embedded `05-ui.json` (8/8, sample and
  packaged demo `presented=30 pixel_check=pass input_check=pass`, verified, 132 s,
  also from space-containing paths), and in-tree `crt-ui-dist` (LVGL on, Skia off)
  verifies 01..05. The 05-ui `verify_dist` logic moved into `validate_ui_stage()`
  with fake-tree tests in `tools/test_verify_dist.py` (15 tests; the first draft's
  check makes one fail). The earlier "old 04 SDK rejects the 05 recipe" finding is
  resolved: a 04 SDK built from a 03 SDK packaged from this tree carries the
  current `crt_stage_recipe.py`.

  The clean build exposed two defects the previous `out/` had hidden: libc++'s
  sparse checkout failed on Windows with "Filename too long" (neither long paths
  nor git `core.longpaths` enabled), fixed by passing `-c core.longpaths=true` to
  `tools/crt-libcxx-build.py`'s own git commands; and `capture_mf.c` could compile
  before the mingw-w64 headers were fetched (`'mfapi.h' file not found`) because
  `crtmedia_backend_objects` did not depend on `crtgfx-mingw-w64-headers-fetch`,
  now ordered in `libcrtmedia/CMakeLists.txt`. macOS/arm64 and Linux/x86_64
  branches of the stage project are written but unverified.

## 2026-10-02

- **`crtui` Tranche 6C replayed on Linux/x86_64 with no change -- Tranche 6
  closed.** `crtui_media_view_test` decodes the real H.264 fixture through VA-API
  and composes it in the final Vulkan present: `backend=vulkan interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, identical over three runs, and 5/5
  frames in a real Wayland/Vulkan window. (The `interop=` label is derived from
  the backend by the test; the real evidence is `cpu_frames=0` plus the Linux
  import accepting only VA-API dma-buf frames.) Mutation `(void)previous;` in
  `crtui_skia_media_provider_submit()` fails ownership (`releases=1`), reverted.
  ctest 154/155 (sound-card test only), tooling 80/80, `crt-ui-dist` verifies and
  installs `libcrtui_skia_media.a`/`crtui/skia_media.h`; `libcrtui.so` still
  exports only `crtui_*`. Needed only `CRTMEDIA_ENABLE_FFMPEG=ON` in the dev tree.

- **`crtui` Tranche 6C replayed on macOS/arm64 with no change.**
  `crtui_media_view_test` decodes the real H.264 fixture with VideoToolbox and
  composes it in the final Metal present: `backend=metal interop=zero-copy
  gpu_frames=5 cpu_frames=0 releases=5`, byte-identical over five runs, and 5/5
  frames in a real Cocoa window. The replaced-image-release mutation fails the
  ownership check. Full ctest 162/162 (expected camera skip), tooling 80/80,
  `crt-ui-dist` verified with `libcrtui_skia_media.a` installed. Linux replay is
  next.

- **`crtui` MediaView Tranches 6A/6B accepted on Windows/x64.** Core `libcrtui`
  gained `crtui_media_view_create()` / `CRTUI_WIDGET_MEDIA_VIEW`, a distinct
  widget that follows the SurfaceView scene rules (clip, ancestor opacity,
  z-order, damage, hit-testing) and appears in the external-surface snapshot;
  core still holds no decoder, frame, GPU or Skia object. The optional static
  C++ companion `crtui_skia_media` (`crtui/skia_media.h`) owns one imported image
  per MediaView through `crtgfx_skia_import_media_frame()`: success moves frame
  ownership, failure leaves the frame untouched, CPU frames are rejected rather
  than labelled zero-copy, and replace/clear/destroy release after any
  in-flight compositor reference. The Skia compile settings shared by the two
  companions were folded into one CMake function.

  `crtui_media_view_test` decodes the real H.264 fixture through the
  hardware-preferring decoder: `backend=d3d12 interop=gpu-copy gpu_frames=5
  cpu_frames=0 releases=5` with UI composed below and above the video, a resize,
  replacement and clear, and the same path presents 5/5 frames in a real D3D12
  window. The first run failed on the test's own cleared-view check, which built
  its empty target at the pre-resize size and was correctly rejected by
  `crtui_skia_compose()`; the test now uses the current window size. The Skia-
  enabled `crt-ui-test` gate is 8/8, privacy is exact at `declared=68
  exports=68 lvgl_exports=0 objects=1 headers=5`, and full Windows CTest is
  177/177 (one expected no-camera skip). This machine had no Skia build, so the
  pinned Skia was fetched and built (834 steps) with `crtgfx-skia-build` and
  `CRT_USE_IMPORTED_LIBCXX=ON`. Tranche 6C (macOS, Linux) remains.

- **Interrupted libc++ runtime sparse fetches now recover instead of poisoning
  later builds.** A pulled Windows/x64 working copy failed `crt-c-dist` with
  `libunwind: checkout_subdir 'libunwind' not found`: the
  `.clone-libunwind` cache still contained valid Git metadata, sparse rules and
  the pinned commit, but every sparse worktree file was missing. The fetch
  driver had treated `clone_dir` existence as proof that clone/fetch/checkout
  had completed, so every retry reused the incomplete cache and failed before
  it could repair it.

  `tools/crt-libcxx-build.py` now validates an existing clone cache,
  re-establishes the requested cone-mode sparse paths, force-checks out an
  already-cached 40-character pinned commit when available, fetches only when
  that commit is absent, and removes non-Git/incomplete cache paths before
  recloning. The forced checkout is intentional: `git sparse-checkout
  reapply` restores skip-worktree policy but does not recreate tracked files
  that have been deleted from the worktree. A local-repository regression test
  now deletes all three sparse trees while preserving clone metadata and
  proves that the next fetch reconstructs `libunwind`, `cmake` and
  `runtimes/cmake`; it is registered in CTest. Verified on Windows/x64:
  `crt-libcxx-fetch` repaired the real damaged cache, the libc++ fetch and
  predecessor tests pass 2/2, and the originally failing `crt-c-dist` command
  completes through `CRT distribution verified`.

- **`crtui` External Surface Tranche 5 closed on all three hosts after the
  unchanged Linux/x86_64 replay and a clean-distribution dependency fix.** The
  Skia-enabled official UI gate passes 7/7 and stayed green for three
  consecutive runs; the fresh LVGL-OFF contract/layout/surface/input gate is
  4/4. A real Wayland/Vulkan window presented 5/5 frames three times with
  `gpu/ordering/clip/opacity/damage/resize/ownership=pass` and balanced
  producer ownership. Core `libcrtui.so` retains no Skia, Vulkan or `crtgfx`
  dynamic dependency, and package privacy is exact at
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`.

  The clean cumulative replay found that `crt-gfx` installed
  `libcrtgfx_skia.so` without any aggregate target building it: the existing
  Skia window demo only links the static bridge. `crt-gfx-media-build` now
  explicitly builds `crtgfx_skia_shared` after `crt-gfx-simple-dist`. A new
  build directory then generated and verified every cumulative distribution
  stage through `05-ui` without a manual target invocation, including the
  optional `libcrtui_skia.a` and `crtui/skia.h`. Tooling is 79/79. The full
  feature-enabled CTest result is 158/159; every `crtui` test passes, and the
  sole failure is the already-recorded independent playback wall-time residual
  on this no-sound-device host. With Windows/x64, macOS/arm64 and Linux/x86_64
  accepted, the External Surface contract now unblocks `06-web`; media binding
  remains Tranche 6.

## 2026-10-01

- **The macOS AVFoundation capture lifecycle residual is fixed and the full
  suite is green.** Two independent defects had been hidden by the earlier
  no-camera authorization path. First, dequeue transferred a queued frame's
  storage to the caller without clearing the slot, so backend teardown could
  free caller-released storage again. Queue take/drop/clear now make ownership
  transfer explicit; release detaches the sample-buffer delegate, drains its
  dispatch queue and only then destroys capture state. A resource-free queue
  ownership regression runs with the existing AVFoundation conversion test.

  Second, the backend synthesized AVFoundation preset and pixel-buffer keys as
  strings. It consequently reported 640x480 while this camera delivered
  1920x1080, and the lifecycle encoder rejected its first frame. The PAL now
  uses the framework's real `AVCaptureSessionPreset*` and CoreVideo
  `kCVPixelBuffer*Key` ABI symbols and explicitly requests the negotiated
  dimensions. The real lifecycle gate passes all 15 capture and 15
  VideoToolbox cycles: 450 captured/released/decoded frames and 1,500 hardware
  encoded/released/decoded frames. Full macOS CTest is 160/160.

- **`crtui` External Surface Tranche 5 replayed and accepted unchanged on
  macOS/arm64.** The official Skia-enabled UI gate passes 8/8, and a fresh
  `CRTUI_ENABLE_LVGL=OFF` tree passes contract/layout/surface/input 4/4. The
  real Metal-window compositor presented 5/5 frames with
  `gpu/ordering/clip/opacity/damage/resize/ownership=pass` and balanced
  producer ownership. Core `libcrtui.dylib` has no Skia, Metal or `crtgfx`
  dynamic dependency; privacy remains exact at
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`.

  `crt-ui-dist` rebuilt and verified cumulative `dist/05-ui`, with the
  optional `libcrtui_skia.a` and `crtui/skia.h` installed separately from core
  `libcrtui`. Tooling is 79/79. The initial UI replay exposed only the
  independent macOS camera lifecycle residual; the AVFoundation ownership and
  negotiation fix recorded above then made full CTest 160/160. No functional
  UI source change was needed. Linux replay remains before global Tranche 5
  closure.

- **`crtui` External Surface Tranche 5B accepted on Windows/x64: a real
  texture-backed producer is now composed with CRT UI in a real GPU window.**
  The optional static `crtui_skia` companion keeps Skia/GPU dependencies out of
  core `libcrtui`. It consumes the producer-neutral 5A layer snapshot, renders
  LVGL UI preorder ranges as opaque/transparent planes, draws each
  producer-owned `SkImage` directly between those planes, and calls the
  provider's matching release exactly once. Producer pixels are neither copied
  through LVGL nor read back to the CPU. Geometry-only LVGL placeholders retain
  the complete scene's coordinate and ancestor-clip behavior while
  SurfaceViews remain holes.

  The new `crtui_surface_compositor_test` uses a texture-backed synthetic GPU
  producer. Exact pixel readback proves UI-below/surface/UI-above order,
  ancestor clipping and 50% opacity; it also proves damage propagation, resize,
  stable-id mapping, one acquire/release pair per frame, and the no-frame
  `WOULD_BLOCK` path without a spurious release. Its real-window mode presented
  5/5 frames through a D3D12 swapchain with balanced ownership.
  The Skia-enabled official UI gate passes 7/7, and DLL privacy remains exact:
  `declared=67 exports=67 lvgl_exports=0 objects=1 headers=4`. Tranche 5 remains
  open only for unchanged macOS/arm64 and Linux replay (5C); MediaView remains
  Tranche 6.

- **`crtui` External Surface Tranche 5A accepted on Windows/x64: the
  producer-neutral scene boundary is now real, without mislabelling metadata as
  GPU display.** `crtui_surface_view_create()` now creates a normal opaque-id
  widget that participates in CRT layout, clipping and hit-testing. The new
  allocation-free `crtui_window_get_surface_layers()` snapshot exposes each
  visible SurfaceView's stable id, scene preorder/z, absolute bounds,
  ancestor-intersected clip and multiplicative effective opacity. Damage is
  accumulated per view, clipped at snapshot time, carries a monotonic serial,
  and remains explicit until the compositor clears it. `crtui` stores no
  producer pointer and never acquires, retains, releases or dereferences a
  producer frame; the final compositor maps the stable view id to its own
  surface.

  The new LVGL-free `crtui_surface_test` passes with
  `create/geometry/clip/opacity/z/damage/hit_test/visibility/thread=pass`, including
  capacity-query/no-partial-write and invalid-operation coverage. The complete
  UI gate is 6/6 with LVGL ON and 4/4 in a fresh LVGL-OFF tree. DLL privacy
  remains exact after the additive API: `declared=66 exports=66 lvgl_exports=0`.
  Full Windows CTest is 168/168 (one expected no-camera skip); the cumulative
  `crt-ui-dist` rebuilt and verified through packaged `05-ui`, whose installed
  headers/DLL pass the same 66/66 privacy check. This deliberately does **not**
  close Tranche 5: `crtgfx` still needs the real
  final compositor that interleaves producer-owned GPU content with UI above
  and below it without routing producer pixels through LVGL (TODO 5B), followed
  by macOS/Linux replay (5C).

- **`crtui` Tranche 4 closed on Linux/x86_64 and therefore on all three
  hosts.** The official LVGL-enabled UI gate passed 5/5 three consecutive
  times; a fresh `CRTUI_ENABLE_LVGL=OFF` tree passed layout/input 2/2. The real
  Wayland demo and an externally rebuilt sample using only packaged `05-ui`
  each ran 3/3 with `presented=30 pixel_check=pass input_check=pass`. The
  cumulative distribution verified through `05-ui`, including its generated
  `libxdg-shell-protocol.a`; packaged privacy is `declared=63 exports=63
  lvgl_exports=0 objects=1 headers=3`, and the sample's only dynamic host
  dependency is `libwayland-client.so.0`.

  Tooling is 79/79. Full CTest is 150/151: every `crtui` test and all other
  tests pass except the previously recorded
  `crtmedia_playback_pipeline_test_runs` wall-time check on this host, where
  `aplay -l` still reports no sound cards. No functional source change was
  needed for the Linux replay; the required clean-tree Wayland dependency
  build followed the repository's documented build-then-reconfigure sequence.
  Full evidence is in `docs/acceptance/crtui_acceptance.md`; External Surface is next.

- **`crtui` Tranche 4 replayed and accepted on macOS/arm64; a cumulative-stage
  build race was found and fixed.** The LVGL-enabled UI gate is 6/6; a fresh
  `CRTUI_ENABLE_LVGL=OFF` tree passes layout/input 2/2; direct layout, input,
  render and 40-cycle lifecycle coverage all pass. The in-tree demo and an
  externally rebuilt packaged `05-ui` sample both ran in real Cocoa windows
  with `presented=30 pixel_check=pass input_check=pass`; packaged privacy is
  `declared=63 exports=63 lvgl_exports=0 objects=1 headers=3`.

  The first `crt-ui-dist` run caught a real parallel race: stage-03 dist and
  stage-04 build were siblings, allowing two libc++ staging targets to refresh
  `dist/02-cxx` while a Skia demo linked. The two C++ SDK publishers are now
  serialized, and the stage-04 aggregate/direct Skia consumer waits for the
  complete verified `crt-gfx-simple-dist` predecessor; repeated full runs then
  built and verified every cumulative stage through `05-ui`. Tooling is 79/79.
  Full CTest is 157/158: only the known AVFoundation capture/encode lifecycle
  test fails repeatedly on this camera environment; all six crtui tests pass.
  Linux replay remains before global Tranche 4 closure; full evidence is in
  `docs/acceptance/crtui_acceptance.md`.

- **`crtui` Tranche 4 implemented and accepted on Windows/x64: CRT-owned
  layout, styling and the v1 widget set.** Added free/Row/Column/Stack layout
  with fill/grow/margin/padding/gap/alignment, ScrollView/List, the narrow style
  mask, Image, Switch, Checkbox and single-line TextInput, including committed
  text and editing keys through the crtgfx adapter. The private LVGL renderer
  maps these states to real pixels while the shared DLL still exports only the
  63 declared `crtui_*` functions (`lvgl_exports=0`). The layout engine was
  tightened before acceptance to use allocation-free multi-pass flex traversal,
  so OOM cannot silently leave geometry stale.

  New `crtui_layout_test` passes with LVGL both ON and OFF; input/render gates
  cover the new behavior and real pixels, including 40 lifecycle cycles. Fixed
  `crt-ui-test` to build every executable it invokes (layout/input, conditional
  render, and the macOS shared-contract test) before CTest. Windows results:
  crtui 5/5, full CTest 167/167 with one expected no-camera skip, real Win32
  `crtui_window_demo 30` green, and `crt-ui-dist` verified. The installed
  `examples/ui-basic` was rebuilt in a fresh directory from packaged `05-ui`
  only and ran against its packaged DLLs:
  `presented=30 pixel_check=pass input_check=pass`. macOS/arm64 and Linux replay
  remain open before global Tranche 4 closure; full evidence is in
  `docs/acceptance/crtui_acceptance.md`.
