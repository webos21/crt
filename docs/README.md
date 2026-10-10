# Documentation

Documents are grouped by purpose. Start with [project meaning](design/project_meanings.md),
[stack choices](design/project_stacks.md), and the [runtime roadmap](design/runtime_roadmap.md).
Current work is in [TODO](../TODO.md); completed work is in
[October history](../HISTORY.md) and the [monthly archive](history/README.md).

## Acceptance contracts and measured evidence

- [Allocator Baseline Decision Record](acceptance/allocator_baseline.md)
- [Encode & Capture Acceptance](acceptance/crtmedia_encode_capture_acceptance.md)
- [crtmedia Hardware Decode (Phase A) Acceptance Contract](acceptance/crtmedia_hardware_decode_acceptance.md)
- [Networking & Streaming Acceptance](acceptance/crtmedia_networking_acceptance.md)
- [Zero-Copy Decoded Textures Acceptance Contract](acceptance/crtmedia_zero_copy_decode_acceptance.md)
- [crtui Acceptance (Stage `05-ui`)](acceptance/crtui_acceptance.md)
- [crtweb Acceptance (Stage `06-web`)](acceptance/crtweb_acceptance.md)
- [libcrtgfx Live GPU Presentation Acceptance Contract](acceptance/libcrtgfx_live_presentation_acceptance.md)
- [scanf Edge-Case Matrix](acceptance/scanf_edge_matrix.md)

## Architecture, ABI boundaries, and policy

- [C++ Runtime](design/cxx_runtime.md)
- [Dynamic Loading](design/dynamic_loading.md)
- [Header And Sysroot ABI Policy](design/header_abi.md)
- [Host ABI Firewall](design/host_abi_firewall.md)
- [Job Control Minimal Surface](design/job_control.md)
- [libcrtgfx API Policy](design/libcrtgfx_api_policy.md)
- [libcrtgfx GPU Backend Object Boundary](design/libcrtgfx_gpu_backend_boundary.md)
- [libcrtgfx Wayland Plan](design/libcrtgfx_wayland_plan.md)
- [libcrtmedia API Policy](design/libcrtmedia_api_policy.md)
- [Linux Pthread Lifecycle Notes](design/linux_pthread_lifecycle.md)
- [Process Fork Model](design/process_fork.md)
- [Project Meanings](design/project_meanings.md)
- [Project Stacks](design/project_stacks.md)
- [pthread policy](design/pthread_policy.md)
- [Runtime Roadmap](design/runtime_roadmap.md)
- [Shared Library Artifacts](design/shared_libraries.md)
- [Signal Delivery](design/signal_delivery.md)
- [Windows Fork Emulation: Current Implementation](design/windows_fork_emulation.md)

## Build, distribution, import, and usage guides

- [Android-like Shell Environment](guides/android_shell_environment.md)
- [CRT Build Stages And Distributions](guides/distribution.md)
- [CRT FAQ](guides/faq.md)
- [Bionic Import Policy](guides/import_bionic.md)
- [Developer Preview Release](guides/release_preview.md)
- [Shell Import And Bootstrap Policy](guides/shell_import.md)

## Upstream porting plans, gaps, and status

- [Bionic Libc Completeness Gaps Before `libcrtgfx`](porting/bionic_libc_gaps.md)
- [WebKit CRT Port: Upstream Mapping](porting/crtweb_porting.md)
- [Library Porting Status](porting/porting_status.md)
- [Sysroot Configure/Make Porting](porting/sysroot_ports.md)
- [Toybox Applet Status](porting/toybox_applet_status.md)

## Implementation and source reference

- [Libm Dependency Map](reference/libm_dependency_map.md)

## Published release notes

- [CRT v0.4.0-preview.1](releases/release_notes_v0.4.0-preview.1.md)

## Historical material

- [Monthly history](history/README.md): decisions, causes, evidence, and original-log recovery.
- [Bring-up notes](bringup/README.md): early platform work and detailed fork investigation.

Choose a destination by the document’s primary purpose. Acceptance documents own
gates and measured host evidence; design documents own contracts and rationale;
guides own procedures; porting documents own consumer gaps and recipe status.
Bring-up records and release notes retain their dated scope. Paths in inline
code are repository-relative unless explicitly described as historical or SDK paths.

Keep one owner for each kind of information: project scope in `project_meanings`,
stack rationale in `project_stacks`, stage order in `runtime_roadmap`, procedures
in guides, and measured gates in acceptance documents. Link to these owners
instead of copying feature lists or completed checklists. Fold adopted study
notes into the owning document; remove superseded proposals and unsupported
claims rather than maintaining a second policy or status narrative.
