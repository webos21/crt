# Android libc++ Source Policy

This directory contains CRT-owned provenance and build policy only. It is not
an upstream source checkout.

The complete C++ standard library is Android's `platform/external/libcxx`.
Its paired ABI and unwinder sources are `platform/external/libcxxabi` and
`platform/external/libunwind`. `tools/fetch_libcxx_runtimes.py` fetches all
three at one selected Android ref into
`out/<preset>/external/llvm-runtimes/`; build products belong under that same
active preset tree and are never committed here.

The initial project runtime remains Bionic-shaped: `libstdc++/src/` owns the
small ABI bootstrap Bionic historically provides, while imported libc++,
libc++abi, and libunwind replace that bootstrap only after their static and
shared artifacts link and run through the CRT sysroot on every host.
