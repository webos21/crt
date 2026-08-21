# Android libunwind Source Policy

Android removed the old `platform/external/libunwind` project in 2021. It is
therefore not fetched from the same place as `../libcxx`/`../libcxxabi` and
must not be presented as an equivalent legacy checkout.

The current source policy is AOSP `toolchain/llvm-project/libunwind` --
declared in this directory's own `recipe.json` (a sparse-checkout fetch of
just the `libunwind` subpath out of that full LLVM monorepo mirror, not the
whole tree), built through the CRT toolchain via `tools/crt-libcxx-build.py`
as both static and shared runtime shapes. Sources and artifacts remain under
`out/<preset>/external/llvm-runtimes/`.

Enabled on Linux and Windows only (`recipe.json`'s own `target_os`). macOS
deliberately excludes it: Darwin's own `libSystem` already provides a real
system unwinder, and imported-libc++ exception handling there is verified
working with no libunwind at all -- see this recipe's own `notes` and
`../libcxxabi/recipe.json`'s notes for the full reasoning.
