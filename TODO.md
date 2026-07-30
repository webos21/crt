# TODO: Android-like Shell Environment

This note records the current diagnosis and plan for managing shell/userland
source as part of the CRT port-test loop.

## Direction

The proposed direction fits the project goal: run an Android-like shell and
command environment on top of the Bionic-compatible CRT/PAL, rather than relying
forever on MSYS, Git Bash, WSL, or another foreign Unix runtime for upstream
`configure` scripts.

Android treats `mksh` and `toybox` as separate projects:

- `external/mksh`
- `external/toybox`

The natural first target is:

```text
/system/bin/sh -> mksh
/system/bin/<applets> -> toybox
```

Toybox's own shell can remain a later comparison point, but the first Android-
shaped shell tranche should use `mksh` plus enough toybox applets to run simple
configure scripts.

## What Already Exists In CRT

The current CRT already has enough surface to start probing shell/userland
ports:

- `posix_spawn`, `posix_spawnp`, `waitpid`, `wait`
- `execve` as a documented unsupported Windows policy
- rootfs path mapping on Windows for paths such as `/tmp`, `/system/bin`,
  `/dev/null`, and `/proc/self/exe`
- `pipe`, `dup`, `dup2`, `open`, `close`, `read`, `write`, `lseek`
- `stat`, `lstat`, `opendir`, `readdir`, `closedir`
- `poll`, `select`, `isatty`, selected `ioctl`
- stdio, scanf/printf, malloc, environment variables, locale, wchar, pthread
- sockets and basic process/signal tests

This is enough for direct compile/link probes and some non-interactive process
smoke tests, but not enough for a full shell environment yet.

## Major Missing Areas

### 1. fork/exec Model

Shells assume a Unix process model. Pipelines, command substitution, subshells,
and redirections often assume `fork` plus `exec`.

Windows cannot faithfully implement POSIX `fork()` over `CreateProcess`.
Current CRT policy is that `execve()` returns `ENOTSUP` on Windows because it
cannot replace the current process image.

Options:

- first validate mksh on Linux/macOS CRT where `fork`/`execve` are closer to the
  host model;
- on Windows, investigate whether mksh can be driven through a `posix_spawn`
  path for the non-interactive configure-script use case;
- postpone full interactive shell/job-control behavior until the process model
  is clearer;
- avoid patching upstream first; prefer filling CRT/PAL gaps unless the
  required behavior is fundamentally unavailable on Windows.

### 2. FD Inheritance And close-on-exec

Shells need descriptor inheritance and redirection to work predictably:

- `2>&1`
- pipes
- here-docs
- child fd passing
- `FD_CLOEXEC`
- `F_DUPFD_CLOEXEC`
- nonblocking flags where scripts or applets probe them

The Windows backend needs a child CRT startup path that can import a serialized
parent fd table. The current `posix_spawn` file actions are useful, but the
long-term model must extend beyond only stdin/stdout/stderr.

### 3. Signal And Job Control

Minimum non-interactive shell work likely needs:

- `sigaction`
- `sigprocmask`
- `sigemptyset`
- `sigfillset`
- `sigaddset`
- `sigdelset`
- `sigismember`
- `SIGCHLD`
- `SIGINT`
- `SIGTERM`
- `SIGPIPE`

Interactive shell/job-control work additionally needs:

- `setpgid`
- `getpgrp`
- `tcgetpgrp`
- `tcsetpgrp`
- `killpg`
- stopped child status
- `WUNTRACED`
- process group semantics

Windows needs a documented console Ctrl-C/Ctrl-Break mapping policy.

### 4. Terminal, tty, And pty

For line editing and interactive behavior, mksh will need terminal APIs:

- `termios.h`
- `tcgetattr`
- `tcsetattr`
- `TIOCGWINSZ`
- `/dev/tty`
- possibly pty/ConPTY support later

The first configure-script shell can defer most interactive tty behavior, but
`isatty`, `ctermid`, `/dev/tty`, and window-size `ioctl` behavior should remain
coherent.

### 5. User, Group, And Resource Database

mkshrc, toybox applets, and configure probes are likely to expose:

- `pwd.h`
- `grp.h`
- `getuid`
- `geteuid`
- `getgid`
- `getegid`
- `getpwuid`
- `getpwnam`
- `getgrgid`
- `getgrnam`
- `sys/resource.h`
- `getrlimit`
- `setrlimit`

Windows should use synthetic uid/gid/resource-limit policy first, documented as
PAL behavior. Avoid exposing native Windows account/SID shapes through public
CRT headers.

### 6. Filesystem And Rootfs Expansion

The runtime rootfs should grow deliberately:

```text
/system/bin/sh
/system/bin/toybox
/system/etc/mkshrc
/bin
/usr/bin
/tmp
/data/local/tmp
/dev/null
/dev/tty
/proc/self/exe
/proc/self/fd
```

Toybox applets may eventually require virtual files such as:

- `/proc/mounts`
- `/proc/stat`
- `/proc/self/status`

Add virtual files narrowly as ports require them. Do not pretend to provide a
complete Linux procfs/devfs.

### 7. libc Header And Function Gaps

Likely upcoming public CRT surface:

- `termios.h`
- `sys/resource.h`
- `pwd.h`
- `grp.h`
- `fnmatch.h`
- `glob.h`
- `regex.h`
- `getopt`
- `getopt_long`
- `mkstemp`
- `mkdtemp`
- `umask`
- `chmod`
- `fchmod`
- `chown`
- `lchown`
- `fchown`
- `wcwidth`
- `wcswidth`

Every new public header/type/macro/symbol should be checked against Android
Bionic first.

## libm, libdl, And linker Implications

`mksh` itself probably does not need much `libm`.

Toybox applets may need more `libm` depending on which applets are enabled. Keep
the first toybox config minimal and add applets gradually.

`libdl` is probably not central to the first shell tranche.

The dynamic linker should not block the first milestone. Build `mksh` and
minimal toybox as static CRT executables first. Android-like shared `/system/bin`
behavior can wait until shared ABI/export policy and the project linker/loader
direction mature.

## Proposed Milestones

### Milestone 1: Source And Recipe Pinning

Add recipes:

- `porting/recipes/mksh-android.json`
- `porting/recipes/toybox-minimal.json`

Record:

- upstream URL;
- tag or commit;
- archive/source name;
- hash;
- Android build flags or selected config;
- host status.

### Milestone 2: mksh Compile Inventory

Use Android `external/mksh` source and Android build flags as the reference.
Compile with `crt-cc` and record missing:

- headers;
- types;
- macros;
- symbols;
- errno behavior;
- process/runtime assumptions.

Expected early gaps:

- `termios.h`
- `sys/resource.h`
- `pwd.h`
- `grp.h`
- signal APIs
- process/fd inheritance behavior

### Milestone 3: Non-interactive Shell

Goal:

```sh
sh -c 'echo ok'
```

Then validate:

- variable assignment;
- `for`;
- `case`;
- shell functions;
- basic redirection;
- simple external command invocation.

Explicitly defer:

- job control;
- pty;
- full line editing;
- full signal mask semantics.

### Milestone 4: Pipeline And Subshell

Goal:

```sh
echo hi | cat
x=$(echo hi)
(echo hi)
cat <<EOF
hi
EOF
```

This milestone requires the Windows process/fd model to become substantially
stronger:

- pipe inheritance;
- child fd table serialization;
- redirection actions beyond standard descriptors;
- close-on-exec behavior.

### Milestone 5: Toybox Minimal Applets

Start with a minimal applet set:

- `cat`
- `echo`
- `pwd`
- `true`
- `false`
- `mkdir`
- `rm`
- `cp`
- `mv`
- `test`
- `expr`
- `sed`

Defer applets that need deeper host integration:

- `ps`
- `mount`
- `ifconfig`
- `stty`
- `login`
- full device/procfs-heavy commands

### Milestone 6: Configure Smoke

Run configure scripts through the CRT shell/rootfs:

```sh
/system/bin/sh configure --help
```

Then:

- zlib `./configure`
- libpng `./configure`
- SQLite configure or shell build path

Record all failures as CRT/PAL/sysroot work items before considering upstream
patches.

## Recommended First Target

Do not start with "fully interactive Android shell".

Start with:

```text
non-interactive /system/bin/sh capable of running simple configure scripts
```

Then add toybox applets and rootfs features as configure workloads demand them.
