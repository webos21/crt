# Bounded CRT/PAL residual-defect triage

Date: 2026-10-22. Host: native Windows/x64. This record selects residual
defects before SDK Producer Contract Gate P0; it does not reopen Web acceptance.
The active work queue is [TODO](../../TODO.md#in-progress).

## Classification

| Report | Observed result | Next boundary |
| --- | --- | --- |
| Windows virtual random-device dup | Reproduced: open/read succeeds; dup, dup2 (including self), F_DUPFD and F_DUPFD_CLOEXEC fail with EBADF; mksh redirection also fails before fork | Fix virtual descriptor duplication with Bionic-compatible flags/lifetime tests |
| Windows packaged iostream crash | Reproduced on first cout use, static and shared, after a main-entry marker; include-only and cstdio controls pass | Trace the first stream-use fault and initialization/import boundary, not an assumed unconditional pre-main failure |
| macOS CRT dylib libc binding | Native evidence pending; source risk and existing local fixes identified | Audit actual Mach-O bindings and shared consumers on macOS before a common link-order change |

These are diagnostic observations, not passing regression tests or completed
fixes. In particular, the random-device probe exits zero after reporting failed
operations; its exit code alone is not a pass criterion.

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

Source inspection agrees with this narrower finding:
[Windows PAL](../../libc/src/arch/windows/common/syscall.c) creates URANDOM as
a synthetic descriptor with no native HANDLE, but dup/dup2 first demand a
valid native HANDLE, even before the dup2 self case. Public F_DUPFD variants
in [fd.c](../../libc/src/fd.c) delegate to dup, so they inherit the failure.
The earlier [RNG fork crash](windows_mksh_tranche10_crash.md) is a separate,
already-fixed parent-address ownership issue.

Before implementation, check Bionic's duplication/descriptor-flag contracts;
then add failure-before/fix-after regressions for self-dup, replacement, minimum
fd, CLOEXEC handling, closing either descriptor, repeated read and fd reuse.
Preserve ordinary file/pipe/socket semantics; check sibling virtual devices
without broadening this into a general fd-table rewrite. Accept through the
packaged CRT shell as well as direct tests.

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

Both linkages print only `before-cout`, then exit 139 with the CRT's
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

## macOS native handoff

The common [shared-runtime helper](../../CMakeLists.txt) still adds System.
[crtui](../../libcrtui/CMakeLists.txt) explicitly puts CRT libc first;
[crtmedia's shared target](../../libcrtmedia/cmake/crtmedia_targets.cmake)
has its own equivalent macOS guard. Preserve both fixes and the
[original shared-consumer evidence](../acceptance/crtui_acceptance.md).
Source order alone does not establish how every remaining dylib binds today.

On native macOS, identify the commit, SDK manifest and dylib hashes, then:

1. Inventory enabled dylibs from the actual SDK (core, graphics/GPU/Skia,
   media, UI and Web); retain missing/disabled variants as such.
2. Capture `otool -L <dylib>` and `nm -m <dylib>`, inspecting actual bindings
   for malloc/free and pthread initialization/use/destruction. A dependency
   list alone cannot prove which library defines a call. Separate intended
   host-boundary imports from CRT-object operations.
3. Run a minimal shared-linked create/use/destroy consumer per applicable
   library, including the existing crtui shared contract and media guards.
   Use CRT headers and the identified SDK, not a static-linked substitute.
4. If a wrong binding remains, reproduce it with a direct shared consumer
   before selecting target-local versus common-helper changes. Preserve the
   [host ABI firewall](../design/host_abi_firewall.md); libc itself still needs
   its documented OS imports, so blindly banning libSystem is not a fix.

No macOS runtime or Mach-O result is claimed by this Windows triage. Leave
that gate open until native evidence and the applicable regression are recorded.
