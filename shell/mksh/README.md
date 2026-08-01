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
shell/mksh/src/   imported Android external/mksh source
shell/mksh/glue/  project-owned build/config glue
```

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
