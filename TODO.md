# TODO: CRT Runtime Work Queue

This file tracks the active CRT runtime work queue and its ordered follow-up.
Completed work moves to `HISTORY.md` in reverse chronological order, then to the
[monthly archive](docs/history/README.md). Detailed
policy, acceptance evidence and provenance stay in `docs/` and import manifests.

## Notice

- When the same task ID appears in In Progress and Planned, record the
  verification in HISTORY and link it from both entries in the same update.
  Check both entries together once the evidence satisfies both scopes. If only
  the release scope passes, check the In Progress entry and keep the follow-up
  open with a link to the release evidence.
- Keep recipe statuses current in
  [`porting/recipes/*.json`](porting/recipes) and
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md) whenever a host is
  rerun. Porting policy and the normal configure/make loop live in
  [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md); completed porting
  investigations belong in [`HISTORY.md`](HISTORY.md).
- A port is not done until both static and shared builds are attempted
  in the same pass on each host, with any host-specific deferral recorded
  in the recipe notes and status matrix. See
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md) for status meanings.
- For CMake wiring changes, do not trust a long-lived local `out/`
  directory. Verify with a fresh clone or at least
  `cmake --fresh --preset <preset>` before calling the change done; stale
  `CMakeCache.txt`/rootfs artifacts have hidden real CI-only ordering
  bugs before. The resolved cases are recorded in [`HISTORY.md`](HISTORY.md).
- Keep toybox applet enablement tied to audited CRT/PAL support, especially
  LLP64 assumptions on Windows. The live applet list and deferrals are in
  [`docs/porting/toybox_applet_status.md`](docs/porting/toybox_applet_status.md).
- Keep terminal/tty behavior coherent for shell and configure use. Current
  syscall/ioctl coverage is tracked in
  [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md), with interactive job
  control policy deferred in [`docs/design/job_control.md`](docs/design/job_control.md).
- Treat `CRT_SPAWN_NATIVE_WINDOWS=1` as a narrow launcher hint for native
  host tools only. The wrapper details live in [`tools/crt-cc`](tools/crt-cc),
  [`tools/crt-c++`](tools/crt-c++), [`tools/crt-native-tool`](tools/crt-native-tool),
  and [`docs/porting/sysroot_ports.md`](docs/porting/sysroot_ports.md).
- If a new public libc or `__crt_sys_*` symbol is added, regenerate or replace
  [`porting/recipes/mbedtls-windows-exclude-symbols.rsp`](porting/recipes/mbedtls-windows-exclude-symbols.rsp)
  in the same pass. The reason is documented in
  [`porting/recipes/mbedtls.json`](porting/recipes/mbedtls.json) and
  [`docs/porting/porting_status.md`](docs/porting/porting_status.md).
- Keep work status in exactly one place per purpose, not restated across all
  three: [`HISTORY.md`](HISTORY.md) holds the detailed, dated record of what
  was actually done and why; `TODO.md` keeps only the terse ordered checkbox
  outline needed to see the current tranche and its prerequisites. A checked
  sub-gate may remain in that outline for sequence visibility, but commands,
  dates, measurements and findings move to `HISTORY.md` or the owning
  acceptance document rather than being repeated here. [`STATUS.md`](STATUS.md)
  is updated only when explicitly asked for, not as part of routine
  documentation passes -- do not touch it on a normal work/doc-cleanup turn.

## Done

<!-- Preserve the inbound editor-queue links from CRT-CLI and CRT VS Code. -->
<a name="p1---sdk-support-for-the-first-editor-workflow"></a>

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself. Finished
top-level work moves to `HISTORY.md`; terse checked prerequisite gates may remain
under an active tranche only to make its execution order legible.

Task IDs are scoped to their topic heading. Refer to the repository and topic
when a prefix such as `P1` or `V1` appears under multiple headings.

## In Progress

### Release 0.5.0 - Dev SDK and Host Tools

Qualify the `dev` (`02-cxx`) SDK on the existing five host pairs as release
targets: Linux/x86_64, Linux/aarch64, macOS/arm64, Windows/x64 and Windows/arm64.
Advertise the host and runtime modes verified on the actual release bytes.
The [CLI release queue](https://github.com/webos21/crt-cli/blob/main/TODO.md#release-050---standalone-cli)
owns CLI packaging and exact SDK adoption. The
[extension release queue](https://github.com/webos21/crt-vscode/blob/main/TODO.md#release-050---vs-code-extension)
owns VSIX verification and the editor support matrix. Keep separate version
identities for the Dev SDK, Host Tools, CLI and VSIX.

The checkboxes below cover 0.5.0. Items with the same ID in Planned
cover the follow-up scope. R05 numbers link the release work across repositories.

#### P1 - Dev SDK ABI baseline

- [ ] **P1-02** (R05-01) Capture public exports and native imports from each
  0.5.0 release candidate before changing symbol visibility. Compare to the
  previous published Dev SDK for that host and review ABI differences before
  approval. Reuse the [distribution auditor](tools/verify_dist.py).
  [Linux/x86_64 RC1 comparison](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26)
  records the first candidate; the [Linux/aarch64 RC1](docs/acceptance/release_qualification.md#linuxaarch64-dev-sdk-050-rc1-2026-11-26) compares against the signed
  2026-11-10 Preview (no metadata change). The [macOS/arm64 RC1](docs/acceptance/release_qualification.md#macosarm64-dev-sdk-050-rc1-2026-11-26)
  compares Mach-O surfaces with its signed 2026-11-10 Preview (one `awk` executable export added, no library change).
  [macOS/arm64 RC2](docs/acceptance/release_qualification.md#macos-150-deployment-target-2026-11-26)
  supersedes RC1 and changes only the declared minimum OS on 104 images.
  The [Windows/x64 RC1](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26) compares against the signed 20261110 Preview with the new PE tool
  (additive only). The [Windows/arm64 RC1](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26)
  compares against the signed 2026-11-18 Preview and has the same six additive static symbols. Compare the final candidate for each shipped pair.

#### D5 - Dev SDK application workflow

- [ ] **D5-01** (R05-02) Replay external C and C++ consumers against the
  extracted SDK with the producer tree hidden. Verify public imports, startup,
  supported static/shared link and load modes, and declared host dependencies.
  See [SDK acceptance](docs/acceptance/sdk_split_acceptance.md).
  [Linux/x86_64 RC1 producer replay](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26)
  covers the first native archive and its associated distribution checks; the [Linux/aarch64 RC1](docs/acceptance/release_qualification.md#linuxaarch64-dev-sdk-050-rc1-2026-11-26)
  repeats the producer, editor-policy and hidden-producer consumer replays on aarch64.
  [macOS/arm64 RC2](docs/acceptance/release_qualification.md#macos-150-deployment-target-2026-11-26)
  passes the hidden-producer C++ consumer, C provider and editor policy. The
  programs built by the CLI replay declare macOS 15.0.
  The [Windows/x64 RC1](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26) adds the
  restricted-token consumer, C 36/36, C++17 22/22 and a cleared-environment editor-policy replay.
  The [Windows/arm64 RC1](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26)
  passes the restricted-token static and shared C++ consumer with both source
  checkouts hidden, C provider 36/36 and C++17 22/22.

#### D6 - Dev SDK and Host Tools distribution

- [ ] **D6-01** (R05-02) Verify the resources, notices and source obligations
  of the Dev SDK and Host Tools release bundles. Check the supported runtime
  resource layout with the external consumers from D5-01.
  The [Linux/x86_64 RC1 record](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26)
  covers SDK notices, dependency provenance, source-archive pins and the
  signed Host Tools inventory. The [macOS/arm64 RC1 record](docs/acceptance/release_qualification.md#macosarm64-dev-sdk-050-rc1-2026-11-26)
  and [Windows/x64 RC1 record](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26)
  cover the SDK notices. The [Windows/arm64 RC1 record](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26)
  records the Dev profile and source-asset verification and the retained
  signed Host Tools bundle verification. Check the final release assets.
- [ ] **D6-02** (R05-02) Verify the chosen Host Tools bundle alongside the Dev
  SDK and replay the existing zlib workflow where included. Reuse an existing
  signed Host Tools version if that exact combination passes. See
  [Host Tools acceptance](docs/acceptance/host_tools_acceptance.md).
  [Linux/x86_64 RC1 and signed Host Tools replay](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26)
  records the first exercised pair; the [Linux/aarch64 RC1](docs/acceptance/release_qualification.md#linuxaarch64-dev-sdk-050-rc1-2026-11-26) exercised the signed
  `v0.1.0-hosttools.20261116.linux-aarch64.1` (replay 26/26, zlib D3 25/25). The [macOS/arm64 RC1](docs/acceptance/release_qualification.md#macosarm64-dev-sdk-050-rc1-2026-11-26)
  exercises the signed `v0.1.0-hosttools.20261119.macos-aarch64.1` (replay 18/18, zlib D3 28/28).
  The [RC2 record](docs/acceptance/release_qualification.md#macos-150-deployment-target-2026-11-26)
  leaves the paired Host Tools and zlib replay open for the newer candidate.
  The [Windows/x64 RC1](docs/acceptance/release_qualification.md#windowsx64-dev-sdk-050-rc1-2026-11-26) exercises the signed
  `v0.1.0-hosttools.20261119.windows-x86_64.1` (replay 18/18, six port suites and a real zlib port build).
  The [Windows/arm64 RC1](docs/acceptance/release_qualification.md#windowsarm64-dev-sdk-050-rc1-2026-11-26)
  exercises the signed `v0.1.0-hosttools.20261119.windows-aarch64.1` (native
  bundle replay 18/18, six port suites and a real zlib port build).
- [ ] **D6-03** (R05-04) Update the Dev SDK version, tag and asset naming
  rules for 0.5.0. Stage and verify candidates, use the offline signature and
  approval workflow, then verify the public downloads. Hand the exact
  manifest, archive, policy and content identities to crt-cli. Apply the Host
  Tools signing policy if publishing a new bundle. See
  [release qualification](docs/acceptance/release_qualification.md#sign-before-publishing-qualification-2026-11-25).

#### D7 - Release compatibility

- [ ] **D7-02** (R05-06) Confirm SDK and Host Tools provenance, notices and
  supported host/mode scope in the release records and the
  [CLI compatibility ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#adoption-and-exercised-combinations).
  Review the combinations exercised with the exact release candidates.
- [x] **D7-03** (R05-04) macOS 15.0 is the supported minimum: set, enforced by the release gate and
  [executed on a hosted `macos-15` runner](docs/acceptance/release_qualification.md#hosted-macos-15-execution-2026-11-26).

The published SDK must pass verification before the CLI adopts its exact pin.
The VSIX release uses the approved CLI and SDK combinations.

## Planned

### P1 - Public ABI baseline and runtime architecture hardening

Capture exports/imports from exact published artifacts before narrowing the ABI;
review removals and compatibility impact first. This does not reopen accepted
backend object-boundary work.

Baseline:
[native boundary audit](docs/design/host_abi_firewall.md#existing-real-host-regression-evidence)
and
[future-boundary checklist](docs/design/host_abi_firewall.md#future-boundary-acceptance-checklist)
. Run the new export comparison and affected downstream SDK replays when an ABI change
is proposed.

- [ ] **P1-01** Replace libc's ELF `CRT_1.0 { global: *; };` export policy with an explicit
  Bionic-compatible public-symbol manifest and generated version map, preserving
  required runtime symbols and establishing an ABI evolution baseline
  ([libc build](libc/CMakeLists.txt),
  [ABI boundary policy](docs/design/host_abi_firewall.md)).
- [ ] **P1-02** Extend the [Dev SDK ABI baseline](#p1---dev-sdk-abi-baseline)
  to a separately runnable all-library export/import and compatibility regression gate.
  Reuse the [distribution verification](tools/verify_dist.py) and
  [Web export audit](tools/crt_web_exports.py).
  The [Linux Dev RC1 comparison](docs/acceptance/release_qualification.md#linuxx86_64-dev-sdk-050-rc1-2026-11-26)
  supplies the bounded ELF replay.

### Broader workflow and feature expansion

The following tranches keep their original IDs and capability prerequisites.
D4 is P2 expansion. The [0.5.0 queue](#release-050---dev-sdk-and-host-tools)
covers the Dev SDK release scope of D5/D6. The tranches below retain broader work.

### Tranche 5. Dependencies, variants, locks and cache (D4)

Baselines:
[CRT D3 producer review](docs/acceptance/sdk_split_acceptance.md#d3-closure-review-and-affected-host-plan-2026-11-09)
, [SQLite recipe status](docs/porting/porting_status.md#sqlite-amalgamation) and
[libpng recipe status](docs/porting/porting_status.md#libpng) . Qualify CLI dependency
resolution and store behavior with
[crt-cli D4-01 through D4-03](https://github.com/webos21/crt-cli/blob/main/TODO.md#d4---dependencies-variants-locks-and-local-cache)
.

- [ ] **D4-01** **5.1** Review SQLite `amalgamation` execution and libpng -> zlib
  dependency/provider/variant contracts. Revisit Windows `z.lib` layout before
  dependent ports; distinguish host tools from target ports and SDK providers.
- [ ] **D4-02** **5.2** Support CLI build-key/project-view/cache validation with three-host
  static/shared, ABI-mismatch, collision and rerun/recovery evidence.
  Bind native ports to exact SDK/target/tool identities regardless of storage backend.

### Tranche 6. SDK-only application workflow (D5)

Acceptance basis:
[installed portable provider](docs/design/sdk_development_contract.md#portable-toolchain-contract-crt-31-stage-2)
,
[SDK source-hidden replay](docs/acceptance/sdk_split_acceptance.md#linux-source-hidden-replay-2026-10-27)
, [Web acceptance](docs/acceptance/crtweb_acceptance.md) for the Web worker boundary and
[project lifecycle PR-01 through PR-07](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/lifecycle_acceptance.md#d2d5---project-sdk-and-creation)
. Pair with
[crt-cli D5-01 through D5-03](https://github.com/webos21/crt-cli/blob/main/TODO.md#d5---application-workflow)
and
[crt-cli D2-01](https://github.com/webos21/crt-cli/blob/main/TODO.md#d2---management-beyond-the-first-workflow)
for broader SDK selection.

- [ ] **D5-01** **6.1** Extend public imports and startup/link/runtime-query rules
  to supported Full stages. See the [Dev SDK release scope](#d5---dev-sdk-application-workflow).
- [ ] **D5-02** **6.2** Validate CLI-owned templates and external C/C++/GFX/Media/UI/Web
  consumers, including runtime/worker resources and CLI self-build/run.
- [ ] **D5-03** **6.3** Support broader profile/capability metadata for project-specific SDK
  pins. Native-dev installed selection and explicit pinned first-build acquisition
  have [Linux evidence](docs/acceptance/sdk_split_acceptance.md#linux-pinned-https-sdk-handoff-2026-11-06)
  and [macOS replay](docs/acceptance/sdk_split_acceptance.md#macosarm64-selection-first-build-and-https-handoff-replay-2026-11-06);
  general profile/offline behavior stays here.

### Tranche 7. Deployment and automation contracts (D6)

Acceptance basis:
[release assembly review](docs/acceptance/release_qualification.md#final-combined-assembly-review-2026-10-28)
,
[distribution gates PK-01 through PK-04](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/lifecycle_acceptance.md#d6---distribution-and-diagnostics)
and
[public Preview replay](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/preview_acceptance.md)
as the bounded baseline. Pair with
[crt-cli D6-01 through D6-03](https://github.com/webos21/crt-cli/blob/main/TODO.md#d6---packaging-and-automation)
and the [0.5.0 handoff in D6-04](https://github.com/webos21/crt-cli/blob/main/TODO.md#d6---standalone-cli-packaging-and-promotion).
The complete package and lifecycle gates remain open.

- [ ] **D6-01** **7.1** Extend the SDK-owned runtime/resource/source-notice
  contract to general application packages that run without SDK/build/store roots.
  See the [Dev SDK and Host Tools release checks](#d6---dev-sdk-and-host-tools-distribution).
- [ ] **D6-02** **7.2** Review general CLI application packaging, versioned JSON
  job events, and broader cancellation, locked/offline and recovery behavior
  through consumer evidence. The standalone CLI, scoped Doctor and initial
  recovery checks are in the [CLI release checks](https://github.com/webos21/crt-cli/blob/main/TODO.md#d6---standalone-cli-packaging-and-promotion).
- [ ] **D6-03** **7.3** Review general default/project pin adoption and additional
  qualified host or SDK combinations beyond the [0.5.0 handoff](#d6---dev-sdk-and-host-tools-distribution).
  Keep project locks, release versions and CI bootstrap pins distinct.
  [Prior Windows handoff](docs/acceptance/sdk_split_acceptance.md#windows-cli-preview-publication-handoff-2026-11-10).

### Tranche 8. Developer-tool integration and exercised combinations (D7)

Adoption evidence goes in the
[CLI compatibility ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#adoption-and-exercised-combinations)
. Pair with
[crt-cli D7-01 and D7-02](https://github.com/webos21/crt-cli/blob/main/TODO.md#d7---developer-tool-integration)
,
[crt-vscode native replay](https://github.com/webos21/crt-vscode/blob/main/TODO.md#v1-follow-up---native-replay)
and
[crt-vscode V3-01 through V3-03](https://github.com/webos21/crt-vscode/blob/main/TODO.md#v3---debugging)
for the corresponding editor and debugger runs.

- [ ] **D7-01** **8.1** Support consumer-proven generic SDK/PAL/debugger gaps. `crt-vscode`
  owns extension/clangd/debug UX, CLI owns workflow and public command/JSON APIs;
  standalone Studio retains its later S0-S5 UI/input/IME/editor acceptance.
- [ ] **D7-02** **8.2** Review SDK/CLI/editor combinations beyond the 0.5.0
  release matrix in [D7-02](#d7---release-compatibility), including future
  debugger or editor capabilities.
  Record exact identities and native evidence in the CLI compatibility ledger.
  Windows debug acceptance requires its own ABI and debugger evidence.

Studio SDK-only S0/UI PoCs may proceed earlier. Live build/run requires the
applicable D5/D6 capabilities, not completion of every tranche or D7 approval
([feature prerequisites](docs/design/crt_studio.md#5-staged-goals)).

### SDK workflow beyond the first preview

SDK-01 uses the
[CLI upgrade gates UP-01 through UP-07](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/lifecycle_acceptance.md#d2---cli-self-upgrade)
with
[crt-cli D2-03](https://github.com/webos21/crt-cli/blob/main/TODO.md#d2---management-beyond-the-first-workflow)
. SDK-02 uses
[Full assembly evidence](docs/acceptance/release_qualification.md#final-combined-assembly-review-2026-10-28)
with
[crt-cli Full SDK P2-01](https://github.com/webos21/crt-cli/blob/main/TODO.md#full-sdk-support-p2-producer-gated)
. SDK-03 uses the
[SDK change and release gates](docs/design/sdk_development_contract.md#7-change-validation-and-release-principles)
and requires a native replay for each added pair.

- [ ] **SDK-01** **3.3** Support CLI self-upgrade staging, diagnosis, native activation and
  rollback without changing project SDK/port pins; it does not block D3.
- [ ] **SDK-02** **Full SDK delivery** (formerly S6): a Full SDK release workflow reusing the Dev SDK flow (S1-S5), once that flow is stable. Not scheduled; prerequisites:
  the Full-stage (`06-web`) install metadata/size limits and general stage/profile/version/update selection contracts when their consumer increment starts;
  the Full source/license/security/assembly review (Tranche 7.3 keeps it separate); the hosted runners' time/disk limits for Skia/FFmpeg/WebKit; and per-host scope
  (Windows/arm64 has no `06-web` collector scope, Linux/aarch64 `06-web` is an x86_64 cross target). Policy:
  [Full SDK and Development SDK](docs/design/sdk_development_contract.md#full-sdk-and-development-sdk-adopted-direction-2026-10-22); evidence: Full collection in
  [release qualification](docs/acceptance/release_qualification.md); procedure: [distribution](docs/guides/distribution.md#stage-production).
- [ ] **SDK-03** Expand the new workflow to additional native pairs before advertising it;
  retain existing five-pair results at their original artifact/source identities.

The following sections are independent follow-ups, not prerequisites for the
current SDK/CLI/editor workflow or a reopening of accepted SDK stages;
long-term extensions stay conditional on a real consumer.

### Windows process signals and Toybox `timeout`

Positive-pid existence/access probes and `SIGKILL` already exist. Remaining
work is catchable cross-process delivery, general process groups and real child-exit
siginfo. This is non-interactive lifecycle work, not full POSIX job control
([signal dispatch](docs/design/signal_delivery.md),
[Toybox applet status](docs/porting/toybox_applet_status.md)).

Use the
[Windows signal verification plan](docs/design/signal_delivery.md#windows-verification)
and the owned-spawn result under SIG-04. Coordinate with
[crt-cli D6-01](https://github.com/webos21/crt-cli/blob/main/TODO.md#d6---packaging-and-automation)
only where its owned-child cancellation scope needs a new PAL capability.

- [ ] **SIG-01** Freeze Bionic pid-domain/errno/default-action and SIGCHLD rules. Add bounded
  live/dead-pid and child/group regressions, plus a child exiting 7 whose handler
  must see its real pid, `CLD_EXITED` and status; recheck the timeout deadline
  failure without discarding the existing probe/SIGKILL behavior.
- [ ] **SIG-02** Carry actual child-registry exit data through dispatch, distinguishing
  natural and signal-killed exits; preserve raise's self-delivery, once-per-
  state-change notification, blocked SIGCHLD and pselect behavior.
- [ ] **SIG-03** Design CRT-owned cooperative delivery for catchable signals, with PID-reuse,
  permissions, pending/coalescing and checkpoint rules. Do not equate SIGTERM
  with unconditional TerminateProcess; unsupported delivery must fail honestly.
- [ ] **SIG-04** Owned spawn group cancellation passes on Windows/x64
  ([evidence](docs/acceptance/sdk_split_acceptance.md#windowsx64-owned-spawn-cancellation-2026-11-01)); replay on
  Windows/arm64 and settle general/post-fork membership separately. Preserve
  the narrow SIGKILL scope and documented OS APIs.
- [ ] **SIG-05** Accept through packaged timeout: deadline/status 124, preserve-status exit
  7, and TERM-to-KILL escalation; retain positive-pid/group and SIGCHLD
  pid/code/status tests. Re-run pselect, process stress, fd snapshots, fork,
  waitpid, shell and full Windows CTest before enabling the applet.

### Conditional Scudo production allocator

Keep the [baseline decision](docs/acceptance/allocator_baseline.md#final-decision).
The current allocator has indexed free bins, local coalescing and a heap tail,
not the former full-heap scans; it still has a global lock
([indexed-allocator evidence](docs/acceptance/allocator_baseline.md#web-camera-follow-up-2026-10-15)).
The resolved allocator-domain linkage incident does not justify replacement.

Regression basis: [fork test policy](docs/design/process_fork.md#test-policy-current)
and the allocator baseline below. Adoption remains conditional on the workload and fork
evidence.

- [ ] **ALLOC-01** Evaluate Scudo only after a repeatable current-allocator limitation is
  measured in a real workload; retain the current implementation as reference.
- [ ] **ALLOC-02** Before any swap, decouple both Windows fork implementations from
  `__crt_malloc_os_region_count()`/`_base()`/`_size()` through an allocator-neutral
  heap-region snapshot contract; prove many-region child preservation first.
- [ ] **ALLOC-03** If justified, add an explicit allocator selection with pinned provenance
  and unchanged Bionic public behavior. Compare correctness, fork, performance,
  RSS, distribution and upper-runtime workloads on supported hosts before
  adoption; no mandatory Scudo conversion is scheduled.

### Consumer-driven CRT/PAL extensions

These remain visible for future consumers, not automatic CLI prerequisites.

The links on PAL-EXT-01 through PAL-EXT-06 are existing investigations, evidence or
design triggers. Define a targeted regression and a real consumer before qualifying a
change.

- [ ] **PAL-EXT-01** Re-measure CRT mksh/Make FFmpeg-configure scaling only if it is still a
  current bottleneck, including an optimized build and the indexed allocator;
  do not apply the old allocator diagnosis without new evidence
  ([Windows investigation](docs/bringup/crtweb_windows_tranche8.md)).
- [ ] **PAL-EXT-02** Extend the Linux/Windows built-in DNS resolver beyond A-record UDP lookup
  when required: AAAA, TCP fallback, search domains and caching. IPv6 literals,
  sockets and address lists already exist; macOS uses the system resolver
  ([network port evidence](docs/porting/porting_status.md#curl)).
- [ ] **PAL-EXT-03** Extend Windows AF_UNIX only for a concrete consumer: datagrams/abstract
  names, seqpacket MSG_PEEK, framed-stream ancillary messages, additional fd
  kinds, socket dup and CRT-path address reporting. Preserve accepted stream/
  seqpacket and existing file/pipe/memfd/socket-passing subsets
  ([Web acceptance](docs/acceptance/crtweb_acceptance.md),
  [PAL porting guide](docs/porting/sysroot_ports.md)).
- [ ] **PAL-EXT-04** Revisit a CRT-owned ELF loader only when a real consumer needs semantics
  the host adapter cannot provide
  ([loader milestones](docs/design/dynamic_loading.md#project-elf-loader-decision-and-milestones)).
- [ ] **PAL-EXT-05** Consider a documented FreeType fallback mirror if source availability
  warrants it. Reuse existing shared-fetch retries and cached-archive SHA-256
  validation; these are not missing work
  ([fetch implementation](tools/fetch_ports.py),
  [FreeType recipe](porting/recipes/freetype.json)).
- [ ] **PAL-EXT-06** Package the Skia-less and FFmpeg-less closures on Linux as well: Linux still refuses both single-closure shapes, which Windows/x64, Windows/arm64 and macOS/arm64 package
  ([Windows/x64](docs/acceptance/crtui_acceptance.md#windowsx64-shared-runtime-closure-replay-2026-11-24),
  [Windows/arm64](docs/acceptance/crtui_acceptance.md#windowsarm64-packaging-and-closure-replay-2026-11-24),
  [macOS](docs/acceptance/crtui_acceptance.md#macosarm64-skia-less-and-ffmpeg-less-closure-packages-2026-11-24)). Do it only for a consumer that wants such a package; it needs its own native Linux replay.

### Upper runtime roadmap: long-term extensions

Keep these deferred; none is a prerequisite for the accepted SDK stages.
Scope and dependency policy belong to
[Runtime Roadmap: Deferred Scope](docs/design/runtime_roadmap.md#deferred-scope);
original acceptance evidence stays with each stage.

The [runtime stage gates](docs/design/runtime_roadmap.md#stage-gates) describe the
existing scope. Define feature-specific acceptance after a deferred feature is scoped.

- [ ] **WEB-01** WebRTC: activate only for a concrete real-time communication consumer
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] **WEB-02** JavaScriptCore reuse for non-WebView scripting: scope a separate embedding
  API only for a real consumer, reusing the existing engine rather than adding
  QuickJS or another required SDK stage; validate footprint and runtime closure
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] **WEB-03** WebGPU: define consumer scope and backend/dependency policy before scheduling
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] **WEB-04** EME/DRM: require a product and licensing/platform policy before implementation
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] **WEB-05** JS-native application bindings: define the required application/ABI contract
  before scheduling
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).

### Interactive job control (deferred until it's an actual priority)

Replay basis:
[interactive job-control policy](docs/design/job_control.md#interactive-job-control-decided-policy-not-yet-implemented)
. A future interactive consumer supplies the terminal acceptance cases.

- [ ] **JOB-01** Activate only for a real interactive-shell requirement: console-signal
  delivery, foreground process-group arbitration and an explicit stopped-child
  policy before revisiting mksh's current job-control disablement. This is
  separate from non-interactive timeout/cancellation; do not enable it by merely
  defining `MKSH_UNEMPLOYED`, which disables job control. Follow the
  [owning design](docs/design/job_control.md#interactive-job-control-decided-policy-not-yet-implemented).

### Toybox applet expansion (deferred until it's an actual priority)

Acceptance basis:
[deferred applet status](docs/porting/toybox_applet_status.md#deferred-applets-and-why)
and [Windows process signals SIG-05](#windows-process-signals-and-toybox-timeout) for
the separate timeout case.

- [ ] **TOYBOX-01** Select additional applets only when an actual consumer needs them and the
  audited Bionic-compatible CRT/PAL surface and per-applet regressions exist.
  Keep the applet list, registration details and reasons in
  [Toybox applet status](docs/porting/toybox_applet_status.md); timeout follows the
  bounded process-signal gate above, not a blanket applet-enablement pass.
