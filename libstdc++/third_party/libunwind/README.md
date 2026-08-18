# Android libunwind Source Policy

Android removed the old `platform/external/libunwind` project in 2021. It is
therefore not fetched with the legacy `external/libc++` and
`external/libc++abi` snapshots and must not be used as the project's unwind
runtime.

The current source policy is AOSP `toolchain/llvm-project/libunwind`, built
through the CRT toolchain as both static and shared runtime shapes. Sources and
artifacts remain under `out/<preset>/external/llvm-runtimes/`. Until that build
is integrated, macOS uses the Darwin system unwind PAL for validation; Linux
and Windows imported-libc++ exception tests remain incomplete.
