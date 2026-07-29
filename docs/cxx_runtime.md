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

The Windows bridge bootstrap also provides the first MSVC ABI runtime hooks:

- `_Init_thread_header`;
- `_Init_thread_footer`;
- `_Init_thread_abort`;
- `_Init_global_epoch`;
- `_Init_thread_epoch`;
- `_purecall`.

## ABI Policy

Bionic and Unix-like Clang targets use the Itanium C++ ABI for `__cxa_*`
runtime hooks. That is the ABI shape this project is currently implementing.

Windows is the main policy risk. A normal Clang MSVC target uses the MSVC C++
ABI and emits a different family of runtime hooks for thread-safe statics,
destructors, exceptions, RTTI, and operator support. That does not match
Bionic's `__cxa_*` model. The current Windows C tests verify that the project
exports the Bionic/Itanium-shaped symbols, but they do not claim that ordinary
MSVC-ABI C++ objects are supported.

The project policy is a dual ABI lane:

- CRT-targeted C++ code should use the Bionic/Itanium C++ ABI lane.
- Windows-native C++ DLL interoperability should be handled by a separate MSVC
  ABI bridge lane.

This keeps the project core aligned with Bionic while leaving an explicit path
for Windows ecosystem integration. The two lanes must not be treated as one ABI:
name mangling, exception objects, RTTI, class layout edge cases, destructor
registration, thread-safe statics, and allocation ownership may differ.

The first practical rule is that C ABI boundaries are the default crossing point
between the lanes. Passing C++ objects, exceptions, RTTI-bearing types, STL
containers, or ownership of memory allocated by one lane into the other lane is
unsupported until a narrower bridge contract is defined and tested.

## Windows MSVC ABI Bridge Lane

The Windows bridge lane exists so CRT programs can eventually consume or expose
Windows-native C++ DLL APIs when a project cannot reduce that dependency to a
plain C ABI.

This lane is compatibility infrastructure, not the CRT's core ABI. It should be
implemented as a separate adapter layer with its own tests and documentation.
Expected work includes:

- identifying Clang/MSVC-emitted runtime hooks for static initialization,
  destructor registration, pure/deleted virtual calls, RTTI, and exceptions;
- deciding which hooks can forward into project libc state and which must stay
  isolated;
- defining allocation ownership rules across CRT malloc/new and Windows-native
  allocation APIs;
- deciding whether exceptions may cross the bridge, with the default answer
  being no until unwind interoperability is proven;
- adding DLL load/use tests that exercise C++ exports through an explicit bridge
  API rather than silently mixing ABIs.

The bridge should begin with conservative C-callable wrappers around
Windows-native C++ DLLs. Broader C++ object ABI interop can be added only after
the narrow wrapper model is stable.

The first implementation is deliberately narrow. `_Init_thread_*` implements the
MSVC local-static guard state machine without UCRT TLS acceleration, and
`_purecall` terminates through `abort`. Clang's MSVC ABI frontend also emits a
reference to `_tls_index` for the normal `/Zc:threadSafeInit` fast path, so full
thread-safe local static support on Windows requires a PE TLS directory and
loader TLS policy. Until that exists, Windows C++ frontend smoke tests use
`-fno-threadsafe-statics`, while direct C ABI tests exercise `_Init_thread_*`.
Exceptions and rich C++ object interop remain explicit future work.

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

The current C++ frontend test is compiled with `-fno-exceptions` and
`-fno-rtti`. Linux and macOS keep compiler-emitted thread-safe local-static
guards enabled, which exercises the Bionic/Itanium `__cxa_guard_*` lane.
Windows temporarily uses `-fno-threadsafe-statics` for the frontend smoke test
because the normal MSVC fast path also needs `_tls_index` and PE TLS startup.
Dedicated C ABI tests exercise both `__cxa_guard_*` and `_Init_thread_*`
directly.

## External References

The implementation policy is based on these upstream references:

- LLVM libc++abi's guard implementation and specification for
  `__cxa_guard_*`, `__cxa_atexit`, `__cxa_finalize`, and
  `__gxx_personality_v0`;
- the Itanium C++ ABI termination and DSO destructor model;
- Microsoft's `/Zc:threadSafeInit` documentation, which states that MSVC
  thread-safe local static initialization depends on UCRT runtime support;
- Clang's Microsoft C++ ABI code generation, which emits `_Init_thread_header`,
  `_Init_thread_footer`, `_Init_thread_abort`, and `_CxxThrowException` for the
  MSVC ABI path;
- ReactOS' public CRT implementation as a behavioral reference for the small
  `_Init_thread_*` state machine, used as reference material only rather than
  imported source.

## Next Steps

Recommended next work:

1. Add real C++ frontend compile/link probes for Linux and macOS using
   `-fno-exceptions -fno-rtti`.
2. Add a Windows policy probe that records which C++ ABI hooks Clang emits for
   the selected target/profile.
3. Add `operator new/delete` only after allocator behavior is ready to be a C++
   allocation boundary.
4. Evaluate importing libc++abi's Itanium ABI source after the project has a
   clear libunwind choice.
5. Start a separate Windows MSVC ABI bridge design with C ABI wrapper tests
   before allowing C++ object or exception interop across the bridge.
