# TODO: CRT Runtime Work Queue

This file tracks the active CRT runtime work queue and its ordered follow-up.
Completed work moves to `HISTORY.md` in reverse chronological order. Detailed
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
<a id="relink-kit-test-portability-on-windows"></a>

See [`HISTORY.md`](HISTORY.md) for the full, dated, reverse-chronological
record of completed work. This section stays empty in `TODO.md` itself. Finished
top-level work moves to `HISTORY.md`; terse checked prerequisite gates may remain
under an active tranche only to make its execution order legible.

## In Progress

### CRT developer experience: SDK -> CLI -> Studio

Follow the [SDK contract](docs/design/sdk_development_contract.md) and
[CLI joint sequence D0-D7](docs/design/crt_cli.md#9-joint-development-sequence).
This whole initiative is active. Tranches 1/2 retain their scoped technical
closure. Local Development-SDK installation and stored-contract/project
selection have evidence on all five native pairs, including Windows/x64
([producer evidence](docs/acceptance/sdk_split_acceptance.md),
[CLI evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md)).
The next focus is the bounded local-store integrity/collision closure and the
first Linux user workflow: installed SDK -> first build -> pinned acquisition.
The zlib public adapter can proceed in parallel against fixed contracts.
Full-stage metadata, general updates and self-upgrade are outside that first
workflow; keep them in Planned below. Scope/replay policy is in the
[SDK contract](docs/design/sdk_development_contract.md#change-impact-and-first-user-workflow-adopted-2026-11-05).
Numbered tranches 1-8 map to D0-D7.
SDK split and local assembly rehearsal are complete
([final review](docs/acceptance/release_qualification.md#final-combined-assembly-review-2026-10-28)).
The SDK Producer Contract Gate P0 is the producer work within the early
tranches, developed together with the CLI consumer rather than completed as a
separate prerequisite to all CLI work. Later tranches extend those contracts.
D0 may start with a manually installed identified SDK snapshot; promotion gates
apply before consumer-lock adoption, not before every bootstrap experiment.
Use one authorized local session for the sibling `crt` and `crt-cli` repositories:
produce a Dev SDK candidate, hand off an immutable copy/extraction outside the
producer checkout/build tree, run the CLI consumer, then fix/rebuild/retest.
A public SDK release is not required for this loop; source/build-tree access is
for producer work and diagnosis, while CLI inputs remain the public SDK surface
([candidate policy](docs/design/sdk_development_contract.md#7-change-validation-and-release-principles)).

CRT owns the SDK/host-tool/recipe work below; CLI and
[Studio](docs/design/crt_studio.md) own their implementation and acceptance.
Original reviewed consumer planning baselines: CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93`,
Studio `27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`; these are not artifact locks.
Follow the [producer change procedure](docs/design/sdk_development_contract.md#8-cross-repository-contract-review);
the CLI repository owns adoption evidence in `docs/compatibility.md`.

#### Tranche 1. SDK/CLI contracts and minimal bootstrap (P0 + D0)

- [x] **1.1** Record crt-cli naming, one-executable/data/temporary-helper policy,
  shared immutable SDK references and first-configure/build acquisition.
- [x] **1.2** Review the initial CLI source/module/build layout before implementation;
  share native crt.json/lock, src/main modules and C/C++ src/test with templates.
- [x] **1.3** Identify the existing Linux Dev SDK, public wrappers/imports and host/target
  identities; record local handoff, producer/CLI snapshots, archive/content hashes and tools.
- [x] **1.4** Implement CLI-owned bootstrap input/result records and experimental
  manifest/lock fixtures, without claiming the general SDK contract is implemented.
- [x] **1.5** Validate SDK-owned startup/link rules, basic CLI diagnosis and
  one-executable feasibility on the five recorded native pairs
  ([technical gate review](docs/acceptance/sdk_split_acceptance.md#sdkcli-technical-bootstrap-gate-review-2026-11-02)).
- [x] **1.6** Review relocated SDK-only build/run, producer-tree isolation and
  native ABI/import evidence at those recorded combinations; not production adoption
  ([technical gate review](docs/acceptance/sdk_split_acceptance.md#sdkcli-technical-bootstrap-gate-review-2026-11-02)).
- [x] **1.7** Repair canonical source/cache handling and select coherent common
  Linux 02/03 assets plus the exercised SDK/provider/CLI candidate
  ([selection and limits](docs/acceptance/release_qualification.md#common-source-candidate-selection-2026-11-02)).
  Production adoption/public release remain separate; existing evidence is not relabeled.

#### Tranche 2. SDK-free CLI runtime and host environment (P0 + D1)

- [x] **2.1** Replay the new single-executable/scoped-doctor/resource candidate
  on native Windows/x64 ([Windows record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md#windowsx64-independent-host-environment-replay-2026-11-03));
  Linux and macOS/arm64 proof is [CLI-owned](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md).
- [x] **2.2** Replay the [independent host-tools contract](docs/acceptance/host_tools_acceptance.md)
  and CLI shell on Windows/x64: private imports/aliases, paths and owned-process cleanup.
  [macOS/arm64 replay passes](docs/acceptance/host_tools_acceptance.md#macosarm64-independent-bundle-replay-2026-11-03);
  [Windows/x64 passes](docs/acceptance/host_tools_acceptance.md#final-combined-replay-2026-11-03) after four producer
  repairs (LF-preserving LLVM fetch, mksh async status, directory `X_OK`, explicit child rootfs).
  [macOS private-dylib fixture replay passes](docs/acceptance/host_tools_acceptance.md#macosarm64-private-dylib-closure-replay-2026-11-06);
  Windows private-DLL and Windows/arm64 acceptance stay open.
- [x] **2.3** Repeat D1-07 with Windows/x64's selected package/trust bytes.
  Linux, macOS/arm64 and Windows/x64 pass; earlier host transport records retain their own identities.
  Official release signing/metadata policy stays separate from this local test-identity gate.

#### Tranche 3. Immutable SDK installation and selection (D2)

The existing local dev/install/portable-contract baseline is retained in the
owning acceptance records; this list contains the remaining producer work.

- [x] **3.1** Define the installation-time contract pin/repair handoff using
  existing producer format-1 assets, preserving SDK identity/schema. The
  [Linux consumer slice passes](docs/acceptance/sdk_split_acceptance.md#installation-time-contract-pin-handoff-2026-11-05)
  and [macOS receipt replay passes](docs/acceptance/sdk_split_acceptance.md#macosarm64-installation-receipt-and-repair-replay-2026-11-05);
  [Windows/x64 receipt boundaries also pass](docs/acceptance/sdk_split_acceptance.md#windowsx64-installation-receipt-and-repair-replay-2026-11-05),
  but its first concurrent-install replay hit a native SIGSEGV. Root cause
  found and fixed in the libunwind recipe (callee-saved XMM6-XMM15 were not
  restored on catch; [design note](docs/design/cxx_runtime.md)). Keep the gate
  open until a Development SDK and CLI rebuilt with the fix pass the full
  Windows replay; Windows/arm64 delta replay also remains.
- [ ] **3.2** Finish affected-host SDK path/tool/DLL collision replay. The
  [Linux selection slice passes](docs/acceptance/sdk_split_acceptance.md#linux-installed-sdk-selection-collision-slice-2026-11-05)
  and fixes truncated full-identity matching in CLI;
  [macOS matcher/collision replay passes](docs/acceptance/sdk_split_acceptance.md#macosarm64-selection-first-build-and-https-handoff-replay-2026-11-06).
  [Windows/x64 matcher/collision replay passes](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/sdk_install_acceptance.md#selection-first-build-and-pinned-https-replay-windowsx64-2026-11-06) (CLI-owned; existing rehearsal.8 SDK,
  decoy executables, and a documented working-directory DLL limitation); Windows/arm64 remains. Reuse existing SDKs.
- [x] **3.4** Exercise the explicit developer-pinned Linux metadata/archive/
  portable-contract handoff through HTTPS acquisition and first build
  ([technical evidence](docs/acceptance/sdk_split_acceptance.md#linux-pinned-https-sdk-handoff-2026-11-06)).
  Existing format-1 assets suffice; CLI owns source format 0. Public/default URLs,
  approved trust and actual producer promotion remain preview/release gates.

**3.3** remains the self-upgrade work item, deferred in Planned; identifiers
and historical evidence are not renumbered. CLI owns first-build binding,
external-tool selection/diagnosis and download/install orchestration.

#### Tranche 4. First source-built port (D3)

- [ ] **4.1** Expose the reviewed zlib recipe/compiler adapter against pinned
  SDK/host-tool contracts; this can proceed with the first-build work. Dev already
  includes zlib: distinguish SDK-provided reuse from an explicit isolated source
  build into the CLI port store, preserving source/SDK bytes and ABI policy.
- [ ] **4.2** Review Windows CRT-shell/make static/shared zlib build/link/run;
  require unchanged SDK/source inputs and a separate CLI-owned port store.

#### Tranche 5. Dependencies, variants, locks and cache (D4)

- [ ] **5.1** Review SQLite and libpng-to-zlib recipe/dependency contracts,
  distinguishing host generators, target ports and SDK-provided dependencies.
- [ ] **5.2** Support CLI build-key/project-view/cache validation with three-host
  static/shared, ABI-mismatch, collision and rerun/recovery evidence.
  Bind native ports to exact SDK/target/tool identities regardless of storage backend.

#### Tranche 6. SDK-only application workflow (D5)

- [ ] **6.1** Complete public imports and startup/link/runtime-query rules for
  Development and supported Full stages; keep technical policy in the SDK.
- [ ] **6.2** Validate CLI-owned templates and external C/C++/GFX/Media/UI/Web
  consumers, including runtime/worker resources and CLI self-build/run.
- [ ] **6.3** Support broader profile/capability metadata for project-specific SDK
  pins. Native-dev installed selection and explicit pinned first-build acquisition
  have [Linux evidence](docs/acceptance/sdk_split_acceptance.md#linux-pinned-https-sdk-handoff-2026-11-06)
  and [macOS replay](docs/acceptance/sdk_split_acceptance.md#macosarm64-selection-first-build-and-https-handoff-replay-2026-11-06);
  general profile/offline behavior stays here.

- [ ] **6.4** Audit pure C shared-executable provider policy. The retained
  [Linux first-build provider](docs/acceptance/sdk_split_acceptance.md#linux-installed-sdk-first-build-consumer-slice-2026-11-06)
  emits static C inputs for `executable-shared.link_c`; establish the intended
  policy, then correct or explicitly reject that mode before claiming C shared
  acceptance. Preserve the retained SDK/contract pin and qualify new inputs separately.

#### Tranche 7. Deployment and automation contracts (D6)

- [ ] **7.1** Complete SDK-owned runtime/resource/source-notice contracts for
  packages that run without development SDK/build/store roots.
- [ ] **7.2** Review CLI test/package, versioned JSON/job events, cancellation,
  locked/offline behavior and recovery through original consumer evidence.
  Include the one-executable release goal and self/environment/project doctor scopes.
- [ ] **7.3** Prepare a bounded Linux preview candidate after the first user flow;
  include CLI/SDK notices, trust and external LLVM setup. Coordinate publication
  from the exercised combination:
  promote the SDK first, verify the actual promoted artifact, then adopt its exact
  pin and release the CLI; independent versions need no simultaneous publication.

#### Tranche 8. Studio integration and exercised combinations (D7)

- [ ] **8.1** Support consumer-proven public UI/input/IME/debugger gaps; Studio
  owns S0-S5 GUI/editor/debug/self-development acceptance, CLI owns workflow UX.
- [ ] **8.2** Review actual SDK/CLI/Studio combinations against implemented
  command/JSON/debug capabilities; retain native evidence and immutable adoption identities.

Studio SDK-only S0/UI PoCs may proceed earlier. Live build/run requires the
applicable D5/D6 capabilities, not completion of every tranche or D7 approval
([feature prerequisites](docs/design/crt_studio.md#5-staged-goals)).

## Planned

### SDK workflow beyond the first preview

- [ ] **3.3** Support CLI self-upgrade staging, diagnosis, native activation and
  rollback without changing project SDK/port pins; it does not block D3.
- [ ] Complete Full-stage (`06-web`) install metadata/size limits and general
  stage/profile/version/update selection contracts when their consumer increment starts.
- [ ] Expand the new workflow to additional native pairs before advertising it;
  retain existing five-pair results at their original artifact/source identities.

The following sections are independent follow-ups, not prerequisites for the
active SDK/CLI/Studio initiative or a reopening of accepted SDK stages;
long-term extensions stay conditional on a real consumer.

### Focused CRT/PAL follow-ups: revalidate before fixing

Found by the CLI's Windows replay (reproducers and numbers in the CLI's
[Windows record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#native-json-builder-on-windowsx64-2026-10-29)):

- [ ] Make `fread`/`fwrite` copy blocks instead of looping `fgetc`/`fputc` (`libc/src/stdio.c`; about 70 MB/s against
  5 GB/s for `read()` on Windows), with a throughput regression test.
- [ ] Replay the [implemented UTF-8 narrow-string model](docs/design/windows_narrow_strings.md)
  on Windows/x64 and CRT host tools in Unicode directories; Windows/arm64 passes
  at its recorded snapshot. Retain the pre-1903 and foreign-host DLL limits.
- [ ] Scope the Dev profile's `consumer_compile_options` (`-U_WIN32 ...`, curl headers) per library or make the curl headers
  usable without it: it flips libc++'s `<filesystem>` path type and breaks C++ consumers.
- [ ] Decide whether `posix_spawn` should detect non-CRT children itself instead of requiring `CRT_SPAWN_NATIVE_WINDOWS`.

These older reports need bounded checks against identified current artifacts;
neither later large-consumer passes nor a stale build tree settles them.

- [ ] Revalidate meaningful partial-option combinations of ordinary Linux
  `crt-media-dist`/`crt-ui-dist` with curl enabled. Full Skia+FFmpeg+curl
  dependency publication already exists; close only reproduced gaps in shared
  dependency copying/declaration and packaged example rebuilds, not the
  already-accepted isolated chain
  ([distribution contract](docs/guides/distribution.md),
  [original UI evidence](docs/acceptance/crtui_acceptance.md)).

### Runtime architecture hardening backlog

Independent hardening, not unfinished backend object-boundary acceptance.

- [ ] Replace libc's ELF `CRT_1.0 { global: *; };` export policy with an explicit
  Bionic-compatible public-symbol manifest and generated version map, preserving
  required runtime symbols and establishing an ABI evolution baseline
  ([libc build](libc/CMakeLists.txt),
  [ABI boundary policy](docs/design/host_abi_firewall.md)).
- [ ] Extend existing package/dependency and Web export audits into a separately
  runnable all-library public export/import baseline and compatibility
  regression gate; reuse current checks rather than introducing duplicate lint
  ([distribution verification](tools/verify_dist.py),
  [Web export audit](tools/crt_web_exports.py)).

### Windows process signals and Toybox `timeout`

Positive-pid existence/access probes and `SIGKILL` already exist. Remaining
work is catchable cross-process delivery, general process groups and real child-exit
siginfo. This is non-interactive lifecycle work, not full POSIX job control
([signal dispatch](docs/design/signal_delivery.md),
[Toybox applet status](docs/porting/toybox_applet_status.md)).

- [ ] Freeze Bionic pid-domain/errno/default-action and SIGCHLD rules. Add bounded
  live/dead-pid and child/group regressions, plus a child exiting 7 whose handler
  must see its real pid, `CLD_EXITED` and status; recheck the timeout deadline
  failure without discarding the existing probe/SIGKILL behavior.
- [ ] Carry actual child-registry exit data through dispatch, distinguishing
  natural and signal-killed exits; preserve raise's self-delivery, once-per-
  state-change notification, blocked SIGCHLD and pselect behavior.
- [ ] Design CRT-owned cooperative delivery for catchable signals, with PID-reuse,
  permissions, pending/coalescing and checkpoint rules. Do not equate SIGTERM
  with unconditional TerminateProcess; unsupported delivery must fail honestly.
- [ ] Owned spawn group cancellation passes on Windows/x64
  ([evidence](docs/acceptance/sdk_split_acceptance.md#windowsx64-owned-spawn-cancellation-2026-11-01)); replay on
  Windows/arm64 and settle general/post-fork membership separately. Preserve
  the narrow SIGKILL scope and documented OS APIs.
- [ ] Accept through packaged timeout: deadline/status 124, preserve-status exit
  7, and TERM-to-KILL escalation; retain positive-pid/group and SIGCHLD
  pid/code/status tests. Re-run pselect, process stress, fd snapshots, fork,
  waitpid, shell and full Windows CTest before enabling the applet.

### Conditional Scudo production allocator

Keep the [baseline decision](docs/acceptance/allocator_baseline.md#final-decision).
The current allocator has indexed free bins, local coalescing and a heap tail,
not the former full-heap scans; it still has a global lock
([indexed-allocator evidence](docs/acceptance/allocator_baseline.md#web-camera-follow-up-2026-10-15)).
The resolved allocator-domain linkage incident does not justify replacement.

- [ ] Evaluate Scudo only after a repeatable current-allocator limitation is
  measured in a real workload; retain the current implementation as reference.
- [ ] Before any swap, decouple both Windows fork implementations from
  `__crt_malloc_os_region_count()`/`_base()`/`_size()` through an allocator-neutral
  heap-region snapshot contract; prove many-region child preservation first.
- [ ] If justified, add an explicit allocator selection with pinned provenance
  and unchanged Bionic public behavior. Compare correctness, fork, performance,
  RSS, distribution and upper-runtime workloads on supported hosts before
  adoption; no mandatory Scudo conversion is scheduled.

### Consumer-driven CRT/PAL extensions

These remain visible for future consumers, not automatic CLI prerequisites.

- [ ] Re-measure CRT mksh/Make FFmpeg-configure scaling only if it is still a
  current bottleneck, including an optimized build and the indexed allocator;
  do not apply the old allocator diagnosis without new evidence
  ([Windows investigation](docs/bringup/crtweb_windows_tranche8.md)).
- [ ] Extend the Linux/Windows built-in DNS resolver beyond A-record UDP lookup
  when required: AAAA, TCP fallback, search domains and caching. IPv6 literals,
  sockets and address lists already exist; macOS uses the system resolver
  ([network port evidence](docs/porting/porting_status.md#curl)).
- [ ] Extend Windows AF_UNIX only for a concrete consumer: datagrams/abstract
  names, seqpacket MSG_PEEK, framed-stream ancillary messages, additional fd
  kinds, socket dup and CRT-path address reporting. Preserve accepted stream/
  seqpacket and existing file/pipe/memfd/socket-passing subsets
  ([Web acceptance](docs/acceptance/crtweb_acceptance.md),
  [PAL porting guide](docs/porting/sysroot_ports.md)).
- [ ] Revisit a CRT-owned ELF loader only when a real consumer needs semantics
  the host adapter cannot provide
  ([loader milestones](docs/design/dynamic_loading.md#project-elf-loader-decision-and-milestones)).
- [ ] Consider a documented FreeType fallback mirror if source availability
  warrants it. Reuse existing shared-fetch retries and cached-archive SHA-256
  validation; these are not missing work
  ([fetch implementation](tools/fetch_ports.py),
  [FreeType recipe](porting/recipes/freetype.json)).

### Upper runtime roadmap: long-term extensions

Keep these deferred; none is a prerequisite for the accepted SDK stages.
Scope and dependency policy belong to
[Runtime Roadmap: Deferred Scope](docs/design/runtime_roadmap.md#deferred-scope);
original acceptance evidence stays with each stage.

- [ ] WebRTC: activate only for a concrete real-time communication consumer
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] JavaScriptCore reuse for non-WebView scripting: scope a separate embedding
  API only for a real consumer, reusing the existing engine rather than adding
  QuickJS or another required SDK stage; validate footprint and runtime closure
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] WebGPU: define consumer scope and backend/dependency policy before scheduling
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] EME/DRM: require a product and licensing/platform policy before implementation
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).
- [ ] JS-native application bindings: define the required application/ABI contract
  before scheduling
  ([roadmap](docs/design/runtime_roadmap.md#deferred-scope)).

### Interactive job control (deferred until it's an actual priority)

- [ ] Activate only for a real interactive-shell requirement: console-signal
  delivery, foreground process-group arbitration and an explicit stopped-child
  policy before revisiting mksh's current job-control disablement. This is
  separate from non-interactive timeout/cancellation; do not enable it by merely
  defining `MKSH_UNEMPLOYED`, which disables job control. Follow the
  [owning design](docs/design/job_control.md#interactive-job-control-decided-policy-not-yet-implemented).

### Toybox applet expansion (deferred until it's an actual priority)

- [ ] Select additional applets only when an actual consumer needs them and the
  audited Bionic-compatible CRT/PAL surface and per-applet regressions exist.
  Keep the applet list, registration details and reasons in
  [Toybox applet status](docs/porting/toybox_applet_status.md); timeout follows the
  bounded process-signal gate above, not a blanket applet-enablement pass.
