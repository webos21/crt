# C++ Runtime

This document records the first C++ runtime policy for CRT.

## Direction

The source directory is named `libstdc++/` because the original Android Bionic
tree historically used that directory for small C++ ABI support symbols. The
project policy is not to adopt GNU libstdc++ as the C++ standard library.

The intended stack is:

- project-owned C++ ABI bootstrap library, currently installed as `libc++.a`;
- compiler-rt builtins for compiler-generated helper calls;
- libunwind for exception unwinding once exception support becomes active;
- libc++abi for the full Itanium C++ ABI surface later;
- libc++ as the C++ standard library later.

The first tranche provides only the minimal C++ ABI entry points needed before a
full libc++abi import:

- `__cxa_guard_acquire`;
- `__cxa_guard_release`;
- `__cxa_guard_abort`;
- `__cxa_atexit`;
- `__cxa_finalize`;
- `__cxa_pure_virtual`;
- `__cxa_deleted_virtual`;
- `__dso_handle`.

## ABI Policy

Bionic and Unix-like Clang targets use the Itanium C++ ABI for `__cxa_*`
runtime hooks. That is the ABI shape this project is currently implementing.

Windows is the main policy risk. A normal Clang MSVC target uses the MSVC C++
ABI and emits a different family of runtime hooks for thread-safe statics,
destructors, exceptions, RTTI, and operator support. That does not match
Bionic's `__cxa_*` model. The current Windows C tests verify that the project
exports the Bionic/Itanium-shaped symbols, but they do not claim that ordinary
MSVC-ABI C++ objects are supported.

Before enabling broad C++ source builds on Windows, the project must choose one
of these paths:

- build CRT-targeted C++ code with a Clang target/profile that emits Itanium
  C++ ABI hooks; or
- add an explicit MSVC C++ ABI adapter layer as a separate compatibility mode.

The first path is closer to the Bionic-compatible goal.

## Guard Variables

`__cxa_guard_*` uses the standard first-byte-complete contract expected by
Itanium ABI guard variables. Internally the bootstrap implementation stores a
small integer state in the guard object:

- `0`: uninitialized;
- `1`: initialization in progress;
- `2`: initialization complete.

The value `2` keeps the first byte non-zero after `__cxa_guard_release`, so
compiler-generated fast-path checks see the object as initialized.

The current wait policy spins/yields while another thread initializes the
object. It is correct for the first tranche but should be upgraded to the
project wait/futex layer if highly contended C++ local statics become important.

## Destructors

`__cxa_atexit` stores destructor registrations with their object pointer and DSO
handle. `__cxa_finalize(dso)` runs matching destructors in reverse registration
order and marks each entry as called so repeated finalization is safe.

`exit()` weakly calls `__cxa_finalize(NULL)` before running the existing C
`atexit` stack. This lets C++ destructors work when `libc++.a` is linked while
keeping plain C programs independent of the C++ runtime archive.

## Exceptions, RTTI, And Unwind

Exceptions are not enabled by this tranche. The project has not yet imported or
linked libc++abi/libunwind exception machinery, including:

- `__cxa_throw`;
- `__cxa_begin_catch`;
- `__cxa_end_catch`;
- personality routines such as `__gxx_personality_v0`;
- unwind tables and libunwind backend policy;
- RTTI and demangling helpers.

Early CRT C++ experiments should use `-fno-exceptions` and `-fno-rtti` until
the libc++abi/libunwind tranche is explicit.

## Next Steps

Recommended next work:

1. Add real C++ frontend compile/link probes for Linux and macOS using
   `-fno-exceptions -fno-rtti`.
2. Decide the Windows C++ ABI target policy before enabling ordinary `.cc`
   tests on Windows.
3. Add `operator new/delete` only after allocator behavior is ready to be a C++
   allocation boundary.
4. Evaluate importing libc++abi's Itanium ABI source after the project has a
   clear libunwind choice.
5. Add libunwind linkage experiments separately from the bootstrap `__cxa_*`
   symbol tranche.
