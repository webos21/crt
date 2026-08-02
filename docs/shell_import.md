# Shell Import And Bootstrap Policy

## Goal

The CRT shell is a core runtime artifact. It is not an ordinary porting recipe.
Porting tests should eventually run upstream build scripts through a shell that
is itself built against this Bionic-compatible CRT/PAL.

The target Android-shaped layout is:

```text
/system/bin/sh      -> mksh
/system/bin/toybox  -> toybox
/bin/sh             -> shell launcher/copy
/usr/bin/sh         -> shell launcher/copy
```

`crt_tiny_sh` remains a project-owned bootstrap runner installed as
`/system/bin/sh`. Its role is to exercise the CRT process/fd/signal/rootfs
contract and to provide early configure smoke coverage while imported mksh is
being hardened.

## Upstream References

The first source references are Android platform projects:

- `platform/external/mksh`
  - URL: `https://android.googlesource.com/platform/external/mksh`
  - branch: `refs/heads/main`
  - observed tree during planning: `57ba8b3ab85c6d79171453c42baec1e845d4e30a`
- `platform/external/toybox`
  - URL: `https://android.googlesource.com/platform/external/toybox`
  - branch: `refs/heads/main`
  - observed tree during planning: `6faafc783dbeda82e582dd4241d3b788a91dc827`

mksh is currently pinned to Android external/mksh commit
`1548076841f243748a3f56da23b38794d437bc12`, tree
`57ba8b3ab85c6d79171453c42baec1e845d4e30a`. Exact import pins are recorded as
immutable commit IDs plus archive SHA256 in
`shell/mksh/import_manifest.json`. Toybox remains at the planning-reference
stage until its source is imported.

## Source Layout

Use the following layout:

```text
shell/src/          project-owned bootstrap shell runner
shell/mksh/src/     imported Android external/mksh source
shell/mksh/glue/    project-owned mksh build/config glue
shell/toybox/src/   imported Android external/toybox source
shell/toybox/glue/  project-owned toybox config/build glue
```

Imported upstream source should stay separate from project-owned glue. If a
patch becomes unavoidable, keep it outside the imported source tree first and
record:

- the upstream file and commit;
- the behavior being changed;
- why the gap cannot reasonably be filled in CRT/PAL/sysroot;
- the condition for removing the patch later.

The first mksh exception is Windows LLP64. mksh assumes pointer-sized values fit
in `long`, which is true for Android/Linux/macOS LP64 and Android 32-bit, but
not Windows x86_64. The CRT build defines `MKSH_CRT_ALLOW_LLP64` only for the
Windows mksh target and carries a small guarded source adjustment to use
`intmax_t`/`uintmax_t` in mksh's internal formatter. This should be revisited if
mksh upstream gains a native LLP64 path.

## Build Policy

The first imported shell/userland binaries should be static CRT executables.
Shared runtime behavior under `/system/bin` can wait until the linker/loader
direction is stronger.

The build must use:

- CRT public headers from the active sysroot;
- CRT `crt1.o`;
- CRT `libc.a`, and other CRT libraries only where needed;
- the same `-nostdlib`, `-nostartfiles`, and `-nodefaultlibs` boundary used by
  CRT tests.

Do not expose host libc headers or link host libc libraries to make `mksh` or
`toybox` pass.

## Tiny Shell Scope

`crt_tiny_sh` is intentionally not a full shell. It may grow only enough to:

- smoke-test CRT process behavior;
- run very small configure probes;
- bridge the project until `mksh` is available.

Currently expected bootstrap surface:

- `sh -c "..."`;
- simple tokenization and quotes;
- `;`, `&&`, `||`;
- `|` pipelines;
- `<`, `>`, and single-digit `n>` redirection;
- `$?` and simple `$VAR` expansion;
- leading `VAR=value` assignments;
- builtins for early smoke: `echo`, `cat`, `upper`, `pwd`, `cd`, `true`,
  `false`, `exit`.

Do not turn `crt_tiny_sh` into the final shell. Features such as full POSIX
parameter expansion, globbing, command substitution, here-docs, arithmetic
expansion, functions, traps, and interactive job control should be used to drive
the `mksh` tranche and CRT/PAL gap work.

Imported mksh currently builds as `crt_mksh` and installs as `mksh`. It does
not replace `crt_tiny_sh` as `/system/bin/sh` until all host smoke tests pass
and the remaining shell/process gaps are understood.

## mksh Inventory Tranche

The source-facing inventory is now active:

1. Fetch Android `external/mksh` at the selected commit.
2. Read `Android.bp`, `Android.patch.txt`, `mkshrc`, `NOTICE`, and the source
   build flags.
3. Compile with the CRT freestanding CMake target and later with `tools/crt-cc`
   against the CRT sysroot.
4. Record missing public surface under the CRT categories:
   - headers and types;
   - process/fd/signal behavior;
   - terminal/job-control behavior;
   - user/group/resource database behavior;
   - rootfs/procfs/devfs behavior.
5. Implement missing pieces in CRT/PAL/sysroot by checking Bionic first.

The first inventory pass reached a linked macOS `crt_mksh` binary without
patching imported source. The added CRT/PAL/sysroot surface includes resource,
user/group, termios, langinfo, sysmacros, times, file-lock, signal-name, sleep,
and stat-timespec support.

## Toybox Minimal Applet Inventory

The first toybox applet set should be configure-oriented:

```text
cat echo pwd true false test [ expr basename dirname
mkdir rm cp mv ln chmod uname sed grep
```

Defer applets that primarily validate deeper host integration:

```text
ps mount ifconfig stty login top dmesg losetup modprobe
```

Toybox applet failures should feed the normal porting loop: identify the missing
Bionic-compatible surface, implement it in CRT/PAL/sysroot, then rerun without
modifying upstream source.

The first imported toybox tranche is now present under `shell/toybox/src` with a
project-owned CRT overlay under `shell/toybox/crt`. The overlay narrows
Android's generated toybox configuration to the current source/appset boundary
and disables applets that would pull in zlib, mount/procfs, process table, or
namespace dependencies before the CRT/PAL owns those surfaces.

Android does not put zlib in Bionic libc itself. `zlib.h`/`zconf.h` and `libz`
come from Android `external/zlib` as a separate public library surface, while
some platform components such as the dynamic linker may statically link it.
Follow the same split here: provide zlib as a sysroot/runtime library when the
porting or shell applet surface needs it, and only embed it privately in a
component such as the linker if that component has a direct internal dependency.

Rootfs generation now uses Android-like symlink aliases on POSIX hosts:

```text
/system/bin/sh      -> mksh
/bin/<applet>       -> ../system/bin/toybox
/usr/bin/<applet>   -> ../../system/bin/toybox
```

Windows keeps copy-based `.exe` aliases because symlink creation has different
privilege and UX tradeoffs there.

The remaining shell milestone is not "make toybox compile"; that part is
working for the minimal applet set. On Windows x86_64, rootfs mksh can now run
single external toybox applets such as `ls /system/bin`, `ls -s /`, `ls -l /`,
and `toybox.exe ls -al /` when `CRT_ROOTFS` and a POSIX-style
`PATH=/system/bin:/bin:/usr/bin` are provided.

The Windows fixes that made this work are intentionally in CRT/PAL or
shell-local source compatibility:

- Windows `posix_spawn` resolves the host executable path separately from the
  child command line so applets keep the intended POSIX `argv[0]`.
- Windows `stat()` marks extensionless PE/MZ rootfs aliases executable, matching
  `access(X_OK)`.
- Windows `openat()` and `fdopendir()` can resolve directory fds back to host
  paths, which toybox directory traversal expects.
- toybox `dirtree.extra`, `ls`, the common option parser, and active
  pointer-tagging paths use pointer-width storage where values may hold
  pointers. Windows direct invocation also accepts `\`-separated `.exe` applet
  paths. These avoid LLP64 pointer truncation without changing CRT public ABI.
  Because upstream Android/Linux toybox can assume LP64 for its 64-bit targets,
  additional LLP64 cleanups should be made in the imported toybox source/glue
  rather than hidden behind Windows CRT ABI changes.

The first Windows mksh child-spec adapter is now active. Rootfs mksh can run
single external commands, external-command pipelines, builtin-to-external
pipelines such as `echo hello | tr a-z A-Z`, and basic `<`/`>` redirection
against CRT toybox applets. The adapter keeps the compatibility boundary in the
CRT shell process contract: Windows `TEXEC` launch goes through
`__crt_shell_fork_exec()`, while raw arbitrary post-fork child execution remains
unsupported.

Configure-stage shell coverage has started. Windows rootfs mksh can run zlib
1.3.1 `./configure --static` through `tools/crt-port-build.py --use-crt-shell
--configure-only`. The work needed for that path added rootfs applets commonly
used by configure scripts (`date`, `expr`, `printf`, `tee`) and a Windows mksh
local fallback for grouped commands in pipelines, such as
`( command ) 2>&1 | tee -a configure.log`, so the actual external command still
travels through the CRT child-spec spawn path.

The child-spec path must continue to carry Bionic-shaped process/fd/signal
behavior: cwd/rootfs/env, file actions including fd 3 and above,
close-on-exec filtering, stdio flush policy, child registry/waitpid
integration, and socket fd transport through `WSADuplicateSocketA()` when
needed.

Real Windows `fork()` remains a long-term research tranche. The mksh/toybox
milestone should next focus on the remaining shell patterns around fd 3+
redirections inside mksh, background commands, and broader configure-script
coverage. Also keep auditing disabled toybox applets for remaining LLP64
pointer-to-`long` assumptions before enabling them.
