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

## Additional confirmed defect: directory-search ownership (fixed)

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
must not operate on the parent's opaque native search state.

The fix advances a private epoch in the low-level memory-copy child,
including raw _Fork. DIR records the owning epoch and logical entry count.
On child read it opens new native search state and replays that count;
child rewind starts a new owned search and direct close does not FindClose
the borrowed parent pointer. No copied native heap object is touched.
For an unchanged directory this preserves independent stream position;
it is not a mutation snapshot and cannot reopen a renamed/unavailable
saved path (EIO). Replay costs O(entries already read), once per copied
stream that is actually read. Public DIR remains opaque and unchanged.

`fork_runtime_reset_test` adds Windows opendir/fdopendir cases for fork and
_Fork: resume, rewind without a prior rebind, close without a prior rebind,
copied EOF, and parent position/close after each child. The old dirent
implementation linked into the same test faults; the fixed test passes
100 consecutive repetitions. The minimal old-SDK reproducer recompiled
against the fix exits normally. Full C stage remains 156 passed / one
expected skip. All three changed source files also compile with the
strict Windows/arm64 flags; that is not a native arm64 execution claim.
Evidence: `out/mksh-still-dir-{build,test,repeat,core}.log`,
`out/mksh-still-dir-minimal-after.log`,
`out/mksh-still-dir-regression-before.log`,
`out/mksh-fault-22036-0.dmp` (negative regression).

Bionic's [dirent implementation](https://android.googlesource.com/platform/bionic/+/master/libc/bionic/dirent.cpp)
keeps stream buffering/position around an owned descriptor; the
[POSIX fork contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/fork.html)
permits independent child directory-stream positioning. The Windows fix
must preserve the copied logical position without treating host search
state as an inheritable descriptor. No upstream patch is justified.

## Additional confirmed defect: stale stack extent in a nested fork

After repackaging the first three fixes, a new immutable-input attempt at
`out/tranche10-windows/predecessor05-pal-fixed` / `C:/crtw/t10-pal-fixed`
fails during fresh libwebp configure, before compilation/installation.
The debugger observes 166 created processes and a real PC=0 execute fault.
Full dumps are `out/mksh-fault-6748-{0,1}.dmp`. This is stronger evidence
than the original Application Error record's unknown-module/zero offset,
but does not retrospectively identify every old event's caller.

A corrected, non-breakpoint debugger baseline reproduces the same PC=0
at the same configure point (`out/mksh-still-null-corrected-baseline.log`,
`out/mksh-fault-35436-{0,1}.dmp`). Separate process-local diagnostic
ResumeThread interposition and a one-shot NtContinue breakpoint show:

```
parent TEB.StackLimit = 0x15f000
saved resume SP       = 0x15db90
child return slot     = 0x15ebb8 (below the copied extent)
before ResumeThread   = unreadable, ERROR_PARTIAL_COPY (299)
before NtContinue     = zero; saved PC itself is nonzero and correct
after fork epilogue   = ret reads zero -> execute fault at PC=0
```

The prior memory-copy child has real committed stack pages below its
initial TEB limit, because VirtualAllocEx did not update that TEB field.
A nested fork trusted the stale field and omitted live frames. This is
not a failed native DLL lookup or an upstream configure error. Evidence:
`out/mksh-still-stack-limit-before.log`,
`out/mksh-still-loader-own-config-shell.log`,
`out/mksh-still-null-{stack,memory,startup,resume,frame}.log`.

Both architecture backends now query the saved SP's allocation, then walk
reservation regions from AllocationBase to locate the contiguous committed
readable extent. They copy that extent through StackBase and fail if child
reservation clamping would exclude a live resume frame. This retains
already committed loader headroom, not an arbitrary extra stack margin.
[VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex)
only scans forward from the requested page: using its SP query alone is
insufficient. An intermediate SP-page-only attempt exposed native loader
writes below that page (`out/mksh-still-stack-loader-stack.log`); the
reservation walk is the accepted implementation, not that attempt.

`fork_runtime_reset_test` adds a bounded two-generation fork with a live
16 KiB marker. A short-lived frame first commits headroom, then returns,
so descendants stay inside existing committed memory: automatic guard-page
growth remains a separate known limit. The same final fixture fails when
linked with the old stack implementation, and passes 100 repetitions with
the fix. Core remains 156 passed / one expected skip. Logs:
`out/mksh-still-stack-final-regression-before.log`,
`out/mksh-still-final-stack-{repeat,core}.log`.

The diagnostic debugger was also audited against the official
[debug-event handle lifecycle](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-waitfordebugevent).
It now keeps its own DuplicateHandle reference and leaves event process/thread
handles to ContinueDebugEvent's automatic close. An earlier instrumented
attempt with invalid read/dump handles is discarded
(`out/mksh-still-loader-return-slots.log`); the corrected baseline above
does not patch target code and still faults. Build-host instrumentation is
not linked into product binaries, and early debug passes are not acceptance.

The failed trial's runtime files match their builds, but the crash leaves
one empty `data/local/shpicurb.tmp`, so its complete inventory is no longer
equal to its initial snapshot. Report:
`out/tranche10-windows/pal-fixed-fresh-port.json` (exit 1, unchanged false).
The directory is retired diagnostic state, not a predecessor for future
acceptance; no original `predecessor05` bytes are changed or restored from it.

The first stack-fixed fresh trial (`predecessor05-pal-stack-fixed` /
`t10-pal-stack-fixed`) completes configure in 84.6 seconds and reaches make,
but the diagnostic debugger exhausts its 20,000 cumulative-process slots.
Its generic `STOP fault=1 ... children=20000` line is a capacity stop, not
an observed access violation: there is no FAULT event in that trial's log.
It is incomplete, not a passing build or runtime-crash regression. The
debugger now reuses exited tracking slots and retains only owned live
process handles. The next trial uses a separate execution SDK copied into
its work root, leaving the frozen input untouched even if shell scratch
files remain after interruption. No product code changes for these
diagnostic-capacity/input-isolation corrections.

## Fresh port replay after all four corrections

The final option-ON predecessor is frozen at
`out/tranche10-windows/predecessor05-pal-replay-final`. Its libc.a/libc.dll,
mksh and make hashes match the newly built outputs, including an explicit
make rebuild (not an old install stamp). A separate execution copy at
`C:/crtw/t10-pal-replay-final/sdk` is used with new port work and dependency
prefixes. The pinned libwebp source and recipe are unchanged for the build;
no old object/install tree or host-shell substitution is used.

Configure completes in 87.9 s, make -j4 in 92.3 s and make install in 14.7 s.
Both static and shared round trips report `libwebp_roundtrip_test: ok`
(238-byte lossless result, decoder 10600). The corrected debugger observes
26,782 created processes, root exit 0, no FAULT or DEBUG_INVALID_HANDLE
events, and no capacity/deadline stop. The frozen input's complete
3,353-entry inventory is unchanged. Local evidence:

- `out/mksh-still-final-stack-sdk.log`: final predecessor package validation.
- `out/mksh-still-fresh-libwebp-final.log`: fresh configure/build/install/tests.
- `out/tranche10-windows/pal-replay-final-fresh-port.json`: exit 0,
  input unchanged, runtime-build identity match, separate execution SDK.
- `out/tranche10-windows/predecessor05-pal-replay-final-inventory.json`:
  byte/alias inventory for the new immutable input.
- `out/mksh-still-final-original-input.log`: the original predecessor's
  3,353 entries and v8 source identity remain unchanged; revoked diagnostic
  identities remain revoked, with no accepted Windows 06-web output.

This is the bounded fresh-port runtime checkpoint, not full isolated
Windows 06-web/relocation acceptance or proof of every historical heap
event's cause. No source-stage builder/browser run is left active by it.

## Separate evidence: historical native heap read fault

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

The HANDLE, RNG-cache, DIR-search and nested-stack defects are fixed and regression-tested; the whole crash
incident and Windows Tranche 10 are **not closed**. The new option-ON input
and bounded fresh libwebp replay pass. Next use that immutable predecessor
with a fresh source asset and fresh isolated work root for the full stage.
Do not revive v2/v3/v4/v8's revoked diagnostic identities. A recurrence of
either native heap or unknown-module faults requires native stack/full-memory
capture rather than unbounded retries. Only a successful fresh build and
installed/space-path consumer replay can close the Windows SDK gate in
[`crtweb_acceptance.md`](../acceptance/crtweb_acceptance.md#current-windows-blocker-2026-10-19).
