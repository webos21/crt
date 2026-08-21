# Android libc++abi Source Policy

See `../libcxx/README.md` for the shared source/build policy. This directory's
own `recipe.json` declares Android's paired `platform/external/libcxxabi`
checkout and the CMake options `tools/crt-libcxx-build.py` uses to build it,
including how it links against `../libunwind` on hosts where that is enabled
(see this recipe's own `notes` for why that link happens at the final
consumer rather than inside libcxxabi's own CMake build). Fetched source and
build products stay under the active preset's
`out/<preset>/external/llvm-runtimes/libcxxabi/` directory.
