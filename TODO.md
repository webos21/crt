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

## in progressing

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

- Run the next configure/make/install targets through project-owned mksh and
  make on Windows:
  - libpng after zlib;
  - libffi;
  - SQLite follow-up builds beyond the current amalgamation smoke.
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
