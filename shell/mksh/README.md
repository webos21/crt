# mksh

This directory contains the Android `external/mksh` import used as the first
full `/system/bin/sh` candidate.

The current target is a non-interactive Android-shaped shell:

```sh
/system/bin/mksh -c 'echo ok'
```

`crt_tiny_sh` still owns the bootstrap `/system/bin/sh` contract. Imported mksh
builds as `crt_mksh`/`mksh` and is copied into `rootfs/system/bin/mksh`; it
should replace `/system/bin/sh` only after Linux, macOS, and Windows smoke
coverage is deliberately widened.

## Layout

```text
shell/mksh/                 imported repo metadata (Android.bp, NOTICE, mkshrc, ...)
shell/mksh/src/             imported Android external/mksh C source
```

Windows-only source adjustments are kept as small guarded edits directly in the
imported C source (see "Project-Owned Source Adjustments" below) rather than a
separate glue directory. Unlike mksh, toybox keeps its project-owned build/config
glue in a dedicated `shell/toybox/crt/` directory.

## Current Pin

- URL: `https://android.googlesource.com/platform/external/mksh`
- Branch: `refs/heads/main`
- Commit: `1548076841f243748a3f56da23b38794d437bc12`
- Tree: `57ba8b3ab85c6d79171453c42baec1e845d4e30a`
- Archive SHA256:
  `88149f218a8ae330ca82b2a20147559887f98691ff9014fb9e2aa759fe440fda`

## Import Rules

- Follow the shared policy in `docs/shell_import.md`.
- Keep imported source separate from project-owned glue.
- Prefer CRT/PAL fixes over upstream source patches.
- If a patch is unavoidable, keep it small and record why the behavior cannot be
  represented through the CRT/PAL layer.
- Record source URL, commit, archive, hash, and license metadata in
  `import_manifest.json`.

## CRT Gaps Found By First Compile Inventory

- `sys/sysmacros.h`, `sys/resource.h`, `pwd.h`, `grp.h`, `termios.h`,
  `libgen.h`, `langinfo.h`, `sys/times.h`, and `sys/file.h`.
- `NSIG`, `sig_t`, `sys_signame`, `sys_siglist`, `sigsuspend`, `alarm`, and
  `sleep`.
- `getuid`, `getgid`, `getegid`, `getpgid`, `getsid`, `setresuid`,
  `setresgid`, `setgroups`, `getrlimit`, `setrlimit`, and `getrusage`.
- `struct stat` timespec fields, `S_ISUID`, `S_ISGID`, and `O_ACCMODE`.
- `strlcpy` and `strlcat`.

These were filled in CRT/PAL/sysroot code rather than by patching mksh.

## Project-Owned Source Adjustments

Imported source should remain unchanged unless the behavior cannot reasonably be
represented by CRT/PAL. The first required exception is Windows LLP64 support:

- Windows x86_64 has 64-bit pointers and `size_t`, but 32-bit `long`.
- mksh R59 assumes `sizeof(size_t) <= sizeof(long)` for its internal formatter.
- `MKSH_CRT_ALLOW_LLP64` is defined only for the Windows CRT mksh target.
- The guarded source adjustment skips that LP64 assertion and widens the
  internal formatter integer path from `long`/`unsigned long` to
  `intmax_t`/`uintmax_t`.

Linux and macOS keep the unguarded LP64 assertion path.

The second required exception is the Windows shell child-spec adapter:

- Real public Windows `fork()` remains a long-term CRT/PAL research item.
- `MKSH_CRT_SHELL_CHILD_SPEC` is defined only for the Windows CRT mksh target.
- When mksh reaches an external `TEXEC` node on Windows, `jobs.c` launches it
  through `__crt_shell_fork_exec()` instead of raw `fork()` plus child-side
  `execve()`.
- Pipeline `TCOM` nodes are allowed to run far enough in the parent shell to
  become external `TEXEC` nodes, then the child-spec path owns the actual child
  process. This keeps pipe/redirection fd state in the CRT fd table and avoids
  the unsupported arbitrary post-fork Windows child path.
- If a pipeline segment is a builtin that runs in the parent during this early
  Windows tranche, the following external segment may start a fresh job instead
  of joining a missing fork-created process list.

ABI impact:

None on the CRT/Bionic public ABI. This is mksh-local Windows glue over the
private CRT shell process contract.

Removal condition:

This adapter can be reduced or removed if the Windows PAL gains a real,
Bionic-compatible `fork()` implementation that can safely resume mksh's normal
post-fork child branch.
