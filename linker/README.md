# CRT Linker

This directory is reserved for the future project-owned dynamic linker/loader.

The current runtime does not build a linker target yet. `libdl` is the public
dynamic loading API surface, and the first implementation delegates to
host-native loader facilities on Windows and macOS while keeping Linux real
object loading deferred in the freestanding profile.

The long-term policy is documented in:

```text
docs/linker_loader.md
```

Initial `linker/` work should start only when a concrete ELF loader milestone is
selected, such as loading one dependency-free CRT-built shared object on Linux.
Until then, keep loader policy in documents and keep `libdl` behavior explicit.
