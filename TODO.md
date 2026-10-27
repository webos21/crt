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

### CRT SDK split: Full SDK and Development SDK

CRT produces two kinds of SDK per native pair from the same sources, stage outputs and port
recipes (no second build system, ABI or recipe set); the contract is
[Full SDK and Development SDK](docs/design/sdk_development_contract.md#full-sdk-and-development-sdk-adopted-direction-2026-10-22).
The **Full SDK** is the existing cumulative `01-c` .. `06-web` archives, unchanged. The **Development
SDK** is `02-cxx` plus zlib, Mbed TLS and libcurl (static and shared), with no graphics, media, UI or
Web content; it exists so `crt-cli` can be built without CRT's source or build tree, and its
shared runtime files are byte-identical to the Full SDK of the same release. The CLI runtime bundle
is assembled by `crt-cli` from its own link result. The `crt-cli` repository is laid out as
`crt new` generates it and must build and run with its own `crt build`/`crt run`
([layout and self-test](docs/design/crt_cli.md#9-joint-development-sequence)); its adoption evidence
stays in the CLI repository. Residual-defect triage finished 2026-10-22
([record](docs/bringup/crt_pal_residual_triage.md)).

- [x] S0 Freeze the split in the design docs: Development SDK content rule, archive and release asset
  names, how the `dev` profile is expressed (manifest stays `02-cxx`; identity in the development-contract
  sidecar), notice/SBOM scope (no FFmpeg, so no LGPL source offer), and who supplies the CLI's TLS trust
  material (the curl port bakes in no CA bundle).
- [x] S1 Add a CLI-shaped reference consumer to this repository's tests (C++17: HTTPS GET with
  certificate verification plus zlib inflate) linked only through the Development SDK wrappers; its
  ELF/Mach-O/PE imports, via `crt_binary_dependencies.py`, define the runtime closure instead of a guessed list.
- [x] S2 Producer: build zlib, Mbed TLS and libcurl through one shared implementation with the
  `04-gfx-media` stage builder, then assemble the Development SDK on top of
  `02-cxx` for Linux x86_64/aarch64, macOS arm64 and Windows x64/arm64.
  - [x] Shared producer/profile implementation and recorded Linux/x86_64,
    macOS/arm64 and Windows/x64 production; [remaining matrix](docs/acceptance/sdk_split_acceptance.md#remaining-matrix).
  - [x] Linux/aarch64 native Development producer compile/package replay;
    [evidence](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-native-producer-and-package-replay-2026-10-27).
  - [x] Windows/arm64 native Development producer compile/package replay;
    [evidence](docs/acceptance/sdk_split_acceptance.md#windowsarm64-native-producer-and-package-replay-2026-10-27).
- [x] S3 Verification: `verify_dist.py` profile checks (no `03+` content, ports present static and shared,
  declared dependencies and notices, relocation, the macOS binding audit with a shared consumer, Windows
  imports resolving at load); an external build and run from the Development SDK alone, with CRT's source
  and build tree hidden.
  - [x] Linux/x86_64 relocated static/shared consumer with producer/source SDK
    inaccessible; [evidence](docs/acceptance/sdk_split_acceptance.md#linux-source-hidden-replay-2026-10-27).
  - [x] Linux/aarch64 packaged, relocated static/shared consumer with producer/source SDK
    inaccessible; [evidence](docs/acceptance/sdk_split_acceptance.md#linuxaarch64-native-producer-and-package-replay-2026-10-27).
  - [x] macOS/arm64 packaged, relocated static/shared consumer, shared-consumer
    binding audit and producer-tree-hidden replay;
    [evidence](docs/acceptance/sdk_split_acceptance.md#macosarm64-source-hidden-and-shared-consumer-replay-2026-10-27).
  - [x] Windows/x64 packaged, relocated static/shared consumer with producer trees
    inaccessible under a restricted token; [evidence](docs/acceptance/sdk_split_acceptance.md#windowsx64-source-hidden-and-package-replay-2026-10-27).
  - [x] Windows/arm64 native producer-tree-hidden replay under a restricted token;
    [evidence](docs/acceptance/sdk_split_acceptance.md#windowsarm64-source-hidden-replay-2026-10-27).
- [x] S4 Release tool: compose `prepare_release_assets.py` gates with the Development profile collector
  (`crt_sdk_release.py`) for both kinds (checksums, release manifest, recipes
  pointing at the release, `VERSION` equal to the tag) and checks that the two share the source commit and the
  runtime-file digests. Local implementation/fixtures are B4 evidence; actual positive collection belongs to
  [release qualification](#release-qualification-and-assembly-rehearsal), not a second rehearsal here.
- [ ] S5 Native replay on Linux x86_64, macOS arm64 and Windows x64 (aarch64 targets compile-only unless
  run natively), including negative cases (missing port, mismatched digest, stale Full SDK). Hand the
  identity to `crt-cli`, whose lock owns adoption.
  - [x] Linux/x86_64 packaged S5 replay and candidate handoff; Linux/aarch64
    required negatives are already recorded natively; [S5 evidence](docs/acceptance/sdk_split_acceptance.md#s5-native-negative-and-candidate-handoff-gate).
  - [ ] macOS/arm64, Windows/x64 and Windows/arm64 stale-Full-source replay and
    remaining candidate handoff receipts; promotion/CLI adoption stay separate.

CI cost stays bounded: one host smoke in CI, the full matrix at release time. Not goals here: signing and
publisher authentication, a static CLI, and any change to the Full SDK stages.

## Planned

SDK Producer Contract Gate P0 below continues after, and partly through, the SDK split in progress above.
The remaining sections are independent follow-ups, not a prerequisite chain for
CLI/Studio or a reopening of accepted SDK stages; long-term extensions stay
conditional on a real consumer.

### Release qualification and assembly rehearsal

Start after build-system consolidation and the required native SDK producer /
verification gates. B4 supplied the local tools, not release-qualified artifacts;
the [acceptance contract](docs/acceptance/build_system_acceptance.md#release-qualification-scope-split-2026-10-23)
owns the checks and existing provenance limitations.

- [ ] Select a clean fixed source commit and release/rehearsal version; generate
  matching Development/Full SDKs and source assets without relabeling old builds.
- [ ] Run real `crt-dev-release` and `crt-full-release` positive collection;
  verify archives, checksums, manifests, source identity and runtime equality.
- [ ] Complete final evidence review before accepting release assembly. Tag
  creation, upload and public release require separate authorization.

### CRT developer experience: SDK -> CLI -> Studio

Follow the [SDK contract](docs/design/sdk_development_contract.md) and
[CLI joint sequence D0-D7](docs/design/crt_cli.md#9-joint-development-sequence).
[Studio](docs/design/crt_studio.md) is a later CLI consumer; the development
contracts remain design work, not an implemented or released product.

**SDK Producer Contract Gate P0** (feeds CLI D0/D1; not started):

- [ ] Supply external-consumer identity/capability fixtures, distinguishing
  existing manifest/stage/port formats from the proposed development contract.
- [ ] Publish the SDK-owned startup/link/runtime-query rules and public imports for both the Full
  and Development SDK, and the runtime-bundle contract: closure computed from the CLI's
  ELF/Mach-O/PE imports, app-local placement, Windows import resolution at load.
- [ ] Define host-tools/rootfs identity and runtime closure separately from the
  target sysroot; validate actual native tool/alias execution.
- [ ] Validate the minimum CRT-native CLI HTTP/TLS stack and trust inputs for bootstrap without an
  installed development SDK, including fail-closed negative cases (the producer side is S1 above).
- [ ] Record independent consumer build/run and ABI evidence for each initial
  native pair before promoting the corresponding artifact/consumer lock.

Follow the [producer change procedure](docs/design/sdk_development_contract.md#8-cross-repository-contract-review);
the CLI repository owns adoption evidence in `docs/compatibility.md`.
Add lightweight automated contract/link checks once fixtures exist, without
duplicating full WebKit builds for documentation changes.

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
