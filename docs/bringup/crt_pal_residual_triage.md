# Bounded CRT/PAL residual-defect triage

Date: 2026-10-22. Host: native Windows/x64. This record selects residual
defects before SDK Producer Contract Gate P0; it does not reopen Web acceptance.
The active work queue is [TODO](../../TODO.md#in-progress).

## Classification

| Report | Observed result | Next boundary |
| --- | --- | --- |
| Windows virtual random-device dup | Initial failure reproduced; fixed and replayed on Windows/x64 below | Windows fix closed; other native host/architecture replays are not claimed |
| Windows packaged iostream crash | Initial first-cout fault reproduced; fixed and replayed static/shared on Windows/x64 below | Windows fix closed; native macOS binding audit remains separate |
| macOS CRT dylib libc binding | Audited on native macOS/arm64 below: one reproduced defect (in-tree `libcrtgfx_gpu.dylib`) fixed, the rest classified | Fix closed on macOS/arm64; Linux/Windows have no Mach-O binding question |

The original triage observations are not passing regressions; the later
random-device fix has its own evidence below. In particular, the original
random-device probe exits zero after reporting failed operations; its exit
code alone is not a pass criterion.

## Windows inputs and execution boundary

- Source HEAD: `5b18bf5eaf60921ff5f0825f95fe613e47560879`, with documentation
  edits in the working tree; no runtime source was changed for this triage.
- SDK: `out/windows-host-ninja-debug/dist/02-cxx`, manifest format 1,
  windows/x86_64, created `2026-10-20T08:57:04.487962+00:00`.
- Host compiler: LLVM Clang 22.1.8; link imports from Windows SDK
  `10.0.26100.0/um/x64`. No host libc/C++ runtime is substituted.
- Compilation uses the packaged CRT mksh and `tools/crt-cc`/`tools/crt-c++`,
  not Git Bash. C++ selects `CRT_CXX_RUNTIME_LINKAGE=static` or `shared`;
  execution searches the identified SDK's `bin` first for DLLs.
- Each compilation has a 60-second bound; each execution has a 30-second
  bound, with no retry and no visible console window. The marker variant is
  a changed diagnostic, not a retry-until-pass loop.

Artifact SHA-256 values:

```text
manifest.json  1f95c62cf20445da39d1445b929ba238e9a27f60c8a8d833d2f4048f9d2787ca
lib/libc.a     09d9be01329c32a23dae5086508d9103b8b90d237a50492554961ead25f9fb9c
bin/libc.dll   05b740d4fdc29ed25b83045add652df4b4ff3aa5dab1d65afd8ca51f300b5880
lib/libc++.a   71d055845678bcfff601e548c6e6345992f68a28e979756b726e39147877ad10
bin/libc++.dll fa31ae1e0bfa3c270605eb7c71257cb57f72b4db5e74451d5ee5ee42a95fa059
```

The in-tree `lib/libc.a` and `bin/libc.dll`, and the imported runtime install's
`lib/libc++.a` and `bin/libc++.dll`, hash identically to these SDK copies.
That establishes artifact equality, **not** an independent rebuild or proof
that all current sources have been rebuilt. A fresh/rebuilt comparison remains
part of accepting any fix.

The first sandboxed wrapper invocation did not complete and was interrupted.
A separate `mksh -c 'exit 0'` control timed out after five seconds inside the
sandbox but exited zero outside it. The recorded functional results below are
from the unsandboxed bounded run. Do not attribute that environment timeout to
dup, iostream or a runtime regression.

Local, untracked diagnostic sources/driver and raw command/results are under
`out/residual-triage/`: `random_dup.c`, `cstdio_control.cc`,
`iostream_include.cc`, `iostream_use.cc`, `run.py`, `results.json` and
`results-marker.json`. Generated files are not part of the SDK or acceptance
contract; the decisive cases and reproduction interface are retained here.

## Random-device descriptor boundary

The minimal C consumer opens `/dev/urandom` with O_RDONLY, reads one byte,
sets/gets FD_CLOEXEC, and independently invokes:

```c
dup2(fd, fd);
dup(fd);
dup2(fd, 32);
fcntl(fd, F_DUPFD, 32);
fcntl(fd, F_DUPFD_CLOEXEC, 32);
```

All five return -1/errno 9. The original descriptor still reads one byte and
closes successfully afterward. Closing/reading duplicates and checking their
flags cannot yet be exercised because no duplication succeeds. The shell case
is `read -N 1 value < /dev/urandom`; it exits 1 with `can't finish (dup)
redirection` and `Bad file descriptor`.

At the original triage checkpoint, source inspection agreed with this finding:
[Windows PAL](../../libc/src/arch/windows/common/syscall.c) creates URANDOM as
a synthetic descriptor with no native HANDLE, but dup/dup2 first demanded a
valid native HANDLE, even before the dup2 self case. Public F_DUPFD variants
in [fd.c](../../libc/src/fd.c) delegate to dup, so they inherited the failure.
The earlier [RNG fork crash](windows_mksh_tranche10_crash.md) is a separate,
already-fixed parent-address ownership issue.

The selected fix gate was Bionic contract review, before/after regressions for
self-dup, replacement, minimum fd, CLOEXEC handling, closing either descriptor,
repeated read and fd reuse, plus packaged CRT shell acceptance. The following
checkpoint records that bounded fix rather than a general fd-table rewrite.

## Windows random-device dup fix (2026-10-22)

Contract references: Bionic's
[dup implementation](https://android.googlesource.com/platform/bionic/+/f7c40da307be6a9cbe2287306d9b7c25f16bb09b/libc/bionic/dup.cpp),
[dup2_same regression](https://android.googlesource.com/platform/bionic/+/master/tests/unistd_test.cpp)
and [fcntl dispatcher](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/bionic/fcntl.cpp).
Valid dup2 self-dup preserves the fd; an invalid self-dup fails with EBADF.
Distinct duplicates clear FD_CLOEXEC, except F_DUPFD_CLOEXEC; F_DUPFD honors
the minimum requested fd through the existing public wrapper.

The Windows PAL now duplicates URANDOM/ZERO descriptor state without treating
its placeholder as a HANDLE. Dup2 validates the source before replacing the
destination, preserves the self case, and uses the existing kind-aware close
path for occupied destinations in both virtual and ordinary-file branches.
No public header/symbol or snapshot format changes. Socket-source duplication,
cross-process virtual-fd snapshot transport and shared open-file-description
status-flag storage are not added by this fix.

[virtual_device_dup_test.c](../../libc/tests/virtual_device_dup_test.c) is a
routine, 15-second-bounded CTest on all hosts. It checks random reads without
assuming byte values, zero-fill semantics, descriptor flags, self/invalid dup,
both F_DUPFD forms, minimum fd, independent close lifetime, repeated reuse,
virtual-over-virtual and real/virtual replacement. A replaced pipe writer must
produce EOF at its reader, exposing leaked native handles instead of merely
checking return values. Windows also exercises `/dev/random`; `/etc/random`
is not a supported alias and is not added as part of this fix.

Evidence, source baseline `196fddd` plus this fix, native Windows/x64:

- Fresh configure/build in `out/windows-random-dup`, Debug, imported libc++ OFF,
  Clang 22.1.8, Windows SDK 10.0.28000.0 imports. The new test fails before
  the PAL fix at `dup2(fd, fd)` with errno 9, then passes with the fix.
- `cmake --build out/windows-random-dup --target crt-c-test -j 8`: 160
  executed passes, zero failures and the existing `fork_heap_snapshot_test`
  native-COW skip (161 selected). Full CTest time: 41.58 seconds. This includes
  file/pipe/socket, descriptor snapshots, fork/reset, process-stress and shell
  coverage. Logs: `out/windows-random-dup/Testing/Temporary/LastTest.log`.
- Changed PAL and test sources compile with the strict Windows/aarch64
  freestanding/emulated-TLS/fixed-x18 flags. This is compilation only.
- A new local `01-c` SDK, `out/windows-random-dup/packaged-01-c`, installs the
  fresh runtime/shell with `tools/create_dist.py --component crt-c` and passes
  `tools/verify_dist.py --stage 01-c`. The unchanged Make port and pinned
  02-cxx successor recipe were reused from the existing native build; this is
  not a fresh port build, full isolated-chain replay or published release.
- Rebuild the tracked regression externally with that SDK's `tools/crt-cc`:
  static execution prints its success marker. For DLL coverage compile the C
  object with `crt-cc`, then link it with the packaged `crt-c++` shared-core
  lane (`CRT_CXX_RUNTIME_LINKAGE=shared`, `CRT_CXX_BUILDING_RUNTIME=1` to omit
  C++ libraries). The marker and exit status pass, and PE imports confirm
  `libc.dll`. Merely setting the linkage variable on the Windows C executable
  wrapper does not select shared libc; ad-hoc mixed static/DLL links are not
  accepted as shared-runtime proof.
- Packaged mksh now passes `read -N 1 value < /dev/urandom`, plus explicit
  fd 8 -> fd 9 duplication/read after closing fd 8. Zero-device fd duplication
  and closing also pass (without a NUL-discarding shell read loop).

The bounded local package driver and raw commands/results are
`out/windows-random-dup/run_packaged_probe.py` and `packaged-probe.json`.
All native execution was outside the sandbox, hidden-window, timeout-bounded.
The packaged libc archive/DLL and mksh hashes match the fresh build artifacts:

```text
lib/libc.a           2f922adcad18ba629cfa97c957d186b8c4d2607b7b786407673e57aeaf1dc300
bin/libc.dll         da212614f9ba6cfbd256179ecf0611388e0ee80e1e9acf295559baaa3daeba79
system/bin/mksh.exe  a94f881de8f99d5aff8c2d2dde138565be72493b5bfaa64b19de9cc79cd06a00
```

No Linux/macOS or native Windows/arm64 runtime replay is claimed. The portable
test is wired into existing CTest/CI selection, not a second CI workflow.
Iostream and macOS binding remain open and unchanged.

## Iostream: include-only is not stream-use coverage

The controls are `#include <cstdio>` plus `std::puts`, and `#include <iostream>`
plus a plain `write` main-entry marker. Both compile/run successfully with
static and shared runtime linkage. The decisive consumer is:

```cpp
#include <iostream>
#include <unistd.h>
int main() {
  write(1, "before-cout\n", 12);
  std::cout << "cout-main" << std::endl;
  write(1, "before-cerr\n", 12);
  std::cerr << "cerr-main" << std::endl;
  return std::cout.good() && std::cerr.good() ? 0 : 1;
}
```

At the original checkpoint both linkages print only `before-cout`, then exit 139 with the CRT's
`0xc0000005 (SIGSEGV)` safety-net diagnostic. The cerr step is not reached;
no separate cerr failure is established. Including the header does not prove
that a static archive's stream initializer was pulled in or run.

For reproduction, use the SDK activation environment, compile each snippet
through `tools/crt-c++` with `-std=c++17`, select each linkage explicitly,
and run with that SDK's DLL directory first. A fix must explain stream
initialization, startup/import behavior and the first fault; do not conclude
that DWARF exception handling caused it merely because the safety net reports
it. Add include-only and actual stream-use regressions and re-run the imported
libc++ smoke under the [C++ runtime policy](../design/cxx_runtime.md).

## Windows iostream priority-initialization fix (2026-10-22)

The first-cout fault is an initialization-range defect, not a request for an
explicit application `ios_base::Init` workaround or an upstream libc++ patch.
The pinned libc++ object has ordinary `.ctors` entries and the standard-stream
initializer in `.ctors.65435` (priority 100). The former plain `.ctors` end
marker preceded the priority subsection, so both EXE and DLL startup omitted
it. Include-only did not pull in the stream definitions and hid this failure.

MinGW LLD groups/sorts priority subsections with `.ctors`/`.dtors`; see the
[linker implementation](https://github.com/llvm/llvm-project/blob/main/lld/COFF/Writer.cpp).
Clang encodes the suffix as 65535 minus priority. A `.99999` end marker brackets
the full valid range. GNU constructors must then run in reverse list order
(low priority number first), GNU destructors forward. The independent MSVC
forward-init/reverse-fini ranges remain unchanged. Apply the same policy to
module-local DLL startup, not only the executable walker.

The project-owned `windows_ctor_priority_test` and
`windows_ctor_priority_dll_test` deliberately declare functions out of priority
order and require constructor sequence 123 and destructor sequence 321. Both
failed before the fix with constructor sequence 3; both pass after it,
including an explicit DLL unload/destructor notification. The DLL test supplies
the matching libc DLL directory rather than accepting a loader-path failure
as priority evidence.

The imported-runtime smoke now includes actual streams before main, in main
and in an application destructor, with exact buffered stdout/stderr required
after exit. It also exercises string streams and keeps the existing container,
exception/RTTI and string-ABI smokes. No explicit Init, forced whole-archive or
retry hides the startup gap. The source-stage asset must include the new
`libstdc++/tests/imported_libcxx_iostream_test.cc` alongside its runner.

Counterfactual checkpoint: corrected startup objects make the actual static
stream test pass with the unchanged libc++ archive, while the shared test still
fails with the old libc++ DLL, whose startup objects are embedded in that DLL.
Rebuilding only the application is therefore insufficient for shared runtimes.

Final native Windows/x64 evidence, source baseline `e4cd69c` plus this fix:

- Fresh CMake cache replay in `out/windows-random-dup` selects 163 C-stage
  cases: 162 executed passes, zero failures and the existing native-COW skip,
  46.35 seconds. Log: `out/iostream-fix/core-replay.log`.
- The changed startup code and priority fixture compile for Windows/aarch64
  with freestanding/emulated-TLS/fixed-x18 warnings-as-errors flags. This is
  not a native arm64 runtime result.
- The normal project driver rebuilt libunwind, libc++abi and libc++ in separate
  nested build trees, using `out/iostream-fix/predecessor-01-c`. The install
  prefix was seeded with the prior install's unchanged metadata/headers, then
  all three runtime recipes built/installed against the corrected C-stage
  predecessor. No new upstream patch or runtime substitution was introduced.
- An earlier diagnostic setup incorrectly used a cumulative 02-cxx sysroot;
  duplicate C++ header search paths caused compilation errors. That failed
  setup is not acceptance evidence. The corrected C-only predecessor run is
  retained in `out/iostream-fix/runtime-build-c-predecessor.log`; the earlier
  failure remains in `runtime-build.log` for distinction.
- `out/iostream-fix/packaged-02-cxx` is a separate local package from that
  predecessor and rebuilt runtimes. It passes `verify_dist.py --stage 02-cxx`.
  Existing Make and the 03 successor recipe were reused, not freshly fetched;
  this is not a new full isolated-chain or release acceptance claim.
- `tools/test_libcxx_runtime.py` passes the existing vector/string/exception/
  RTTI and string-ABI checks plus the exact stream lifetime/output regression,
  for static and shared. Repeating the consumer build from
  `out/iostream-fix/external-consumer` uses byte-identical copies of the
  packaged wrappers, only consumer source files and the packaged SDK. Both
  stream linkages pass; the shared PE imports libc++, libc++abi, libunwind and
  libc DLLs. Logs: `final-smoke.log` and `external-smoke.log` in the work root.
- Predecessor/fetch/source-closure tooling passes 19 tests. A new closure
  regression requires all imported-runtime smoke sources in the 02-cxx asset.
  The iostream smoke has bounded execution, exact outputs and hidden Windows
  processes; neither repeated retries nor explicit Init repairs are used.
- Runtime predecessor fingerprints now include startup objects, builtins and
  canonical Windows DLL/import-library bytes, not only libc/libm/libdl in
  `lib/`. A cache regression changes only startup or DLL inputs and proves
  stale nested runtime links are invalidated. Otherwise an ordinary incremental
  build could keep the old libc++ DLL despite the corrected startup source.

Selected SHA-256 values of the validated 02-cxx package:

```text
lib/crt1_ctors_end.o    82665b6fba79c36dd711cc46e539dd2ea412a3b89826e159e74690fbd0a75576
lib/crt1_ctors_walker.o f20f4b7431e70467b83ac1225c8d9deafef4e603122669c301138e2c7d78bbd7
lib/dllcrt.o            bc4a1f892ee24fef72ec302bf2f6ca8863c58647149caee81e6823e7260b14cf
bin/libc++.dll          32a4e9ed14e2e121c39a3cd4a686324af07bd5e99cac1024c1b8bcebcb534ba5
lib/libc++.a            f3e9b13391e1588a980115063f8633882b4b25bc98100140c3d353793f80899b
```

The original native SDK's libc++ DLL still hashes to
`fa31ae1e0bfa3c270605eb7c71257cb57f72b4db5e74451d5ee5ee42a95fa059`;
it was not overwritten. Existing higher-stage binaries are not upgraded by
copying startup objects into a sysroot: affected DLLs/applications need relink
and applicable stage replay. No Linux/macOS, native Windows/arm64 or full Web
SDK replay is claimed. TODO Tranche 2 is closed for its native Windows/x64
scope; Tranche 3's macOS binding evidence remains open.

Producer/consumer review under the
[SDK procedure](../design/sdk_development_contract.md#8-cross-repository-contract-review):
CRT owns the private startup objects and 02-cxx source/smoke changes; public
headers/symbols, manifest format 1, port schema 1 and stage schema 2 are unchanged.
Inspected consumer baselines are CLI `5fcdc995d231fe2f9dbab4292000a81e0b4fce93`
and Studio `27548e1c6d29b5a63d7eb6adce8aaf47b2f5d807`. Neither consumer was
changed or run, no artifact/consumer lock was promoted, and no runnable
combination is approved here. The CLI-owned
[adoption ledger](https://github.com/webos21/crt-cli/blob/5fcdc995d231fe2f9dbab4292000a81e0b4fce93/docs/compatibility.md)
remains the owner of any future adoption; this producer fix is not a new
development-contract version or a CLI/Studio compatibility assertion.

## macOS native binding audit (2026-10-22)

Native macOS/arm64, source baseline `d6c00cf` plus this fix. Method as handed off above, with one addition: the
inventory is a tool, [`tools/audit_macos_bindings.py`](../../tools/audit_macos_bindings.py), that takes every symbol the
SDK's `libc.dylib`/`libm.dylib`/`libdl.dylib` export (1,304 in the stage SDK) and, for every Mach-O image, reads
`nm -m -u` to see which library each undefined reference is bound to. `otool -L` order alone is not the evidence.

Audited SDKs: the isolated `04-gfx-media -> 05-ui -> 06-web` chain built the same day (04: 111 images, 06: 151 images:
49 dylibs and 102 executables, including the Web private closure), and the in-tree developer SDKs
`out/macos-host-ninja-debug/dist/{01-c,02-cxx,03-gfx-simple,04-gfx-media,05-ui}`.

Results:

| Image | Observation | Decision |
| --- | --- | --- |
| In-tree `libcrtgfx_gpu.dylib` | **Reproduced defect.** `libSystem` precedes `libc.dylib` on the link line, so `pthread_mutex_*`/`pthread_cond_*`, `calloc`/`free` and `clock_gettime` bound to Darwin. Darwin's `pthread_mutex_t` is 64 bytes, the CRT's 40 (`pthread_once_t` 16/4, `pthread_rwlock_t` 200/56), and `crtgfx_gpu_fence` is laid out by the CRT headers. A direct shared consumer (`libcrtgfx/tests/gpu_test.c` built with `CRT_CXX_RUNTIME_LINKAGE=shared` against the in-tree 05-ui SDK) trapped in Darwin's `os_unfair_lock_lock_slow`; the same source passes against the isolated-stage SDK, whose link order was already correct. | Fixed: `crtgfx_gpu_shared` puts the CRT libc/libm first (target-local, like crtmedia/crtui/Skia). The consumer passes and the audit is clean. |
| In-tree `libcrtgfx_skia.dylib` | libc first (already) but no libm: 49 math functions bound to Apple's libm. Functionally benign (same ABI, no state). | Fixed with the same libm-first option, so the gate can be strict for dylibs. |
| `libcrtgfx.dylib` (window) | `calloc`/`malloc`/`realloc`/`free`/`memcpy`/`clock_gettime` bound to `libSystem` (libSystem is listed before libc). `window_cocoa.c` deliberately uses the host allocator for the buffers it gives CoreGraphics and frees in its own release callback; no allocation crosses the public API, and `crtgfx_now_ms()` accepts both clock ids. `window_smoke_test` and `synthetic_event_test` pass as shared consumers. | Unchanged; allowlisted per symbol with this reason. Changing it would move CoreGraphics-owned buffers to the CRT allocator. |
| `libc.dylib` | `dlsym` bound to `libSystem`: `pthread.c` and `ifaddrs.c` use `dlsym(RTLD_NEXT)` to reach the real Darwin functions, and libdl depends on libc. | Intended host boundary; allowlisted. |
| `bin/curl` | The one executable that carries its own static libc (own `malloc`) and loads CRT dylibs bound to `libc.dylib` (`libcurl`, `libmbedtls`, `libz`): two allocator/pthread registries in one process. Forty HTTP rounds (300 KB bodies, varying request headers) were clean; no crossing allocation was found. | Not reproduced as a failure; allowlisted as a latent risk, with the reason in the tool. Relinking it against the shared libc is the fix if a failure appears. |
| In-tree `crtgfx_skia_gpu_window_demo` | Static libc; 49 pure math functions and one `dlsym` bound to `libSystem` because the frameworks precede `libm.a`/`libdl.a` on that link line. | Math: a note (same ABI, no state). `dlsym`: allowlisted (Metal path loads no library); the isolated-stage rebuild of the example binds both to the CRT. |
| Everything else | Every CRT-owned reference in the other 140+ images binds to `libc`/`libm`/`libdl`. | Clean. |

Shared-consumer evidence (stage SDK, fully shared libc, no static CRT archive, executables checked to define no
`malloc`): `libcrtmedia` 11 tests (frame, format, player, transport queue, GPU frame, extractor/codec, demux audio/video/
malformed, encode+mux, timing discontinuity), `libcrtgfx_gpu` `gpu_test`, `libcrtgfx` window smoke and synthetic events,
`libcrtui` contract, input, layout, render and surface tests, and a create/use/destroy consumer for the redistributed
`libz`, `libcurl` (including memory handed back with `curl_free`) and mbedTLS; all print their success marker.
`libcrtgfx_skia` is exercised as a shared consumer by the stage's own `crtgfx_skia_raster_smoke`. The `libcrtweb.dylib`
API test needs the WebKit gate environment (host process, fonts) and is not claimed as shared-consumer evidence here;
the audit shows its bindings clean and the 06-web stage ran its own gates. Probe sources are the repository tests
themselves; the driver files are local under `out/macos-binding-audit/`.

The audit is now a gate: `tools/verify_dist.py` runs it on macOS SDKs when executed on macOS (the 04-gfx-media stage and
the in-tree distributions pass it), and `tools/test_audit_macos_bindings.py` covers the decision logic. A CRT image that
binds a CRT libc symbol to `libSystem`, or a new executable that links a static libc next to CRT dylibs, fails
verification unless it is added to the tool's reasoned allowlist.

No Linux, Windows or macOS/x86_64 result is claimed; the question is specific to Mach-O two-level namespaces.
