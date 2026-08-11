# HISTORY: CRT Resolved Work Log

Reverse-chronological record of completed work on the CRT shell/rootfs/porting
loop and PAL, one dated entry per resolved item (grouped by the commit(s) that
landed it). Current/open work lives in `TODO.md`; this file is append-only
history. Dates are the git author date of the commit that introduced or last
substantively updated each entry, so an entry whose investigation spanned
multiple days is dated by its span (`start..resolved`) or by its last
substantive update.

## 2026-08-11

- **Root-caused and fixed the real mksh bug found chasing the
  `sed: bad pattern` errors earlier.** Three rounds of investigation this
  session (the second explicitly re-opened per user request after the
  `crt_toybox` `fork_capable_relaunch.c` change, on the hypothesis that
  it might be related -- ruled out: the `sed: bad pattern` symptom
  already existed before that change, and re-testing after it changed
  nothing; the third round used real interactive `lldb` debugging to go
  from "precise minimal repro" to "confirmed exact buggy line").
  - **Found the missing piece**: libtool's own `func_execute_cmds()`
    (`ltmain.sh`, present verbatim in every generated `libtool` script)
    evaluates each stored `*_cmds` command *twice* -- once implicitly
    when the script itself was first parsed, and again explicitly via
    `eval cmd=\"$cmd\"` inside the function -- not once, as every earlier
    repro attempt this session (including the first sed-gap investigation
    earlier) had assumed. Reproducing *that exact two-eval shape* (not a
    single eval) was the key: a minimal script mimicking it byte-for-byte
    reproduces mksh's real corruption exactly, while a single-eval
    version never did, no matter the input.
  - **Ruled out the `DODBMAGIC`/`XSUBPAT` hypothesis from the first round
    with hard evidence**, not just inspection: added a temporary
    `fprintf` at `eval.c`'s `case XSUB:` (reverted before landing,
    working tree clean afterward), rebuilt `crt_mksh` alone, and ran the
    minimal repro against it directly. `f` was `0x4b` at every relevant
    call -- `DODBMAGIC` (`BIT(15)`, `0x8000`) was never set, and
    `sh.h`'s own comment confirms it's scoped to `[[ x = $y ]]`-style
    test expressions only (`exec.c`'s `dbteste_getopnd()`, its only call
    site) -- nowhere near a plain `eval cmd=\"$cmd\"` assignment. Also
    confirmed via the same debug output that the corruption is already
    present in `x.str` *before* `XSUB`'s own output-emission logic runs
    at all -- meaning the bug is in the **lexer** (tokenizing the
    `eval`'d assignment text), not in expansion/substitution as first
    suspected.
  - **Bisected to a precise, minimal trigger** by systematically reducing
    the real failing sed script down to single characters (each step
    re-tested against the real mksh binary): a `[...]` bracket expression
    appearing *after* a `\(...\)` backslash-group anywhere later in the
    same double-quoted string -- even outside the parens entirely --
    retroactively corrupts that earlier `\(`/`\)` (inserts a stray `/`
    right before it). A bracket expression appearing *before* any
    backslash-group in the same string never triggers it. Minimal
    reproducer (2 lines, no libtool/sed/nm involved at all):
    ```sh
    raw_cmds="'s/x\\\\([^ ]*\\\\)z/\\\\1/'"
    for cmd in $raw_cmds; do eval cmd=\"$cmd\"; echo "$cmd"; done
    # real bash: 's/x\([^ ]*\)z/\1/'      (correct)
    # this mksh: 's/x/\([^ ]*\)z/\1/'     (corrupted: stray '/' before '\(')
    ```
  - **`lex.c:283`'s `CMDASN`/array-subscript theory ruled back out** on
    closer reading: `case SDQUOTE:` (`shell/mksh/src/lex.c`) only special-
    cases the closing `"`, `goto Subst` for everything else, and
    `Subst:`'s own switch has an explicit `default: store_char:` tail
    (line 581) that just emits any unhandled character -- including `[`
    -- as a plain `CHAR` token, with zero special array-subscript
    handling. That code path (`case SBASE:`, `cf & CMDASN`) is simply
    never reached from inside a double-quoted string at all.
  - **Found the real mechanism by instrumenting `debunk()` directly**
    (temporary `fprintf` dumps of its input/output, MAGIC bytes rendered
    as `<M>`; reverted before landing, working tree clean afterward) and
    running it against the real minimal repro. The root cause traces back
    to `func_execute_cmds`'s own `eval cmd=\"$cmd\"` idiom itself: the
    `\"..\"` around `$cmd` are **backslash-escaped literal quote
    characters, not real quoting**, from the *outer* (pre-`eval`) shell's
    perspective -- so `$cmd` is substituted as a genuinely **unquoted**
    reference, subject to the same glob-pattern "magic" marking and
    field handling any bare `$var` would get. Confirmed directly: `[`,
    `]`, and `*` in `$cmd`'s value each get a `MAGIC` sentinel byte
    prefixed (`shell/mksh/src/eval.c`'s `case ORD('['):`/`case
    ORD('*'):` block around line 1109, gated on `f & (DOPAT|DOGLOB)`) --
    exactly the glob-metacharacter-protection mechanism real *quoted*
    text should never go through. The debug dump showed the word being
    `debunk()`-processed in **multiple separate calls** rather than once
    for the whole string -- each individual call correctly strips its own
    `MAGIC` bytes (e.g. `(<M>[^` -> `([^`, `<M>]<M>*\)z/\1/'"` ->
    `]*\)z/\1/'"`), but something in how these separately-processed
    fragments get **reassembled** into the final value is where the
    stray `/` actually enters -- that reassembly code itself is not yet
    located.
  - **Got real interactive debugging working, third round**: `lldb.exe`
    ships with this project's own LLVM install (`C:\Program Files\LLVM\
    bin\lldb.exe`) but crashed instantly (`unable to find 'python311.dll'`)
    -- the LLVM Windows package links `lldb` against Python 3.11
    specifically, but this machine's own Python install is 3.14. Fixed by
    adding the machine's already-installed (separate, from some earlier
    unrelated setup) `...\Programs\Python\Python311\` directory to `PATH`
    before invoking `lldb.exe` -- no reinstall needed. `lldb -b -s
    <command-file>` (batch mode, sourcing a plain-text command script)
    works well for this environment's non-interactive tool-call model;
    `breakpoint command add -o "cmd1" -o "cmd2" ...` (single-line `-o`
    flags) is required over the interactive `breakpoint command add`
    .../`DONE` block form, which silently produces no visible output
    through a sourced command file here. `crt_mksh` already builds with
    full debug info (`-g -gcodeview`, part of this preset's default
    flags) -- breakpoints resolve real source file/line locations
    directly, no extra CMake changes needed.
  - **First obstacle**: `crt_mksh` links `fork_capable_relaunch.c` (see
    that file's own docs) and self-relaunches into a *child* process on
    every invocation until ASLR mitigation is confirmed applied -- so a
    breakpoint set on the parent (the process `lldb` actually launches)
    never fires; all the real work happens in an un-debugged child lldb
    never attaches to. Worked around by temporarily `#if 0`-ing out
    `__crt_windows_ensure_fork_capable_relaunch()`'s body (a same-session,
    fully reverted change -- working tree was clean before and after;
    not a real fix, just a debugging aid, and specifically safe here
    since this repro never calls `fork()`).
  - **Traced the real call chain with a live breakpoint on `debunk()`**:
    it is *not* called directly from `emit_word:` for this repro -- the
    backtrace showed `debunk` <- `globit` <- `glob_str` <- `glob` <-
    `expand` <- `eval`. `f=0x4b` (`DOPAT|DOGLOB`-shaped) confirmed real
    filesystem **pathname globbing** (`glob()`) is being attempted on
    `$cmd`'s value -- consistent with the "not really quoted" theory
    earlier, since real quoted text is never glob-eligible.
  - **Found the literal fragmentation, not just inferred it**: breakpoints
    at `glob()`'s entry (`eval.c:1711`) and its "no real file matched,
    fall back to the literal text" branch (`eval.c:1716`,
    `XPput(*wp, debunk(cp, cp, strlen(cp) + 1))`) showed `glob()` being
    invoked **twice, independently**, for two disjoint halves of what
    should be one continuous string: `cp="cmd=\"'s/x\([^"` (ending mid-
    bracket-expression, right after `[^`) and a *separate* call with
    `cp="]*\)z/\1/'\""` (starting right after, from `]`) -- confirming
    the earlier fragmentation observation was a real word-level split,
    not just an internal `debunk()` buffering detail. The literal space
    character that should be inside `[^ ]` is present in *neither* half
    -- consumed as an ordinary (unquoted, from the lexer's perspective)
    IFS field separator, exactly like any bare `$var` with embedded
    spaces would be split, because the shell has no concept of "this
    space is inside a regex bracket expression" -- that protection is a
    sed/regex-level concept the shell's own byte-level field-splitter
    cannot see.
  - **Traced how the split fragments get reassembled, and ruled out
    where they *don't* get corrupted**: `func_execute_cmds`'s
    `eval cmd=\"$cmd\"` is not really an assignment from mksh's execution
    model's point of view -- it is the `eval` *builtin* (`funcs.c`'s
    `c_eval()`) receiving `cmd=\"$cmd\"` as its own (now field-split-into-
    two) argument list. `c_eval()` feeds its multiple argv words back in
    via a dedicated `SWORDS`/`SWORDSEP` source type
    (`shell/mksh/src/lex.c:1276-1289`) that re-lexes them as one
    continuous stream, inserting `T1space` between consecutive words --
    exactly POSIX's own "join eval's arguments with a space" behavior.
    Suspected `T1space` itself next (`shell/mksh/src/sh.h` has *two*
    conflicting `#define T1space` -- `" "` at line 1140 vs. a
    string-pool-offset `(Treal_sp2 + 5)` at line 974, gated on whether
    `HAVE_STRING_POOLING` survives a `#ifdef __GNUC__`/`#if __GNUC__ < 4`
    chain this project's `-DHAVE_STRING_POOLING=2` build flag interacts
    with in a not-immediately-obvious way) -- **ruled this out too**,
    empirically: a breakpoint right after the `s->str = T1space;`
    assignment (`lex.c:1287`) showed `s->str` was a genuine, correct
    single-space C string (`" "`) at runtime. The rejoin mechanism itself
    is completely correct.
  - **Confirmed the exact buggy line with a live breakpoint at
    `globit()`'s entry** (`shell/mksh/src/eval.c:1745`, using the lldb
    setup earlier): the corruption traces to `globit()`'s recursive
    path-component walk unconditionally re-inserting a **hardcoded
    canonical `/`** as the separator between reconstructed components
    (the old `if (xp > Xstring(*xs, xp)) *xp++ = '/';` at line 1808-1809)
    while a *separate* piece of the same function copies the *original*
    separator byte verbatim just later it (the `while (mksh_cdirsep(*sp))
    *xp++ = *sp++;` loop) -- for `x\([^ ]*\)z...`, `mksh_sdirsep()`
    (`sh.h`, `MKSH_CRT_WINPATH`-gated: `strpbrk(s, "/\\")`, i.e. `\` is
    *also* a recognized path separator, for legitimate Windows-pathname
    support) finds the `\` right after `x` and treats it as a component
    boundary; `globit()` NULs it, recurses, and on the way back down
    writes a **fresh canonical `/`** for "a boundary was here" -- but the
    *original* byte at that boundary was `\`, not `/`, so the
    reconstructed text ends up with the wrong separator character
    substituted in, corrupting a completely ordinary sed backslash-escape
    into `/\(`.
  - **This is a real, portable bug in `globit()` itself, not a
    Windows-only quirk** -- `mksh_sdirsep()`'s non-`MKSH_CRT_WINPATH`
    (POSIX) definition is `strchr(s, '/')`, so vanilla upstream mksh runs
    through the *exact same* hardcoded-`/`-reinsertion code whenever a
    non-pathname string (like this sed script, which contains real `/`
    delimiters) gets routed into `glob()` -- POSIX unquoted-parameter-
    expansion rules do this legitimately for any bare `$var`, which is
    exactly what `\"$var\"` inside `eval` amounts to (the escaped quotes
    are literal data, not real quoting, so `$cmd` is genuinely unquoted
    and glob-eligible -- confirmed in the round earlier). It is invisible
    on POSIX purely because the hardcoded replacement (`/`) always
    happens to equal the original byte (`/`) there -- a silent no-op
    corruption. `MKSH_CRT_WINPATH` recognizing `\` as *also* a separator
    is what turns this from a latent, byte-identical no-op into a visible
    corruption, by making "original separator" and "hardcoded
    replacement" diverge for the first time.
  - **Root fix implemented in `shell/mksh/src/eval.c`**: threaded the
    actual separator byte through `globit()`'s recursion instead of
    hardcoding `/`. Added a `char dirsep` parameter to `globit()` (and its
    forward declaration); `glob_str()`'s top-level call passes `'/'` (a
    harmless default -- `xp` is always empty on that very first call, so
    the "insert a separator" branch can never fire yet); both of
    `globit()`'s two recursive call sites (the non-globbing debunk-and-
    recurse path, and the real `opendir()`/`readdir()` match path) now
    pass `odirsep` -- the exact separator byte that was just consumed
    from `sp` a few lines earlier in the same stack frame, previously
    computed but never threaded any further than that frame's own local
    variable. `*xp++ = '/'` became `*xp++ = dirsep`. On POSIX this is a
    provable no-op (`dirsep` is always `'/'` there, matching the old
    hardcoded value byte-for-byte); on `MKSH_CRT_WINPATH` it now
    reconstructs the *original* separator faithfully instead of silently
    canonicalizing it, so backslash-escape sequences are no longer
    corrupted.
  - **Verified three ways**: (1) the minimal repro now matches real bash
    exactly -- `'s/x\([^ ]*\)z/\1/'` in, byte-identical out, no stray `/`;
    (2) full `ctest` after rebuilding `crt_mksh`: 81/81 passed, no
    regressions; (3) a real `libpng` port build (`tools/crt-port-build.py
    libpng`) produced **zero** `sed: bad pattern` errors anywhere in its
    output, confirming the fix holds under the actual libtool
    `func_execute_cmds` workload that originally surfaced this, not just
    the isolated repro.
  - Was already confirmed harmless for libpng specifically even before
    the fix (the DLL still builds via `__declspec(dllexport)` markers
    regardless of the corrupted export-symbol-list script), but this was
    a real, general mksh correctness bug: `\"$var\"` (escaped-quotes, not
    real quoting) around any glob-metacharacter-containing variable,
    evaluated via `eval`, is exactly the shape GNU Autoconf/Libtool's own
    `func_execute_cmds`/`func_quote_for_eval`-family helpers use
    throughout every generated `configure`/`libtool` script -- so libpng
    was just the first place this session's real build activity happened
    to exercise it with the right ingredients (both a backslash-group and
    a later bracket in the same value). Fixed at the root rather than
    worked around specifically so that a future macOS/Linux mksh build
    (using the same vendored `shell/mksh/src/eval.c`, just without
    `MKSH_CRT_WINPATH` defined) inherits the fix automatically instead of
    carrying the same latent, currently-invisible bug forward.

- **Enabled toybox's `id`/`xargs` applets the same way (same `newtoys.h`/rootfs-alias gap as `which`/`readlink`/`stat` earlier), found and fixed two more real bugs, and used the second one to close out this session's `libpng` cleanliness pass end to end.** Found while reviewing a real libpng `configure` log the user reported as "very clean now": `checking xargs -n works` and `checking whether UID '...' is supported` both fell back ungracefully (`xargs`/`id: inaccessible or not found`) for the identical reason as the earlier applets -- source compiled into `CRT_TOYBOX_SOURCES` but never registered in `newtoys.h` or aliased in `tools/create_rootfs.py`'s `TOYBOX_APPLETS`. Added `USE_ID`/`USE_XARGS` (matching their real upstream `NEWTOY` signatures) and the two rootfs aliases; both `CFG_ID`/`CFG_XARGS` were already enabled in the base Android config.
  - **Found via `id`: `getpwuid(geteuid())` always failed with "bad uid 1".** `libc/src/arch/windows/common/syscall.c`'s `__crt_sys_geteuid()` hardcoded `return 1`, but `libc/src/user_group.c`'s synthetic passwd/group database (the "shell" user this whole PAL's rootfs is built around) only ever recognized uid **0** -- a genuine, long-standing mismatch between two pieces of this project's own code that nothing had exercised until `id`'s `getpwuid()` lookup actually needed them to agree. Fixed by changing `__crt_sys_geteuid()` to return 0, matching the synthetic passwd entry instead of the other way around (no code anywhere depended on the value being specifically 1 -- checked all `tests/*.c` usages, which only assert `geteuid() != (uid_t)-1` and internal self-consistency with `st_uid`, both unaffected by the actual value). This also changes every `stat()`-reported file's `st_uid` from 1 to 0, consistent with `id`'s own new `uid=0(shell)` output.
  - **Found via `xargs`: fork()-crashed every single invocation** (`fork_memcopy: stack commit failed`) -- the exact ASLR-mitigation self-relaunch requirement this session's earlier work (and the original Windows `fork()` effort, `docs/windows_fork_emulation.md`) already established: only binaries that link `fork_capable_relaunch.c` and opt in at startup can call `fork()`/`vfork()` on this PAL. `crt_toybox` (`shell/CMakeLists.txt`) never did -- only `crt_mksh` and the `ctest` suite did -- so *any* toybox applet that forks (today just `xargs`; potentially others later) was silently guaranteed to crash. Fixed by adding the identical conditional `fork_capable_relaunch.c` source to `crt_toybox` that `crt_mksh` already has (same Windows x86_64/aarch64 guard).
  - **Verified end-to-end**: rebuilt the full preset; `id` now prints `uid=0(shell) gid=0(shell) groups=0(shell)`, `xargs` runs real commands correctly (`echo "hello world" | xargs echo prefix:` -> `prefix: hello world`). Full `ctest` 81/81 after each change. Re-ran the real libpng `configure` afterward: `checking xargs -n works... yes`, `checking whether UID '0' is supported by ustar format... yes` -- both probes that used to degrade ungracefully now pass cleanly.
  - **Also surfaced, investigated, and left open**: two `sed: bad pattern` errors during libpng's DLL link step (libtool's own `nm`-output-parsing script), confirmed non-fatal (the DLL still builds via libpng's `__declspec(dllexport)` markers, and every consumer links fine regardless). Root-caused as far as time allowed: an isolated, reproducible test proved this project's own `mksh` over-collapses backslash pairs inside double-quoted strings compared to real bash (`"\\\\("` should keep 2 backslashes per POSIX, this mksh keeps only 1), a genuine mksh double-quote/`eval` bug -- but two separate attempts to intercept the *actual* failing `sed` invocation with a debug argv-logging wrapper both failed to reproduce the exact failure context (the wrapper never got invoked, once because `crt-port-build.py` always rebuilds the `rootfs` target first and silently overwrote the wrapper, once because a nested `mksh ./libtool` sub-process couldn't resolve the wrapper's own `#!/system/bin/mksh` shebang for a still-unexplained reason). Genuinely unresolved which exact code path produces the observed corruption; tracked under "in progressing" later rather than claimed as fixed.
  - **Unrelated, but confirmed while re-running libpng repeatedly this session**: a real, reproducible-once `make install` `Error 5` (Windows raw `ERROR_ACCESS_DENIED`, no error text) on `install-binSCRIPTS`, matching the same unexplained-`Error 5` pattern seen earlier this session on a different target (`install-man5`, aarch64, see the `libpng` shared-pass entry's own notes). Did **not** reproduce on an immediate retry. The user separately applied the Windows Defender process/folder exclusions this session's `README.md` update (later) documents, and the retry after that ran clean -- suggestive, not conclusive (one data point, and several other rebuilds were running concurrently in the background at the time confounding any timing comparison), but consistent with the working theory that these sporadic `Error N`-with-no-message failures are Defender real-time-scan file-handle contention, not a real toybox/CRT bug.

- **Documented Windows Defender build-performance exclusions in `README.md`.** The user ran a real build with process exclusions (`cmake.exe`/`ninja.exe`/`clang.exe`/`clang++.exe`/`clang-cl.exe`/`lld.exe`/`llvm-nm.exe`) and folder exclusions (the LLVM install dir and the whole project tree) applied via `Add-MpPreference` from an elevated PowerShell session. Added the exact commands to the `### Windows 11` prerequisites section, with a one-line rationale (real-time scanning inspects every file a build writes, which is significant given how many small object files, port-build artifacts, and rootfs entries a full build and porting-loop run produce). Separately noted for the record (not applied, the user's own security posture call): the user's machine already had Cloud-delivered protection, Automatic sample submission, and Tamper Protection all off, which combined with these exclusions means (a) process exclusions are broader than folder exclusions -- they skip scanning anything the named binaries touch, anywhere, not just inside the project tree -- and (b) with Tamper Protection off, the exclusion list itself isn't protected from being extended by anything else that gains code execution on the machine.

## 2026-08-10

- **Actually enabled toybox's `which`/`readlink`/`stat` applets (source had been compiled in for a while, but nothing had registered or exercised them as real, runnable commands) and fixed two genuine bugs found doing it.** `shell/CMakeLists.txt`'s `CRT_TOYBOX_SOURCES` already listed `which.c`/`readlink.c`/`stat.c`, but toybox's own applet-dispatch table (`shell/toybox/crt/generated/newtoys.h`, a hand-curated subset of upstream toybox's full catalog -- see that file) never had `USE_WHICH`/`USE_READLINK`/`USE_STAT` entries, and `tools/create_rootfs.py`'s `TOYBOX_APPLETS` rootfs-alias list didn't have them either, so `toybox which ...` genuinely failed with `Unknown command which` and no rootfs `/system/bin/which` existed at all -- confirmed directly before touching anything. Added the three `USE_*(NEWTOY(...))` lines (matching each applet's own upstream `NEWTOY` signature exactly) and the three rootfs aliases; the base Android config these CRT-local files layer on top of (`shell/toybox/src/android/linux/generated/config.h`) already had `CFG_WHICH`/`CFG_READLINK`/`CFG_STAT` enabled, so no config changes were needed there.
  - **Found via `which` itself: `getcwd(NULL, 0)` returned `EINVAL`.** `which.c` calls toybox's own `xgetcwd()` (`shell/toybox/src/lib/xwrap.c`), which is exactly `getcwd(NULL, 0)` -- the real POSIX.1-2008/GNU extension ("allocate a buffer as large as necessary automatically"), which Android Bionic's own `getcwd()` implements too. `libc/src/fd.c`'s `getcwd()` had never implemented this at all, rejecting *any* NULL `buf` outright regardless of `size` as `EINVAL`. Fixed per Bionic's own approach (no grow-and-retry loop like glibc; a single `malloc(size ? size : PATH_MAX)` then one real `getcwd()` call into it): split the existing body into a `getcwd_into()` helper reused by both the caller-supplied-buffer path and a new malloc'd-buffer path. `buf != 0 && size == 0` is still the genuine `EINVAL` case (a real buffer with no usable capacity). Added a regression case to `tests/file_path_test.c` (`getcwd(NULL, 0)` matches the known-good `getcwd(buf, size)` result; `getcwd(buf, 0)` still fails `EINVAL`).
  - **Found via `readlink` itself: silently failed (exit 1, no error text) on every real symlink whose target didn't fit in toybox's own 64-byte starting buffer.** `readlink`'s default mode runs quiet-on-error (upstream toybox behavior, matching real coreutils -- not a bug), which made this look like nothing happened at first. Root cause: `__crt_sys_readlink()` (`libc/src/arch/windows/common/syscall.c`) converted the reparse point's wide-char target directly into the caller's own `size`-byte buffer via `WideCharToMultiByte()` -- which *fails outright* (returns 0, `ERROR_INSUFFICIENT_BUFFER`) when the target doesn't fit, rather than truncating. Real POSIX `readlink(2)` never fails for that reason; it silently truncates to `size` bytes and returns however much it wrote (which can legitimately equal `size`, the caller's own signal to retry bigger) -- exactly the growth-loop toybox's `xreadlinkat()` (`shell/toybox/src/lib/xwrap.c`, starts at 64 bytes and doubles) depends on, and exactly what this project's own raw Linux `readlink(2)` passthrough already provides for free. Since every real absolute path in this rootfs is well over 64 bytes, the very first growth-loop iteration always hit this and gave up immediately. Fixed by converting into an unbounded temporary buffer first (`MAXIMUM_REPARSE_DATA_BUFFER_SIZE` bytes, always enough headroom), then copying/truncating into the caller's real buffer and returning the true (possibly-larger-than-`size`-clamped-to-`size`) length -- matching real `readlink(2)` truncation semantics.
  - **Verified end-to-end, not just compiled**: rebuilt the full `windows-host-ninja-debug` preset; `which ls` now prints `/system/bin/ls`, `stat README.txt` prints real file metadata, and `readlink` on a real symlink (whose target is a long, rootfs-translated absolute path -- confirmed too long for the old 64-byte first attempt) now succeeds instead of silently failing. Added a permanent regression test, `crt_mksh_rootfs_which_stat_readlink_runs` (`shell/CMakeLists.txt`, same Windows-only `crt_mksh_rootfs_*` chain as the existing external/pipeline/redirection/command-substitution/exec-builtin tests), exercising all three applets together through the real rootfs mksh. Full `ctest` 81/81 (79 pre-existing + `pselect_sigchld_test_runs` + this new test).
  - General CRT/PAL fixes, not toybox-specific: any other program calling `getcwd(NULL, 0)` or `readlink()` on a long target on Windows was equally affected.

- **Implemented real `SIGCHLD` delivery for Windows, replacing the honest
  no-op stub.** The user asked for the stub to actually be implemented
  rather than left as documented-but-unfixed. Windows has no kernel
  mechanism for an async child-exit signal the way Linux/macOS do, but does
  have everything needed for a real, synchronously-polled equivalent,
  reusing state that already exists for `waitpid()`: a live child's process
  `HANDLE` (already tracked in `syscall.c`'s child registry) becomes
  kernel-signaled the moment it exits. Added
  `__crt_windows_check_sigchld_pending()` (`libc/src/arch/windows/common/
  syscall.c`): a cheap, non-blocking scan of that registry
  (`WaitForSingleObject(handle, 0)` per live child), gated on `SIGCHLD`
  being currently unblocked, marking each observed exit in a new parallel
  `child_notified_table` so it is reported exactly once (edge-triggered,
  matching real `SIGCHLD`). Two call sites, matching the two points a real
  kernel would actually deliver: `__crt_signal_backend_set_mask()`
  (`signal_backend.c`) -- delivers an already-pending exit synchronously the
  moment `sigprocmask()` unblocks `SIGCHLD`, which is *also* exactly what
  `pselect()`'s existing atomicity check depends on, so `poll.c` itself
  needed zero Windows-specific changes -- and `__crt_sys_poll()`'s own
  1ms-`Sleep()` busy-wait loop (`syscall.c`), covering a child that exits
  while genuinely blocked in `pselect()`/`select()`/`poll()` rather than
  having already exited beforehand. Both call sites deliver synchronously
  on whichever thread is already running, never from a new background
  thread, so no new locking was needed anywhere. Scope: covers `pselect()`/
  `select()`/`poll()` interruption only (the case that actually motivated
  this backend interface, GNU make's `jobserver_acquire()`) -- does not
  cover interrupting a plain blocking `read()`/`write()`/etc, which would
  need a much larger overlapped-I/O rework; every signal other than
  `SIGCHLD` stays exactly as before (pure software bookkeeping). Updated
  `tests/pselect_sigchld_test.c` to drop its Windows special case entirely
  -- it now asserts the same fast-`EINTR` behavior on every host. See
  `docs/signal_delivery.md`'s rewritten "Windows" section for the full
  design writeup.
  - **Verification status: confirmed on a real Windows host.** Rebuilt the
    full `windows-host-ninja-debug` preset (`cmake --build --preset
    windows-host-ninja-debug`) and ran the full suite: `ctest` 80/80 (was
    79 before this test was added), with `pselect_sigchld_test_runs`
    itself completing in **0.23s** -- the same fast-`EINTR`-wakeup path
    already confirmed on Linux (~0.2s) and macOS (0.21s), not the
    5s-bounded-timeout fallback the old no-op stub would have hit. This
    independently confirms the real, polled `SIGCHLD` mechanism actually
    fires on Windows, not just "doesn't crash."
  - **Went further and tested the actual motivating real-world scenario**
    (matching the Linux/macOS verification style earlier): temporarily
    overrode `tools/crt-port-build.py`'s hardcoded `jobs = 1 if
    target_os == "windows" ...` restriction (a local, reverted-immediately
    test patch, not a real change) and reran a real port build
    (`zlib`, `./configure && make -j 8 && make install`) with genuine
    parallel jobs on Windows for the first time. **Found a second, separate
    bug this uncovers**: `make.exe: /system/bin/mksh: Bad file descriptor`
    followed by `make.exe: INTERNAL: Exiting with 1 jobserver tokens
    available; should be 8!` -- GNU Make's own process-spawn failure
    message when creating a *concurrent* recipe shell fails, not a
    `pselect()`/`SIGCHLD` symptom at all (that mechanism is confirmed
    working correctly by the regression test earlier). Points at a race or
    gap in this Windows PAL's own concurrent process-spawn/fd-inheritance
    path (plausibly `child_process_table`/`CRT_FD_TABLE_SIZE` bookkeeping,
    or jobserver-pipe fd duplication, under two-or-more near-simultaneous
    spawns) that has never been exercised before, since every Windows port
    build has always run with `-j 1`. **Not root-caused or fixed this
    session** -- reverted the test patch immediately, rebuilt `zlib`
    normally (`-j 1`) to restore a known-good state, and reran full `ctest`
    (80/80, clean) to confirm no residual corruption from the failed
    parallel attempt. The `jobs = 1 if target_os == "windows"` restriction
    in `tools/crt-port-build.py` **must stay in place** until this new bug
    is separately root-caused -- the `SIGCHLD` fix alone was necessary but
    not sufficient to make real Windows parallel builds safe. See the new
    "in progressing" entry later.

- **Verified the Linux signal backend (`docs/signal_delivery.md`) on a real
  Linux aarch64 host, and added the permanent `fork()` + blocked-`SIGCHLD` +
  `pselect()` regression test.** Both were open "in progressing" items
  blocked on actual Linux hardware being available; this session had one.
  Full `ctest` 74/74 via `cmake --build --preset linux-host-ninja-debug` +
  `ctest --preset linux-host-ninja-debug`. Also ran the real motivating
  scenario end to end: `port-rebuild-zlib`'s `./configure && make -j 4 &&
  make install` (this host has 4 cores, so `crt-port-build.py` picks `-j 4`
  rather than the macOS repro's `-j 10`, same jobserver/`pselect()` path)
  completed cleanly, `ldd` on the resulting `libz.so.1.3.1` resolved
  `libc.so`/`libm.so`/`libdl.so`/`libc++.so` to this project's own sysroot,
  and `examplesh`'s real compress/uncompress round trip passed. Added
  `tests/pselect_sigchld_test.c` (registered as `pselect_sigchld_test_runs`
  in `tests/CMakeLists.txt`, `TIMEOUT 30` outer safety net): installs a real
  `SIGCHLD` handler, blocks `SIGCHLD`, forks a child that exits immediately,
  sleeps briefly so the kernel queues the now-pending signal, then calls
  `pselect()` (unblocking `SIGCHLD`) against a pipe kept deliberately
  unreadable, with a bounded 5s timeout; asserts `-1`/`EINTR` in well under
  2s on Linux/macOS, and the honest bounded-timeout behavior on Windows
  (no-op signal backend, see doc). Verified the test actually exercises the
  fix, not just passes vacuously: temporarily disabled the `pselect()`
  atomicity check in `libc/src/poll.c` and confirmed the test then blocked
  for its full 5s timeout and failed, before reverting. See
  `docs/signal_delivery.md`'s new "Linux Verification"/"Regression Test"
  sections for the full writeup.
  - **Update: confirmed on macOS too.** The user ran the full suite on a
    real macOS machine after pulling this change: `ctest --preset
    macos-host-ninja-debug` -- 74/74 passing, `pselect_sigchld_test_runs`
    itself in 0.21s (the fast `EINTR`-wakeup path, same as Linux), so the
    new test exercises macOS's real `sigaction`/`sigprocmask` backend
    correctly too, not just Linux's.

- **Root-caused and fixed the recurring `libtool: error: Could not determine host file/path name corresponding to ... Continuing, but uninstalled executables may not work.` warnings during every libpng `make`/`make install`, and a real, silent path-corruption bug they were masking.** Traced through the generated `configure`/`libtool` scripts, not guessed: `case $host in *-*-mingw* ) case $build in *-*-mingw*|*-*-windows* ) # actually msys -> lt_cv_to_host_file_cmd=func_convert_file_msys_to_w32`. Autoconf's own authors used "`$build` *also* looks like mingw/windows" purely as a historical proxy for "`configure` is running inside a real MSYS2 shell" -- true for every toolchain they anticipated, but not for this project's own from-scratch `mksh`/toybox PAL, which is a genuinely native `$build` with no MSYS/Cygwin runtime underneath at all. `func_convert_file_msys_to_w32`'s real implementation shells out to a literal `cmd //c echo ...`, relying on real MSYS's own automatic POSIX-argv-to-Windows-path translation (a feature of `msys-2.0.dll`'s `exec()` layer, not of `cmd.exe` itself) -- confirmed directly that this project's rootfs `$PATH` (deliberately scoped to just its own sysroot bin dirs) can never reach a real `cmd.exe` (`mksh.exe: cmd: inaccessible or not found`), so the "conversion" always returns empty, and libtool falls back to its own documented "deliberately simplistic" recovery: a blind `s/:/;/g` on the *original*, already-host-native string -- which corrupts every path this project uses (`C:/Users/...`), since the drive-letter colon isn't a path-list separator the way a real POSIX build's colons would be. Confirmed directly in a real generated wrapper: `LIB_PATH_VALUE` had become `"C;/Users/..."` -- silently invalid, not merely a cosmetic warning.
  - **Fixed the standard, sanctioned way, not a script patch**: `lt_cv_to_host_file_cmd` and its sibling `lt_cv_to_tool_file_cmd` are both ordinary autoconf cache variables (the same `${VAR+y}` idiom already exploited for `$LD`/`$DLLTOOL`/`$OBJDUMP`/`$NM`/`lt_cv_deplibs_check_method` skips detection entirely when pre-set). Preset both to `func_convert_file_noop` -- libtool's own built-in "paths are already in host format, nothing to convert" case, the exact value a real non-mingw/non-cygwin host already gets in that same `case` statement's "otherwise" branch, so this isn't an invented value, just the value libtool itself uses for hosts that were never MSYS to begin with. `to_host_path_cmd` has no cache variable of its own -- it's derived at runtime from `to_host_file_cmd` by libtool's own `func_init_to_host_path_cmd`, so fixing the file variant fixes the path-list variant too. Generalized directly into `tools/crt-port-build.py`'s `make_env()` (alongside `lt_cv_deplibs_check_method`), not left recipe-local, since it's a fixed, permanent fact about this toolchain, true for any Windows configure-based recipe going through GNU Libtool.
  - **Deliberately scoped, verified not to touch DLL/EXE generation**: this `$build`-keyed sub-decision lives entirely *inside* the outer `case $host in *-*-mingw* )` branch that governs `archive_cmds`/import-lib naming/`-DDLL_EXPORT`/shared-library detection (this session's whole earlier libpng/libffi shared-build fix chain) -- the outer branch, keyed only on `$host`, is completely untouched by this fix.
  - **Verified end-to-end**: warning count dropped to 0 across a full rebuild (was ~27); re-inspected a real generated wrapper's `LIB_PATH_VALUE`, now correctly `"C:/Users/.../.libs:"` with the drive letter intact; ran the freshly-built `pnggetset.exe` directly (a wrapper whose `EXE_PATH_VALUE` has 3 colon-joined directory entries, the most exercised multi-entry case) -- all 6 of its PLTE/hIST/tRNS/tEXt/sPLT/unknown-chunks get-then-set roundtrip subtests `PASS`, `rc=0`. Full `ctest` 79/79.
  - Full writeup: `porting/recipes/libpng.json`'s own notes.

- **Fixed the remaining `:`-vs-`;` `PATH`-separator gap the `func_convert_file_noop` fix earlier left behind, and made it a general, reusable piece for any future port.** `LIB_PATH_VALUE`/`EXE_PATH_VALUE` stayed `:`-joined even after that fix (correct drive letters, wrong list separator for the real Windows `PATH` env var these strings eventually feed). None of libtool's own built-in `to_host_path_cmd` implementations do "leave paths alone, just rejoin with `;`" -- the one shape this toolchain actually needs -- and injecting a *new* one would mean hand-patching the generated `libtool` script, which is off the table by this project's own no-upstream-patching discipline. The wrapper's own `lt_update_lib_path()`/`lt_update_exe_path()` (ltmain.sh's generated code) do nothing more than blind string concatenation onto `getenv("PATH")`, so the corruption survives all the way into this project's own `environ` by the time `_spawnv()` is about to actually launch the target program.
  - **Fixed at the one point in this whole chain that's genuinely this project's own code**: `_spawnv()` itself, in `porting/shims/win32/libtool_wrapper_compat.h` (the same shared, `force_include`-driven shim already used for the `_getcwd`/`_stat`/etc. fix). Added `_crt_libtool_wrapper_fix_path_env()`/`_crt_libtool_wrapper_fix_path_seps()`: right before spawning, rewrite `PATH`'s list separators from `:` to `;`, leaving every drive-letter colon untouched. Not a heuristic: Windows filesystem rules forbid `:` from appearing anywhere in a real path except the drive-letter position, so any `:` that isn't in exactly that position is unambiguously a stray list separator.
  - **Verified directly, not inferred**: a standalone test (a fake helper process that prints back its own inherited `PATH`) confirmed a parent's `:`-joined value (`C:/Users/.../lib:C:/Users/.../bin:C:/Users/.../.libs:/system/bin:/bin:/usr/bin`) arrives at the real spawned child correctly rewritten to `C:/Users/.../lib;C:/Users/.../bin;C:/Users/.../.libs;/system/bin;/bin;/usr/bin` -- every drive letter intact, every list separator now real. Re-ran the full libpng rebuild afterward (0 errors, 0 warnings) and both `pngtest.exe` (`libpng passes test`, `rc=0`) and `pnggetset.exe` (all 6 subtests `PASS`, `rc=0`) end to end. Full `ctest` 79/79.
  - **Already generalized, not libpng-specific**: since the fix lives inside `libtool_wrapper_compat.h` itself (the shared shim, opted into per-recipe via the existing `force_include` mechanism in `tools/crt-port-build.py`'s `apply_recipe_env()`), any future Windows configure-based recipe that hits the same `.libs/lt-*.c`-wrapper class of bug gets this fix automatically the moment it adds the same one-line `force_include` entry to its own `target_overrides.windows` -- no new wiring needed, same as the `_getcwd`/`_stat`/`_chmod`/`_putenv`/`_setmode`/`_spawnv` declarations it already provides.
  - Full writeup: `porting/recipes/libpng.json`'s own notes.

- **Root-caused and fixed libpng's Windows shared-library build for real (`static-pass` -> `shared-pass`), instead of the documented-but-abandoned earlier attempt.** Per explicit direction to fix the actual root cause rather than patch around it (a generated-`libtool`-script post-processing shortcut was considered and rejected in favor of this). Four independent, genuine bugs found and fixed in order, each verified against the real `configure && make && make install` flow and the full `ctest` suite (79/79 throughout):
  1. **The real reason libtool always concluded "no shared-library support," even after every other GNU-ld/GNU-C detection probe was already passing**: `tools/crt-cc`'s actual Windows linker backend was never `lld-link.exe` in MSVC-compatible mode, as earlier session notes assumed -- verified directly via `crt-cc -shared -v`, the real invocation is `ld.lld -m i386pep`/`-m arm64pe`, a genuine, capable GNU/MinGW-compatible personality (confirmed: `ld.lld -m i386pep --help` lists real `--enable-auto-import` support). But libtool's own `cygwin*|mingw*` archive_cmds branch gates behind a literal `$LD --help 2>&1 | grep 'auto-import'` probe, and a *bare* `$LD --help` (no `-m` flag) falls back to `ld.lld`'s generic ELF frontend, whose help text lists PowerPC options, not PE ones -- `auto-import` is never there. Fixed at the root in `tools/crt-native-tool`: when the wrapped tool is `ld.lld` (matched case-insensitively, and matching both the real filename and the NTFS 8.3 short-path alias `tools/crt-port-build.py`'s `windows_short_path()` substitutes whenever the real path contains spaces, e.g. `LDLLD~1.EXE` -- an earlier version of this fix matched only the long filename and silently never took effect through the real pipeline, caught by re-testing through the exact same short-path invocation configure actually uses), always inject the right `-m` emulation up front. This made `checking whether ... supports shared libraries`/`checking if libtool supports shared libraries` resolve `yes` for the first time ever in this project.
  2. **New blocker exposed once shared compilation was reached**: `unknown type name 'PNG_DLL_EXPORT'`. libtool always compiles a second, `-DPIC -DDLL_EXPORT` pass for the shared variant; `pngpriv.h`'s own, upstream-documented mechanism (`#ifdef DLL_EXPORT -> PNG_BUILD_DLL -> PNG_IMPEXP=PNG_DLL_EXPORT`) then needed `PNG_DLL_EXPORT`/`PNG_DLL_IMPORT`, which `pngconf.h` only defines inside its own `#if defined(_WIN32) || ... || defined(__CYGWIN__)` block -- exactly what this recipe's CFLAGS deliberately undefine (to keep libpng off the `#include <windows.h>` path this sysroot doesn't have). Fixed via the command-line override `pngpriv.h` itself documents as sanctioned ("the builder of the library may set this on the command line"): added `-DPNG_DLL_EXPORT=__declspec(dllexport) -DPNG_DLL_IMPORT=__declspec(dllimport)` to the recipe's Windows CFLAGS.
  3. **Next blocker**: `fatal error: 'malloc.h' file not found`, in libtool's own auto-generated `.libs/lt-*.c` wrapper sources (used to run an uninstalled test/contrib executable against a not-yet-installed shared library) -- not part of libpng itself, and this sysroot never had a `<malloc.h>` at all. Added `include/malloc.h` following Android Bionic's own convention exactly (`bionic/libc/include/malloc.h`): a thin compatibility header that just re-exports `<stdlib.h>`'s malloc family for source compatibility with pre-POSIX code that expects it from the older header location; deliberately declares no allocator-extension symbols (`malloc_usable_size()`, `memalign()`, ...) since this libc doesn't implement them yet.
  4. **Final blocker**: even with linker-family detection fixed, libtool still refused to link `libpng16` as a DLL against zlib's already-built shared library specifically, silently falling back to static-only with the warning "you do not appear to have [a shared version], ... none of the candidates passed a file format test using a file magic." Root cause: libtool's `deplibs_check_method='file_magic file format (pei*-i386(.*architecture: i386)?|pe-arm-wince|pe-x86-64|pe-aarch64)'` with `file_magic_cmd='$OBJDUMP -f'` never matches this toolchain's real output -- `llvm-objdump -f` reports `file format coff-x86-64` for this project's own real `.so`/`.dll` files, not GNU objdump's `pe-x86-64` the hardcoded regex expects (a genuine LLVM-vs-GNU-binutils naming difference, confirmed by running `llvm-objdump -f` on the real installed `libz.so.1.3.1` directly). Since the regex lives inside the generated `configure`/`libtool` scripts (not reachable via a recipe CFLAGS override), fixed the standard, portable way real cross-toolchain builds already handle exactly this situation: `lt_cv_deplibs_check_method=pass_all` as a recipe env var -- autoconf's `${VAR+set}` cache-variable idiom (confirmed by reading the actual generated `configure`'s own `if test ${lt_cv_deplibs_check_method+y} then :; (cached) ... else ...` logic) honors a pre-set environment variable and skips the whole `case $host_os` detection block entirely, the exact same mechanism `$LD`/`$DLLTOOL`/`$OBJDUMP` already rely on elsewhere in this project.
  - **Verified end-to-end, not just "it built"**: a standalone test program (compiled via `crt-cc`, linked against the installed `libpng16.dll.a` import lib + `libz`) dynamically loaded the real `libpng16-16.dll` and successfully called `png_access_version_number()`/`png_get_header_ver()` (both correctly returned `1.6.57`/`10657`), then completed a real `png_create_write_struct()`/`png_create_info_struct()`/`png_destroy_write_struct()` round trip through the DLL.
  - **One more thing found and worked around along the way, real but deliberately not chased further this session:**
    - `make install`'s libtool-generated `install-header-links`/`install-exec-hook` steps (`rm -f X; ln -s Y X` for each top-level header/lib alias) intermittently failed with `ln: ... File exists` on a rebuild, specifically when `X` was already a valid symlink from a prior successful install in the same session. An isolated, minimal `rm -f <symlink>; ln -s` repro via the exact same rootfs mksh + toybox `rm` succeeded cleanly every time, so this looks like Windows delete-pending/handle-timing noise from repeated same-session rebuilds rather than a general toybox/CRT `rm`-on-symlink bug; worked around this session by fully wiping the port's own install-directory footprint before the final clean rebuild. Worth a closer look if it reproduces from a genuinely cold `out/` directory.
  - Full writeup, including the exact `crt-cc -shared -v` transcript that found the real linker identity and the `ld.lld -m i386pep --help` vs. bare `ld.lld --help` comparison that found the detection gap: `porting/recipes/libpng.json`'s own notes.

- **Fixed a real libpng build-time `libtool: syntax error: unexpected '|'` (an empty `$global_symbol_pipe`, i.e. `nm ... |  | sed ...` with nothing between two pipes).** Root-caused via `config.log`: `NM='nm'`, never resolved past the literal, unusable default -- `checking command to parse ... nm output` had been failing since the very first configure log read this session, just not investigated until now. Unlike `$LD`/`$DLLTOOL`/`$OBJDUMP`, `tools/crt-port-build.py`'s `make_env()` never preset `$NM` to a real tool at all. Fixed the same way as those three: preset `$NM` to `llvm-nm.exe` (via `find_windows_host_tool()`) and added `NM` to the `crt-native-tool` wrapping loop (`AR`/`RANLIB`/`STRIP`/`LD`/`DLLTOOL`/`OBJDUMP`/`NM`). Verified: `checking command to parse ... nm output... ok`; full libpng rebuild's `syntax error` count dropped to 0; `ctest` 79/79. General toolchain fix, applies to every Windows configure-based recipe, not libpng-specific.

- **Fixed the libpng `.libs/lt-*.c` wrapper `_getcwd`/`_stat`/`_chmod`/`_putenv`/`_setmode`/`_spawnv`/`_P_WAIT` undeclared-function errors** (previously listed later under "in progressing" as the `png-fix-itxt` issue -- turned out to affect every uninstalled-execution wrapper libtool generates for this port, not just that one tool, once actually chased down). Root cause: GNU Libtool's own generated wrapper template (`ltmain.sh`'s `func_emit_wrapper`, materialized fresh as `.libs/lt-*.c` per executable -- not part of libpng's own source) has two *independent*, mismatched conditionals: an `#if defined _WIN32 && !defined __GNUC__` that correctly selects `<unistd.h>` once this recipe's CFLAGS undefine `_WIN32` (steering libpng onto the generic POSIX code paths this sysroot actually provides), but a separate `#elif defined __MINGW32__` macro-rename block (`getcwd`->`_getcwd`, `stat`->`_stat`, `chmod`->`_chmod`, `putenv`->`_putenv`, `setmode`->`_setmode`) that fires regardless, since `__MINGW32__` stays defined (needed elsewhere, e.g. the `make` port's own `dir.c` fix) -- so the wrapper renames calls to underscore-prefixed MSVCRT-only spellings that nothing declared, even though the `<unistd.h>` branch was correctly taken.
  - **Fixed with a new project-owned shim**, `porting/shims/win32/libtool_wrapper_compat.h`: real `posix_spawn()`-based `_spawnv()`/`_P_WAIT` (deliberately `posix_spawn()`, not `fork()`+`execv()` -- a first attempt using `fork()` failed with "stack commit failed" in isolated testing, since `fork()` on this project's Windows PAL requires the *calling program itself* to have opted into the ASLR-mitigation self-relaunch dance at startup, which only `crt_mksh`/the `ctest` suite do; `posix_spawn()` is both the semantically correct "run a different program" primitive and needs no such opt-in) and a no-op `_setmode()` (this PAL's I/O is already byte-transparent, no CRLF mode to actually switch), plus `#define _getcwd getcwd` (and `_stat`/`_chmod`/`_putenv`) which safely cancels out against the wrapper's own later `#define getcwd _getcwd`-style macros via the C preprocessor's "blue paint" self-reference rule -- whichever spelling ends up in the wrapper body resolves back to this project's real Bionic-style POSIX name exactly once, regardless of order.
  - **Wired in via a new `force_include` recipe field**, added to `tools/crt-port-build.py`'s `apply_recipe_env()` (mirrors the existing `include_dirs` mechanism, but for `-include <file>` instead of `-I<dir>`) and set on `libpng.json`'s `target_overrides.windows`. First attempt folded the `-include` flag into `CPPFLAGS` (like `include_dirs` does) -- this compiled cleanly for ordinary library sources (`png.c`, `pngerror.c`, ...) but the real rebuild still showed the *exact same* set of undeclared-function errors, since Automake's `LINK` rule (`$(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@ ...`) never references `$(CPPFLAGS)` at all -- and that `LINK` line is precisely where libtool generates *and* compiles a `.libs/lt-*.c` wrapper in one step when linking an executable against an uninstalled shared library, so the flag never reached it. Confirmed directly by diffing the real build log's `pngtest.o` *compile* line (had `-include`, via `CPPFLAGS`) against its `pngtest.exe` *link* line (missing `-include` entirely). Fixed by folding `force_include` into `CFLAGS` instead -- the one accumulator variable Automake's compile and link rules both read.
  - **Verified end-to-end, not just compiled**: the isolated shim was first sanity-checked standalone (a small `shim_check.c` mimicking the real wrapper's exact conditional structure, compiled and run directly). Then, after the `CFLAGS` fix, a full libpng rebuild dropped the undeclared-function error count from 160 to 0, and `pngtest.exe`/`pngcp.exe` (previously entirely absent from the build/install output) both now build and install. Ran the real, freshly-built `pngtest.exe` (the top-level libtool wrapper compiled from `lt-pngtest.c`, exercising the shim's `_spawnv()`/`_getcwd()`/etc. at actual runtime, not just compile time) directly: it printed libpng's full self-test output ending in `libpng passes test`, `rc=0`. Full `ctest` 79/79.

- **Fixed a real, general Windows `lstat()` bug found in libpng's own `configure` output: `rm: conf14228.dir/conf14228.file: Input/output error` (harmless-looking but a genuine PAL defect, not a real autoconf failure).** Root-caused, not just silenced: `__crt_sys_lstat_path()` (`libc/src/arch/windows/common/syscall.c`) used to unconditionally delegate to `__crt_sys_stat_path()` first, which opens via a plain `CreateFileA()` (no `FILE_FLAG_OPEN_REPARSE_POINT`) -- Windows transparently follows a symlink to its target for that call, the exact opposite of what `lstat()` means. For a *dangling* symlink (a completely normal, valid case -- e.g. autoconf's own `ln -s conf$$.file conf$$.dir` "does `ln -s` work" sanity probe, present in essentially every generated `configure` script, deliberately creates one as part of detecting the MSYS `ln -s file dir` gotcha) that follow-through open fails outright since there's nothing at the far end, so `lstat()` itself failed even though the symlink unquestionably exists and `lstat()` is specifically the call that's supposed to work on it regardless of whether the target does. Confirmed directly via an isolated repro (`ln -s conf$$.file conf$$.dir`, then `ls -la`/`rm -f` on the resulting dangling link): `ls -la` showed the entry's own metadata as all `?` (its `lstat()` call was failing too) and `rm -f` reproduced the exact same "Input/output error". Fixed by making `__crt_sys_lstat_path()` check `GetFileAttributesA()` first (which never follows a reparse point on its own) and, only for symlinks, open the link itself via `FILE_FLAG_OPEN_REPARSE_POINT` (the same flag `__crt_sys_readlink()` already uses for the identical reason) instead of the target -- non-symlink paths are unaffected, still delegating to the existing `__crt_sys_stat_path()`. Verified: the isolated repro now succeeds end to end (`ls -la` shows correct symlink metadata, `rm -f`/`rmdir` both exit 0); a real libpng `./configure` re-run no longer emits the error at all; added a regression case to `tests/file_path_test.c` (create a symlink to a nonexistent target, assert `lstat()` succeeds and reports `S_ISLNK`, assert `remove()` succeeds) since no existing test exercised a dangling symlink specifically. Full `ctest` 79/79. General CRT/PAL fix, not port-specific -- any future recipe (or any other program) that ever creates or encounters a dangling symlink on Windows was equally affected.

- **Fixed a real, severe Windows `fork()` correctness bug found chasing an apparent libffi build "hang": `libc/src/malloc.c`'s OS-region tracking table for memory-copy `fork()` silently stopped recording new heap regions once a process's total heap crossed 256MB (`CRT_MALLOC_MAX_OS_REGIONS` was 4096 * the 64KB chunk size), with no error of any kind -- `malloc()` kept succeeding normally, but any region allocated past that point was invisible to `fork_memcopy.c`'s `copy_heap_chunks()`, so it was silently never copied into the child. Root-caused while investigating an apparent 15-hour "hang" rebuilding libffi's Windows shared library (see the libffi entry later): a deeply self-recursive `mksh` interpreting a large generated `libtool` script (`mksh ./libtool --mode=link mksh crt-cc ...`, itself re-invoking `mksh`) is exactly the kind of long-lived, memory-growing process that can cross 256MB in practice, and the mandatory subshell fork right after it (`( cd ".libs" && rm -f ... && ln -s ... )`, a `TPAREN` -- always a real fork per the earlier-fixed libpng-era bug) then produces a child running on silently-corrupted memory, indistinguishable from a genuine hang or an intermittent crash depending on exactly what got dropped.
  - **First ruled out simpler explanations with real measurements**, not guesses: wrote a standalone `fork()` microbenchmark (linked with the real `fork_capable_relaunch.c` opt-in, matching how `crt_mksh` itself is built) and confirmed `fork()` cost scales roughly linearly with live heap size (~1ms/MB) but is fundamentally bounded -- ~110ms at 100MB, topping out around ~300ms at the (then-4096-region) cap -- nowhere near the observed multi-second-to-hours delays, ruling out "`fork()` itself is just slow" as the explanation. Also ruled out antivirus/file-location effects with a direct timing comparison (plain `ar` into the project's own `out/` build tree vs. the OS temp directory: both fast, no anomalous delay for a bare tool invocation outside the recursive `libtool` self-invocation chain).
  - **Then found and directly confirmed the real bug**: reading `append_chunk()`'s region-tracking code showed a silent `if (heap_os_region_count < CRT_MALLOC_MAX_OS_REGIONS) { ...track... }` with no `else` -- once full, new regions are simply never recorded, no error. Wrote a second, targeted test: allocate well past 256MB, stamp a known pattern into a chunk located past the (old) cap, `fork()`, and check in the child whether the pattern survived. It did not -- the child crashed (exit status 5, consistent with an access violation) touching memory the parent never copied because the tracking table had already silently stopped recording it.
  - **Fixed two ways, not just one**: (1) raised `CRT_MALLOC_MAX_OS_REGIONS` from 4096 to 65536 (256MB -> 4GB of trackable heap -- a still-fixed table, since growing it dynamically would need its own `mmap()`/`munmap()`-based allocation path to avoid the same malloc()-reentrancy hazard `append_chunk()`'s own existing comment already flags, and would introduce a *second* "is this array's own backing memory visible to fork()?" bookkeeping problem; 4GB is enough headroom that hitting it in practice should now be exceptionally rare). (2) far more importantly, `append_chunk()` now fails the allocation outright (`ENOMEM`) once the table is genuinely full, instead of silently succeeding untracked -- turning any future occurrence into an honest, immediately-visible allocation failure at the point of the oversized `malloc()` (a completely normal, already-handled failure mode every caller already expects), instead of a correctness time bomb that only detonates later, as memory corruption inside some unrelated later `fork()` call.
  - Verified: the same targeted repro now passes (`child exit status: 0`, pattern intact) at ~268MB, well past the old 256MB cap and safely within the new 4GB one. Full `ctest` 79/79, no regressions (this file backs every allocation in every process on Windows, so this was the highest-stakes check of this whole fix).
  - This is a general CRT/PAL correctness fix, not specific to libffi or even to `mksh` -- any sufficiently memory-hungry process using this project's own `fork()` on Windows could have hit the exact same silent corruption.

- **Achieved libffi's Windows shared-library build for real (root-caused a genuine libtool infinite loop, not a hang or a workaround), building on the libpng-era `ld.lld -m` fix and the newly-found `malloc.c` fork() bug.** After the `ld.lld -m i386pep`/`-m arm64pe` fix (already generic from the libpng work) let libtool's shared-library detection pass unmodified, and after the `malloc.c` fork()-region fix earlier, libffi's real `--mode=link` build for `libffi.la` still appeared to hang indefinitely (first observed as 15+ CPU-hours on one occasion). Root-caused for real, not worked around:
  1. **Bisected the exact trigger** by isolating the real `libtool --mode=link` command outside the full build (a standalone repro script driving `crt-port-build.py`'s own `make_env()`) and testing each of libffi's own `AM_LTLDFLAGS`/`-rpath`/`-bindir` flags individually and in combination, each with a generous, self-cleaning timeout (`taskkill /T /F` on the whole process tree on timeout, since a plain `subprocess.run(timeout=...)` only kills the immediate child, not the descendants a real fork()-heavy recursive `mksh`/`libtool` chain spawns -- learned the hard way after repeated orphaned-process pileups). `-bindir` combined with a real shared build (`-no-undefined` + `-rpath` together) was the one, and only, combination that never completed.
  2. **Found the actual infinite loop** by extracting and single-stepping libtool's own `func_normal_abspath`/`func_relative_path`/`func_dirname` functions in isolation (a minimal standalone `mksh` script sourcing just those definitions): `func_normal_abspath` classifies "is this an absolute path" by checking for a literal leading `/` (`case $path in /*) ... ;; *) prepend pwd ;; esac`) -- a genuine Windows drive-letter path (`C:/Users/...`, and `pwd` itself, also `C:/...`-shaped in this environment) never matches, so it's misclassified as relative and `pwd` gets prepended, producing a string that *still* never starts with `/`. Every subsequent step (the sed-based per-component ascent loop) *also* requires a leading `/` to make any progress at all -- so the string never changes, the loop's `while :; do if test / = "$path"; ...` termination condition can never become true, and it forks a fresh `echo`/`sed` pair every single iteration forever. A textbook infinite loop, confirmed directly (not inferred): fed the exact real path in, watched `$path` stay byte-for-byte identical across iterations.
  3. **Fixed via the standard, sanctioned mechanism, not a script patch**: libffi's own `Makefile.am` sets `AM_LTLDFLAGS = -no-undefined -bindir "$(bindir)"` unconditionally -- `AM_*FLAGS` variables are Automake's own documented end-user override point, precisely for cases like this. Overriding it via `make AM_LTLDFLAGS=-no-undefined` (dropping just the offending `-bindir`, keeping the legitimate `-no-undefined` requirement) is not a workaround or an upstream patch at all, just using the tool as designed. Required two additions, not one: (a) a new, general `target_overrides.<os>.make_args` recipe mechanism in `tools/crt-port-build.py` (merged into the base `make_args` the same way `configure_args`/`cflags` already are -- previously only the top-level `build.make_args` was read, silently ignoring any host-scoped override); (b) discovered via a debug print that a plain command-line override reaches the *first* `make` invocation correctly but libffi's own `SUBDIRS = include testsuite man doc .` triggers a *recursive* self-invocation of the same Makefile (to build `libffi.la` itself, as the trailing `.` entry) that does **not** reliably inherit it -- root cause not fully isolated (plausibly this project's own Windows process-spawn implementation not perfectly preserving GNU Make's own `MAKEFLAGS`-based recursive-variable-propagation convention) -- worked around by *also* exporting `AM_LTLDFLAGS` as a real environment variable and adding `-e` (`--environment-overrides`) to `make_args`, relying on ordinary OS-level environment inheritance across process spawns instead of GNU Make's own (apparently unreliable, in this specific recursive case) internal mechanism.
  - Verified end-to-end on Windows x86_64: `libffi-8.dll`/`libffi.dll.a` build and install cleanly (`make`: 85.5s, `make install`: 18.7s, no hangs at any step), and a standalone test program `dlopen()`ed the real DLL and ran a genuine `ffi_call()` round trip through it (`add_ints(3, 4) == 7`). Full `ctest` 79/79.
  - Explicitly **not** re-tested: the pre-existing, documented, aarch64-specific X19 double-call bug (see the "done" entry earlier and `porting/recipes/libffi.json`'s own notes) is unrelated to shared-vs-static linking and stays open; whether an analogous issue exists on x86_64 was not checked this session (only a single `ffi_call()` was exercised, not the two-calls-in-a-row repro that trips X19 on aarch64). `libffi`'s status stays `partial` for that reason -- the shared-library *build* capability is now solid and verified, but the `ffi_call()`-repeat-call correctness family is untouched.
  - Full trail, including the exact isolated `func_dirname`/`func_normal_abspath` repro and the bisection matrix across every flag combination: `porting/recipes/libffi.json`'s own notes.

## 2026-08-07

- **Synced `docs/porting_status.md` and `porting/recipes/*.json` status
  fields with facts already narrated in this file but never propagated to
  the status doc/recipes.** Found via user report ("porting status looks
  wrong"): commit `4628c6c` ("Record macOS/Linux confirmation of
  sqlite-amalgamation shared build") only ever updated this file's prose,
  never `docs/porting_status.md`'s table or the recipe JSONs' own
  `status`/`notes` fields -- same gap for the earlier zlib/libpng Linux
  `ldd` and macOS `otool` confirmations. Also resolved an open question
  about `make`: it is **not** Windows-specific -- `tools/crt-port-build.py`'s
  `build_port()` unconditionally builds and installs the `make` port before
  any `configure`-system recipe on every host, and `make_env()` prefers the
  freshly-built `PORT_PREFIX/bin/make` over host `make` via `$MAKE` on all
  three OSes (verified by reading the code, not just prose). Updated,
  matching the user's direct confirmation that zlib/libpng/libffi/
  sqlite-amalgamation all pass on macOS/Linux:
  - `make`: `linux`/`macos` `pending` -> `manual-pass` (matching Windows;
    kept at `manual-pass` rather than higher since no standalone
    `make --version`-style direct check has been separately recorded for
    these two hosts, only indirect, repeated use as the build driver for
    every other port).
  - `zlib`: `linux` `manual-pass` -> `shared-pass`, `macos` `configure-pass`
    -> `shared-pass` (the `ldd`/`otool` confirmations from the cross-port
    rpath fix work earlier were already real verification of "shared library
    ... load[ing] and run[ning] correctly at runtime", just never reflected
    in the status value).
  - `libpng`: `linux` `pending` -> `shared-pass`, `macos` `configure-pass`
    -> `shared-pass` (unlike Windows, libpng's real GNU Libtool build
    already produces a working shared library on Linux/macOS through this
    project's real system `ld`/`ld64` -- confirmed via the same `ldd`
    session that showed `libpng16.so.16.57.0` resolving its `libz.so.1`
    dependency correctly).
  - `sqlite-amalgamation`: `linux` `smoke-pass` -> `amalgamation-pass`
    (matching macOS/Windows; the `amalgamation` build system's own ceiling
    status stays `amalgamation-pass` even with a full shared-library
    round trip verified, matching the existing Windows-row convention of
    recording shared-library depth in notes rather than a separate status
    tier).
  - `libffi`: status values left unchanged (`partial`/`configure-pass`/
    `partial`) -- the well-documented X19 callee-saved-register `ffi_call()`
    runtime bug is real and independent of shared-vs-static linking, so a
    blanket "all pass" was **not** applied here; only added a clarifying
    note that libffi's own shared library does build successfully on
    Linux/macOS (already evidenced in this file's zlib-Linux-shared-bug
    writeup: "`libffi`/`libpng` shared builds succeeded [on Linux]"), which
    is a separate axis from the open runtime bug.

- **Windows fork() implementation: concluded.** Marking this closed as its
  own entry: the spawn-broker retirement -> Cygwin/MSYS-style memory-copy
  `fork()` transition, the libpng `configure`/`make`/`make install`
  blocker chain that exercised it, the sqlite-amalgamation/libffi porting
  follow-up, and the reverted process-reparenting attempt (each recorded
  as its own dated entry in this file) together brought the work to a
  genuinely done state: both Windows architectures have a working
  memory-copy `fork()` (`docs/windows_fork_emulation.md`), zlib/libpng/
  libffi/sqlite-amalgamation all build (libffi with one documented,
  unresolved X19 bug), and shared-library support is confirmed across all
  3 OSes. Any new fork-related problem found from here on gets its own
  fresh entry, not appended into this one.

- **Extended `porting/recipes`' `amalgamation` build system with real
  shared-library support, and turned it on for sqlite-amalgamation.**
  `tools/crt-port-build.py`'s `build_amalgamation_port()` gained a
  `"shared": true` recipe opt-in: when set, it also compiles a second,
  `-fPIC`-flagged pass over the recipe's own sources and links them via
  `tools/crt-cc`'s existing `-shared`/`-dynamiclib` support (which already
  handles everything OS/arch-specific about shared linking -- this new
  code only supplies the flags specific to *this* library's own name/
  version). Naming/versioning mirrors zlib's own established convention
  on macOS/Linux (a real versioned file plus SONAME-style symlink
  aliases), while Windows gets a plain, unversioned `<name>.dll` (no
  `lib` prefix, no version suffix) -- matching how upstream SQLite itself
  actually ships its own precompiled Windows binary as a bare
  `sqlite3.dll`. No `.lib` import library generated on Windows, same
  precedent as zlib's own shared build this session (`lld-link` can link
  a consumer directly against the built `.dll` by its exact filename).
  Turned on for `sqlite-amalgamation.json` via `"shared": true`. Verified
  end-to-end on Windows aarch64 *and* a real x86_64 cross-build
  (`out/windows-x64-cross-debug`): `sqlite3.dll` compiles, links, reports
  the correct architecture via `llvm-objdump -f`, and a standalone test
  program dynamically linked against it (built directly via `crt-cc`,
  mirroring how `examplesh`/`minigzipsh` link against zlib's shared
  build) ran a real `sqlite3_open`/`CREATE TABLE`/`INSERT`/`SELECT` round
  trip successfully on both architectures. Full `ctest` 79/79 after the
  `tools/crt-port-build.py` change.
  - **Update: confirmed on macOS and Linux too.** `otool -L` on macOS
    shows `libsqlite3.dylib` depending only on its own self-identity
    (`libsqlite3.3.dylib`, matching the `-install_name` set at link time)
    and `/usr/lib/libSystem.B.dylib` -- no accidental `@rpath/libc.dylib`
    pickup the way zlib's build hit (sqlite's own build has no equivalent
    of zlib's stray `LDSHAREDLIBC=-lc` flag to trigger it). `ldd` on
    Linux shows `libsqlite3.so`'s `libc.so`/`libm.so`/`libdl.so`/
    `libc++.so` dependencies all correctly resolving to this project's
    own sysroot. Shared-library support now confirmed working across all
    3 OSes (Windows aarch64+x86_64, macOS aarch64, Linux aarch64) for
    zlib, and across Windows aarch64+x86_64 plus macOS aarch64 and Linux
    aarch64 for sqlite-amalgamation.

- **Fixed cross-port shared-library resolution on macOS/Linux (one
  port's `.so`/`.dylib` finding *another port's*), and macOS's own
  `libc.dylib` rpath gap uncovered along the way.** Reported via the same
  `ldd` session earlier: `libpng16.so`'s `libz.so.1` dependency resolved to
  `/lib/aarch64-linux-gnu/libz.so.1` (Ubuntu's own system zlib package)
  instead of this project's own, freshly-built one sitting right next to
  `libpng16.so` in the same `PORT_PREFIX/lib` directory. Root cause: the
  `tools/crt-cc`/`tools/crt-c++` `-rpath` added for the earlier `libc.so`
  fix only covers this project's own sysroot (`${CRT_SYSROOT}/lib`) --
  a completely different directory from where third-party ports install
  their own shared libraries (`PORT_PREFIX/lib`, e.g. `port-tests/
  install/lib`), which was on no rpath at all. Fixed in
  `tools/crt-port-build.py`'s `make_env()`: `$LDFLAGS` now also carries
  `-Wl,-rpath,<PORT_PREFIX>/lib` on macOS/Linux (skipped on Windows,
  which has no rpath concept at the PE/COFF level and where `lld-link`
  in MSVC-compatible mode doesn't understand the flag at all).
  While investigating, the user separately shared `otool -L` output on
  macOS that turned up a second, related bug this same session's earlier
  "macOS is fine, deliberately left static, no need to touch" conclusion
  had missed: `libz.dylib` carried a real `@rpath/libc.dylib` dependency
  despite `tools/crt-cc`'s macOS branch only ever naming static `.a`
  archives in its own `libs` list. Cause: zlib's own `configure` sets
  `LDSHAREDLIBC=-lc` unconditionally except on MinGW, appended
  independently to its shared-library link line -- and since this
  project's own CMake build always produces both `libc.a` *and*
  `libc.dylib` side by side in `${CRT_SYSROOT}/lib`, `ld64`'s default
  `-l<name>` resolution prefers the dylib over the same-named static
  archive when both exist on the search path, silently reintroducing a
  dynamic dependency this script was specifically trying to avoid. Since
  nothing set an `-rpath` there either, that `@rpath`-relative dependency
  had no way to resolve at actual load time (libtool-built ports like
  libpng/libffi never pass a stray bare `-lc` the way zlib's own
  hand-written Makefile does, so they didn't hit this). Fixed the same
  way as Linux: `tools/crt-cc`/`tools/crt-c++`'s macOS `shared_mode`
  entry flags now also add `-Wl,-rpath,${CRT_SYSROOT}/lib`, so if a
  dylib reference sneaks in via some mechanism outside this script's own
  control, it still resolves to this project's real one. Windows
  regression-checked here (rebuilt zlib's shared library after each
  change, no behavior change -- the windows case block itself was never
  touched).
  - **Update: confirmed fixed on both hosts, from clean `out/` rebuilds.**
    Linux: `libpng16.so.16.57.0`'s `libz.so.1` dependency now resolves to
    this project's own `port-tests/install/lib/libz.so.1`, not the
    system's; the stray `/lib/ld-linux-aarch64.so.1`/system `libc.so.6`
    entries seen on an earlier, non-clean `libpng16.so.16.57.0` build
    were apparently a stale-incremental-build artifact -- gone on a
    clean rebuild, not a real bug in this fix.
    macOS: `otool -L` alone can't distinguish "has an unresolvable
    `@rpath` dependency" from "has one that resolves fine" (it only
    lists dependencies, not `LC_RPATH` commands), so `otool -l ... |
    grep -A2 LC_RPATH` on `libz.dylib` was checked instead and shows
    both expected `LC_RPATH` entries present: `.../sysroot/lib` (finds
    this project's own `libc.dylib`) and `.../port-tests/install/lib`
    (finds sibling ports' `.dylib`s, same as the Linux fix earlier) --
    `@rpath/libc.dylib` still appears in `otool -L`'s dependency list
    (expected and correct: the fix makes it *resolvable*, not absent)
    and should now load correctly at runtime.

- **Fixed the Linux `-shared` fix earlier finding the wrong `libc.so` at
  runtime.** The Linux non-PIC-`libc.a` fix landed the build/link step,
  but `ldd` on the resulting `libz.so.1.3.1` on a real Ubuntu/Debian
  aarch64 machine reported `error while loading shared libraries:
  /lib/aarch64-linux-gnu/libc.so: invalid ELF header`. Root cause: none
  of this project's own `libc.so`/`libm.so`/`libdl.so`/`libc++.so` CMake
  targets set an explicit `-soname`, so each defaults to its own bare
  output filename ("libc.so", ...) as its `DT_SONAME` -- and that's the
  literal string `tools/crt-cc`/`tools/crt-c++`'s Linux `shared_mode`
  linking then records as `libz.so.1.3.1`'s own `DT_NEEDED` entry. At
  runtime, the dynamic loader has no memory of the absolute sysroot path
  used at link time -- it re-searches the bare name via the standard
  system path, and on Debian/Ubuntu aarch64, `/lib/aarch64-linux-gnu/
  libc.so` genuinely exists as part of `libc6-dev`: a plain-text GNU-ld
  `INPUT()` linker script meant only for the *host* toolchain's own
  link-time use, not a real loadable ELF image -- so `ld.so` rejected it
  outright. Fixed by adding `-Wl,-rpath,${CRT_SYSROOT}/lib` to
  `shared_mode`'s Linux entry flags in both `tools/crt-cc` and
  `tools/crt-c++`: bakes this project's own sysroot lib dir into the
  resulting object's `DT_RUNPATH`, which `ld.so` consults (ahead of the
  system default path) specifically when resolving *that object's own*
  `DT_NEEDED` entries, so it finds this project's real `libc.so` there
  first regardless of what unrelated same-named file the host happens to
  have. Not yet re-verified with `ldd` on the user's real Linux machine
  (this fix was prepared, not run, per current session convention where
  the user runs build/regression verification directly) -- pending.
  - **Update: confirmed fixed.** `ldd` on the user's real Linux aarch64
    machine now shows `libz.so.1.3.1`'s `libc.so`/`libm.so`/`libdl.so`/
    `libc++.so` dependencies all correctly resolving to this project's
    own sysroot (`.../out/linux-host-ninja-debug/sysroot/lib/...`), not
    the host system's.

- **Implemented `__crt_sys_readlink()` for real (was an honest `-ENOSYS`
  stub).** Found while separately verifying the Windows build wasn't
  regressed by the Linux fix earlier: rebuilding zlib's shared library a
  *second* time (install dir already has `libz.so`/`libz.so.1` symlinks
  from the previous run) failed with `rm: .../libz.so: Function not
  implemented`. Traced to toybox's `dirtree.c` -- shared by every
  directory-walking applet, including `rm` -- calling `readlinkat()` on
  every symlink entry it visits to populate `try->symlink`; the stub's
  `-ENOSYS` propagated straight up into `rm` aborting. This was flagged
  as a known gap when `__crt_sys_symlink()` was implemented earlier this
  session ("nothing currently needs it") -- turned out something did,
  just not until a *rebuild* scenario exercised it. Implemented via
  `CreateFileA(..., FILE_FLAG_OPEN_REPARSE_POINT)` (opens the link
  itself rather than transparently following it, the opposite of a plain
  open) + `DeviceIoControl(FSCTL_GET_REPARSE_POINT)`, parsing the
  `SymbolicLinkReparseBuffer` arm of `REPARSE_DATA_BUFFER` (field-for-
  field per real winnt.h) and extracting `PrintName` (the human-facing
  target string `CreateSymbolicLinkA` was actually given, as opposed to
  `SubstituteName`, which may carry an NT-namespace `\??\` prefix for
  absolute targets) via `WideCharToMultiByte` (UTF-16 `PathBuffer` ->
  narrow `char*`, `CP_ACP`, matching every other narrow-char Win32 API
  this file already calls). `tests/file_path_test.c`'s Windows symlink
  block updated to match (was asserting `-ENOSYS` as the expected
  "policy"; now asserts a real round trip, mirroring the non-Windows
  branch). Verified: `file_path_test` passes, full `ctest` 79/79, and
  rebuilding zlib's shared library twice in a row (the exact scenario
  that surfaced this) no longer errors either time.

- **Fixed `tools/crt-cc`/`tools/crt-c++` `-shared` mode statically linking
  non-PIC archives on Linux.** Reported: `zlib`'s shared build, which
  worked on macOS, failed its own `configure`-time shared-library probe
  on a real Linux aarch64 host (`libffi`/`libpng` shared builds succeeded
  there, `zlib`'s specifically didn't). `configure.log` showed the real
  cause: `ld` refused `libc.a(stdio.c.o)`'s `R_AARCH64_ADR_PREL_PG_HI21`
  relocations against `stdin`/`stdout`/`stderr` ("dangerous relocation:
  unsupported relocation ... recompile with -fPIC") the moment the probe
  needed a `stdio.c.o` symbol (`getchar()`) -- `libffi`/`libpng`'s shared
  builds happened not to need any `stdio.c.o`/`env.c.o` symbol from
  `libc.a` directly, so they never tripped this. Root cause: `crt-cc`/
  `crt-c++`'s `-shared`/`-dynamiclib` mode statically linked
  `libc.a`/`libm.a`/`libdl.a`/`libc++.a` (never compiled with `-fPIC`,
  since nothing about a normal executable requires it) on every OS, not
  just macOS/Windows where that happens not to be a hard error. Fixed on
  Linux by linking the already-built *shared* counterparts
  (`libc.so`/`libm.so`/`libdl.so`/`libc++.so` -- already present in the
  sysroot via this project's own `c_shared`/`m_shared`/`dl_shared`/
  `cxx_shared` CMake targets) instead, for `shared_mode` only (normal
  executables still statically link the `.a` archives, unaffected).
  Deliberately did **not** make the same change on macOS: it was already
  confirmed working via static linking (Mach-O/AArch64 is unconditionally
  position-independent at the ABI level regardless of `-fPIC`, so this
  isn't a hard error there the way it is on Linux's stricter ELF `ld`),
  so switching it too would only add an unproven `.dylib`
  `install_name`/`@rpath` runtime-loadability question with no upside.
  `libclang_rt.builtins.a` is left static in both modes everywhere
  (leaf compiler-intrinsic code, no global-data relocations of this
  kind -- statically linking compiler-rt/libgcc into shared objects is
  itself completely normal). Not yet verified past the link step itself
  succeeding: whether the resulting `libz.so` can actually find
  `libc.so`/etc at runtime when dynamically loaded (`ldd`/`LD_LIBRARY_PATH`
  question, since they're linked by absolute sysroot path rather than
  via an installed system location) is a real follow-up to check.

- **Fixed the CMake `port-rebuild-sqlite-amalgamation` target failing
  outright on native Windows, and stopped forcing macOS/Linux port builds
  through this project's own rootfs mksh.** Reported: `sqlite-amalgamation`
  on x86_64 Windows failed with `FileNotFoundError: [WinError 2]` trying to
  spawn `tools/crt-cc` directly (a shebang script, no `.exe`) -- root
  caused to `CMakeLists.txt`'s `crt_add_build_port_target()` only adding
  `--use-crt-shell` for `configure`/`android_host_tool` recipe build
  systems, not `amalgamation`; without it, `crt-port-build.py`'s
  `make_env()` hands the bare script path to `CreateProcess`, which cannot
  interpret a shebang line the way a POSIX host can. Fixed by adding
  `amalgamation` to that condition. While investigating, also confirmed
  (by reading `tools/crt-cc`, `crt-port-build.py`'s `make_env()`/
  `build_configure_port()`, and every recipe's macOS/Linux
  `target_overrides`) that `--use-crt-shell`'s *other* effect -- routing
  `./configure`/`make`/every compiler invocation through this project's
  own from-scratch mksh, and putting the rootfs's toybox applets ahead of
  the host's own coreutils on `$PATH` -- was never actually needed on
  macOS/Linux: nothing in any recipe depends on it (the CRT sysroot
  integration is carried entirely by `CC`/`CXX` pointing at `tools/crt-cc`/
  `tools/crt-c++`, which works identically either way), and those hosts
  already have a real, complete, natively-shebang-capable shell +
  coreutils -- exactly what upstream `configure` scripts are actually
  tested against, unlike this project's deliberately-minimal,
  Windows-motivated toybox applet set. So `CRT_TARGET_OS STREQUAL
  "windows"` was added to the same condition, scoping `--use-crt-shell`
  (and the `rootfs` build dependency it requires) to native Windows only.
  Expected effect beyond fixing the immediate crash: real correctness risk
  removed (macOS/Linux configure probes now see the same coreutils/awk/
  grep upstream projects are tested against, not this project's own
  applets) and likely a real speedup for macOS/Linux configure runs (no
  longer routed through this project's own mksh for the thousands of tiny
  subprocess probes a typical `configure` script runs). Verified in this
  session only via a fast `cmake --preset` reconfigure + inspecting the
  generated `build.ninja` (`--use-crt-shell` still present for the Windows
  preset's port targets, unchanged); the real x86_64 Windows
  `sqlite-amalgamation` rebuild that reported the original crash is still
  pending verification by the user directly on that host.
  - **Update: macOS confirmed.** The user rebuilt zlib, libpng, libffi, and
    sqlite-amalgamation on a real macOS machine after this change and
    confirmed both predictions: the build is noticeably faster, and the
    previously-present spurious errors/warnings during `configure`/`make`
    are gone (`zlib`'s `libz.1.3.1.dylib` in particular confirmed to build
    correctly). Also confirmed, on request, exactly what still ties these
    builds to the CRT sysroot despite no longer routing through this
    project's own mksh: `tools/crt-cc` passes `-nostdinc
    -isystem${CRT_SYSROOT}/include` (host system headers excluded
    entirely, only this project's own Bionic-compatible headers visible)
    and links via `${CRT_SYSROOT}/lib/crt1.o -L${CRT_SYSROOT}/lib` --
    unrelated to, and unaffected by, which shell drives `configure`/`make`
    itself. See `docs/porting_status.md`'s zlib/libpng/libffi/
    sqlite-amalgamation rows for the per-port notes.

- **Fixed GNU make's Windows x86_64 build (`dir.c` compile error, then two
  more bugs found chasing it).** Reported from a real x86_64 Windows machine
  (this project's own dev machine is aarch64, whose `make` build had never
  hit any of these): `src/dir.c:1241: error: array type 'char[256]' is not
  assignable` at `d->d_name = xmalloc(len)`. Root-caused to
  `tools/crt-cc`/`tools/crt-c++` targeting `*-w64-mingw32` predefining
  `__MINGW32__` but not `__MINGW32_MAJOR_VERSION`/`__MINGW32_MINOR_VERSION`
  (only a real mingw-w64 install's own `_mingw.h` does), so `dir.c`'s `#if
  __MINGW32_MAJOR_VERSION < 3 ...` guard (an ISO C preprocessor-arithmetic
  undefined-macro-as-0 trap) wrongly took an ancient-mingw compat branch
  treating `d_name` as a pointer -- doesn't compile against this project's
  Bionic-style fixed-array `struct dirent`. Fixed by defining both macros
  in `tools/crt-cc`/`tools/crt-c++`'s Windows case block to match a real,
  current mingw-w64 install's actual values (arch-independent: same fix for
  both `aarch64-w64-mingw32` and `x86_64-w64-mingw32`; Windows-only, since
  `__MINGW32__` is never defined on macOS/Linux). Verifying this on an
  x86_64 cross-build (`out/windows-x64-cross-debug`, this project's own
  aarch64 dev machine's x64-emulation cross-arch setup from earlier this
  session) surfaced two more, genuinely x86_64-only problems past the fixed
  compile step:
  - Link failed on `undefined symbol: ___chkstk_ms` -- the MinGW-mangled
    name clang emits calls to (instead of the MSVC-triple `__chkstk` name
    this project already implemented in `libc/src/arch/windows/x86_64/
    chkstk.S`) when a function's stack frame is large enough to need a
    guard-page-safe stack probe. Both names are, per LLVM's own
    compiler-rt, the exact same routine under historical MSVC-vs-MinGW
    C-symbol-naming-convention names -- fixed by adding `___chkstk_ms` as a
    second label on the same code, right in `chkstk.S`. Not needed on
    aarch64: AArch64 COFF never had the leading-underscore name-mangling
    split x86/x86_64 did, so mingw-w64 uses the same `__chkstk` name there
    as MSVC (confirmed by checking `libc/src/arch/windows/aarch64/
    chkstk.S`, which needs no such alias).
  - Then `undefined symbol: __main` -- clang's `*-w64-mingw32`-only
    implicit call inserted at the top of every `main()`, a decades-old
    GCC/MinGW convention for running `.ctors`-section constructors a PE
    loader wouldn't run itself. This project's own CRT startup
    (`src/arch/windows/common/crt1.c`) already runs constructors through
    its own mechanism before `main()` is ever reached, so `__main()` itself
    has nothing left to do -- matching modern mingw-w64's own runtime,
    which keeps it only as an empty stub for the same reason. Added exactly
    that: an empty `void __main(void) {}` in `libc/src/arch/windows/
    common/compiler_abi.c` (same file/pattern as the pre-existing
    `__clear_cache()` compiler-support-symbol stub), arch-independent (pure
    C, no per-arch asm needed) since nothing rules out some other Windows
    port also needing it on aarch64 eventually.
  - Also found and fixed, while setting up the x86_64 cross-build repro: a
    latent `tools/crt-port-build.py` bug where `--target-arch`/
    `CRT_TARGET_ARCH` was used only for the `@CRT_MINGW_TRIPLE@` recipe-
    string substitution and never actually exported to the `crt-cc`/
    `crt-c++` child processes -- which independently auto-detect arch via
    `uname` when `$CRT_TARGET_ARCH` is unset, silently building the
    *host's* architecture instead of the requested one on a genuine
    cross-arch build (caught because a deliberately-requested x86_64 build
    on this aarch64 dev machine came out as an aarch64 binary with no
    error). Fixed in `make_env()`: export `$CRT_TARGET_ARCH` derived from
    the same already-resolved `mingw_triple`.
  Verified end-to-end on x86_64: `make.exe` compiles, links, reports
  `architecture: x86_64` via `llvm-objdump -f`, runs `--version`, and
  correctly evaluates `$(wildcard *.txt)` (exercising the exact `dir.c`
  code path that started this). Verified on aarch64 too (link succeeds,
  no regression). Full `ctest` and the real x86_64 build/regression run
  this depends on (`zlib` -> `make`) were confirmed passing by the user
  directly rather than by this session -- see
  `docs/porting_status.md`'s `make` row for the full writeup.

- **Windows shared-library (DLL) build support, end to end for zlib.**
  Discovered `porting/recipes/*.json` never produced `.so`/`.dll`/`.dylib`
  outputs at all (only static archives), traced to two gaps and fixed both:
  - `tools/crt-cc`/`tools/crt-c++` had no `-shared`/`-dynamiclib` support --
    always hardcoded EXE-building flags (`crt1.o` + entry point). Added
    `shared_mode` detection (alongside the existing `compile_only`) that
    swaps in the right start object/entry flags per OS: macOS/Linux drop
    `-e,_start` (a shared object has no `_start`), Windows swaps `crt1.o`
    for a new `dllcrt.o` and links with `/entry:crtDllMainCRTStartup
    /DLL /OPT:REF`. `dllcrt.o` (`crtDllMainCRTStartup`, in
    `libc/src/arch/windows/common/dllcrt.c`) already existed for this
    project's own `c.dll`/`c++.dll`/etc CMake DLL targets but was never
    installed into the sysroot as a standalone, reusable object the way
    `crt1.o` is for EXEs -- added that install rule to `libc/CMakeLists.txt`.
    Verified via a minimal hand-built test DLL (`llvm-readobj
    --file-headers` shows `IMAGE_FILE_DLL` set correctly; loads for real via
    `LoadLibraryA`/`FreeLibrary`), then via `crt-cc -shared` directly, then
    via the real zlib port build.
  - `libc/src/arch/windows/common/syscall.c`'s `__crt_sys_symlink()` was a
    pure `-ENOSYS` stub, which broke zlib's Makefile SONAME step (`ln -s
    libz.so.1.3.1 libz.so`) even after the DLL itself linked successfully.
    Implemented via real `CreateSymbolicLinkA()` (note the Win32 arg order
    is target/link *reversed* from POSIX `symlink(target, linkpath)`), with
    `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` and a
    `GetFileAttributesA()`-based best-effort directory-vs-file flag guess
    (the target need not exist yet, matching upstream Makefiles that
    symlink before the real file lands). `readlink()` stays `-ENOSYS`
    (reparse-point parsing via `DeviceIoControl`/`FSCTL_GET_REPARSE_POINT`
    is real extra work nothing currently needs). Also added `ERROR_PRIVILEGE_
    NOT_HELD` (1314) -> `EPERM` to `map_windows_error()` (previously fell
    through to a generic, unhelpful `EIO`) after hitting it for real:
    `CreateSymbolicLinkA()` with the unprivileged-create flag still requires
    Windows Developer Mode to be enabled on the machine for a non-elevated
    process -- confirmed by testing on this dev machine with Developer Mode
    off (real `EPERM` failure), then again after the user enabled it via
    `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock\
    AllowDevelopmentWithoutDevLicense` (real success). Updated
    `tests/file_path_test.c`'s Windows-only symlink block to match (it
    previously *asserted* `-ENOSYS`, documenting the old stub as "policy" --
    now asserts a real create succeeds and only `readlink()` stays
    `-ENOSYS`).
  - `porting/recipes/zlib.json`'s `configure_args` changed from
    `["--static"]` to `[]` (zlib's own default already builds both). Full
    `ctest` 79/79 after the `syscall.c` change. Verified past "it built"
    into "it actually works": zlib's own `examplesh`/`minigzipsh` test
    binaries, dynamically linked against the freshly built
    `libz.so.1.3.1`, ran a real compress/uncompress/gzip round trip
    successfully.
- **libpng and libffi shared-library attempts: root-caused, not achieved.**
  Both go through real GNU Autoconf + Libtool (unlike zlib's hand-written
  Makefile), and Libtool's MinGW shared-library detection doesn't recognize
  this toolchain: `checking for dlltool`/`checking for objdump` both came up
  false (this project ships LLVM's `llvm-dlltool`/`llvm-objdump`, not
  binaries under those literal GNU-binutils names) and `checking if the
  linker is GNU ld` incorrectly resolves `yes` for `lld-link.exe` running in
  MSVC-compatible mode (its `-v` banner says "compatible with GNU linkers",
  which apparently still matches whatever pattern this libtool.m4 vintage
  uses). Extended `tools/crt-port-build.py`'s `make_env()` to pre-set
  `$DLLTOOL`/`$OBJDUMP` the same way `$LD` already was, which fixed those
  two probes -- but `checking whether the ... linker ... supports shared
  libraries` still resolves `no` afterward, via a block of libtool-internal
  shell logic with no compiler/linker invocation logged in between (a
  static case-statement decision, not a failed compile probe). Not narrowed
  further; both ports still build and install cleanly as static-only
  (`configure_args` no longer force `--disable-shared`, matching zlib's
  convention of not fighting Libtool's own default, but the practical
  result on Windows is unchanged from before). Full trail in each recipe's
  own `notes`.

- Ran the two remaining Windows porting tests (sqlite-amalgamation and
  libffi), following up on the libpng work earlier.
  - **sqlite-amalgamation: full success.** sqlite3.c's own `SQLITE_OS_WIN`
    detection (`defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) ||
    defined(__MINGW32__) || defined(__BORLANDC__)`) fired because
    `tools/crt-cc` targets `*-w64-mingw32` (needed for the GNU-C/GNU-ld
    detection fix from the libpng chain earlier), which predefines those
    macros -- fixed the same way as zlib/libpng, via
    `target_overrides.windows.cflags` undefining them so SQLite takes its
    generic `SQLITE_OS_UNIX` path instead of `#include "windows.h"`.
    Verified via the full recipe flow (compile/archive/ranlib/install) plus
    a standalone program that actually opened an in-memory db, created a
    table, inserted, and selected the correct value back.
  - **libffi: builds and installs successfully; core features work in
    isolation; one real, well-characterized bug remains unresolved.**
    Blocker chain: `config.guess` doesn't recognize plain Windows `uname`
    (same `--build=aarch64-w64-mingw32` workaround as libpng/zlib) -> a
    broken top-level "multilib dispatcher" `Makefile` libffi's own build
    generates (a bundled `makefile.sed` mishandles a Windows drive-letter
    colon as a `Makefile` target separator, corrupting `MAKE=C:/...` into
    `MAKE=C:`) -> routed around via a new, general
    `target_overrides.<os>.make_subdir` mechanism in
    `tools/crt-port-build.py` that points `make`/`make install` straight at
    the real subdirectory `Makefile` -> `dlmalloc.c`/`ffi.c` both
    `#include <windows.h>`, which this sysroot doesn't have. First tried
    the usual `-U_WIN32` CFLAGS trick (same as zlib/libpng/sqlite), but
    that turned out wrong here specifically: `_WIN32` also gates libffi's
    own Windows-aware avoidance of the X18/TEB-reserved register in its Go-
    closures code and its `FFI_WIN64` default ABI, so hiding it silently
    re-broke both. Settled instead on a small, project-owned
    `porting/shims/win32/windows.h` (new `include_dirs` recipe mechanism)
    providing just the handful of Win32 APIs those two files actually
    call, keeping `_WIN32` defined normally so every one of libffi's own
    Windows-aware decisions resolves exactly as upstream intends. Along the
    way, discovered `InterlockedCompareExchange`/`InterlockedExchange`/
    `InterlockedCompareExchangePointer` (needed by `dlmalloc.c`'s spinlock)
    are real winnt.h compiler intrinsics, not kernel32 exports -- provided
    via `__sync_val_compare_and_swap`/`__sync_lock_test_and_set` instead --
    and added `__clear_cache()` (`libc/src/arch/windows/common/
    compiler_abi.c`) since this project's Windows builds have no
    compiler-rt builtins archive at all. With all of that, the full build
    succeeds, and `ffi_call()` alone and `ffi_closure_alloc()`/
    `ffi_prep_closure_loc()` alone (a real trampoline, exercising the
    VirtualAlloc/mprotect-equivalent `PROT_EXEC` path) each work correctly
    in isolation -- but calling `ffi_call()` and then making *any* further
    libffi call in the same process reliably segfaults, and only when the
    caller is compiled at `-O1`/`-O2` (never `-O0`). Root-caused (via a
    minimal ~20-line repro, disassembly, and ruling out X18 corruption,
    instruction-cache staleness, and shared-`ffi_cif`-state as causes) to a
    callee-saved GPR (observed: X19) that clang trusts AAPCS64 to preserve
    across the `ffi_call()` call getting corrupted somewhere in the
    `ffi_call()`/`ffi_call_SYSV` chain (`src/aarch64/ffi.c` +
    `src/aarch64/sysv.S`'s unusual caller-provided-stack-frame convention)
    -- not yet isolated to an exact instruction; would need single-
    stepping `ffi_call()`'s own compiled code with a real debugger. Status
    left at `partial` (matching Linux's existing status) rather than a
    false "pass". Full trail and the repro recipe: see
    `porting/recipes/libffi.json`'s own notes.

## 2026-08-06

- Ported Windows aarch64's Cygwin/MSYS-style memory-copy `fork()` (Phase C
  earlier) to x86_64. Most of the design carries over unchanged --
  `fork_capable_relaunch.c` (the startup self-relaunch under the ASLR-
  disabling mitigation policy) is pure Win32 API with zero
  architecture-specific code, so it was moved from `aarch64/` to a shared
  `libc/src/arch/windows/common/` rather than duplicated. What's genuinely
  new for x86_64 is `libc/src/arch/windows/x86_64/fork_memcopy.c`:
  - `CONTEXT_AMD64` (winnt.h), transcribed field-for-field and
    cross-checked directly against a real Windows SDK `winnt.h` rather
    than from memory (`P1Home` through `LastExceptionFromRip`,
    `XSAVE_FORMAT`/`M128A` included) -- then independently verified via a
    standalone `offsetof()` probe (every offset matched exactly, including
    `sizeof(CONTEXT) == 1232`) before ever being wired into the real
    build, given how costly a wrong offset would be here.
  - TEB access: x86_64 has no equivalent of aarch64's reserved X18
    platform register -- reads it via the GS segment directly
    (`%gs:0x30`, no register reservation needed anywhere else in the
    build, unlike aarch64's globally-applied `-ffixed-x18`).
    `NT_TIB.StackBase`/`StackLimit` sit at the identical `+0x08`/`+0x10`
    offsets on both architectures, so `copy_current_stack()`'s actual
    logic needed no changes.
  - setjmp()/CONTEXT register mapping: Windows x64's callee-saved set
    (`libc/src/arch/windows/x86_64/setjmp.S`) is `rbx`/`rbp`/`rdi`/`rsi`/
    `r12`-`r15`/`rsp`/return-address plus `xmm6`-`xmm15` -- and unlike
    aarch64 (AAPCS64 only guarantees the low 64 bits of `v8`-`v15`), the
    Windows x64 ABI preserves `xmm6`-`xmm15` in full (128 bits each), so
    both halves of each register needed copying into `CONTEXT.FltSave.
    XmmRegisters[6..15]`.
  - Found and fixed a real bug while wiring this up: `crt1.c`'s weak
    symbol reference to `__crt_windows_ensure_fork_capable_relaunch()`
    (and its call site) were still guarded by `#if defined(__aarch64__)
    ...` only -- so on x86_64 the startup self-relaunch silently never
    ran at all (the symbol didn't exist in that translation unit, so
    "call it if non-null" was compiled out, not evaluated false). Caught
    via `fork_test` failing with a stack-commit error whose parent-side
    address changed on every run -- exactly what unmitigated ASLR looks
    like, immediately after `GetProcessMitigationPolicy()` had been
    independently verified (via a standalone probe) to correctly report
    "not yet mitigated" vs. "mitigated" in this same x64-under-emulation
    environment. Fixed by extending that `#if` too.
  - Verified end-to-end: set up a same-OS cross-arch build
    (`-DCRT_TARGET_ARCH=x86_64` on this Windows aarch64 machine, requiring
    a new `CMAKE_C_COMPILER_TARGET`/`CMAKE_CXX_COMPILER_TARGET`/
    `CMAKE_ASM_COMPILER_TARGET` = `x86_64-pc-windows-msvc` cross-compile
    path added to the top-level `CMakeLists.txt` -- Clang needs no
    separate per-arch install, just a different `--target`), then ran the
    real x86_64 binaries under this machine's built-in x64 emulation
    (Prism/xtajit). Full `ctest` 79/79, including `fork_test`/
    `fork_signal_test`/`fork_runtime_reset_test`. Not yet re-verified on
    real x86_64 hardware -- see `docs/windows_fork_emulation.md`, "Current
    Open Issues".

- **Retired the spawn broker; moving to a Cygwin/MSYS-style `fork()` instead.**
  The broker (see "done" earlier) fixed zlib and got libpng most of the way,
  but kept surfacing new structural failure modes of its own this session
  (orphaned `mksh.exe` processes, named-pipe races, I/O timeouts, a
  process-tree-reparenting attempt that regressed the working state and had
  to be reverted -- see the entry later). Decided to isolate it out of the
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
    did before the broker existed; `libpng`'s build (see later) is blocked
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
    "fd-inheritance gap" update later). Several things came up that the
    design write-up earlier didn't anticipate:
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
      past what was committed at fork time; x86_64 not yet ported at the
      time this was written -- see the 2026-08-06 "Ported Windows aarch64's
      Cygwin/MSYS-style memory-copy `fork()` ... to x86_64" entry for the
      x86_64 port).
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
    necessary but **not sufficient on its own** -- see "still open" later:
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
    later, which was the real remaining cause in the cases actually
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
    bug..." earlier) then blocks the fork for it too, since nothing there
    distinguishes "a TCOM already isolated by a real fork" from "a TCOM
    that IS the not-yet-forked subshell's own content." Concretely:
    `(exit $ac_status)` as the last statement of a `{ ...; }` group --
    exactly the idiom automake's generated `configure` uses to probe for
    optional tools like `tar` -- ran `exit` in the *interpreter itself*
    instead of a subshell, killing the whole `./configure` script
    instead of just that one probe attempt. This is what was actually
    behind the `checking how to create a ustar tar archive` exit-127
    failure recorded later (the pipe-broker races earlier are real,
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
    exists (see "Phase C" earlier). With the broker gone and memory-copy
    `fork()` in its place, `configure` now gets *substantially* further --
    all the way past `checking whether the C compiler works... yes` (the
    exact step that used to fail) and through `checking build system
    type`/`checking host system type` -- before hanging (not crashing:
    confirmed genuinely stuck via the CPU-delta technique, unchanged CPU
    across a 10s window) at the very next step, `checking for a sed that
    does not truncate output`. This is autoconf's own self-test that
    builds a `sed` script by repeatedly doubling a fixed pattern string,
    then pipes the whole thing through `sed` to find the length where
    truncation starts. **Root-caused and fixed.** Re-ran with
    `CRT_PORT_SHELL_XTRACE=1` and caught the exact stuck command via a
    live `Monitor` on the trace log: `echo "$ac_script" | sed 99q
    >conftest.sed`, where `$ac_script` is an ~11 KB doubled string. `echo`
    is a shell builtin; mksh's `MKSH_CRT_SHELL_CHILD_SPEC` Windows port
    (`shell/mksh/src/jobs.c`'s `exchild()`) skips a real `fork()` for a
    `TCOM` pipeline stage to avoid this platform's expensive memory-copy
    `fork()` when it turns out to be an external command -- but when the
    stage is a *builtin* instead, it runs synchronously in-process with no
    concurrent reader forked yet, and its `write()` into the pipe
    (`CreatePipe()`'s default buffer, ~4096 bytes) blocks forever once it
    exceeds the buffer. Binary-searched the exact threshold with a minimal
    `echo "$s" | wc -c` reproduction: 4051 bytes OK, 4101+ hangs
    indefinitely (confirmed via `timeout`, not just CPU-delta). Fixed by
    giving every `CreatePipe()` call in `syscall.c` (the generic `pipe()`
    syscall, the posix_spawn() fd-snapshot bootstrap pipe, and the
    fork-capable self-relaunch's fd handoff added just earlier) an explicit
    4 MiB buffer (`CRT_PIPE_BUFFER_SIZE`) instead of the system default --
    the latter two share the exact same synchronous-write-before-resume
    shape and would have hit the identical deadlock for a large enough fd
    table/snapshot, just not yet observed in practice. Verified: the
    isolated repro now succeeds well past the old threshold; `ctest` stays
    at 78/78; a full libpng `configure` re-run sails straight through the
    sed self-test and `checking for grep that handles long lines and -e`,
    reaching a *new*, much later, and non-hanging stopping point: `checking
    for egrep... configure: error: no acceptable egrep could be found` --
    `egrep`/`fgrep` are simply missing from `tools/create_rootfs.py`'s
    `TOYBOX_APPLETS` alias list (toybox's `grep.c` natively supports both
    as `OLDTOY` aliases of `grep`; just need to be added to the list and
    have their `USE_EGREP`/`USE_FGREP` config macros enabled in this
    project's hand-picked toybox build). See
    `docs/windows_fork_emulation.md`, "Windows Pipe Buffer Size", for the
    full writeup.
  - **Update: egrep/fgrep alias, ERE alternation, and pipe-lseek, all
    fixed; configure now blocked on a plain missing `ld`.** Three more
    real, independent bugs found and fixed chasing `checking for
    egrep`/`fgrep`, in order:
    1. `egrep`/`fgrep` weren't just missing from `tools/create_rootfs.py`'s
       `TOYBOX_APPLETS` alias list -- this project also doesn't use
       toybox's Kconfig `.config`; it hand-lists every enabled applet in
       `shell/toybox/crt/generated/newtoys.h`, which had no `OLDTOY(egrep,
       grep, ...)`/`OLDTOY(fgrep, grep, ...)` entries at all (toybox's own
       `grep.c` already supports both natively). Added both to
       `newtoys.h` and to `TOYBOX_APPLETS`.
    2. `grep -E 'bar|baz'` didn't match either alternative -- `|` was
       matched as a literal character. `libc/src/regex.c` was a 370-line
       hand-rolled backtracking matcher with **no alternation support at
       all** (not a regression; it was simply never implemented). Per
       explicit instruction, replaced it wholesale with the real Bionic/
       NetBSD Henry-Spencer strip-VM regex engine (`libc/src/regex/`,
       ported from `libc/upstream-netbsd/lib/libc/regex/` on Bionic
       `main`) -- full POSIX BRE/ERE, backreferences, bounded repetition,
       POSIX character classes, opt-in GNU BRE extensions. See
       `third_party/bionic/README.md`'s "Regex Tranche" for the full file
       list and adaptation notes (the interesting one: this project's real
       `wint_t` is `unsigned short`, which breaks the engine's negative
       sentinel comparisons, so the NLS/real-wide-char path is
       deliberately left off in favor of utils.h's own signed-`short`
       fallback -- see `libc/src/regex/netbsd-compat.h`'s top comment).
       Also added `reallocarray()` (`libc/src/reallocarray.c`) and
       `MB_LEN_MAX` (`include/limits.h`), both needed by the ported
       engine and missing from this libc before now.
    3. Even with alternation working, `grep`/`egrep`/`fgrep` still matched
       *nothing at all*, on *any* pattern, but only when reading from a
       **pipe** (a real file argument worked fine) -- root-caused to
       toybox `grep.c`'s "only run binary-file sniffing on lseekable fds"
       guard (`!lseek(fd, 0, SEEK_CUR)`): this project's Windows
       `__crt_sys_lseek()` called `SetFilePointerEx()` on whatever handle
       it was given with no check for whether it was actually seekable,
       so the guard's intended skip-on-pipe behavior silently didn't
       trigger -- grep's binary-sniffing peek-and-rewind ran on piped
       stdin too, consumed the pipe's data during the peek, and (since a
       pipe can't be rewound) never got it back, so the real read loop
       that followed started from an already-drained pipe. Fixed by
       checking `GetFileType(handle) == FILE_TYPE_PIPE` up front and
       returning `-ESPIPE`, matching POSIX `lseek(2)` on a pipe/FIFO.
       Regression-covered in `tests/fd_errno_test.c`.

    All three verified together: `ctest` 78/78 (plus new `regex_test.c`
    coverage for alternation/bounded-repetition/backreferences/POSIX
    classes/case-insensitivity, and the `fd_errno_test.c` lseek-on-pipe
    check); a full libpng `configure` re-run now sails through `checking
    for egrep`/`checking for fgrep` (`... /system/bin/grep -E` / `-F`) and
    reaches yet another new, later, ordinary (non-hanging) stopping point:
    `configure: error: no acceptable ld found in $PATH` -- this project's
    rootfs has no standalone `ld` binary at all (it drives `lld-link`/
    `crt-cc` directly, never a bare `ld`); next step is likely either
    aliasing one or making libpng's `configure` accept the existing
    toolchain wrapper instead. Not yet investigated.
  - **Update: `ld` not found, fixed.** `crt-cc` already links via
    `-fuse-ld=lld` (`tools/crt-cc`), so `ld.lld.exe` (shipped by the LLVM
    install) is this project's real linker backend already -- it just
    wasn't reachable under any name/location libtool's `AC_PROG_LD`
    ("checking for non-GNU ld") search would find, because native-Windows
    `--use-crt-shell` configure runs with `PATH` hard-restricted to this
    project's own rootfs (`/system/bin:/bin:/usr/bin`), unlike macOS/
    Linux, which append the *host* PATH and so already have a real system
    `ld` there (Xcode CLT / binutils) -- this was a Windows-only gap.
    Fixed in `tools/crt-port-build.py`'s `make_env()`: pre-set `$LD` to
    `ld.lld.exe`'s real path (found the same way `CRT_HOST_CC`/
    `CRT_HOST_CXX` already are, via `find_windows_host_tool()`, wrapped
    with the same `CRT_SPAWN_NATIVE_WINDOWS=1` prefix AR/RANLIB/STRIP
    already use) -- autoconf/libtool only search `PATH` for `ld` when
    `$LD` isn't already set, so this skips the broken search entirely.
    Verified: `checking for non-GNU ld... ...ld.lld.exe`,
    `checking if the linker (...) is GNU ld... no` (ld.lld's `-v` banner
    says "compatible with GNU linkers", not literally "GNU", so libtool
    correctly treats it as non-GNU-but-compatible), and configure sails
    through the entire libtool linker/shared-library-support detection
    phase (`checking whether ... linker ... supports shared libraries...
    yes`, ranlib/strip detection, PIC flags) to a new, much later, and
    completely different next blocker: `checking if awk () works...
    inaccessible or not found` / `configure: error: ... no` -- this
    project's rootfs has no `awk` at all yet (`checking for gawk/mawk/
    nawk/awk... no` earlier in the same log; toybox's own `awk.c` is a
    `pending`, not-yet-enabled applet).
  - **Update: real AWK ported and working.** Per explicit direction,
    ported Brian Kernighan's reference `onetrueawk` (NetBSD/many BSDs'
    own system awk) into `shell/awk/`, built with this project's own CRT
    like `mksh`/`toybox` -- deliberately *not* solved by pointing
    configure at a host-installed awk (e.g. Git for Windows' bundled
    `gawk.exe`), which would be an uncontrolled, per-machine, different-
    runtime dependency breaking the same self-containment principle that
    justified building `mksh`/`toybox` in the first place. See
    `shell/awk/README.md` and `import_manifest.json` for the full
    writeup; summary:
    - Installed `win_flex_bison` (via `winget`) to generate
      `awkgram.tab.c`/`.h` from upstream's `awkgram.y`, and built/ran
      upstream's own `maketab.c` (as a native host tool, via this
      project's own `crt-cc` against its own sysroot) to generate
      `proctab.c`. Both generated outputs are vendored as pristine,
      checked-in files (matching how `shell/toybox/crt/generated/*.h`
      are already vendored rather than regenerated at build time) -- this
      project's CMake build gained no new bison/yacc dependency.
    - One adaptation to upstream source: `parse.c`'s `ptoi()`/`itonp()`
      pointer-smuggling helpers cast through `long`, truncating on
      Windows LLP64; changed to `intptr_t`.
    - Filled several real, general (not awk-specific) CRT/libm gaps found
      compiling and then actually *running* awk programs: `<stdnoreturn.h>`
      (missing entirely), `atan2()`/`atan()` (FreeBSD msun, a separate
      upstream from the regex import), `system()`, `rand()`/`srand()`/
      `random()`/`srandom()` (a rand48-family LCG, not a literal port of
      BSD's own proprietary `random()` -- POSIX doesn't mandate a specific
      sequence), `SIGFPE` `FPE_*` `si_code` constants, and `popen()`/
      `pclose()`.
    - Found and fixed a real, general `printf`/`snprintf` bug this way
      too: `%g` with an *explicit* precision (`%.6g`, `%.30g`, ... --
      onetrueawk's own number-to-string conversion always uses `%.30g`)
      zero-padded the already-rendered digit string a second time (e.g.
      `%.6g` of `4.0` printed `"000004"`, not `"4"`) because
      `format_double_general()`/`format_long_double_general()`
      (`libc/src/printf.c`) forgot to clear `spec->precision_set` before
      their final `write_formatted()` call, unlike their fixed-point/
      exponential siblings which already did. No prior test had ever
      exercised `%g` with a non-default precision. Regression-covered in
      `tests/printf_test.c`.
    - Verified: `ctest` 79/79 (new `crt_awk_basic_runs` plus the printf
      regression cases earlier); manual smoke covering print/field-split/
      pattern-match/arrays/`printf`/`split`/`sqrt`/`atan2`/`rand`/`srand`/
      `getline`-from-`popen` all correct. A full libpng `configure`
      re-run now passes `checking for gawk... (cached) awk` /
      `checking if awk (awk) works... yes` and reaches yet another new,
      much later, and completely different next blocker: `checking for
      zlibVersion in -lz... no` / `configure: error: zlib not installed`
      -- despite zlib's own install stamp already being present in
      `PORT_PREFIX`; not yet investigated (a link/library-path issue,
      not a missing-tool issue like everything earlier).
  - Continued past that: `checking for zlibVersion in -lz... no` turned out
    to be a COFF-vs-Unix static-library-naming mismatch, not a missing
    build. This project's toolchain links via `clang -fuse-ld=lld`, whose
    lld-link backend resolves `-lfoo` to a file literally named `foo.lib`
    (matching this project's own CMake-built libs, e.g. `c.lib`/`m.lib`),
    but zlib's own autoconf/make install produces the Unix-conventional
    `libz.a`, which `-lz` could never find. Fixed generally (for all future
    Windows ports, not just zlib) with a new post-install step,
    `alias_unix_static_libs_for_windows_link()` in `tools/crt-port-build.py`:
    after every port build, copies each installed `libfoo.a` to `foo.lib`
    alongside it (skipped if `foo.lib` already exists). Verified: rebuilding
    zlib now also produces `z.lib`, and a full libpng `configure` re-run
    reaches `checking for zlibVersion in -lz... yes`.
  - Next blocker after that: `fatal error: 'windows.h' file not found`
    (`pngpriv.h:569`, guarded by
    `#if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)`). This
    project's sysroot has no real Windows SDK `<windows.h>` (its own Win32
    API surface is declared privately inside `libc/src/arch/windows/`, not
    exposed publicly). Nothing near that include in
    `pngpriv.h`/`png.c`/`pngerror.c` actually references a real Windows API
    symbol from it, so -- matching the exact technique zlib's own recipe
    already uses (`porting/recipes/zlib.json`'s `-U_WIN32` etc. `CFLAGS`) --
    fixed by adding the same `-U_WIN32 -U_WIN32_WCE -U__WIN32__ -UWIN32
    -U__NT__ -U_MSC_VER` `CFLAGS` to `porting/recipes/libpng.json` (plus the
    extra `-U__NT__`, since libpng's own Windows guard also checks that
    macro), keeping libpng on its generic/POSIX code paths.
  - Next blocker after that: `fatal error: 'arm_neon.h' file not found`
    (`pngrtran.c:26`). Root-caused to `tools/crt-cc`/`tools/crt-c++`
    hardcoding `resource_dir=""` on Windows, entirely skipping clang's own
    `-print-resource-dir` query -- meaning compiler-provided architecture-
    intrinsic headers (`arm_neon.h`, `immintrin.h`, ...) were never on the
    Windows include path at all. Also found a compounding latent bug while
    fixing this: both scripts build `common_flags`/`user_args`/`libs` as
    plain strings and pass them via *unquoted* expansion, relying on word-
    splitting to become separate argv entries -- which silently breaks any
    single value containing a space, and a stock Windows LLVM install's
    resource-dir is almost always under `"C:\Program Files\..."`. Fixed
    both scripts by always querying `resource_dir` (all platforms), and
    passing `-isystem "$resource_dir/include"` as its own separately-quoted
    argument pair directly on each `exec` line, instead of folding it into
    the unquoted `common_flags` string. Verified: `ctest` 79/79 still
    passing (these scripts compile everything in the Windows build, so this
    was the highest-stakes check of this whole chain); a standalone
    `crt-cc` compile of a translation unit including `<arm_neon.h>` now
    succeeds.
  - Next blocker after that: `png.c` compiled, but `libtool`'s own link
    step picked `lib -OUT:...` (the MSVC-native static archiver, which
    this project has no `lib.exe` for) instead of using `$AR`. Root-caused
    to the *value* previously used for `AR`/`RANLIB`/`STRIP`/`LD`: the
    literal string `"CRT_SPAWN_NATIVE_WINDOWS=1 <tool-path>"`, relying on
    the calling shell to recognize that leading `VAR=val` as an
    environment-assignment prefix whenever the value is expanded
    unquoted. POSIX shells only ever do that for literal, parsed-at-parse-
    time source text -- never for a variable's word-split expansion at
    runtime. Makefile recipes happened to work anyway (make substitutes
    `$(AR)` textually into a *fresh* shell command line each time), but
    libtool's own "is `$LD` GNU ld" probe inside `configure` -- `` `$LD
    -v` ``, a command substitution of an already-parsed variable -- tried
    to run a program literally named `CRT_SPAWN_NATIVE_WINDOWS=1`, got
    "not found", and silently concluded `with_gnu_ld=no`, which is what
    sent libtool down the wrong archiving path. Fixed by adding
    `tools/crt-native-tool`, a real wrapper script (the same proven
    pattern as `tools/crt-cc`'s own `$CC` value: `"<mksh> <script>"`) that
    does `export CRT_SPAWN_NATIVE_WINDOWS=1; exec "$tool" "$@"` --
    `AR`/`RANLIB`/`STRIP`/`LD` now point at `"<mksh> tools/crt-native-tool
    <real-tool>"`, which only ever needs the calling shell to word-split a
    command name from its arguments (always reliable).
  - Next blocker after that: `with_gnu_ld` was *still* `no` even with `$LD`
    now correctly probed as GNU-compatible -- a second, independent cause:
    libtool's own per-tag config has `case $host_os in cygwin*|mingw*...)
    if test yes != "$GCC"; then with_gnu_ld=no; fi`, and `$GCC` (was our
    compiler detected as GNU-compatible at all?) was also `no`. Root-caused
    to a stock Windows LLVM install's `clang.exe` defaulting to its own
    host triple, `*-pc-windows-msvc`, whenever no `--target` is given --
    that predefined-macro set doesn't define `__GNUC__` at all (only
    `_MSC_VER`, mimicking real MSVC), which autoconf's near-universal
    "checking whether the compiler supports GNU C" probe (and libtool's
    check earlier) both read as "not GNU". Fixed by adding an explicit
    `--target=aarch64-w64-mingw32` (arch auto-detected, `CRT_TARGET_ARCH`
    overridable) to `tools/crt-cc`/`tools/crt-c++` on Windows, matching
    what this project's own `--build=aarch64-w64-mingw32` recipe
    workaround already expects. Verified harmless to this project's own
    build (we pass `-nostdinc`/`-nostdlib` and our own `-isystem`/`-L`
    throughout, so the triple only affects predefined macros and calling-
    convention details, not which headers/libs get used).
  - With both of those fixed, `png.c` through the last `.c` file compiled,
    archived via the correct `$AR` path, and linked into `libpng16.a` and
    every `contrib/tools`/`contrib/libtests` sample binary -- `make`
    completed in full. Then `make install` hit one more, final blocker:
    `./libtool: ./install-sh: can't execute: Permission denied`. Root-
    caused to two compounding gaps, both now fixed in
    `libc/src/arch/windows/common/syscall.c`:
    1. `__crt_sys_posix_spawn()` (which `execve()` is itself implemented
       on top of, via `posix_spawn()` + `waitpid()` + `_exit()`) could
       previously only ever launch real PE binaries -- nothing in this
       project's PAL had ever taught it to interpret a `#!` shebang line,
       since every script run so far had always been invoked with its
       interpreter spelled out explicitly. `install-sh` (execed directly
       by libtool's own `--mode=install`, no interpreter prefix) is the
       first thing this project has hit that assumes shebang execution
       works at the OS level. Added `windows_read_shebang()` and taught
       `__crt_sys_posix_spawn()` to re-resolve and re-exec the named
       interpreter (Linux kernel semantics: at most one unsplit optional
       argument) when the target file starts with `#!`.
    2. That alone wasn't enough: mksh's own command dispatch
       (`search_access()` in `shell/mksh/src/exec.c`) checks
       `access(path, X_OK)` *before* ever calling `execve()`, and this
       project's `stat()`/`access()` emulation only ever reported
       `S_IXUSR`/etc. for files with a real PE `"MZ"` signature (Windows
       has no on-disk executable-permission bit at all, and NTFS ACLs
       don't map onto `S_IXUSR` either, so file *content* has always been
       the only signal available). A `#!` script has no MZ signature, so
       mksh rejected `install-sh` as "can't execute: Permission denied"
       before fix 1's shebang logic ever ran. Renamed the helper
       (`windows_handle_has_mz_signature` ->
       `windows_handle_looks_executable`) and taught it to also recognize
       a `#!` prefix, used by both `stat()`'s executable-bit computation
       and the PATH-search "is this a candidate executable" check.
    Verified: `ctest` 79/79 still passing at every step earlier (the
    `stat()`/`access()` change in particular is broad -- every executable-
    permission check in the whole Windows build goes through it).
  - **End-to-end result: libpng 1.6.57's real `configure && make && make
    install` now completes in full on Windows aarch64** -- `libpng16.a`/
    `libpng.a` built, archived, and installed into `PORT_PREFIX`, along
    with all of libpng's own `contrib/tools` and `contrib/libtests` sample
    binaries, `install-sh`-driven header/pkgconfig/man-page installation,
    and the `libpng16`->`libpng` alias-copy `install-exec-hook`/
    `install-data-hook` steps. This was the single longest blocker chain
    of this whole session -- ld, awk, a real printf bug, static-lib
    naming, `windows.h`, `arm_neon.h`, `AR`/`LD` shell-quoting, the GNU-
    ld/GNU-C misdetection, and finally shebang-script execution -- each
    one a real, general CRT/PAL/tooling gap fixed on its own merits, not
    a libpng-specific workaround.
  - Checked whether any of the fixes earlier are Windows-specific-only or
    could affect macOS/Linux (couldn't literally build for those hosts
    from this Windows aarch64 machine -- static review only): the
    `lseek()` fix lives entirely in `libc/src/arch/windows/common/
    syscall.c` (macOS/Linux use raw `syscall.S` kernel syscalls, which
    already return `ESPIPE` for pipes correctly, unaffected); the `LD`
    env var fix is explicitly `if target_os == "windows"`-scoped; the
    regex engine port, `reallocarray()`, `MB_LEN_MAX`, and the `egrep`/
    `fgrep` alias additions all live in shared, non-OS-forked files with
    no Windows-specific API references, so they apply identically (and
    identically safely, given `NLS` is deliberately never defined
    regardless of platform) across all three targets.

## 2026-08-04

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

## 2026-08-03

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

## 2026-08-02

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

