# Independent CRT Host Tools (experimental format 1)

CRT owns `crt.experimental.host-tools`, format 1. It is a separate distribution
from a target SDK and from the CLI executable. The first producer assembles
verified native SDK tool bytes without changing that SDK; running the bundle
needs neither the SDK nor CRT sources/build outputs. This does not replace the
SDK's existing tools or introduce a general SDK contract version.

The bundle preserves `system/bin`, `bin`, `usr/bin` and `system/etc/mkshrc`.
Its four native roles are `shell` (mksh), `toybox`, `awk` and `make`; every
supported applet/alias is enumerated. Relative symlinks remain internal;
existing native Windows alias copies remain copies. Windows build/source hosts
still require Developer Mode for real job-rootfs directory symlinks; diagnose
failure rather than change settings. No MSYS markers or host shell fallback.

`manifest.json` contains:

- `schema`, `format`, `version`, execution `host` (OS/architecture), and
  `promotion: local-only` for this experiment;
- producer assembler commit/dirty/script identities, and the source SDK's exact
  manifest hash, producer identity and build provenance (not a runtime input);
- `tools`: relative native paths for the four roles;
- `rootfs`: layout, virtual shell/make/awk paths and writable directories;
- `aliases`: path, actual file/symlink type, target/hash and dispatch role;
- `executable_files`: the sorted union of primary tools and aliases; every
  entry must remain executable (permission removal is rejected even when bytes match);
- private runtime dependency closure, explicit OS dependencies and notices;
  Linux `interpreters` are read from PT_INTERP, independently of DT_NEEDED,
  and require the architecture's declared OS ELF loader (not bundled);
- `inventory`: sorted `[relative-path, file|symlink, SHA-256|target]` entries for
  every payload file/link except `manifest.json`, plus its canonical JSON SHA-256.

The caller supplies the manifest's SHA-256 separately. CLI validation rejects
unknown format/host, escaped links/paths, missing/extra/changed inventory,
non-native tool images and non-executable tools before any job starts. A hash
binds the explicitly selected local candidate; it is not publisher authentication
or production adoption. Host identity must match the CLI execution host even
when a project later selects a different target SDK.

For optional port-shell work, the CLI creates a fresh caller-owned job rootfs:
`system`, `bin` and `usr` link to the immutable bundle; `tmp` and `data/local`
are private writable directories. `CRT_ROOTFS` points to that overlay, while
`CRT_SYSROOT` is set only for an explicitly selected project SDK. PATH contains
only the bundle's physical `system/bin`; user/global PATH and build flags are
not inherited. The existing PAL's rootfs path rules remain in force; this is
not an OS sandbox for arbitrary scripts.

`SHELL`, `CONFIG_SHELL`, `MAKE` and `AWK` use the manifest's whitespace-free
virtual paths. `MAKEFLAGS=SHELL=/system/bin/sh` (Windows: `sh.exe`) also selects
the CRT shell for GNU make, which ignores the environment's SHELL on POSIX.
This avoids splitting a physical shell path containing spaces, without an
upstream make patch or permanent launcher. Jobs execute argv directly and use
the common bounded owned-process runner; interactive terminal/job control is
outside this contract. Source-port recipe execution and compiler-wrapper
adapters remain D3; this increment does not implement a recipe engine.

Producer procedures, exercised native scope and original receipts belong to
[host-tools acceptance](../acceptance/host_tools_acceptance.md); CLI command/JSON
adoption belongs to its compatibility ledger. Every execution host needs its
own native bundle/adapter evidence; old SDK tool probes do not qualify a new
independent artifact. Current replay outcomes belong to acceptance, not this contract.
