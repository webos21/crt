# Windows mksh/PAL Tranche 10 crash investigation

Date: 2026-10-19. Host: native Windows/x64. This is a runtime diagnostic,
not Windows `06-web` SDK acceptance. The accepted Linux/macOS slices and
the frozen Windows predecessor remain unchanged.

## Confirmed defect: copied wait HANDLEs are not child-owned

Windows memory-copy fork starts a new `CreateProcessA` process. File
descriptors are deliberately made inheritable, but the parent's private
process-wait HANDLEs are not. Copying the executable's writable sections
nevertheless copies their numeric values in `child_process_table` and
`private_wait_process`.

`__crt_fd_after_fork_child()` used to close those numbers. A number can be
invalid in the new process, or can already refer to an unrelated object
created by the child loader or the earlier signal-state reset. Neither
`GetHandleInformation()` succeeding nor the number being nonzero proves
ownership. The memory-copy child must discard this registry without
closing it. The legacy, unsupported-architecture NT clone path still closes
the handles it really inherits (`windows_unregistered_clone`).

The native debug tree caught `0xc0000008` (`INVALID_HANDLE`) at:

```
ntdll!KiRaiseUserExceptionDispatcher
KERNELBASE!CloseHandle
__crt_fd_after_fork_child             syscall.c
crt_runtime_after_fork_child         process.c
__crt_atfork_child -> fork
mksh exchild -> execute
```

`libc/tests/fork_runtime_reset_test.c` now creates an unreaped sibling,
finds its parent-owned process handle, then uses `_Fork()` to exercise the
private fd-reset hook before `fork()` normally calls it. A test-only
CloseHandle IAT interposer records any attempt to close that parent-owned
number; unrelated closes delegate to the real kernel32 entry. No production
API/test hook is added. This is a bounded, Windows implementation-level
handle-table fixture, not a claim about portable handle numbering.

The same test **fails before the fix** with `child closed a handle it does
not own`, passes after the fix, and passes 100 consecutive repetitions.
The parent can still reap both children. The Windows/arm64 test source
also compiles with the CI-shaped Clang target/flags; native arm64 execution
has not been performed here.

## Additional confirmed defect: parent-only random-device DLL address

A second minimal fixture, independent of the child wait registry, opens
and reads `/dev/urandom` in the parent, forks, and reads the inherited fd.
The old SDK deterministically faults with an **execute** access violation
at `0x7ff9c8391010` from `__crt_sys_read`. The parent's function pointer
was resolved through advapi32's SystemFunction036 forwarder to cryptbase;
the child has neither cryptbase nor that executable mapping. Full-memory
dump and LLDB stack now establish this defect directly:

```
unmapped cryptbase address
__crt_sys_read(fd=3, count=8) -> read -> main
```

The cache now pairs an atomically published function pointer with its
owning PID. Both read of an inherited random-device fd and a new open
re-resolve/pin the native function in the child before calling it. PID
checking deliberately covers `_Fork()`, not just `fork()`'s reset hook.
The low-level fork child also invalidates the owner PID, so recycling an
older ancestor's PID cannot validate a copied stale address.
Failures do not publish an unusable cache entry. Concurrent first users
can resolve the same process-local target without a copied mutex becoming
stuck in the child. This is an internal PAL correction, not a new public
RNG API or a change to the upstream library.

The same minimal fixture recompiled against the fixed runtime passes;
its child loads advapi32/cryptbase/bcryptprimitives itself. The permanent
regression covers inherited fd read and getrandom after both fork and
raw _Fork, with 100 repetitions passing. Relevant new local evidence:

- `out/mksh-still-rng-before.log`, `out/mksh-fault-32968-{0,1}.dmp`
  (full-memory dumps), `out/mksh-still-rng-stack.log`.
- `out/mksh-still-rng-minimal-after.log`, `out/mksh-still-rng-after.log`.
- `out/mksh-still-core-build.log`, `out/mksh-still-core-ctest.log`:
  156 passed and one expected native-COW skip.

The separate historical heap read fault below is **not** explained merely
by this execute-fault proof. A direct native HeapAlloc/HeapFree probe in
eight fork workers / 160 nested children also passes on the original SDK
(`out/mksh-still-native-heap-before.log`), although bad HANDLE closes are
observed. This is counter-evidence against calling that probe a decisive
heap-incident regression.

A direct mksh `read -N 1 value < /dev/urandom` attempt fails in both
versions **before fork**, because virtual random-device descriptor
duplication/redirection returns EBADF. It is not claimed as a shell RNG
crash reproducer or a passing regression. That pre-existing descriptor
gap is a separate follow-up; the C-level inherited-fd fixture above is
the decisive RNG crash reproduction.

## Separate evidence: historical native heap read fault

### Confirmed directory-search ownership crash (fix in progress)

A further minimal old-SDK consumer opens a DIR, reads one entry, forks,
then calls readdir in the child. A full dump now establishes another
native-state ownership defect: FindNextFileA receives the parent's native
search-state pointer, which was not copied as a CRT heap region:

```
ntdll!RtlEnterCriticalSection + 77
KERNELBASE!FindNextFileW -> FindNextFileA
readdir -> main
```

Evidence: `out/mksh-still-dir-before.log`,
`out/mksh-fault-30812-{0,1}.dmp`, `out/mksh-still-dir-stack.log`.
This is a write access violation at `ntdll + 0x1ceed`, not proof of the
same caller as the historical RtlAllocateHeap read fault below. A child
must not operate on the parent's opaque native search state. Rebinding
and position/rewind/close regressions are the next implementation slice;
they are not yet accepted with the HANDLE/RNG changes.

Bionic's [dirent implementation](https://android.googlesource.com/platform/bionic/+/master/libc/bionic/dirent.cpp)
keeps stream buffering/position around an owned descriptor; the
[POSIX fork contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/fork.html)
permits independent child directory-stream positioning. The Windows fix
must preserve the copied logical position without treating host search
state as an inheritable descriptor. No upstream patch is justified.

The original SDK's shell also reproduced the previously logged access
violation at `ntdll + 0x31577`, identified as `RtlAllocateHeap + 0x27`:

```
exception = 0xc0000005 (read)
RCX/RDI   = 0x20000
read      = [RDI + 0x10] = 0x20010
```

The failing PIDs were 5788 and 17616 in the same debug run. Both had
previously reported the bad wait-handle closes. This establishes temporal
correlation, **not proof that the HANDLE defect explains this heap fault
or the unknown-module/zero-offset event class** (a zero reported module
offset does not prove a zero instruction pointer). The first small minidumps do
not contain the faulting stack range/PEB needed for a decisive native
caller analysis. Later bounded old-SDK runs did not reproduce that fault;
changing debugger timing is not evidence of a fix. Do not redesign loader
startup, patch upstream sources, or disable shell tests on this evidence
alone. If it recurs, capture full memory and a native caller stack before
changing the fork bootstrap.

## Reproduction and verification

Deterministic regression (all executables must be relinked against the fix):

```powershell
cmake --build out/windows-host-ninja-debug --target crt-c-build -j 4
ctest --test-dir out/windows-host-ninja-debug -R '^fork_runtime_reset_test_runs$' --repeat until-fail:100 --output-on-failure
ctest --preset windows-host-ninja-debug
```

Windows/x64 results: full C stage **156 passed, one expected native-COW
skip, zero failures**; the eight focused fork/signal/process/mksh cases
also passed. The unchanged original SDK was used for old-code diagnostics;
`rootfs` was rebuilt separately for fixed-code tests. No frozen predecessor
file was replaced.

The local native debugger follows only the process tree it creates,
collects exception context, and terminates only its owned live processes
on a fault/deadline. Trials run serially with a 60-second debugger deadline.
No system-wide crash-dump/registry policy was changed. Local entry points
and evidence (generated `out/` artifacts, not redistributed SDK tools):

- `out/mksh_debug_tree.c` / `.exe`: native build-host debugger;
  `out/mksh_crash_probe.py`: explicit old-SDK/fixed-rootfs selection.
- `out/mksh-pal-regression-before.log`, `...-after.log`,
  `out/mksh-pal-ownership-repeat.log`: failure/pass/repeat evidence.
  `out/mksh-pal-ownership-fixture-flake.log` and
  `out/mksh-pal-ownership-repeat-diagnostic.log` preserve why the initial
  event-collision fixture was replaced: it could not force Windows to
  reuse the desired number. The final interposer detects the wrong close
  whether that number is invalid or already reused, and the same final
  fixture fails against old cleanup with child status `0x5300`.
- `out/mksh-pal-core-build.log`, `out/mksh-pal-core-ctest.log`,
  `out/mksh-pal-focused.log`: relink and core/focused results.
- `out/mksh-debug-parallel100.log`: native heap fault and preceding bad
  handle closes; `out/mksh-fault-5788-{0,1}.dmp`,
  `out/mksh-fault-17616-1.dmp`; `out/mksh-lldb-av.log` and
  `out/mksh-cdb-av.log`: exception analysis. Additional full-memory
  capture attempts did not reproduce this fault; no successful full
  heap-fault dump is claimed.
- `out/mksh-debug-fixed20.log`: eight workers, 20 unmodified upstream
  `install-sh` calls per worker, 1,772 observed processes, normal exits,
  no observed invalid-handle or access-violation exception.
- `out/mksh-debug-fixed-install3.log`: actual retained libwebp `make
  install`, 1,361 observed processes, root exit zero, no observed
  invalid-handle or access-violation exception. This is diagnostic only:
  make/dependencies are the old retained build, not a fresh SDK build.
  Two earlier attempts failed because a plain rootfs lacks SDK make/tools;
  those are fixture-setup failures, not accepted installation runs.

The parallel script shape is eight background subshells, each calling the
original libwebp `install-sh -c -m 644 src/webp/types.h` into its own
temporary destination. Commands run with CRT mksh and CRT applets, not
Git Bash. For the actual install replay, the newly built rootfs uses the
existing CRT make explicitly (including recursive `MAKE`), and its
`/tools` is a diagnostic junction to the retained SDK's CRT tools. This
does not alter upstream configure/install scripts or provide host headers.
The temporary junction is removed after replay, leaving its SDK target
untouched. The frozen predecessor's complete 3,353-entry byte/alias
inventory is reconfirmed unchanged by `out/verify_web10_closeout.py`.

## Open gates

The HANDLE and RNG-cache defects are fixed and regression-tested; the whole crash
incident and Windows Tranche 10 are **not closed**. Next, publish a newly
built, option-ON predecessor into a new immutable path, verify its runtime
identity, then use a fresh source asset and fresh isolated work root.
Do not revive v2/v3/v4/v8's revoked diagnostic identities. A recurrence of
either native heap or unknown-module faults requires native stack/full-memory
capture rather than unbounded retries. Only a successful fresh build and
installed/space-path consumer replay can close the Windows SDK gate in
[`crtweb_acceptance.md`](../acceptance/crtweb_acceptance.md#current-windows-blocker-2026-10-19).
