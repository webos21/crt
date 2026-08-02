# TODO: CRT Shell, Rootfs, And Porting Loop

This file tracks remaining work for the Android-like shell/rootfs environment
used by CRT porting tests. Completed historical milestones are intentionally
removed from this file; provenance and policy details belong in `docs/` and
import manifests.

## Current Baseline

- `shell/` is a core CRT artifact area, not a third-party port recipe.
- `crt_tiny_sh`, Android `external/mksh`, and Android `external/toybox` are
  built by CMake.
- The generated runtime rootfs contains Android-like paths such as
  `/system/bin`, `/bin`, `/usr/bin`, `/tmp`, `/dev`, and `/proc/self`.
- `mksh` and a minimal toybox applet set are installed into the rootfs.
- Windows uses copy-based applet aliases; Linux/macOS use symlink aliases.
- `posix_spawn`, `waitpid`, fd snapshot transport, rootfs path translation, and
  the first shell child process contract exist.
- Windows public `fork()` remains unsupported today; making mksh work correctly
  on Windows requires implementing fork semantics in libc/PAL, not adding
  mksh-specific spawn shortcuts.

## Immediate Verification

- Windows x86_64 build and CTest currently pass with the shell imports:
  `ctest --preset windows-host-ninja-debug --output-on-failure` reports 72/72.
- With `CRT_ROOTFS` set to the generated rootfs and
  `PATH=/system/bin:/bin:/usr/bin`, Windows rootfs mksh now verifies:
  - `ls /system/bin`
  - `ls -s /`
  - `ls -l /`
  - `toybox.exe ls -al /`
  - `cd /system/bin; test -x ./ls; test -x ./ls.exe`
- Windows direct rootfs applet invocation now also verifies:
  - `system/bin/ls.exe -l /`
  - `system/bin/toybox.exe ls -s /`
  - `system/bin/sed.exe 'N;s/\n/-/'`
  - `system/bin/toybox.exe getopt -o a:b -- -a hello -b world rest`
- Remaining Windows mksh failures are the flows that require mksh's normal
  `fork()` path:
  - `cd /system/bin; ./ls`
  - `echo hi | cat`
  - `echo hi > /tmp/a; cat /tmp/a`
- Continue with the Windows `fork()` tranche in libc/PAL. Do not add
  mksh-specific `posix_spawn` bypasses.
- The active toybox tranche has LLP64 fixes for `dirtree.extra`, `ls`, the
  common option parser, Windows applet path lookup, and known active
  pointer-tagging paths. Keep auditing disabled applets for remaining
  pointer-to-`long` assumptions before enabling more applets. Fix these in
  toybox source/glue as pointer-width portability issues, not by changing CRT
  public ABI.
- Treat zlib as Android `external/zlib`-style sysroot/runtime library surface,
  not as part of Bionic libc. Link it privately into components such as the
  dynamic linker only if the component itself needs an internal static
  dependency.

## Windows Fork Tranche

- Design and implement Windows `fork()` in libc/PAL so unmodified mksh can use
  its normal `fork()` path.
- Preserve the Bionic/POSIX contract that child execution resumes at the
  `fork()` call site with return value `0`.
- Reuse existing Windows child bootstrap pieces:
  - `CreateProcess`
  - fd snapshot export/import
  - cwd/rootfs/env propagation
  - signal mask/default propagation
  - child registry and `waitpid()`
- Add the missing fork-resume machinery:
  - saved register/context state;
  - stack mapping/copy policy;
  - writable runtime/data segment policy;
  - TLS/current-thread reset in the child;
  - malloc/pthread/stdio/fd after-fork reset hooks;
  - ASLR/base-address constraints or documented failure mode.
- Keep mksh source changes limited to already-required Windows ABI/build
  compatibility. Do not special-case mksh external commands around `fork()`.

## Shell Process Model After Fork

- Validate mksh command execution through libc `fork()`:
  - simple external commands;
  - `cmd > file`;
  - `cmd < file`;
  - `2>&1`;
  - fd 3 and higher;
  - `cmd | cmd`;
  - multi-stage pipeline teardown;
  - child exit status propagation.
- Keep Linux/macOS on the normal native `fork()` path.

## Signal, Wait, And Job Control

- Harden `waitpid()` for multiple shell children and pipeline children.
- Add `SIGCHLD`-oriented tests for completed children.
- Decide whether Windows child notification remains polling/wait-handle based
  or grows a signal-like event bridge.
- Improve interactive job-control approximation:
  - Ctrl-C / Ctrl-Break delivery policy
  - foreground process group behavior
  - stopped-child status policy
- Keep non-interactive configure-script behavior as the first priority.

## Terminal And TTY

- Keep `/dev/tty` and `/dev/console` coherent on all hosts.
- Improve Windows console handling for:
  - `isatty`
  - `tcgetattr` / `tcsetattr`
  - `TIOCGWINSZ`
  - close-on-exec behavior on console fds
- Add mksh interactive smoke tests only after non-interactive command execution
  is stable.

## Toybox Applet Expansion

Keep the enabled toybox applet set minimal and configure-oriented. Add applets
only when the backing Bionic-compatible CRT/PAL surface is present.

Before enabling gzip/gunzip/zcat or other compression-heavy applets, decide
whether to use toybox's built-in deflate implementation or the sysroot `libz`.
Do not move zlib into libc for this; Android exposes it separately as `libz`.

Next likely applets:

- `which`
- `readlink`
- `stat`
- `printf`
- `date`
- `touch`
- `chmod`
- `ln`
- `grep`
- `sed`
- `test`
- `expr`

Defer applets that require deeper Linux-like host integration:

- `ps`
- `mount`
- `df`
- `ifconfig`
- `stty`
- `login`
- device-manager or procfs-heavy commands

## Virtual Rootfs Files

Add virtual files narrowly as shell or porting workloads require them. Do not
pretend to provide a complete Linux procfs/devfs.

Likely next virtual paths:

- `/proc/mounts`
- `/proc/self/status`
- `/proc/self/cmdline`
- `/proc/self/environ`
- `/proc/stat`
- `/dev/zero`
- `/dev/random`
- `/dev/urandom`

## Porting Loop Integration

- Switch selected porting smoke tests from host shell usage to CRT rootfs shell
  once mksh plus toybox can run simple configure scripts.
- Start with configure-only probes:
  - `configure --help`
  - zlib `./configure`
  - libpng `./configure`
  - SQLite build probes
  - libffi configure probes
- For every failure:
  1. identify missing header/type/macro/symbol/behavior;
  2. check Android Bionic public headers and implementation;
  3. extend CRT/PAL/sysroot rather than patching upstream first;
  4. record host-specific policy differences in `docs/`.

## Known CRT/PAL Gaps Exposed By Shell Work

- Windows `fork()` emulation remains the main shell blocker.
- Shell redirection and pipeline handling on Windows should be validated through
  mksh's normal `fork()` path after libc/PAL fork support lands.
- Full interactive job control is not complete.
- `/proc` and `/dev` are intentionally partial.
- Some toybox applets remain disabled until supporting APIs are implemented.

## Documentation To Keep Current

- `docs/android_shell_environment.md`
- `docs/process_fork.md`
- `docs/windows_fork_emulation.md`
- `docs/job_control.md`
- `docs/sysroot_ports.md`
- `shell/mksh/README.md`
- `shell/toybox/README.md`
- `shell/*/import_manifest.json`
