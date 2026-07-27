# Bionic Import Policy

This document defines how Android Bionic headers and source files are imported
into this project.

## Import Location

Bionic source should be handled in two different locations:

- `third_party/bionic/`
  - provenance records, import manifests, license notes, and optional pristine
    source snapshots.
- project runtime directories such as `libc/string/`, `libm/`, `libdl/`, and
  `linker/`
  - curated files that are actually compiled by this project.

The default policy is curated import. Do not import the full Bionic tree until a
specific milestone requires it.

## Source Commit Recording

Every import tranche must record:

- upstream repository URL;
- upstream branch or tag;
- upstream commit hash;
- original file path;
- local file path;
- license family;
- whether the local file is pristine or adapted;
- a short note explaining any local adaptation.

The first import baseline is:

```text
Repository: https://android.googlesource.com/platform/bionic
Branch: main
Observed main commit: 731631f300090436d7f5df80d50b6275c8c60a93
```

Some first-tranche portable string/memory files are taken from the older
`ics-mr0` Bionic tree because modern Bionic uses architecture-specific assembly
or reorganized upstream source for several of these functions. Those files must
still be recorded individually in `third_party/bionic/README.md`.

## License And Provenance Policy

Bionic is used because it is Android's real libc/libm/libdl/linker stack and
because its libc code is permissively licensed. Imported files must preserve
their original copyright and license headers.

For each import:

- preserve the original file license header;
- preserve the upstream path and commit in a local provenance comment or manifest;
- do not remove attribution;
- keep license-relevant files and notes under `third_party/bionic/`;
- avoid importing Linux internal kernel headers;
- prefer Bionic cleaned kernel headers when kernel UAPI headers are needed.

## No Direct Edits To Pristine Sources

If a pristine Bionic snapshot is imported under `third_party/bionic/`, do not edit
it directly.

Local changes should live in project-owned runtime directories, such as
`libc/string/`, and must be recorded as adaptations. This keeps the upstream
source auditable and makes future updates easier.

## Patch Location Policy

Local adaptations should be small and explicit. Examples:

- include path adjustments;
- C99/freestanding compatibility fixes;
- Windows LLP64 portability fixes;
- replacing Bionic private dependencies with project PAL interfaces;
- disabling Android-only fortify or platform hooks until their tranche exists.

For larger changes, prefer a project wrapper around the imported implementation
rather than rewriting the imported file in place.

## First Import Tranche

The first tranche imports only low-dependency string/memory functions:

- `memcpy`
- `memmove`
- `memset`
- `strlen`
- `strcmp`

The goal is to establish the import and test workflow while keeping the existing
`write`/`_exit` hello bring-up intact.
