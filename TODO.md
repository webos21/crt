# TODO: CRT Runtime Work Queue

This file tracks the active CRT runtime work queue and its ordered follow-up.
Completed work moves to `HISTORY.md` in reverse chronological order, then to the
[monthly archive](docs/history/README.md). Detailed
policy, acceptance evidence and provenance stay in `docs/` and import manifests.

## Notice

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

<!-- Preserve the old link from dated snapshots; completed details live in HISTORY. -->
<a name="focused-crtpal-follow-ups-revalidate-before-fixing"></a>

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself. Finished
top-level work moves to `HISTORY.md`; terse checked prerequisite gates may remain
under an active tranche only to make its execution order legible.

Task IDs are scoped to their topic heading. Refer to the repository and topic
when a prefix such as `P1` or `V1` appears under multiple headings.

## In Progress

<a name="crt-developer-experience-sdk---cli---studio"></a>

### CRT developer experience: SDK -> CLI -> VSCode

The immediate path is SDK -> CLI -> `crt-vscode`/other editors; standalone
[Studio](docs/design/crt_studio.md) remains a long-term consumer. In Progress
is the selected focus, not a claim that every item is already being implemented.
Prioritize current Dev SDK correctness, signed publication and the first editor
workflow; D4 expansion, Full delivery and self-upgrade stay in Planned.

Queue reviewed on 2026-11-20 against local CRT `278c5d9`, CLI `e362a51` and
extension `b547406`. This is source/document review, not a new native replay or
artifact adoption. Completed Tranches 1-4 and host qualification remain in
[HISTORY](HISTORY.md), [SDK acceptance](docs/acceptance/sdk_split_acceptance.md)
and [host-tools acceptance](docs/acceptance/host_tools_acceptance.md#host-qualification-closure-2026-11-20).
Follow the [SDK contract](docs/design/sdk_development_contract.md) and existing
D0-D7/tranche IDs; **3.3** remains deferred in Planned.

CRT owns producer policy/artifacts; [CLI](https://github.com/webos21/crt-cli/blob/main/TODO.md)
owns workflow/API implementation and [crt-vscode](https://github.com/webos21/crt-vscode/blob/main/TODO.md)
owns editor integration. Preserve original reviewed planning baselines (CLI
`5fcdc995d231fe2f9dbab4292000a81e0b4fce93`, Studio
`27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`); they are not artifact locks.
Use the [producer review procedure](docs/design/sdk_development_contract.md#8-cross-repository-contract-review)
and CLI adoption ledger: build a candidate, hand off immutable public SDK inputs
outside producer trees, validate consumers, then promote and adopt exact pins.
A public release is not required for that loop.

#### P1 - Sign before publishing SDKs (D6)

Acceptance baseline:
[hosted draft rehearsal](docs/acceptance/release_qualification.md#hosted-sdk-release-dry-run-s3-2026-11-17)
and
[publisher trust and signature scope](docs/security/publisher_signing.md#what-a-signature-does-and-does-not-establish)
cover the existing flow. Validate the new finalization path jointly with
[crt-cli signing P1-01](https://github.com/webos21/crt-cli/blob/main/TODO.md#p1---sign-before-publishing).

- [ ] **P1-01** Qualify the hosted staged-SDK and CLI finalization path:
  confirm main-only required reviewers, exercise non-finalizable rehearsals,
  signature/inventory refusal and retry, and verify public bytes on the first
  owner-approved release. The manual host-tools path shares the signing policy,
  not the automated finalizer.
- [x] **P1-02** Implement approvals at both staging and finalization, no
  finalization receipt for rehearsals, and signatures for all payload files
  including checksums and metadata. [Procedure and trust boundary](docs/security/publisher_signing.md#signing-procedure-owner).
- Acceptance: absent/wrong signatures, changed assets or failed approval leave
  the release non-public; valid finalization verifies the downloadable bytes.

## Planned

### P1 - SDK support for the first editor workflow

Producer baseline:
[installed portable provider](docs/design/sdk_development_contract.md#portable-toolchain-contract-crt-31-stage-2)
and
[source-hidden SDK replay](docs/acceptance/sdk_split_acceptance.md#linux-source-hidden-replay-2026-10-27)
. Supply fixtures to
[crt-cli P0-01 through P0-04](https://github.com/webos21/crt-cli/blob/main/TODO.md#p0---minimal-public-editor-api-d7-slice-coordinated-with-d5d6)
if the existing provider needs changes.
[crt-vscode V0-01 through V0-03](https://github.com/webos21/crt-vscode/blob/main/TODO.md#v0---crt-cli-contract-owned-by-crt-cli-requirements-only-here)
consumes the resulting CLI contract.

- [ ] **P1-01** Support CLI-owned compilation-database/project-state contracts with
  installed SDK C/C++ compile-policy fixtures and external consumers. The CLI
  generates `compile_commands.json` from its actual build plan; CRT supplies
  public compiler policy, not a VS Code-specific flag reconstruction path.
- Acceptance: affected consumer flags, headers, SDK/target/lock identities and
  unsupported modes are checked against exact artifacts before adoption.

### P1 - Native compatibility beyond hosted runners

Existing bounds:
[Linux/aarch64 SDK candidate](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-dev-preview-candidate-2026-11-10)
and
[Linux/arm64 hosted replay](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/ci_acceptance.md#linuxarm64-leg-2026-11-11)
. Replay with
[crt-cli CI-02](https://github.com/webos21/crt-cli/blob/main/TODO.md#ci---monitoring-and-pin-promotion). Raspberry Pi acceptance remains pending.

- [ ] **P1-01** Qualify explicit Linux distribution/glibc/loader/LLVM combinations,
  including a real Raspberry Pi ARM64 device. Record OS and tool identities,
  SDK/CLI pins, C/C++ build/run and actual runtime dependencies; Ubuntu 26.04 /
  glibc 2.43 hosted ARM64 evidence does not qualify other systems.

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
- [ ] **P1-02** Extend existing package/dependency and Web export audits into a separately
  runnable all-library public export/import baseline and compatibility
  regression gate; reuse current checks rather than introducing duplicate lint
  ([distribution verification](tools/verify_dist.py),
  [Web export audit](tools/crt_web_exports.py)).

### Broader workflow and feature expansion

The following tranches keep their original IDs and capability prerequisites.
D4 is P2 expansion; the D5/D6 consumer and deployment work can be scoped after
the current correctness/API gates without waiting for all D4 or Full SDK work.

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

- [ ] **D5-01** **6.1** Complete public imports and startup/link/runtime-query rules for
  Development and supported Full stages; keep technical policy in the SDK.
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
[crt-cli D6-01 through D6-04](https://github.com/webos21/crt-cli/blob/main/TODO.md#d6---packaging-and-automation)
. The complete package and lifecycle gates remain open.

- [ ] **D6-01** **7.1** Complete SDK-owned runtime/resource/source-notice contracts for
  packages that run without development SDK/build/store roots.
- [ ] **D6-02** **7.2** Review CLI test/package, versioned JSON/job events, cancellation,
  locked/offline behavior and recovery through original consumer evidence.
  Include the one-executable release goal and self/environment/project doctor scopes.
- [ ] **D6-03** **7.3** Review explicit default/project pin adoption against the published five-host
  Dev SDK/CLI combinations (Linux x86_64/aarch64, macOS/arm64, Windows x64/arm64);
  the latest published CLI and the CI bootstrap pin may differ. Retain the
  [Windows publication handoff](docs/acceptance/sdk_split_acceptance.md#windows-cli-preview-publication-handoff-2026-11-10).
  Keep unqualified Linux C shared SDK mode excluded until 6.4 is resolved. Full SDK delivery
  and its source/license/security/assembly review stay separate; D4 does not block the preview.

### Tranche 8. Developer-tool integration and exercised combinations (D7)

Adoption evidence goes in the
[CLI compatibility ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#adoption-and-exercised-combinations)
. Pair with
[crt-cli D7-01 and D7-02](https://github.com/webos21/crt-cli/blob/main/TODO.md#d7---developer-tool-integration)
,
[crt-vscode V1-10](https://github.com/webos21/crt-vscode/blob/main/TODO.md#v1---extension-mvp)
and
[crt-vscode V3-01 through V3-03](https://github.com/webos21/crt-vscode/blob/main/TODO.md#v3---debugging)
for the corresponding editor and debugger runs.

- [ ] **D7-01** **8.1** Support consumer-proven generic SDK/PAL/debugger gaps. `crt-vscode`
  owns extension/clangd/debug UX, CLI owns workflow and public command/JSON APIs;
  standalone Studio retains its later S0-S5 UI/input/IME/editor acceptance.
- [ ] **D7-02** **8.2** Review actual SDK/CLI/editor combinations against implemented
  command/JSON/debug capabilities; retain native evidence and immutable adoption
  identities in the CLI compatibility ledger. Start with `crt-vscode`; do not
  infer Windows debugger support from build or exception-runtime passes.

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
