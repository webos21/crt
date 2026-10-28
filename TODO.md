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
This whole initiative is active; the current focus is **Tranche 1 Linux bootstrap
and remaining native-pair replay**. Numbered tranches 1-8 map to D0-D7.
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
- [ ] **1.5** Supply SDK-owned startup/link rules for minimal CLI help/version/bootstrap
  diagnosis and single-executable link feasibility with the chosen CRT/C++/HTTP/TLS stack.
  Linux passes; native macOS/arm64 and Windows/x64 replay remains.
- [ ] **1.6** Validate the external Development-SDK CLI build/run and actual ABI/imports
  from the relocated candidate with producer trees unavailable;
  review native consumer/ABI evidence before each initial pair's adoption
  ([CLI D0 acceptance baseline](https://github.com/webos21/crt-cli/blob/5fcdc995d231fe2f9dbab4292000a81e0b4fce93/docs/acceptance/bootstrap_acceptance.md)).
  Linux passes; see the [current CLI native evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#native-json-builder-on-linuxx86_64-2026-10-28).
- [ ] **1.7** Select coherent exact assets before promotion/consumer locks;
  resolve the rehearsal's same-name source-asset variants without relabeling builds.

#### Tranche 2. SDK-free CLI runtime and host environment (P0 + D1)

- [ ] **2.1** Prove the single-executable CLI on SDK-free native hosts through actual
  imports, self-diagnosis and declared OS dependencies; review trust/resource/helper policy.
- [ ] **2.2** Separate host-tools/rootfs identity and closure from target sysroots;
  validate native CRT tools/aliases, paths and bounded owned-process cleanup.
- [ ] **2.3** Validate CLI SDK-free HTTP/TLS/trust and fail-closed negatives with
  the CLI-owned D1-07 gate; reuse the [SDK transport evidence](docs/acceptance/sdk_split_acceptance.md).

#### Tranche 3. Immutable SDK installation and selection (D2)

- [ ] **3.1** Supply exact stage/profile artifacts and contract metadata for CLI
  installation/selection/update; promote producer artifacts before consumer locks.
- [ ] **3.2** Review three-host consumer transactions, tamper/interruption recovery
  and path/DLL collisions; keep project-lock updates explicit.
- [ ] **3.3** Support CLI-owned self-upgrade staging, candidate diagnosis, OS-specific
  activation and rollback; upgrading the CLI must preserve project SDK/port pins.

#### Tranche 4. First source-built port (D3)

- [ ] **4.1** Expose the existing zlib recipe/driver through a versioned public
  adapter, retaining SDK-owned configure/build and ABI policy.
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
- [ ] **6.3** Support project-local SDK selection by version/profile/target/content
  identity; CLI owns storage mode, acquisition timing and failure/offline behavior.

#### Tranche 7. Deployment and automation contracts (D6)

- [ ] **7.1** Complete SDK-owned runtime/resource/source-notice contracts for
  packages that run without development SDK/build/store roots.
- [ ] **7.2** Review CLI test/package, versioned JSON/job events, cancellation,
  locked/offline behavior and recovery through original consumer evidence.
  Include the one-executable release goal and self/environment/project doctor scopes.
- [ ] **7.3** Coordinate SDK/CLI publication from the exercised combination:
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

The following sections are independent follow-ups, not prerequisites for the
active SDK/CLI/Studio initiative or a reopening of accepted SDK stages;
long-term extensions stay conditional on a real consumer.

### Focused CRT/PAL follow-ups: revalidate before fixing

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
work is catchable cross-process delivery, process groups and real child-exit
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
- [ ] Implement only the descendant/process-group subset needed by timeout,
  evaluating documented Windows process groups/Job Objects against post-fork
  setpgid. Do not introduce undocumented suspend/resume APIs.
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
