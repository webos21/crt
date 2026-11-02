# Independent Host-tools Acceptance

Producer contract: [experimental host tools](../design/host_tools_contract.md).
Work queue: [Tranche 2](../../TODO.md#tranche-2-sdk-free-cli-runtime-and-host-environment-p0--d1).
The CLI owns commands, adoption and native execution in its
[compatibility ledger](https://github.com/webos21/crt-cli/blob/main/docs/compatibility.md#independent-host-tools-and-d1-linux-adoption-2026-11-02)
and [D1 evidence](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md).
This document owns assembly, provenance, prerequisites and producer regressions.

## Local Linux/x86_64 candidate (2026-11-02)

Reviewed CRT `8616fd64b13fa41925c1fce2b08f4937b7e8dcf1` and CLI
`adc03b04ed124c71f0726a895da2cc2cde98b78d` before this uncommitted increment.
CRT owns the new assembler/CMake entry point and host-tools format; CLI owns
the environment, scoped diagnosis and process adapter. SDK/provider/project
formats and production locks do not change. This is local technical evidence,
not a clean committed assembler, release, promotion or three-host acceptance.

| Input / output | Exact identity |
| --- | --- |
| Native source SDK | `out/tranche17-20261102/build5/dist/01-c`, clean CRT `49879bf82e2742baeb1878e492d42c7d561b6f7a`, `v0.4.0-sourcefix.20261102.5` |
| Source SDK manifest SHA-256 | `5b0914ceefd5f4ef79343fb58fa2ac7fd84ef649d6866c37b1b93afb8e1c3576` |
| Selected output | `out/tranche2-20261102/host-tools-r4`, `v0.1.0-hosttools.20261102.4`, Linux/x86_64 |
| Host-tools manifest SHA-256 | `59bdf8f83ce7817175010953abe652fe6f52852f8aff9a5528f2ce8b64812652` |
| Payload inventory SHA-256 | `3b75fb9bbab8c9e9a1c7a542f5ced227f1c66bb92ed875e87d78f86e1aba11bf` |
| Assembler script SHA-256 | `9599fad6b3749d034f0034eabc6cd20eb8c867d05281e0e04d53a291d6d06e0f` |
| ELF inspection module SHA-256 | `c878c5c449b3074e8acf635e7810bcc6cf6ea1fc05b84f666c7359fde66e6fa0` |

The manifest truthfully records assembler baseline `8616fd6` with
`working_tree_dirty: true`. Its source SDK remains the separate clean candidate
from [Tranche 1.7](release_qualification.md#common-source-candidate-selection-2026-11-02).
The pending tracked patch, new files and their hashes are retained in
`out/tranche2-20261102/review-snapshot/` for the baseline above.
Assembly needs only native `01-c` public artifacts, CRT-owned notice/recipe inputs
and producer Python. It does not build or require C++/GFX/Media/UI/Web. Runtime
consumption uses the standalone copied bundle, without source SDK or Python.

The bundle has 120 inventory entries, 90 executable paths and 86 declared
aliases; physical copied aliases are retained as supplied by this SDK. Four
primary images are CRT-built ELF x86_64 and have no DT_NEEDED. mksh, toybox and
awk have no runtime search path; make has an **empty RUNPATH tag**, which is
allowed rather than described as an absent tag. All four name the real OS
interpreter `/lib64/ld-linux-x86-64.so.2`. `interpreters` and explicit external
prerequisites record it alongside the Linux kernel ABI; the OS loader is not
bundled, and no host libc or host C++ runtime is imported.

An audit caught that copying `01-c` prerequisites alone omits this PT_INTERP
boundary. Candidate r4 adds actual interpreter inspection and rejects foreign
loader paths; earlier r1-r3 manifests stay retained and are not the final
dependency declaration. The executable payload bytes did not change.

GNU make's missing SDK notice is supplied from
`third_party/crt-runtime/make.COPYING`, verified against the exact official
AOSP `toolchain/make` `44fc4fe66a484b91844c302f03eaa8438e065d17` COPYING blob,
SHA-256 `8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903`.
The companion source receipt, adopted make recipe and actual source-SDK recipe
must agree. A changed cached COPYING fails before publication. Notices and
provenance for CRT libc/compiler-rt, mksh, toybox and separate awk are retained;
the compiler-rt notice can be installed from CRT's pinned LLVM notice registry
when a C-only SDK omits the combined LLVM-runtime notice directory.

## Reproduction and checks

From CRT, with a previously verified native public C SDK:

```sh
python3 -m unittest tools/test_host_tools.py tools/test_crt_elf.py
python3 tools/create_host_tools.py \
  --sdk-root out/tranche17-20261102/build5/dist/01-c \
  --output out/new-host-tools-candidate --version v0.1.0-hosttools.20261102.4
```

For the CMake entry point, configure a fresh native preset output with
`CRT_HOST_TOOLS_PREDECESSOR=<public-01-c-SDK>`,
`CRT_HOST_TOOLS_VERSION=<independent-version>` and optionally
`CRT_HOST_TOOLS_OUTPUT=<new-path>`, then:

```sh
cmake --build out/tranche2-20261102/producer-final --target crt-host-tools-test
```

The ordinary default predecessor is `crt-c-dist`; an explicit predecessor
avoids rebuilding it. Select a **new output path/version** when inputs change;
the assembler refuses to replace an existing candidate. Repeated unchanged
builds use the shared content-aware cache, followed by inventory/native-image
verification. Logs are retained under `out/tranche2-20261102/` as
`producer-final-configure.log`, `producer-final-reconfigure.log`,
`producer-final-r4-build.log` and `producer-final-r4-cache.log`.
The CMake output `host-tools-r4-cmake` matches r4's manifest and payload bytes.

21 focused host-tools/ELF parser tests pass. Fixtures cover SDK independence,
native host/image rejection, dispatcher/notice corruption, immutable outputs,
escaped links, symlinked output parents, permission removal and changed cached
make notices. ELF fixtures cover real PT_INTERP without DT_NEEDED, ELF32 big
endian/ELF64 little endian, malformed strings/truncated headers, absent
interpreters and foreign-loader rejection. Fixture passes are not native tool
execution proof; that belongs to the linked CLI replay.

## Native scope and remaining gates

Linux consumes the independently copied artifact with source/SDK/provider/LLVM
paths hidden and host shells/Python/build drivers masked. Exact native tool,
make/awk/alias, process cleanup, bootstrap-resource and HTTPS results are
[CLI-owned](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/host_environment_acceptance.md).
No global PATH/security setting, upstream source, runtime ABI or target SDK was
changed. PAL rootfs mapping and environment filtering are not a security sandbox.

Replay the new bundle/adapter on **native Windows/x64 and macOS/arm64** before
closing Tranche 2/D1. Verify actual private DLL/dylib initial-load resolution,
aliases, Unicode paths and real Windows directory links/owned-child cleanup.
Earlier SDK tool probes remain historical and do not qualify this independent
artifact. Interactive terminal/job control, source-port/compiler adapters,
cross-target execution, official trust/metadata policy and public release are
separate work; no new production consumer lock is adopted here.
