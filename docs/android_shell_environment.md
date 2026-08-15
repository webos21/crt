# Android-like Shell Environment

## Goal

The long-term porting-test environment should not depend on MSYS, Git Bash, WSL,
or another foreign Unix runtime to execute upstream `configure` scripts. Those
tools are useful bootstrap aids, but the project direction is to run a
Linux/Android-style command environment on top of this Bionic-compatible
CRT/PAL.

The intended shape is:

```text
Windows Terminal / host launcher
  -> CRT-built /system/bin/sh
    -> Bionic-compatible libc/PAL
      -> Android-like rootfs
        -> /system/bin/sh and command applets
          -> configure, make, and library build scripts
```

This keeps the build shell in the same compatibility world as the libraries
being ported. The host OS still provides kernel facilities through PAL backends,
but source packages see Bionic/POSIX-shaped headers, paths, process APIs, and
runtime behavior.

## Core Artifact Policy

The shell is a first-class project artifact, not an ordinary third-party
porting recipe. Source lives under `shell/`, at the same architectural level as
`libc/`, `libm/`, `libdl/`, `libstdc++/`, and `linker/`.

The intended project-owned outputs are:

```text
shell/tiny_sh -> rootfs/system/bin/sh bootstrap runner
shell/mksh   -> rootfs/system/bin/mksh, later rootfs/system/bin/sh
shell/toybox -> rootfs/system/bin/toybox and selected applet entry points
```

`porting/recipes/` should continue to describe external libraries and tools that
are tested against the CRT. It should not own `mksh` or `toybox` as ordinary
porting targets, because those programs become part of the runtime environment
used by the porting tests themselves.

## Sysroot vs Rootfs

The compiler sysroot and runtime rootfs are intentionally separate:

- `out/<preset>/sysroot`
  - compiler-facing headers, libraries, startup objects, and runtime archives;
  - used by `tools/crt-cc`, `tools/crt-c++`, CMake, and package builds.
- `out/<preset>/rootfs`
  - process-facing runtime filesystem namespace;
  - used by shell commands through `CRT_ROOTFS`;
  - contains Android-like directories such as `/system/bin`, `/tmp`, `/dev`,
    `/proc`, `/data`, and `/home`.

The initial CMake target is:

```sh
cmake --build --preset windows-host-ninja-debug --target rootfs
```

It creates:

```text
out/windows-host-ninja-debug/rootfs/
  system/bin/
  system/etc/
  system/lib/
  bin/
  usr/bin/
  usr/lib/
  tmp/
  dev/
  proc/
  proc/self/
  proc/self/fd/
  data/
  data/local/
  data/local/tmp/
  home/
```

When `crt_shell_artifacts` is built, the bootstrap shell is copied into:

```text
rootfs/system/bin/sh
rootfs/bin/sh
rootfs/usr/bin/sh
```

On Windows the rootfs also receives `.exe` copies for host launch convenience,
while the extensionless names remain available for CRT path translation.

Android mksh is also copied to:

```text
rootfs/system/bin/mksh
rootfs/system/etc/mkshrc
```

**Update: `/system/bin/sh` (and `/bin/sh`, `/usr/bin/sh`) is now mksh, not
`crt_tiny_sh`.** `tools/create_rootfs.py`'s `--mksh` flag (passed by every
current rootfs build) installs mksh to `system/bin/mksh` and aliases/copies
it over `sh` in all three shell directories, superseding the `tiny-sh`
install described above (which still gets installed, under the name
`tiny-sh`, not `sh`, once mksh is present). See "Bootstrap Tiny Shell
Status" below for `crt_tiny_sh`'s current, now-historical role.

## Initial Namespace Policy

On Windows, `CRT_ROOTFS` enables POSIX absolute path mapping in the PAL:

- `/tmp/...` maps under `<CRT_ROOTFS>/tmp/...`;
- `/system/bin/...` maps under `<CRT_ROOTFS>/system/bin/...`;
- `/dev/null` maps to the Windows null device;
- `/proc/self/exe` maps to the current executable path;
- native absolute paths such as `C:\...` remain native escape paths.

This is a bootstrap namespace, not a full virtual filesystem. As shell and
configure workloads expand, add narrowly documented virtual paths rather than
pretending that every Linux procfs/devfs entry exists.

### `/proc/self/exe` On macOS

Linux resolves `/proc/self/exe` natively (a real kernel-provided symlink), so
`libc/src/process.c`/`libc/src/fd.c` pass it straight through unchanged
there. macOS has no `/proc` filesystem at all -- not a namespace-policy
choice, a real host fact (`ls /proc` itself fails with `ENOENT` on real
macOS) -- so a literal `"/proc/self/exe"` handed to a raw `execve(2)`/
`posix_spawn(2)`/`access(2)`/`stat(2)` syscall there always fails with
`ENOENT` too.

This went unnoticed for a while because the only *self-relaunch* code that
exercised it in `ctest` runs (`tests/windows_fd_snapshot_test.c`'s fork+exec
subtest, `tests/rootfs_process_test.c`'s `/proc/self/exe` checks) turned out
to not actually run that code path on macOS the way it looked at a glance --
easy to misread when skimming, since the guard is a `#if` several lines above
the call site, not right next to it. `tests/process_stress_test.c`
(many concurrent `posix_spawn()`-based workers self-relaunching via
`/proc/self/exe`, portable by design so it runs on every host) was the first
test to genuinely exercise this on macOS, and it failed with every worker
exiting `127` -- this project's `posix_spawn()` is built on `fork()` +
`execve()` (`libc/src/process.c`'s `__crt_sys_posix_spawn()`), and a failed
`execve()` there falls through to `_exit(127)`, the conventional
"exec failed" code.

Fixed by resolving `/proc/self/exe` through `_NSGetExecutablePath()` (the
real Mach-O API for "what is my own running executable's path", declared
locally per this project's usual convention for macOS host APIs -- e.g.
`time.c`'s `clock_gettime_nsec_np()` -- rather than including the real
`<mach-o/dyld.h>` SDK header) in both places this project resolves paths
before syscalls:

- `libc/src/process.c`'s `translate_exec_path_for_rootfs()`, used by
  `execve()`/`posix_spawn()`;
- `libc/src/fd.c`'s `rootfs_path_for_host()`, used by `open()`/`stat()`/
  `access()`/etc., so non-exec uses (e.g. a `readlink("/proc/self/exe", ...)`-
  style "find my own path" idiom, which `shell/toybox/src/lib/portability.c`
  uses) resolve correctly too, not just exec targets.

Windows already had the equivalent fix (`GetModuleFileNameA()` in
`libc/src/arch/windows/common/syscall.c`); macOS simply never got its own
version until this was tracked down.

## Shell Candidate Order

1. `mksh`
   - Strong Android precedent for `/system/bin/sh`.
   - Good first candidate for an Android-shaped command environment.
   - Needs careful review of process creation assumptions on Windows.

2. `toybox`
   - Strong Android precedent for command applets.
   - Natural candidate for `/system/bin/cat`, `/system/bin/ls`, `/system/bin/sed`
     and similar commands.
   - `toybox sh` should be evaluated, but command coverage and configure
     compatibility need testing.

3. `busybox`
   - Broad command coverage and familiar configure bootstrap behavior.
   - Less Android-specific than toybox, but useful as a compatibility benchmark.
   - May carry stronger `fork` assumptions depending on applet and shell mode.

The recommended first shell tranche is `mksh` plus enough toybox applets to run
simple configure scripts. BusyBox remains a fallback or benchmark.

The source location policy is:

- `shell/tiny_sh`: project-owned `crt_tiny_sh` bootstrap runner.
- `shell/mksh`: Android `external/mksh` import and project-owned glue.
- `shell/toybox`: Android `external/toybox` import, minimal config, and
  project-owned glue.
- `porting/recipes`: external packages that consume the CRT shell.

## Process API Tranche

**Update: this section describes the process API surface as it stood before
Windows `fork()` was implemented for real -- see `docs/process_fork.md` and
`docs/windows_fork_emulation.md` for the current, actual state (a real,
general-purpose Cygwin/MSYS-style memory-copy `fork()`, verified on both
Windows architectures, not limited to the shell-child-spec case described
below).** Kept for context on the process API bootstrap sequence; treat
anything below that says Windows lacks `fork()` as historical.

Linux and macOS shell work should continue to exercise the normal Unix
`fork()` path. Android mksh, toybox applets, configure scripts, pipelines,
redirections, and command substitution all use that process model as their
baseline.

Windows cannot obtain full POSIX `fork()` semantics from `CreateProcessA`
alone. For the mksh/toybox milestone, the supported Windows contract is a
CRT-owned shell child spec for the common fork-then-exec case. The compatibility
boundary still lives in libc/PAL: mksh/toybox glue may route eligible Windows
child launches through that helper, but it must not expose host SDK process
semantics as public Bionic ABI.

The first process APIs are:

- `posix_spawn()` / `posix_spawnp()`;
- `waitpid()` / `wait()`;
- `execve()`, kept as an OS backend boundary.

Current Windows bootstrap behavior:

- `posix_spawn()` uses `CreateProcessA`;
- `posix_spawnp()` asks the Windows loader path search to resolve simple program
  names, matching the role of Bionic's `execvpe` path in broad intent;
- child process handles are tracked internally until `waitpid()`;
- `waitpid()` supports blocking waits and `WNOHANG`;
- exit status is returned in POSIX wait-status form;
- `execve()` is supported only for the shell-child contract: it spawns the
  target with the CRT child bootstrap path, waits for it, and exits the current
  process with the child status. This is useful for shell `exec` flow but does
  not claim Bionic/Linux in-place image replacement semantics;
- Bionic-shaped `posix_spawnattr_*` and `posix_spawn_file_actions_*` objects are
  available as opaque pointer types;
- file actions are applied for the standard descriptors that `CreateProcessA`
  can model through `STARTUPINFO`: `addopen`, `addclose`, `adddup2`, and
  `addchdir_np` are supported for the bootstrap shell path when they affect
  stdin/stdout/stderr or current directory;
- `POSIX_SPAWN_SETPGROUP` and `POSIX_SPAWN_SETSID` are approximated with a new
  Windows process group;
- explicit `envp` blocks are converted to ANSI Windows environment blocks.

Next required process improvements (as of when this section was written --
most of these were later resolved by the real `fork()` work; check
`TODO.md`/`HISTORY.md` before treating any of these as still open):

- implement `posix_spawn_file_actions_*` for stdin/stdout/stderr and
  non-standard fd redirection;
- apply Bionic-style spawn attributes where host semantics exist;
- extend file actions beyond standard descriptors by teaching child CRT startup
  how to import a serialized fd table;
- implement `posix_spawn_file_actions_addfchdir_np` once `fchdir()` is available;
- refine signal mask/default and scheduler attributes as those libc surfaces
  mature;
- grow `__crt_shell_fork_exec()` into the Windows child-spec contract for
  external commands, pipelines, fd 3+ redirections, command substitution, and
  exec-builtin-like replacement.

See `docs/process_fork.md` for the fork contract and the current Windows
`fork()` implementation.

## Bootstrap Tiny Shell Status (historical)

**Update: mksh is now the default `/system/bin/sh` (see "Sysroot vs Rootfs"
above); this section describes `crt_tiny_sh`'s original bootstrap role
before mksh was imported, kept for context, not current status.**

`crt_tiny_sh` was the first shell artifact. It existed to exercise the CRT
process model before importing Android `external/mksh`. It is still built
and installed (as `tiny-sh`, not `sh`, once mksh is present -- see the
"Sysroot vs Rootfs" update above), and still fills its original bootstrap
role for early smoke coverage, but mksh is what actually runs configure
scripts and rootfs shell workloads now.

Its bootstrap surface, unchanged since:

- `sh -c "..."`;
- simple tokenization and quotes;
- `;`, `&&`, `||`;
- pipeline and redirection smoke coverage;
- `$?` and simple `$VAR` expansion;
- leading `VAR=value` assignments;
- builtins: `echo`, `cat`, `upper`, `pwd`, `cd`, `true`, `false`, `exit`.

This was never meant to be the final shell. Missing shell features became
mksh/toybox import inventory items rather than tiny-shell growth, as
intended.

## Porting-Test Direction

The porting loop remains:

1. Run upstream configure or a direct compile/link/run probe.
2. Identify missing CRT/PAL/sysroot behavior.
3. Check Bionic's header/source/ABI policy first.
4. Extend this CRT/PAL toward Bionic-compatible behavior.
5. Re-run the unmodified upstream package.

For shell/userland work, the same rule applies: do not patch mksh, toybox, or
BusyBox first. Fill the CRT/PAL/rootfs gaps unless the upstream code depends on
behavior that is fundamentally unavailable on the host.
