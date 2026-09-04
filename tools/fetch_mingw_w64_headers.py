#!/usr/bin/env python3
"""Fetch mingw-w64's own Win32 API headers (mingw-w64-headers/include).

Windows/D3D12 Ganesh vertical slice (2026-09-04): Skia's own public
include/gpu/ganesh/d3d/GrD3DTypes.h unconditionally #includes <d3d12.h>/
<dxgi1_4.h>. A real attempt at pointing those directly at the raw
Microsoft Windows SDK's own um/shared headers hit a genuine dead end,
not just a missing-flag problem: d3d12.h itself does `#include
"windows.h"` (quoted, resolves relative to its own SDK um/ directory,
bypassing any project-side header-search trick), pulling in the raw
SDK's *complete* windows.h -- whose own winnt.h assumes real MSVC-only
architecture macros (_M_AMD64) *and* real MSVC-only atomic/memory-fence
compiler intrinsics (ReadNoFence/WriteRelease8/...) that clang only
implements for its own `*-windows-msvc` target under -fms-compatibility,
not this project's `--target=x86_64-w64-mingw32` one.

This is exactly the class of problem the mingw-w64 project's own real
header set (a separate, community-maintained header tree specifically
written to compile clean under plain clang/gcc on the mingw target, no
MSVC compatibility mode needed) exists to solve -- see libstdc++/
third_party/win32_shim/windows.h's own top comment for the full story
of how this project's existing minimal shim there defers to these real
headers via #include_next once they are on the include path.

Deliberately narrow, mirroring tools/fetch_skia.py's own clone_ref()
sparse-checkout technique (see that file's own comment for the full
"why" -- a real, --depth 1 + --filter=blob:none + sparse-checkout
fetch, not a full clone): mingw-w64-headers/include alone (~95MB) is the
pure Win32 API header set (windows.h, d3d12.h, dxgi*.h, ...); the
sibling mingw-w64-crt/ (the actual C runtime headers/implementation,
e.g. stdio.h, and the crt1.o startup objects) is a completely separate,
much larger part of the same monorepo this project has no need for and
deliberately never fetches -- this project supplies its own libc.

Pinned to the v14.0.0 release tag (real, dereferenced commit hash
verified via `git ls-remote --tags` at the time this was written,
2026-09-04) for reproducibility, matching fetch_skia.py's own
--expected-commit precedent.
"""
import argparse
import json
import shutil
import subprocess
from pathlib import Path


def run(args, cwd=None):
    print("+", " ".join(str(arg) for arg in args), flush=True)
    subprocess.run(args, cwd=cwd, check=True)


def checked_output(args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


#: Real defaults from mingw-w64-headers/configure.ac (read directly,
#: 2026-09-04): DEFAULT_MSVCRT_VERSION's own `--with-default-msvcrt`
#: case statement's `ucrt*|*)` fallback branch -- i.e. plain `./configure`
#: with no override, what every stock mingw-w64 toolchain build actually
#: uses today -- sets it to 0xE00; DEFAULT_WIN32_WINNT's own `--with-
#: default-win32-winnt` AC_ARG_WITH default is 0xa00 (Windows 10). This
#: project links against neither mingw-w64-crt nor any real msvcrt/ucrt/
#: Win32 API version gate (it supplies its own libc, and gpu_win32.c's
#: own real D3D12/DXGI calls are hand-declared, independent of whichever
#: _WIN32_WINNT-gated declarations these headers expose), so neither
#: value has a real runtime consequence here; both are kept at the real,
#: standard defaults purely so this generated header matches what a
#: genuine `./configure && make` mingw-w64-headers install would have
#: produced, rather than arbitrary made-up values.
_DEFAULT_MSVCRT_VERSION = "0xE00"
_DEFAULT_WIN32_WINNT = "0x0A00"


def clone_headers(repo, ref, dest):
    # Mirrors tools/fetch_skia.py's own clone_ref() sparse path exactly
    # (see that file's own comment for the full real reasoning behind
    # each flag) -- --depth 1 on the initial clone matters and must
    # stay (a real mistake caught the hard way there, see its own
    # comment), and the ref is fetched separately (not via `--branch`)
    # since a release tag still needs a real `git fetch <ref>` to land
    # locally before `git checkout --detach FETCH_HEAD` can see it.
    #
    # mingw-w64-headers/crt/ (2026-09-04, real, confirmed necessary):
    # <windows.h>/<rpc.h> et al reach for `#include <_mingw.h>`
    # unconditionally -- a real file the mingw-w64-headers build normally
    # *generates* (via a full `./configure`, from crt/_mingw.h.in) into
    # include/_mingw.h, not a static file shipped directly under
    # include/ itself; that generated _mingw.h then itself `#include
    # "_mingw_mac.h"` (and, transitively, more real crt/*.h siblings --
    # _mingw_secapi.h, _mingw_off_t.h, ...), which normally reach
    # include/ the same way real `make install` does: mingw-w64-headers/
    # crt/Makefile.am installs every crt/*.h file (all 84 of them, real,
    # static, no .in template -- confirmed zero filename collisions
    # against include/'s own files) flat into the same shared include/
    # prefix as _mingw.h itself. generate_mingw_h() below reproduces
    # exactly that flattening, without needing real autotools for it: of
    # the two `.h.in` templates in this repo, only crt/_mingw.h.in is
    # ever #included by anything under include/ (the other, top-level
    # config.h.in, is pure build-system package metadata -- PACKAGE_
    # VERSION and friends -- read by nothing under include/), and of
    # _mingw.h.in's own 674 lines, exactly one real autoconf substitution
    # token appears (@DEFAULT_MSVCRT_VERSION@) -- a plain, direct
    # substitution is enough here, real autoreconf/configure deliberately
    # not invoked for one token in one file.
    run(
        ["git", "clone", "--filter=blob:none", "--no-checkout", "--depth", "1", repo, str(dest)],
        cwd=None,
    )
    run(["git", "sparse-checkout", "init", "--cone"], cwd=dest)
    run(["git", "sparse-checkout", "set", "mingw-w64-headers/include", "mingw-w64-headers/crt"], cwd=dest)
    run(["git", "fetch", "--depth", "1", "origin", ref], cwd=dest)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=dest)
    generate_mingw_h(dest)
    flatten_crt_headers(dest)


def generate_mingw_h(dest):
    template = dest / "mingw-w64-headers" / "crt" / "_mingw.h.in"
    out = dest / "mingw-w64-headers" / "include" / "_mingw.h"
    text = template.read_text(encoding="utf-8")
    substitutions = {
        "@DEFAULT_MSVCRT_VERSION@": _DEFAULT_MSVCRT_VERSION,
        "@DEFAULT_WIN32_WINNT@": _DEFAULT_WIN32_WINNT,
    }
    for token in substitutions:
        if token not in text:
            raise SystemExit(
                f"{template}: expected substitution token '{token}' not found -- mingw-w64-headers' "
                "own _mingw.h.in template has changed shape; re-check it directly (and this script's "
                "own top-of-clone_headers comment) before adjusting this script."
            )
    for token, value in substitutions.items():
        text = text.replace(token, value)
    out.write_text(text, encoding="utf-8")
    print(f"Generated {out} (MSVCRT_VERSION={_DEFAULT_MSVCRT_VERSION}, WIN32_WINNT={_DEFAULT_WIN32_WINNT})")


def flatten_crt_headers(dest):
    # Real, confirmed (2026-09-04): mingw-w64-headers/crt/ is NOT purely
    # the small set of mingw-internal config headers _mingw.h itself
    # #includes -- it is the *full* mingw-w64-crt header source tree (84
    # files), most of it genuine C-standard-library/CRT headers
    # (string.h, stdio.h, stdlib.h, math.h, errno.h, time.h, assert.h,
    # ...) that a real mingw-w64 *toolchain* install ships alongside its
    # own libc, meant to be used together. This project supplies its own
    # libc (already on the include path via -isystem<sysroot>/include)
    # -- copying crt/string.h here would shadow it (a plain -I always
    # outranks -isystem, confirmed the hard way: crt/string.h's own real
    # `#include <sec_api/string_s.h>` -- a SAL-secure-CRT extension
    # header this project has no other use for and does not vendor --
    # broke the build the first time every crt/*.h file was copied
    # unfiltered).
    #
    # Real C standard library header names never start with `_` (a
    # reserved-identifier convention the C standard itself establishes),
    # while every mingw-internal support header d3d12.h/dxgi1_4.h's own
    # real chain actually needs so far (_mingw_mac.h, alongside generated
    # _mingw.h itself) does -- so this filters to just `_*.h`, matching
    # every file in crt/ that is NOT a real C-standard-library name
    # (_mingw_mac.h/_mingw_off_t.h/_mingw_secapi.h/_mingw_stat64.h/
    # _mingw_stdarg.h/_mingw_unicode.h/_bsd_types.h/_cygwin.h/
    # _timeval.h, confirmed by direct listing) while leaving every real
    # C-standard-library name (string.h, stdio.h, ...) to fall through to
    # this project's own real libc via -isystem, exactly as everywhere
    # else in this project's Windows builds.
    crt_dir = dest / "mingw-w64-headers" / "crt"
    include_dir = dest / "mingw-w64-headers" / "include"
    # A real, individually-confirmed exception to the `_*.h` rule above:
    # *clang's own* bundled resource-dir vadefs.h (lib/clang/<ver>/
    # include/vadefs.h, a known clang MS-compat passthrough shim) does
    # `#include_next <vadefs.h>`, expecting a real one further down the
    # include path -- confirmed via `fatal error: 'vadefs.h' file not
    # found` from clang's own vadefs.h, not from any mingw-w64 header.
    #
    # Deliberately NOT extended to malloc.h/crtdefs.h/corecrt.h, even
    # though clang's own bundled <mm_malloc.h> (pulled in transitively
    # by winnt.h's own <x86intrin.h>/<immintrin.h>/<xmmintrin.h> for SSE
    # aligned-alloc intrinsics) needs __mingw_aligned_malloc/
    # __mingw_aligned_free from *some* malloc.h -- a real first attempt
    # at flattening those three in here too surfaced a genuine, deeper
    # conflict: mingw-w64's own corecrt.h (crtdefs.h's own real
    # dependency) redefines wctype_t as `unsigned short`, directly
    # conflicting with this project's own libc's `unsigned long` (real
    # `typedef redefinition with different types` compile error) --
    # this project's own type, not mingw's, is the one every OTHER real
    # header/TU in this project already agrees on. Real fix lives
    # instead in libstdc++/third_party/win32_shim/malloc.h (this
    # project's own minimal shim, #include_next-forwarding to the real
    # project libc's own malloc.h and adding just the two real
    # necessary declarations) plus win32_shim/mingw_w64_compat.h (pre-
    # defines _WCHAR_T_DEFINED/_WCTYPE_T_DEFINED so mingw-w64's own
    # corecrt.h/rpcndr.h skip their own conflicting typedefs entirely,
    # force-included only for mingw-w64-headers-consuming compiles) --
    # see each file's own top comment for the full story.
    extra_files = ("vadefs.h",)
    copy_names = sorted(set(p.name for p in crt_dir.glob("_*.h")) | set(extra_files))
    copied = 0
    for name in copy_names:
        src = crt_dir / name
        target = include_dir / name
        if target.exists():
            raise SystemExit(
                f"{target}: unexpected collision copying a real crt/*.h sibling into include/ -- "
                "mingw-w64-headers' own layout has changed; re-check it directly before adjusting "
                "this script (verified zero collisions at the time this was written, 2026-09-04)."
            )
        shutil.copyfile(src, target)
        copied += 1
    print(f"Flattened {copied} real crt/*.h siblings (underscore-prefixed, plus {extra_files}) into {include_dir}")

    # crt/sec_api/ (2026-09-04, real, confirmed necessary): mingw-w64's
    # own <stralign.h> (reached transitively via windows.h -> ole2.h ->
    # objbase.h -> ... -> winscard.h -> wtypes.h -> stralign.h, needed
    # once D3D12's own real COM chain is compiled) does `#include
    # <sec_api/stralign_s.h>` -- confirmed via `fatal error: 'sec_api/
    # stralign_s.h' file not found`. sec_api/ is mingw-w64's own "secure
    # CRT" extension subdirectory (SAL-annotated _s-suffixed variants,
    # mirroring Microsoft's own real sec_api/ naming convention) -- a
    # small (12 files), self-contained, mingw-specific subdirectory this
    # project's own sysroot has no equivalent of at all (confirmed: no
    # sysroot/include/sec_api directory exists), so copying it wholesale
    # carries no shadowing risk the way a top-level crt/*.h name might.
    sec_api_src = crt_dir / "sec_api"
    sec_api_dst = include_dir / "sec_api"
    if sec_api_dst.exists():
        raise SystemExit(
            f"{sec_api_dst}: unexpected pre-existing directory -- mingw-w64-headers' own layout "
            "has changed; re-check it directly before adjusting this script."
        )
    shutil.copytree(sec_api_src, sec_api_dst)
    print(f"Copied {sec_api_src} -> {sec_api_dst}")


def main():
    parser = argparse.ArgumentParser(description="Fetch mingw-w64's own Win32 API headers for libcrtgfx.")
    parser.add_argument("--dest", required=True, help="destination directory (a mingw-w64 sparse checkout)")
    parser.add_argument("--repo", default="https://github.com/mingw-w64/mingw-w64.git")
    parser.add_argument("--ref", default="v14.0.0", help="git tag/ref to fetch")
    parser.add_argument(
        "--expected-commit",
        default="9b3dd0125792fe94d16cacdc596dbd42fca1b369",
        help="expected commit hash for --ref (real, dereferenced v14.0.0 tag commit -- v14.0.0 is an "
             "annotated tag, so this is `git ls-remote --tags <repo> 'refs/tags/v14.0.0^{}'`, not the "
             "tag object's own hash; pass '' to skip the check)",
    )
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    dest = Path(args.dest).resolve()
    manifest_path = dest / ".crt-mingw-w64-headers-fetch.json"

    if dest.exists():
        if args.force:
            shutil.rmtree(dest)
        elif manifest_path.exists():
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            if manifest.get("repo") == args.repo and manifest.get("ref") == args.ref:
                commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
                if args.expected_commit and commit != args.expected_commit:
                    raise SystemExit(
                        f"mingw-w64-headers commit mismatch in existing checkout\n"
                        f"expected {args.expected_commit}\n"
                        f"actual   {commit}"
                    )
                print(f"mingw-w64-headers already fetched: {dest} ({commit})")
                return
            raise SystemExit(
                f"{dest} already contains a different mingw-w64-headers fetch "
                "(repo/ref changed). Pass --force to replace it."
            )
        else:
            raise SystemExit(f"{dest} already exists and is not managed by fetch_mingw_w64_headers.py")

    dest.parent.mkdir(parents=True, exist_ok=True)
    clone_headers(args.repo, args.ref, dest)
    commit = checked_output(["git", "rev-parse", "HEAD"], cwd=dest)
    if args.expected_commit and commit != args.expected_commit:
        raise SystemExit(
            f"mingw-w64-headers commit mismatch\nexpected {args.expected_commit}\nactual   {commit}"
        )

    manifest = {"repo": args.repo, "ref": args.ref, "commit": commit}
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"mingw-w64-headers fetched: {dest} ({commit})")


if __name__ == "__main__":
    main()
