# License

CRT follows the same license policy as Android Bionic, which it is built
against: there is no single project-wide license. Project-owned code uses one
default license (below), while code imported or adapted from Bionic and other
upstream projects keeps its own original license, tracked per file. This
document records that policy; it is not itself a substitute for the license
text in any individual file.

## Default license (project-owned code)

Unless a source file states otherwise in its own header, project-owned code in
this repository -- everything that is not an import from Bionic or another
upstream project (see below) -- is licensed under the following terms, the
same BSD-style license Android Bionic itself uses for its own code (for
example `Copyright (C) 2009 The Android Open Source Project`, unchanged since
at least 2009 through current Bionic `main`):

```
Copyright (c) 2026 The CRT Project. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
```

New project-owned files should carry this same header (with the current year)
rather than relying solely on this document.

## Imported and adapted code (keeps its own license)

Bionic itself is not single-licensed either: most of its own code carries the
BSD-style header above, but libc/libm source adapted from BSD, FreeBSD,
OpenBSD, NetBSD, and Sun/Lucent's fdlibm keeps each of those original
permissive licenses on a per-file basis, and some Android-added support files
are Apache-2.0/BSD-compatible. CRT follows the same per-file provenance
discipline for everything it imports, rather than relabeling imported code
under the project's own default license:

- **`third_party/bionic/`** -- source adapted from Android Bionic itself
  (`libc/`, `libm/`, headers), which in turn carries a mix of licenses
  depending on where Bionic got each file from. Every imported/adapted file
  is tracked individually in
  [`third_party/bionic/import_manifest.json`](third_party/bionic/import_manifest.json)
  with an explicit `license_family` (`BSD-style`, `FreeBSD/msun permissive`,
  `fdlibm/Sun permissive`, `Lucent permissive via OpenBSD/Bionic`, `Android
  Apache-2.0/BSD-compatible support`, or `project` for CRT's own additions in
  that tree) plus the exact upstream path and commit/tag it came from. That
  manifest is the authoritative per-file record -- this document only
  summarizes it.
- **`shell/mksh/`** -- the MirBSD Korn Shell, imported from Android
  `external/mksh`. Own license in
  [`shell/mksh/NOTICE`](shell/mksh/NOTICE) (a permissive MirOS-style license).
  Import provenance in `shell/mksh/import_manifest.json`.
- **`shell/toybox/src/`** -- toybox, imported from Android `external/toybox`.
  Own license in
  [`shell/toybox/src/LICENSE`](shell/toybox/src/LICENSE) (0BSD-style).
  CRT-owned build glue and patches live alongside it in `shell/toybox/crt/`
  and `shell/toybox/PATCHES.md` under the default project license above.
  Import provenance in `shell/toybox/import_manifest.json`.
- **`shell/awk/`** -- a port of the "one true awk" (`onetrueawk`). Own license
  in [`shell/awk/LICENSE`](shell/awk/LICENSE) (Lucent Technologies permissive).
  Import provenance in `shell/awk/import_manifest.json`.

Every import in this repository follows the same rule: upstream source stays
unmodified where possible, and any unavoidable patch is recorded (upstream
file/commit, the behavior changed, and why) rather than silently altered --
see `shell/toybox/PATCHES.md` for the current example of that record.

## Third-party ports (not distributed)

`porting/recipes/*.json` describe how to fetch and build third-party libraries
(zlib, libpng, libffi, SQLite, GNU Make, ...) against the CRT sysroot for
portability testing. None of that upstream source is committed to this
repository -- see `docs/porting_status.md`'s Policy section. Each of those
projects' own license applies to the source `tools/crt-port-build.py`
downloads and to any binaries you build from it; consult the upstream project
for its license before redistributing anything built from a recipe.
