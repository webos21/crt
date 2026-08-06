# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks the shell/rootfs/porting work queue. The list is ordered by
state: completed work first, current work second, and planned follow-up last.
Detailed policy and provenance stay in `docs/` and import manifests.

## done

- Established `shell/` as a core CRT artifact area, not a third-party port
  recipe.
- Built `crt_tiny_sh`, Android `external/mksh`, and Android `external/toybox`
  through CMake.
- Generated an Android-like rootfs with `/system/bin`, `/bin`, `/usr/bin`,
  `/tmp`, `/dev`, and `/proc/self`.
- Installed mksh and the minimal configure-oriented toybox applet set into the
  rootfs.
- Kept POSIX hosts on symlink aliases and Windows on copy-based `.exe` aliases.
- Added the first Windows shell child process contract:
  - cwd/rootfs/env propagation;
  - fd snapshot export/import;
  - file actions and close-on-exec filtering;
  - child registry integration;
  - `waitpid()` coverage;
  - socket fd transport through `WSADuplicateSocketA()`.
- Documented real Windows `fork()` as a long-term PAL research tranche instead
  of blocking the mksh/toybox milestone on full fork emulation.
- Made Windows rootfs mksh run single external commands, external-command
  pipelines, builtin-to-external pipelines, and basic input/output redirection
  against CRT toybox applets.
- Fixed Windows toybox `ls -al` directory entries that showed `?` metadata for
  `.` and `..`.
- Recorded toybox LP64/LLP64 patches in `shell/toybox/PATCHES.md`; active fixes
  cover `dirtree.extra`, `ls`, the common option parser, Windows applet path
  lookup, and known active pointer-tagging paths.
- Kept zlib aligned with Android's model: zlib is a separate `libz`
  sysroot/runtime library surface, not part of Bionic libc.
- Confirmed AOSP does not carry GNU make under `platform/external`; Android
  carries make source under `toolchain/make` and prebuilts under
  `platform/prebuilts/build-tools`.
- Added `porting/recipes/make.json` and built Android `toolchain/make` as the
  first CRT-owned bootstrap build tool.
- Taught configure recipes to prefer `PORT_PREFIX/bin/make` before falling back
  to host make.
- Unified configure recipe launching through rootfs mksh for Windows, macOS,
  and Linux target flows.
- Made Windows CRT-shell configure recipes run `make -j 1` and pass
  `SHELL=/system/bin/mksh` so recipe commands stay on the project shell/process
  path.
- Completed Windows x86_64 zlib `./configure --static && make && make install`
  through rootfs mksh and CRT-built make.
- Set the zlib recipe to undefine Windows compiler predefines so upstream zlib
  stays on its generic POSIX path rather than selecting the Win32 `<io.h>`
  branch.
- Set zlib `RANLIB=true` because the optional zlib ranlib step is redundant for
  the LLVM archive path and exposed a Windows mksh subshell status quirk.
- Added Bionic/POSIX CRT surface exposed by make/zlib/shell work:
  - `alloca.h`;
  - `ar.h`;
  - `memrchr`;
  - `confstr`;
  - `_CS_PATH` / `_CS_V7_ENV`;
  - `ttyname`;
  - `getlogin`;
  - `eaccess`;
  - `bsd_signal`;
  - `EXIT_SUCCESS` / `EXIT_FAILURE`;
  - `putenv`;
  - `pselect`.
- Added or expanded regression tests for:
  - string memory helpers;
  - `confstr`/sysconf behavior;
  - `pselect`;
  - process signal helpers;
  - Windows fd snapshot and spawn attribute behavior.
- Updated the active status docs:
  - `docs/sysroot_ports.md`;
  - `docs/porting_status.md`;
  - `docs/shell_import.md`;
  - `docs/windows_fork_emulation.md`;
  - `shell/toybox/PATCHES.md`.
- Fixed the `port-rebuild-zlib` `make -j 10` deadlock: `sigaction()`/
  `sigprocmask()` previously only updated process-local bookkeeping with no
  real OS-level signal delivery, so GNU make's jobserver `pselect()` could
  never be interrupted by a real `SIGCHLD`. Added a per-OS
  `crt_signal_backend` (macOS: real `sigaction`/`sigprocmask` via a shared
  Mach-O export-trie helper now also reused by `libdl`; Linux: raw
  `rt_sigaction`/`rt_sigprocmask` syscalls plus an x86_64 restorer
  trampoline; Windows: honest no-op stub) and fixed a separate `pselect()`
  lost-wakeup race (`libc/src/poll.c`) where an already-pending signal was
  silently swallowed by the non-atomic mask-then-select sequence. Verified
  against the real `port-rebuild-zlib` `configure && make -j 10 && make
  install` end to end on macOS. See `docs/signal_delivery.md`.
- Fixed Windows aarch64 compile errors (`init_ntdll`/`fd_set_inherit_for_fork`
  unused-function under `-Werror`): both only backed the x86_64-only
  `RtlCloneUserProcess` fork path and were genuinely dead code on aarch64;
  guarded behind the same `#if defined(__x86_64__) || defined(_M_X64)`
  already used at their call site.
- Fixed 3 Windows aarch64 fork test failures (`fork_test`,
  `fork_signal_test`, `fork_runtime_reset_test`): only one of four
  `fork()`/`_Fork()` call sites treated Windows `ENOTSUP` as an expected,
  graceful pass; extended the same handling to the other three.
- Fixed 3 Windows aarch64 mksh rootfs ctest failures
  (`crt_mksh_rootfs_external_runs`/`_pipeline_runs`/`_command_substitution_runs`):
  root cause was a stale/missing `rootfs` build artifact, not a code bug --
  the `rootfs` CMake custom target had no `ALL` and nothing forced it to
  rebuild before ctest ran. Made `rootfs` part of `ALL` on Windows (the only
  host where any ctest entry depends on it); macOS/Linux keep it opt-in.
- Found and fixed a real mksh/CRT-shell-child-spec bug while investigating a
  separate, silent (`zero output, exit 1`) `port-rebuild-zlib` `./configure`
  failure on Windows aarch64: `MKSH_CRT_SHELL_CHILD_SPEC`'s `exchild()` fast
  path incorrectly ran `TPAREN` (subshells) in-process like `TCOM`, so a
  subshell's own redirection (e.g. `(cmd) 2>/dev/null`) permanently
  clobbered the interpreter's real stderr with nothing to restore it,
  silently swallowing every later error in the same script. Fixed by
  restricting the fast path to `TCOM` only (`shell/mksh/src/jobs.c`) --
  and found a second, independent copy of the same guard inside
  `execute()` itself (`shell/mksh/src/exec.c`), reached directly by
  `comsub()` (backtick/`$(...)` substitution) without ever going through
  `exchild()`, which is why the `jobs.c` fix alone did not change the
  observed behavior; fixed the same way. See
  `docs/windows_fork_emulation.md` for the full diagnosis. This does not make
  `zlib`'s `configure` pass on Windows aarch64 (still needs real `fork()`
  there), but turns the silent corruption into an honest `can't fork - try
  again` failure, and fixes a latent version of the same bug on Windows
  x86_64 (where real fork already exists).
- Made real `fork()` work on Windows aarch64 (`RtlCloneUserProcess` is
  exported there too; the previous x86_64-only guards in
  `libc/src/arch/windows/common/syscall.c` were simply untested
  assumptions) and fixed a genuine `fd_set_inherit_for_fork()` bug found
  along the way (fd 0/1/2 were never marked inheritable, breaking
  `2>&1` inside a forked subshell). This alone didn't make
  `configure`-driven builds work, though: `CreateProcessA()` (and, it
  turned out, `CreatePipe()`) both crash/fail when called from inside an
  unregistered `RtlCloneUserProcess` clone, because the clone never goes
  through CreateProcess's CSRSS registration handshake. Benchmarked the
  cost of routing spawns through `CreateProcessA` instead of raw fork
  (~1.2x, not the order-of-magnitude Cygwin reputation suggests),
  researched prior art (no documented CSRSS re-registration method
  exists; ruled out as too fragile), and built a "spawn broker": `fork()`
  stays untouched (still cheap `RtlCloneUserProcess`), but
  `__crt_sys_posix_spawn()` now detects when it is running inside an
  unregistered clone and, in that case, asks an always-running, never-
  cloned broker process (`libc/src/arch/windows/common/spawn_broker.c`,
  protocol in `libc/include/private/crt_spawn_broker.h`) to create the
  pipe and the real target process on its behalf, handing the resulting
  handles back via `DuplicateHandle`. Verified end to end on real Windows
  aarch64 hardware: `cmake --build --preset windows-host-ninja-debug
  --target port-rebuild-zlib` now completes zlib's full
  `configure && make && make install` with exit 0 (previously failed at
  `can't fork - try again`, then at a `CreateProcessA` crash, then at a
  `CreatePipe()` failure -- each fix exposing the next layer). Full
  `ctest` stays green (77/77) throughout. See
  `docs/windows_fork_emulation.md`, "Chosen Direction: Spawn Broker", for
  the full investigation, the benchmark numbers, the prior-art research,
  and the rejected alternatives (CSRSS re-registration, full Cygwin-style
  memory-copy `fork()`).

## in progressing

- **Retired the spawn broker; moving to a Cygwin/MSYS-style `fork()` instead.**
  The broker (see "done" above) fixed zlib and got libpng most of the way,
  but kept surfacing new structural failure modes of its own this session
  (orphaned `mksh.exe` processes, named-pipe races, I/O timeouts, a
  process-tree-reparenting attempt that regressed the working state and had
  to be reverted -- see the entry below). Decided to isolate it out of the
  active build rather than keep hardening it, and pursue the alternative
  recorded in `docs/windows_fork_emulation.md`'s "Rejected alternatives"
  section instead: a real Cygwin/MSYS-style `fork()` (`CreateProcessA` +
  `WriteProcessMemory` memory copy + `setjmp`/`longjmp` resume), which
  removes the "unregistered clone" problem at its root instead of working
  around it process-by-process.
  - **Phase A (done):** moved `spawn_broker.c`/`crt_spawn_broker.h` into
    `libc/src/arch/windows/legacy_spawn_broker/` (kept, not deleted, but
    excluded from `libc/CMakeLists.txt`'s `CRT_SYSCALL_FILE`); reverted the
    three `__crt_windows_is_unregistered_clone()` branches in
    `__crt_sys_open()`/`__crt_sys_pipe()`/`__crt_sys_posix_spawn()`
    (`libc/src/arch/windows/common/syscall.c`) back to their pre-broker
    direct-`CreateFileA`/`CreatePipe`/`CreateProcessA` form; removed the
    `CRT_SPAWN_BROKER_MODE` dispatch from `crt1.c`. Full `ctest` stays green
    (78/78) -- current test coverage does not exercise fork-then-spawn from
    inside a clone directly, so this is a safe mechanical revert. **Known,
    accepted regression:** until the new `fork()` lands, any real scenario
    that needs a forked clone to spawn a further process (e.g. a subshell
    inside `configure` running the compiler) will fail again the same way it
    did before the broker existed; `libpng`'s build (see below) is blocked
    on this.
  - **Phase B (done):** `/DYNAMICBASE:NO` turned out to be rejected by the
    linker on aarch64 (`lld-link: error: /dynamicbase:no is not compatible
    with arm64` -- ARM64 PE images must always be relocatable, so there is
    no link-time way to disable image ASLR on this architecture at all).
    Verified instead with `STARTUPINFOEXA` +
    `UpdateProcThreadAttribute(..., PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
    ...)` setting `PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF
    | _HIGH_ENTROPY_ASLR_ALWAYS_OFF` at `CreateProcessA` time (a per-process
    creation attribute, not a PE image characteristic): a probe executable
    spawned this way landed its first `malloc()` and a stack-local variable
    at byte-identical addresses across 2 independent top-level launches x 20
    children each (42/42 matching); the image's own code address was
    already deterministic on this system even *without* the mitigation
    policy, so only heap/stack needed it. This is architecture-independent
    in principle (a process-creation attribute, not a link flag), though
    only verified on aarch64 so far. **The Phase C address-matching
    assumption is confirmed feasible on real Windows aarch64 hardware.**
  - **Phase C (done, aarch64):** `__crt_sys_fork()` now dispatches to a new
    `__crt_windows_memcopy_fork()`
    (`libc/src/arch/windows/aarch64/fork_memcopy.c`) on aarch64; x86_64
    keeps the original `RtlCloneUserProcess` path untouched. `ctest` was at
    76/78 immediately after this landed (two known-open gaps in the
    startup self-relaunch's fd inheritance, since fixed -- see the
    "fd-inheritance gap" update below). Several things came up that the
    design write-up above didn't anticipate:
    - `malloc.c`'s `block_header` split chain is not the same thing as the
      underlying OS `mmap()`/`VirtualAlloc()` region boundaries -- a single
      64KB chunk gets subdivided into several non-64KB-aligned sub-blocks
      once anything allocates from it, and `VirtualAllocEx()` requires an
      explicit `lpAddress` to be allocation-granularity-aligned. Fixed by
      adding a separate, dedicated OS-region tracking table in `malloc.c`
      (`__crt_malloc_os_region_count()`/`_base()`/`_size()`, populated only
      in `append_chunk()`), independent of the block-split bookkeeping.
    - The child's `CONTEXT.Pc` was originally redirected to a small
      trampoline function that itself called `longjmp()` on a `jmp_buf`
      copied into the child's memory (matching the original design). This
      reproducibly crashed (`STATUS_ACCESS_VIOLATION`, DEP/execute
      violation, inside `longjmp()`'s own restore sequence) -- a
      `ReadProcessMemory()` readback taken immediately before
      `ResumeThread()` confirmed the copied `jmp_buf` bytes were correct
      at that point, but had become all-zero by the time the child's own
      code tried to read them (bisected with `WaitForDebugEvent()`/
      `ContinueDebugEvent()`, since in-process exception handlers weren't
      reliable here; exact mechanism not fully isolated). Fixed by
      skipping the trampoline/`longjmp()` indirection entirely: the parent
      already holds every register value the trampoline would have read
      from memory, so it writes them straight into the child's `CONTEXT`
      itself via `SetThreadContext()` and never asks the child to read
      resume state back out of memory at all -- a more robust design, not
      just a workaround.
    - Also newly required: fork()-capable processes self-relaunch once at
      startup under the same mitigation policy
      (`libc/src/arch/windows/aarch64/fork_capable_relaunch.c`,
      `__crt_windows_ensure_fork_capable_relaunch()`) -- Phase B only
      verified that *children spawned under the policy* get deterministic
      addresses, not the *original process itself* (ordinary ASLR), so a
      later `fork()` call's own addresses would never have matched a
      mitigated child's without this. This went through two more
      revisions: the "did I already relaunch" check moved from an
      inheritable env var marker (wrong -- survives `execve()` even when
      the new process image is not actually mitigated) to querying
      `GetProcessMitigationPolicy()` directly; and the relaunch itself
      moved from unconditional (every Windows aarch64 process, which broke
      external-command stdio entirely -- see docs) to opt-in per target via
      a weak symbol, linked only into `crt_mksh` and the ctest suite.
    - See `docs/windows_fork_emulation.md`, "Spawn Broker Retired", for the
      full account including known limitations (only the calling thread's
      stack survives into the child, per POSIX; no guard-page preservation
      past what was committed at fork time; x86_64 not yet ported).
    - **Update: fd-inheritance gap across the self-relaunch, fixed.** The
      startup self-relaunch's `CreateProcessA()` hop only forwarded the 3
      standard handles, so any other fd the process itself had received
      (e.g. via `posix_spawn_file_actions_adddup2()`) was silently lost --
      the 2 remaining `ctest` failures (`shell_smoke_test`,
      `windows_fd_snapshot_test`) both exercised exactly this. Fixed by
      reusing the exact mechanism `__crt_sys_posix_spawn()` itself already
      uses for every ordinary spawn (explicit `DuplicateHandle()`-into-
      child + a suspended-child-and-pipe handoff, not bare Windows handle
      inheritance) for the relaunch hop too: three new functions in
      `syscall.c` (`__crt_windows_fd_snapshot_relaunch_begin/_finish/
      _abort()`, declared in `private/crt_fd_table.h`) let
      `fork_capable_relaunch.c` export/duplicate/hand off the current fd
      table without needing its own `fd_table` access. This also required
      reordering `crt1.c` so `__crt_child_bootstrap()` (which imports an
      *incoming* fd snapshot into this process's own fd table) runs
      *before* the relaunch check, not after -- otherwise the relaunch's
      own export would only ever see the default fd 0/1/2 regardless of
      what this process itself had just received. `ctest` is back to
      78/78. As a side effect, this also fixed the previously-unexplained
      `mksh -c "exec mksh -c '...'"` recursive hang (verified stable across
      repeated runs and 3-deep nesting) -- it was the same gap: the
      exec'd-into relaunch was losing the inherited pipe/fd state the
      outer mksh needed.

- Started running libpng's real `configure && make && make install` through
  project-owned mksh/make on Windows aarch64 (`port-rebuild-libpng`, depends
  on zlib already installed to `PORT_PREFIX`). Not passing yet, but found and
  fixed three real, independent CRT/mksh bugs along the way, each verified
  against the full `ctest` suite (78/78 at the time) with no regressions:
  - **`regcomp()`/`regexec()` never implemented capture groups**
    (`libc/src/regex.c`): `\( \)` (BRE) / `( )` (ERE) were either treated as
    literal parenthesis characters to match (BRE) or silently miscounted
    into `re_nsub` (both modes), so `regmatch_t` subexpression bounds were
    never populated. This broke the extremely common autoconf idiom `expr
    "x$opt" : 'x--[^-]*-\(.*\)'` used to parse `--enable-X`/`--disable-X`/
    `--prefix=VALUE` style flags -- `./configure --disable-shared
    --enable-static --prefix=...` was silently corrupted into `--disable-0
    --enable-0 --prefix=0` before libpng's (autoconf-generated, unlike
    zlib's hand-written) `configure` ever got going. Rewrote the matcher to
    track real capture groups through backtracking (see `regex_test.c`),
    and fixed `re_nsub` counting to be BRE/ERE-mode-aware instead of always
    counting unescaped `(`.
  - **mksh never recognized Windows drive-letter paths as absolute**
    (`shell/mksh/src/sh.h`, `mksh_abspath()`): mksh already ships a full
    DOS-path-aware implementation behind `MKSH_DOSPATH`, but that flag also
    switches `PATH`/`CDPATH` to `;`-separated, which conflicts with this
    project's deliberate `:`-separated rootfs `PATH` convention
    (`/system/bin:/bin:/usr/bin`, see `tools/crt-port-build.py`). Added a
    narrower, CRT-owned `MKSH_CRT_WINPATH` define (alongside the existing
    `MKSH_CRT_ALLOW_LLP64`/`MKSH_CRT_SHELL_CHILD_SPEC`) that only patches
    `mksh_abspath`/`mksh_cdirsep`/`mksh_sdirsep` to recognize `X:\`/`X:/`
    and `\` as well as `/`, without touching `MKSH_PATHSEPC`. Without this,
    `cd "$(pwd)"` (autoconf's own `ac_pwd`/`ls -di .` working-directory
    sanity check) silently treated the Windows-native absolute path
    `getcwd()` returns as *relative*, doubling it into `<dir>/<dir>` and
    failing with `configure: error: working directory cannot be
    determined`.
  - **Generic `pipe()` has the same CreateProcessA-adjacent failure as the
    spawn broker's own bootstrap pipe, but was never routed through the
    broker** (`libc/src/arch/windows/common/syscall.c`,
    `spawn_broker.c`/`crt_spawn_broker.h`): `__crt_sys_pipe()` called
    `CreatePipe()` unconditionally, with no check for
    `__crt_windows_is_unregistered_clone()`. mksh forks a real subshell
    (`RtlCloneUserProcess` clone) for every command substitution/pipeline,
    and any further pipe needed *inside* that clone (nested command
    substitution, `cmd1 | cmd2`) hit exactly the already-diagnosed
    `CreatePipe()`-fails-in-an-unregistered-clone bug -- just reached via
    plain shell usage instead of `posix_spawn()`. Extended the broker
    protocol with a `want_plain_pipe` request (broker creates a pipe
    locally and `DuplicateHandle`s *both* ends back into the client,
    instead of attaching one end to a spawned target) and made
    `__crt_sys_pipe()` route through it when inside an unregistered clone.
  - Also found and fixed a fourth real bug along the way, confirmed
    necessary but **not sufficient on its own** -- see "still open" below:
    `ERROR_INVALID_NAME` (Windows error 123, returned for any path
    containing a character Windows never allows in a real filename, e.g.
    `*`) fell through `map_windows_error()`'s default case to `EIO`
    instead of `ENOENT`. Autoconf's own exit-trap cleanup runs `rm -f core
    *.core core.conftest.*`; when the glob doesn't match anything, mksh
    passes the literal pattern through (normal, expected shell behavior),
    and a literal `*` can never exist as a real Windows filename, so
    `ENOENT` is the semantically correct mapping -- and it is exactly what
    toybox's `rm -f` checks for to stay silent
    (`shell/toybox/src/toys/posix/rm.c:110`, `errno == ENOENT`). Fixed in
    `map_windows_error()`. Verified against the full `ctest` suite (78/78).
  - **Broker named-pipe server had two independent races**, found via
    `set -x` tracing and confirmed with per-PID debug logging (removed
    once fixed): (1) the server created the *next* waiting pipe instance
    only *after* servicing the current request, leaving a real window
    (the full service time, e.g. a `CreateProcessA` call, plus
    disconnect/close/recreate) with zero pending instances -- any client
    racing to connect during that window saw `ERROR_FILE_NOT_FOUND`,
    observed as mksh's `can't create pipe - try again` failing
    `checking whether build environment is sane`. Fixed by creating the
    next instance immediately after accepting the current connection,
    before servicing it. (2) `WriteFile()` returning success only means
    the response bytes reached the kernel's pipe buffer, not that the
    client read them; the server's `DisconnectNamedPipe()` tore the
    connection down immediately regardless, intermittently losing the
    response and failing the client's `read_exact()` with
    `ERROR_PIPE_NOT_CONNECTED` (233). Fixed by calling
    `FlushFileBuffers()` (blocks until the client has read everything)
    before disconnecting. Verified with 50+ repeated runs of the exact
    failing sanity-check idiom with no further "can't create pipe"
    failures, and the full `ctest` suite (78/78).
  - **The broker client's `read_exact()`/`write_exact()` had no
    timeout**, discovered while investigating orphaned `mksh.exe`
    processes left running (0% CPU, blocked, no ancestor process left
    alive) after a build had already finished. If the broker ever fails
    to respond for any reason, the mksh subshell that asked it for a
    pipe/spawn blocks in `ReadFile`/`WriteFile` forever -- Windows does
    not cascade-kill it, so it just sits there indefinitely, invisible
    in any log because it never crashes. Rewrote the broker's pipe
    handles (both client and server ends) to use `FILE_FLAG_OVERLAPPED`
    and added a bounded (20s) wait via `WaitForSingleObject` +
    `CancelIoEx` on timeout. This did **not** fully eliminate the
    orphaned-process symptom on its own (see the `TPAREN` bug just
    below, which was the real remaining cause in the cases actually
    investigated) but is an independently correct hardening: no broker
    I/O call can block a caller forever again, regardless of cause.
  - **The real bug behind the remaining orphaned-subshell/exit-127
    symptoms: a subshell reached without `XFORK` already set skipped
    real process isolation entirely** (`shell/mksh/src/exec.c`,
    `execute()`'s top entry check). A `TPAREN` (`(...)`) is supposed to
    always get a real fork via `exchild()`, but the entry check only
    forks when the caller already passed `XFORK` -- and `TLIST`'s
    handling of the *last* item in a sequence (`case TLIST` in the same
    file) passes `flags` straight through unchanged, with no `XFORK`.
    So `{ cmd1; cmd2; (subshell); }` -- a `TLIST` whose last item is a
    `TPAREN` -- reaches the entry check with no `XFORK`, skips it
    entirely, and falls through to the `case TPAREN:` handler, which
    recurses into the subshell's own content with `XFORK` freshly
    added. If that content is a single `TCOM`, the *same* entry check's
    existing `&& t->type != TCOM` term (added for a different, already-
    fixed bug -- see "Found and fixed a real mksh/CRT-shell-child-spec
    bug..." above) then blocks the fork for it too, since nothing there
    distinguishes "a TCOM already isolated by a real fork" from "a TCOM
    that IS the not-yet-forked subshell's own content." Concretely:
    `(exit $ac_status)` as the last statement of a `{ ...; }` group --
    exactly the idiom automake's generated `configure` uses to probe for
    optional tools like `tar` -- ran `exit` in the *interpreter itself*
    instead of a subshell, killing the whole `./configure` script
    instead of just that one probe attempt. This is what was actually
    behind the `checking how to create a ustar tar archive` exit-127
    failure recorded below (the pipe-broker races above are real,
    independently-fixed bugs, but were not sufficient to explain this
    one). Fixed by making the entry check fork on `t->type == TPAREN`
    unconditionally, regardless of whether `XFORK` was already set --
    unlike every other node type, "just run me in this interpreter" is
    never correct for a subshell. Verified with a standalone repro
    (`(exit 99)` as the last statement of a `{ }` group inside a loop,
    ran 3 iterations correctly instead of dying on the first) and the
    full `ctest` suite (78/78); `checking how to create a ustar tar
    archive` now correctly resolves to `none` and configure proceeds
    well past it (through `checking for gcc`) instead of aborting.
  - **Update (post spawn-broker retirement / memory-copy fork()):** the
    `can't create conftest.err: Bad file descriptor` failure this section
    used to describe was a symptom of the spawn broker, which no longer
    exists (see "Phase C" above). With the broker gone and memory-copy
    `fork()` in its place, `configure` now gets *substantially* further --
    all the way past `checking whether the C compiler works... yes` (the
    exact step that used to fail) and through `checking build system
    type`/`checking host system type` -- before hanging (not crashing:
    confirmed genuinely stuck via the CPU-delta technique, unchanged CPU
    across a 10s window) at the very next step, `checking for a sed that
    does not truncate output`. This is autoconf's own self-test that
    builds a `sed` script by repeatedly doubling a fixed pattern string,
    then pipes the whole thing through `sed` to find the length where
    truncation starts -- not yet root-caused. The self-relaunch's
    fd-inheritance gap noted above (now fixed, see "Phase C" update) was
    one candidate explanation and has been ruled out as the sole cause,
    since it's specifically fixed now and this hang has not yet been
    re-tested/re-diagnosed since; remaining candidates are something
    specific to very long single lines being piped through an external
    command from inside a subshell, or something else entirely. Next
    step: retry the libpng configure now that the fd-inheritance fix has
    landed, and if it still hangs, `-x` trace this specific self-test in
    isolation (`CRT_PORT_SHELL_XTRACE=1`, matching earlier sessions'
    approach) to see exactly which command the hang is inside.

- Attempted to fix a real (if currently low-impact) gap in the spawn broker:
  every process it spawns shows up in Windows' own process tree as a child
  of the broker, not of the clone that logically requested it (flat instead
  of nested in Task Manager/Process Explorer/any future toybox `ps
  --forest`; `ps` itself is not enabled yet, `CFG_PS 0`). Tried the official
  `PROC_THREAD_ATTRIBUTE_PARENT_PROCESS` mechanism (same one `explorer.exe`
  uses for UAC-elevated children). Found and fixed one real bug along the
  way (inheritable handles are sourced from the *specified parent's* handle
  table with this attribute, not the actual `CreateProcessA` caller's --
  the fd-snapshot bootstrap pipe's read end had to move from
  "inheritable in the broker" to "duplicated into the client, inheritable
  there"), but hit a second, worse regression that was never fully
  isolated: spawned targets started failing entirely with
  `STATUS_DLL_INIT_FAILED` (a Windows loader-level failure, before the
  target's own `main()` ever runs), and -- the important part -- disabling
  just the reparenting attribute did **not** reliably fix it, meaning
  something in this line of changes broke the plain, non-reparented spawn
  path too, not just the reparented one. Since the actual acceptance test
  (`port-rebuild-zlib`'s real `configure`/`make`/`make install`) regressed
  back to failing, reverted both changed files
  (`libc/src/arch/windows/common/spawn_broker.c` and `.../syscall.c`) via
  `git checkout --` to the last known-good commit rather than ship a
  half-fixed state. Confirmed the revert restores the working state (zlib
  passes, `ctest` 77/77). Full blow-by-blow, what to try differently next
  time, and why job-object inheritance was ruled out as the cause: see
  `docs/windows_fork_emulation.md`, "Attempted And Reverted: Reparenting
  Spawned Processes To The Client".

- Verify the new Linux signal backend (`docs/signal_delivery.md`) on an
  actual Linux host; it is currently code-review-verified only, since this
  project's CMake presets refuse to cross-compile from macOS.
- Add a permanent regression test for the `fork()` + blocked-`SIGCHLD` +
  `pselect()` pattern used to verify the signal delivery fix.

- Keep the Windows mksh child-spec path stable for real configure workloads:
  - external command execution;
  - `cmd | cmd`;
  - builtin-to-external pipelines;
  - `cmd > file`;
  - `cmd < file`;
  - fd 3 and higher redirections;
  - child exit status propagation;
  - multi-child and pipeline teardown.
- Harden Windows `waitpid()` and the child registry for multiple live children,
  configure-script subprocess bursts, and pipeline cleanup.
- Track the current Windows make limitation:
  - serial make is the supported path;
  - parallel make/jobserver fd inheritance is not complete;
  - this should be fixed in CRT/PAL process/fd handling, not by returning to
    host make.
- Audit the Windows mksh subshell status quirk exposed by commands shaped like
  `(command || true) >/dev/null 2>&1`.
- Continue validating that `CRT_SPAWN_NATIVE_WINDOWS=1` remains a narrow
  launcher hint for native host tools such as LLVM `ar`, `ranlib`, and `strip`,
  not an inherited global mode for configure recipes.
- Keep make/zlib/libpng/libffi recipe statuses current as each host is rerun.
- Keep auditing disabled toybox applets for pointer-to-`long` LLP64 assumptions
  before enabling them.
- Keep `/dev/tty`, `/dev/console`, `isatty`, `tcgetattr`, `tcsetattr`, and
  `TIOCGWINSZ` behavior coherent enough for non-interactive shell and configure
  use.
- Preserve the porting loop discipline:
  1. expose the missing header/type/macro/symbol/behavior with upstream source;
  2. check Android Bionic public headers, source, ABI, and errno policy;
  3. extend CRT/PAL/sysroot rather than patching upstream first;
  4. record host-specific policy differences in `docs/`.

## planed

- Finish the libpng/libffi/SQLite configure/make/install pass on Windows
  (see "in progressing" above for the current libpng blocker); libffi and
  the SQLite follow-up build beyond the current amalgamation smoke have not
  been attempted yet.
- Re-run the same make/zlib/libpng/libffi recipe path on macOS and Linux to
  confirm the unified mksh+make flow across hosts.
- Add focused tests for parallel make prerequisites before enabling `make -jN`
  on Windows:
  - inherited pipe fds;
  - jobserver-style pipe transport;
  - concurrent child wait;
  - close-on-exec filtering under load.
- Expand Windows shell smoke tests:
  - fd 3+ redirection inside mksh;
  - grouped commands;
  - background commands where non-interactive semantics are clear;
  - configure-script patterns involving subshells and redirections.
- Decide and document the minimal Windows console process-group policy needed
  for interactive mksh:
  - Ctrl-C / Ctrl-Break delivery;
  - foreground process group approximation;
  - stopped-child status policy.
- Add virtual rootfs files narrowly as porting workloads require them:
  - `/proc/mounts`;
  - `/proc/self/status`;
  - `/proc/self/cmdline`;
  - `/proc/self/environ`;
  - `/proc/stat`;
  - `/dev/zero`;
  - `/dev/random`;
  - `/dev/urandom`.
- Expand toybox applets only when the backing Bionic-compatible CRT/PAL surface
  exists. Likely next applets:
  - `which`: add as a lightweight toybox applet for configure and shell
    usability; mksh has `whence`/`command -v`-style builtins, but `which`
    should be provided as an external applet.
  - `readlink`;
  - `stat`;
  - `touch`.
- Keep deeper Linux-like applets deferred until the PAL owns enough backing
  behavior:
  - `ps`: add through toybox only after the rootfs/PAL provides enough
    `/proc` process data; this is not an mksh builtin.
  - `mount`;
  - `df`;
  - `ifconfig`;
  - `stty`;
  - `login`;
  - device-manager or procfs-heavy commands.
- Continue long-term Windows `fork()` research separately from the immediate
  mksh/toybox milestone:
  - saved register/context state;
  - stack mapping/copy policy;
  - writable runtime/data segment policy;
  - TLS/current-thread reset in the child;
  - malloc/pthread/stdio/fd after-fork reset hooks;
  - ASLR/base-address constraints or documented failure mode.
