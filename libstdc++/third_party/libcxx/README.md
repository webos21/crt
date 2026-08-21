# Android libc++ Source Policy

This directory contains CRT-owned provenance and build policy only. It is not
an upstream source checkout.

The complete C++ standard library is Android's `platform/external/libcxx`.
`recipe.json` in this directory declares its source (git repository + ref)
and CMake build options, following the same schema as the sibling
`../libcxxabi/recipe.json` and `../libunwind/recipe.json`. `tools/crt-libcxx-
build.py` reads all three recipes and drives fetch/configure/build through
the CRT toolchain (`tools/crt-cc`/`tools/crt-c++`); see that script's own
module docstring for the recipe schema. Sources and build products land under
the active preset's `out/<preset>/external/llvm-runtimes/` and are never
committed here.

The initial project runtime remains Bionic-shaped: `libstdc++/src/` owns the
small ABI bootstrap Bionic historically provides, while imported libc++,
libc++abi, and libunwind replace that bootstrap only after their static and
shared artifacts link and run through the CRT sysroot on every host.
