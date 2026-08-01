# mksh

This directory is reserved for the Android `external/mksh` import.

The first target is a non-interactive Android-shaped shell:

```sh
/system/bin/sh -c 'echo ok'
```

## Import Rules

- Record the Android source URL, commit, source date, license, and hash before
  importing source files.
- Keep imported source separate from project-owned glue.
- Prefer CRT/PAL fixes over upstream source patches.
- If a patch is unavoidable, keep it small and record why the behavior cannot be
  represented through the CRT/PAL layer.

## Expected CRT Gaps

- `termios.h` and basic terminal attributes.
- `sys/resource.h` and resource limit policy.
- `pwd.h`, `grp.h`, and synthetic user/group databases.
- signal set APIs and `sigaction`.
- process and fd inheritance details, especially on Windows.
