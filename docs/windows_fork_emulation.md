# Windows Fork Emulation: Current Implementation

> For the full chronological investigation (spawn broker design/
> implementation/retirement, benchmarks, prior-art research, reverted
> attempts, and the exact bugs found while building each piece below), see
> [`bringup/windows_fork_emulation_history.md`](bringup/windows_fork_emulation_history.md).

## Summary

Windows has no native `fork()`. Both architectures this project supports
use the same design -- Cygwin/MSYS-style memory-copy `fork()` -- selected
in `libc/src/arch/windows/common/syscall.c`'s `__crt_sys_fork()`:

- **aarch64**: `libc/src/arch/windows/aarch64/fork_memcopy.c`,
  `__crt_windows_memcopy_fork()`. Verified on real Windows aarch64
  hardware.
- **x86_64**: `libc/src/arch/windows/x86_64/fork_memcopy.c`, same function
  name and design, ported from the aarch64 version. It was first verified
  through the Windows aarch64 development machine's built-in x64 emulation
  (Prism/xtajit), and has since passed the formal fork/runtime-reset suite and
  real parallel port builds on a separate native Windows x86_64 machine.

In both cases the child is a normal, fully `CreateProcessA`-registered
process, so it has none of the restrictions a raw NT-level clone (the
`RtlCloneUserProcess` approach this project used before -- see history
doc) would have: `CreateProcessA`/`CreatePipe`/write-mode `CreateFileA`
all work from inside a forked child, so fork-then-spawn (real POSIX
subshells running external commands) works, not just "fork, then only do
direct syscalls, then `_exit()`".

Both backends share the same fd-table snapshot/bootstrap mechanism
(`private/crt_fd_table.h`, described below), used by `posix_spawn()`/
`execve()` on Windows to hand a spawned child's fd table across the
process boundary.

## Windows Memory-Copy `fork()`

`__crt_windows_memcopy_fork()` (`libc/src/arch/windows/{aarch64,x86_64}/
fork_memcopy.c` -- two independent files, same design, most of their logic
identical) implements `fork()` in the Cygwin/MSYS style: spawn a fresh,
normal process and manually reproduce the parent's live state in it,
rather than cloning the address space at the OS level.

Steps, in the order they actually happen:

1. `setjmp()` at the top of `__crt_windows_memcopy_fork()` captures the
   calling thread's callee-saved registers, stack pointer, and return
   address (aarch64: `X19`-`X28`, `Fp`/`X29`, `Sp`, `Lr`, and the low 64
   bits only of `D8`-`D15` per AAPCS64; x86_64: `Rbx`/`Rbp`/`Rdi`/`Rsi`/
   `R12`-`R15`, `Rsp`, the return address, and the *full* 128 bits of
   `Xmm6`-`Xmm15`, which the Windows x64 ABI preserves whole).
2. Self-relaunch via `CreateProcessA(..., CREATE_SUSPENDED, ...)` with the
   `PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY` attribute set
   (`PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF |
   _HIGH_ENTROPY_ASLR_ALWAYS_OFF`). This is what makes the child's
   heap/stack addresses land at the *same* virtual addresses the calling
   process itself is using -- verified empirically on aarch64 (42/42
   matching addresses across independent launches; see history doc,
   "Phase B"), then on x86_64 under emulation and on a separate native
   Windows x86_64 machine. The child never runs
   `mainCRTStartup`; its first instruction is whatever the parent points
   `CONTEXT.Pc`/`CONTEXT.Rip` at in step 6.
3. `WriteProcessMemory` copies three categories of memory into the child
   at identical addresses:
   - the `malloc.c` heap, via a dedicated OS-region tracking table
     (`__crt_malloc_os_region_count()`/`_base()`/`_size()` in `malloc.c`)
     -- *not* a walk of malloc's own `block_header` split chain, which
     subdivides one 64KB `mmap()` region into several non-64KB-aligned
     sub-blocks that `VirtualAllocEx()` rejects;
   - the image's own writable PE sections (`.data`/`.bss`, found via
     manual PE header parsing) -- these are already committed by the
     loader, so this is a plain `WriteProcessMemory` with no
     `VirtualAllocEx()`;
   - the calling thread's stack, commit-only (`MEM_COMMIT`, no
     `MEM_RESERVE`) since the loader already reserves the full stack
     region at thread creation -- adding `MEM_RESERVE` on top of an
     existing reservation fails with `ERROR_INVALID_ADDRESS`. The commit
     target is clamped to the *child's own* stack reservation
     (`VirtualQueryEx()` on the child's initial `Sp`/`Rsp`), not the
     parent's `TEB.StackLimit` -- the child's fresh thread has not run any
     code yet, so its own `TEB.StackLimit` is nowhere near where the
     parent's has receded to from real call-stack depth.
4. The current thread's `crt_thread_context` block (`tls.c`) is copied
   explicitly -- a standalone `VirtualAlloc()`, not covered by any of the
   three regions above.
5. The copied `thread_tls_index` (part of the `.data` copy in step 3) is
   patched back to `CRT_TLS_OUT_OF_INDEXES` in the child via a targeted
   `WriteProcessMemory()`, so the existing post-fork hook chain
   (`__crt_atfork_child()` -> `__crt_thread_after_fork_child()` -> `tls.c`'s
   `windows_tls_index()`) allocates a fresh, legitimate TLS index there
   instead of reusing the parent's -- a copied index number is only valid
   in the *parent's* Win32 TLS bitmap.
6. The child's initial `CONTEXT` is constructed *directly* from the values
   `setjmp()` captured in step 1 via `SetThreadContext()`, then
   `ResumeThread()`. The child resumes exactly as if `setjmp()` had
   returned nonzero at the original call site -- no trampoline function,
   no `longjmp()` call in the child, no reading of resume state back out
   of copied memory (an earlier version did exactly that and it was
   unreliable; see history doc for why).

TEB access -- needed only for step 3's stack-bounds lookup
(`copy_current_stack()`) -- is the one piece of this that genuinely has no
shared implementation between the two architectures: aarch64 reads it via
the ARM64-specific X18 platform register (requiring `-ffixed-x18`
globally, applied in the top-level `CMakeLists.txt`); x86_64 reads it via
the GS segment directly (`%gs:0x30`, no register reservation needed
anywhere else in the build). `NT_TIB`'s `StackBase`/`StackLimit` fields
sit at the same `+0x08`/`+0x10` offsets on both, so everything past the
TEB pointer itself is identical code.

The `CONTEXT_AMD64` struct transcription in the x86_64 file (`P1Home`
through `LastExceptionFromRip`, `XSAVE_FORMAT`/`M128A` included) was
cross-checked field-for-field against a real Windows SDK `winnt.h` rather
than from memory, and independently verified via a standalone
`offsetof()` probe (matched exactly, `sizeof(CONTEXT) == 1232` as
expected) before ever being wired into the real build.

## Startup Self-Relaunch (fork()-Capable Targets Only)

Making memory-copy `fork()` viable requires the *calling* process's own
heap/stack addresses to already be in the deterministic, mitigated range
-- not just the freshly-spawned child's. `libc/src/arch/windows/common/
fork_capable_relaunch.c`'s `__crt_windows_ensure_fork_capable_relaunch()`
handles this (architecture-independent: plain Win32 API calls throughout,
shared unchanged by both aarch64 and x86_64 -- lives in `common/`, not
either arch directory): called once from `mainCRTStartup()`, it checks
`GetProcessMitigationPolicy(GetCurrentProcess(), ProcessASLRPolicy, ...)`,
and if bottom-up ASLR is still enabled, relaunches itself under the same
mitigation-policy attribute used in step 2 above, then waits for the
relaunched child and exits with its status.

This is **opt-in per build target**, not applied to every Windows
process. `crt1.c` calls it through a weak symbol reference
(`void __crt_windows_ensure_fork_capable_relaunch(const char*)
__attribute__((weak))`) that stays a null function pointer -- and is
skipped -- unless a target explicitly links `fork_capable_relaunch.c`.
Today that is only `crt_mksh` (`shell/CMakeLists.txt`) and the ctest suite
(`tests/CMakeLists.txt`, `add_crt_test()`). Toybox and any other leaf
external command do not link it, and get ordinary, non-relaunched
startup: no extra process-launch latency, and -- as important -- none of
the stdio-inheritance regression that applying this unconditionally to
every process caused (see history doc). If a new target needs to call
`fork()` itself, it needs to opt in the same way mksh does.

(Porting pitfall worth recording: when this weak-symbol wiring was first
written, both the declaration and the call site in `crt1.c` were guarded
by `#if defined(__aarch64__) || defined(_M_ARM64)` -- correct at the time,
since only aarch64 had a `fork_memcopy.c` to opt into. Porting x86_64
required updating `tests/CMakeLists.txt`/`shell/CMakeLists.txt`'s own arch
condition and `libc/CMakeLists.txt`'s source list, but *not* this `#if` in
`crt1.c`, was tried first -- compiled and linked cleanly, but the
self-relaunch silently never ran on x86_64 at all: the weak reference
itself didn't even exist in that translation unit, so the "call it if
non-null" check was compiled out entirely rather than evaluating to null.
`fork_test` then failed with a `stack commit failed` error whose addresses
gave it away -- the parent's own stack address was different on every
run, exactly what unmitigated ASLR looks like. Fixed by updating this
`#if` too.)

The relaunch's own `CreateProcessA()` call hands the current process's
*entire* fd table across to the relaunched child, not just the 3
standard handles: `crt1.c` deliberately runs `__crt_child_bootstrap()`
(which imports whatever fd table an incoming `posix_spawn()` handed this
process) *before* the relaunch check, and the relaunch itself reuses
`__crt_sys_posix_spawn()`'s own duplicate-into-child + suspended-child-
and-pipe transport (`__crt_windows_fd_snapshot_relaunch_begin()`/
`_finish()`/`_abort()` in `syscall.c`, declared in `private/
crt_fd_table.h`) to export and hand off a fresh snapshot targeting the
relaunched child specifically, rather than trusting bare Windows handle
inheritance (which cannot reach anything past the 3 standard handles, and
cannot carry sockets across a process boundary at all).

## fd Table Snapshot / Bootstrap (Shared By Both fork() Backends And `posix_spawn()`)

The Windows PAL owns the descriptor table in
`libc/src/arch/windows/common/syscall.c`:

- `fd_table[64]` stores host `HANDLE` values or Winsock socket handles.
- `fd_kind[64]` distinguishes empty slots, file handles, and sockets.
- `fd_flags[64]` tracks `FD_CLOEXEC` and `O_APPEND` for descriptor
  inheritance across a spawn.
- descriptors 0, 1, and 2 are initialized from `GetStdHandle()`.
- `open()`, `pipe()`, and `dup()` allocate project-owned fd slots.
- `close()` closes the host handle and clears the fd slot.

`private/crt_fd_table.h` defines the private in-memory snapshot ABI used
to hand a process's fd table across to a spawned child (since Windows
handle inheritance alone is not enough for the CRT's own fd-table
bookkeeping): magic/version/capacity header, one entry per fd (number,
kind, flags, host handle value), `CRT_FD_SNAPSHOT_FLAG_INHERITABLE` for
directly-inheritable handles, and `CRT_FD_SNAPSHOT_FLAG_REMOTE_PROCESS_
HANDLE` for handles already duplicated into the concrete child process.

`__crt_fd_snapshot_export()`/`_import()`/`_encode()`/`_decode()` implement
export/import and the text transport format. `posix_spawn()` injects the
encoded snapshot as `CRT_FD_SNAPSHOT` in the child's environment block
under the `CRT_CHILD_BOOTSTRAP=1` contract; when the snapshot contains
socket fds, it instead creates the child suspended with an inherited
bootstrap pipe, calls `WSADuplicateSocketA()` once the child's real PID is
known, writes the updated snapshot through the pipe, and resumes the
child. `__crt_child_bootstrap()`, called from `mainCRTStartup()` before
`main()`, imports whichever form is present and restores cwd/rootfs/
signal-mask bootstrap state.

Windows `execve()` is implemented only in terms of this same bootstrap
contract: it `posix_spawn()`s the target with the current fd table/cwd/
environment, waits, and exits the current process with the child's exit
status. It does not claim Bionic/Linux in-place image replacement
semantics -- there is no way to actually replace a running Windows
process's image, so this is the closest available approximation.

## Windows Pipe Buffer Size

Every `CreatePipe()` call in `syscall.c` (the generic `pipe()` syscall, the
fd-snapshot bootstrap pipe used by `posix_spawn()`, and the fork-capable
self-relaunch's fd handoff above) requests an explicit
`CRT_PIPE_BUFFER_SIZE` (4 MiB) instead of the system default (`nSize=0`,
observed ~4096 bytes on this host). Every one of these pipes has a
synchronous, pre-resume/pre-fork write on one side (the writer runs to
completion before anything is positioned to drain the pipe), so a
default-sized buffer deadlocks the instant that write exceeds it.

This was diagnosed for real via mksh specifically: its
`MKSH_CRT_SHELL_CHILD_SPEC` Windows port (`shell/mksh/src/jobs.c`'s
`exchild()`) deliberately skips a real `fork()` for a pipeline stage that's
a plain `TCOM`, to avoid this platform's expensive memory-copy `fork()`
when the stage turns out to be an external command (which can instead
`posix_spawn()` directly, concurrently with the rest of the pipeline). But
when such a stage resolves to a shell **builtin** instead (e.g. `echo
long-string | sed ...`), nothing ever forks a concurrent process for it --
the builtin's `write()` into the pipe runs synchronously, in-process,
*before* the reader is ever spawned. Binary-searched empirically: writes up
to ~4051 bytes completed, 4101+ hung indefinitely, confirmed via a minimal
reproduction (`echo "$s" | wc -c` for a growing `$s`) run in isolation with
`timeout`. This was the exact cause of the libpng `configure` hang
documented below in earlier revisions of this doc: GNU Autoconf's own
`checking for a sed that does not truncate output` self-test pipes an
~11 KB doubled string built by `echo` into `sed`.

Fixed by enlarging the buffer rather than reworking mksh's job-control/fork
semantics to give every pipeline stage true concurrency -- simpler and
lower-risk, at the cost of not being a fix for arbitrarily large
single-write payloads (a write bigger than the buffer would still
deadlock; 4 MiB comfortably covers realistic shell/configure-script
usage).

## Status: Basic Implementation Concluded

As of this writing, basic verification of the Windows `fork()` mechanism
itself (both architectures) is considered complete. Since it landed, real
third-party ports have exercised it in practice well beyond the `ctest`
suite: zlib, libpng, sqlite-amalgamation, and libffi all `configure`/`make`
through project-owned `mksh` on Windows (fork-then-spawn, subshells,
pipelines), and this project's own shared-library work (see `TODO.md`) has
since run successful builds on a real, separate x86_64 Windows machine, not
just this project's aarch64 development machine's x64 emulation. New
problems found from here on -- shell/porting-related or otherwise -- get
their own fresh `TODO.md` entry rather than reopening this document's
narrative; see "Current Open Issues" below for what remains genuinely
unresolved about the mechanism itself.

## Current Open Issues

The formal fork/runtime-reset suite and real parallel port builds have now run
on Windows x86_64 as well as the original aarch64 bring-up environment. The
earlier x86_64 verification caveat is closed. Remaining mechanism limits are:

- **Only the calling thread's stack survives into the child** (matches
  POSIX `fork()` semantics -- other threads do not survive into the child
  at all -- but pthread-created OS threads' own stacks are not otherwise
  touched or cleaned up in the child beyond the existing
  `__crt_pthread_after_fork_child()` hook).
- **No guard-page preservation**: the stack copy does not preserve a guard
  page beyond what was already committed at `fork()` time, so the child
  cannot auto-grow its stack past that point.
