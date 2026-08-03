# Legacy Import Review

This file classifies legacy Bionic imports against the current project policy:
Bionic `main` first, legacy only as a documented exception.

The current official `refs/heads/main` observation is:

```text
Repository: https://android.googlesource.com/platform/bionic
Commit: 731631f300090436d7f5df80d50b6275c8c60a93
Date: 2025-03-26
```

The machine-readable source of truth is
`third_party/bionic/import_manifest.json`. This review explains the policy
behind the `review_class` values in that manifest.

## Review Classes

- `bootstrap_keep`: keep the legacy source for now because replacing it would
  force a larger tranche, source-family change, or premature ABI policy.
- `main_replace_candidate`: re-check Bionic `main` before doing performance or
  architecture work; these are likely candidates for builtin, arch-common, or
  current-main replacement.
- `project_owned_transition`: the imported code is now small enough or adapted
  enough that the project should either freeze it as project-owned behavior or
  replace it with a cleaner current-main source.
- `main_current`: already based on current Bionic `main`.
- `project_owned`: intentionally not imported from Bionic.

## Current Legacy Summary

| Source ref | Files | Classification | Decision |
| --- | ---: | --- | --- |
| `ics-mr0` string/memory core | 8 | `main_replace_candidate` | Keep temporarily, but review against Bionic main or compiler builtin policy before optimization. |
| `ics-mr0` simple string helpers | 10 | `project_owned_transition` | Prefer project-owned freeze unless current Bionic upstream-openbsd/freebsd source is cleaner. |
| `ics-mr0` numeric conversion | 4 | `bootstrap_keep` | Keep until the wider `inttypes.h`, `strtoimax`, locale, and overflow-policy tranche. |
| `884e4f8` fdlibm core | 8 | `bootstrap_keep` | Keep while the project follows an fdlibm/msun source family for early libm. |
| FreeBSD upstream msun exception | 2 | `bootstrap_keep` | Keep as explicit source-family exceptions where observed Bionic refs lack direct files. |

## File-Level Decisions

### `ics-mr0` string/memory core

These files remain legacy only because they gave a small, portable C bootstrap
surface before architecture dispatch existed:

- `libc/src/string/bcopy.c`
- `libc/src/string/memcpy.c`
- `libc/src/string/memmove.c`
- `libc/src/string/memset.c`
- `libc/src/string/strlen.c`
- `libc/src/string/strcmp.c`
- `libc/src/string/memchr.c`
- `libc/src/string/memcmp.c`

Decision: `main_replace_candidate`.

Next action: map current Bionic `main` for each symbol. If Bionic `main` routes
through architecture-specific assembly or compiler builtins, decide explicitly
whether this project wants builtin-backed C, per-architecture import, or a
project-owned portable fallback.

### `ics-mr0` simple string helpers

These files are small BSD/OpenBSD-style routines and have already been adapted
to the project header and symbol surface:

- `libc/src/string/strcat.c`
- `libc/src/string/strchr.c`
- `libc/src/string/strcpy.c`
- `libc/src/string/strncmp.c`
- `libc/src/string/strncpy.c`
- `libc/src/string/strrchr.c`
- `libc/src/string/strcspn.c`
- `libc/src/string/strdup.c`
- `libc/src/string/strpbrk.c`
- `libc/src/string/strspn.c`
- `libc/src/string/strstr.c`

Decision: `project_owned_transition`.

Next action: compare against current Bionic `upstream-openbsd` or equivalent
sources. If no meaningful current-main advantage exists, freeze them as
project-owned behavior while preserving attribution.

### `ics-mr0` numeric conversion

These files are stable and already adapted to the project errno/base policy:

- `libc/src/atoi.c`
- `libc/src/atol.c`
- `libc/src/strtol.c`
- `libc/src/strtoul.c`

Decision: `bootstrap_keep`.

Next action: revisit when `inttypes.h`, `strtoimax`, `strtoumax`, locale-aware
conversion, and full overflow behavior are opened.

### `884e4f8` fdlibm sources

These files preserve the early libm fdlibm/msun source family:

- `libm/src/freebsd/e_exp.c`
- `libm/src/freebsd/e_expf.c`
- `libm/src/freebsd/e_log.c`
- `libm/src/freebsd/e_logf.c`
- `libm/src/freebsd/e_pow.c`
- `libm/src/freebsd/e_powf.c`
- `libm/src/freebsd/s_scalbn.c`
- `libm/src/freebsd/s_scalbnf.c`

Decision: `bootstrap_keep`.

Next action: keep them until native float tranches, `errno`, and `fenv` policy
are mature enough to compare against the current Bionic `main` math graph.

### FreeBSD upstream msun exception

These files are not imported through Bionic, but they come from the same
FreeBSD/msun source family used by Bionic's current math tree:

- `libm/src/freebsd/e_log2.c`
- `libm/src/freebsd/e_log2f.c`

Decision: `bootstrap_keep`.

Next action: re-check Bionic `main` before optimizing `log2`/`log2f` or changing
the math source-family policy.

## Replacement Order

1. Review `main_replace_candidate` string/memory core first.
2. Freeze or replace `project_owned_transition` string helpers second.
3. Defer numeric conversion until locale/inttypes policy.
4. Defer `884e4f8` fdlibm and the FreeBSD upstream exception until libm
   accuracy and fenv policy are ready.
