# C++ Runtime

This document records the first C++ runtime policy for CRT.

## Direction

The source directory is named `libstdc++/` because the original Android Bionic
tree historically used that directory for small C++ ABI support symbols. The
project policy is not to adopt GNU libstdc++ as the C++ standard library.

The intended stack is the same separation Android uses: Bionic's small
`libstdc++` ABI surface plus the separately maintained LLVM libc++, libc++abi,
and libunwind projects. It is not a plan to hand-write STL containers.

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

The bootstrap `cxx` library still defaults to `-fno-exceptions` and
`-fno-rtti`. The imported Android libc++/libc++abi lane is different: its build
sets `CRT_CXX_ENABLE_EXCEPTIONS=1` and `CRT_CXX_ENABLE_RTTI=1`, and the macOS
runtime smoke now verifies a real `std::runtime_error` throw/catch together
with `std::vector` and `std::string`.

macOS currently gets `_Unwind_*` from the documented libSystem PAL boundary.
Linux and Windows must not fall back to a host C++ runtime. Their remaining
gate is a CRT-built LLVM libunwind. The previously fetched
`platform/external/libunwind` main checkout is not that source: its tip removed
the Android build in 2021 because the repository was no longer used. Current
AOSP LLVM runtime work lives in `toolchain/llvm-project`; its `libunwind`
source must be wired as the next static/shared runtime component.

### Windows exception-table format: DWARF CFI, not native SEH

This is a deliberate, project-important decision, not an incidental compiler
default. Clang's out-of-the-box exception-table format for the
`*-w64-mingw32` target is native SEH -- the Windows OS's own unwind format
(`.pdata`/`.xdata` tables walked by `RtlVirtualUnwind`/`RtlUnwindEx` inside
`ntdll`), signaled to source via the `__SEH__` predefine. CRT instead builds
Windows C++ with `-fdwarf-exceptions`, which forces the portable Itanium
DWARF CFI format used on Linux and macOS.

The reasoning is not "libc++/libc++abi are self-built, therefore use DWARF"
in the abstract -- it is specifically about which component owns the actual
unwind *engine*:

- CRT already builds its own LLVM libunwind from source on every OS
  (`libstdc++/third_party/libunwind/recipe.json`), after explicitly
  evaluating and rejecting a host-provided `libunwind-dev` package on Linux
  (see that recipe's own `notes`). That from-source libunwind is a
  table-based DWARF CFI unwinder -- the same implementation, same table
  format, same unwind algorithm on Linux, macOS, and Windows.
- Native SEH is a genuinely different mechanism: the actual stack walking
  and personality-routine dispatch happens inside the Windows OS itself
  (`RtlUnwind`), not in CRT-owned code. libunwind does ship an
  `Unwind-seh.cpp` bridge that can adapt Itanium `_Unwind_*` semantics onto
  it, but choosing that path as the default would mean CRT's Windows C++
  exception behavior is driven by an OS-native facility outside the
  project's own runtime, diverging from how Linux/macOS unwind and
  reintroducing exactly the kind of host-runtime dependency the PAL design
  exists to eliminate.
- This is the same "own the toolchain, never substitute a host-provided
  runtime for the project's own" principle already applied twice elsewhere:
  rejecting host `libunwind-dev` on Linux, and Skia's policy of never
  linking a host `libc++` as a substitute for the project's own.
- This does not mean avoiding Windows APIs altogether. CRT's libunwind still
  calls ordinary documented Win32 facilities for auxiliary bookkeeping --
  for example `EnumProcessModules`/`psapi.h` for PE module enumeration in
  `AddressSpace.hpp`'s `findUnwindSections()` -- matching how the rest of the
  Windows PAL already declares raw `__declspec(dllimport)` Win32 prototypes
  (`libc/src/arch/windows/`) rather than pulling in `<windows.h>` wholesale.
  What is specifically avoided is depending on the OS's native SEH *unwind
  engine* as the mechanism that walks the stack and dispatches destructors
  and catch handlers; documented Win32 calls for bookkeeping are the normal,
  already-established PAL pattern.

`-fdwarf-exceptions` must be set on both `CMAKE_CXX_FLAGS` and
`CMAKE_C_FLAGS` in `tools/crt-libcxx-build.py`: libunwind has plain-C sources
(`UnwindLevel1.c`, `UnwindLevel1-gcc-ext.c`, `Unwind-sjlj.c`) that also
transitively reach the `__SEH__`-gated `<windows.h>` include in `unwind.h` via
`libunwind_ext.h`, which this project's `-nostdinc` freestanding build cannot
satisfy. See `tools/crt-libcxx-build.py`'s own comment at the flag's
definition for the full empirical trail (confirmed via
`clang++ --target=x86_64-w64-mingw32 -dM -E` showing `__SEH__` defined by
default and undefined under `-fdwarf-exceptions`), and `TODO.md`'s C++
runtime prerequisite section for the adoption plan this decision is step 1
of.

### Known cost: DWARF-compiled code has zero Windows-native unwind info

This was checked directly (2026-08-21), not assumed: compiling identical code
for `x86_64-w64-mingw32` with `-fseh-exceptions` versus `-fdwarf-exceptions`
and diffing the object's sections shows `-fdwarf-exceptions` emits **no
`.pdata`/`.xdata` at all** -- only a non-standard `.eh_frame` section that
only CRT's own from-source libunwind understands. This is not limited to
functions that actually throw: a plain C function that does nothing but call
another function loses `.pdata`/`.xdata` too under `-fdwarf-exceptions`. The
personality also differs: SEH builds reference `__gxx_personality_seh0` (a
real SEH-compatible bridge `ntdll` can invoke); DWARF builds reference
`__gxx_personality_v0` (pure Itanium, opaque to the OS).

This matters beyond C++ `catch` interop. The Windows x64 calling convention
requires unwind info for every non-leaf function; when `RtlLookupFunctionEntry`
finds no entry for an address, the OS assumes that function is a leaf (never
adjusts the stack) and skips register restoration when unwinding past it. A
real non-leaf CRT/libc++ function built with `-fdwarf-exceptions` violates
that assumption, so any OS-driven unwind that has to walk through such a frame
computes the wrong caller context -- this is undefined behavior by the ABI's
own contract, not just "no handler found." Concretely, this can be hit by:

- a hardware exception (access violation, divide-by-zero, stack overflow)
  raised inside a CRT/libc++ (DWARF) frame that needs to propagate past it
  (the OS's own second-chance SEH search walks `.pdata`-less frames
  incorrectly, which can corrupt the search or crash the process outright,
  independent of whether any CRT code is even trying to catch anything);
- any Windows-native stack walk that has to cross a CRT/libc++ frame for a
  reason unrelated to C++ exceptions at all -- a debugger's call-stack view,
  a Windows Error Reporting minidump, an ETW-based profiler, or
  `RtlCaptureStackBackTrace`;
- CRT/libc++-compiled code exposed directly as a native OS callback (a window
  procedure, a `CreateThread` entry point, a vectored exception handler, a COM
  vtable method) invoked without an intervening real-SEH boundary frame.

Plain DLL loading is **not** affected by any of this: `LoadLibrary` plus
calling a non-throwing exported function is pure PE-loader/import-resolution
mechanics and does not consult unwind tables at all. The risk is specifically
about unwinding actually having to traverse a DWARF-only frame, whether
that's driven by a C++ `throw` or by the OS itself.

This is a real, accepted cost of `-fseh-exceptions` not being used, common to
any non-native exception model on x86-64 Windows (the older 32-bit-only
"DW2" GCC mingw model avoided this because 32-bit SEH is a linked-list
mechanism rather than table-based, which does not generalize to x64). It
sharpens, rather than replaces, the existing Windows MSVC ABI Bridge Lane
caveat above ("exceptions may cross the bridge... default answer being no
until unwind interoperability is proven"): the gap is not only personality/
RTTI/name-mangling incompatibility, it is that CRT/libc++ frames are not
OS-unwindable at all today.

**Mitigated (2026-08-21) for the third bullet** (a raw native OS callback,
never for the first two -- see the honest scope breakdown below):
`libc/src/arch/windows/common/dwarf_unwind_safety_net.c` installs a
process-wide vectored exception handler (`AddVectoredExceptionHandler`) at
CRT startup (both `crt1.c` for executables and `dllcrt.c` for DLLs). See
that file's own top comment for the full empirical design story (TODO.md
item 7), but the short version: the boundary-shim idea this section
originally proposed ("wrap every native-callback entry point in a real-SEH
frame") was tried and empirically DISPROVED first -- a single `-fseh-
exceptions`-compiled `__try`/`__except` wrapped directly around a call into
DWARF-compiled code does **not** catch a hardware fault raised several
frames deeper, because the OS's own frame-based search still has to walk
through the untabled frames beneath the boundary to reach it, and fails at
the first one it meets. `AddVectoredExceptionHandler`, by contrast, does not
depend on walking the stack at all -- it is dispatched directly from the
fault's own context. The one real risk with VEH (it fires unconditionally,
*before* any frame-based `__try`/`__except` gets a chance, so a naive
"always terminate" handler would break a legitimate application-level catch
elsewhere in the process) is closed by gating this file's own handler on
`RtlLookupFunctionEntry()` -- the exact same table lookup the OS's own
frame-based dispatch relies on -- called on the faulting address itself:
when it returns non-NULL (real `.pdata` present, regardless of whether it
has anything to do with this project's own CRT/libc++), the handler defers
completely (`EXCEPTION_CONTINUE_SEARCH`), confirmed empirically to let a
real `__except` elsewhere in the chain catch the exception exactly as if
this file were never linked in; only when it returns NULL (the actual gap)
does the handler take over, reporting a brief diagnostic and calling
`ExitProcess()` with a `128 + <POSIX signal number>` exit code (this
project's own existing convention, see `libc/src/signal.c`'s `abort()`)
instead of leaving the fault to the OS's own corrupted second-chance search.

What this does and does not fix, stated plainly:
- **Fixed**: a hardware fault whose address has no Windows-native unwind
  info now produces a controlled, deterministic process exit instead of
  undefined behavior -- this is exactly what makes CRT/libc++-compiled code
  safe to register directly as a raw native OS callback (this section's
  third bullet above), and needs no per-callsite boundary shim at all: one
  process-wide registration covers every thread and call path.
- **Not fixed**: a C++ `catch` still cannot recover from a hardware fault
  (never promised -- matches this project's existing non-`/EHa` semantics,
  same as Linux/macOS). Third-party tools that do their own separate,
  OS-native frame-based walk (a debugger's live call-stack view, WER's own
  minidump writer, an ETW-based profiler) still misbehave crossing a DWARF
  frame exactly as described above (this section's first two bullets) --
  this project's own diagnostic runs first and is unaffected by that, but
  cannot repair what a third-party tool shows afterward. No backtrace is
  attempted either (only the single faulting address is reported) --
  `dwarf_unwind_safety_net.c`'s own top comment explains why a real
  DWARF-CFI-based backtrace via this project's own libunwind was
  deliberately left as a follow-up rather than pulled into this pass (it is
  only an optional `CRT_USE_IMPORTED_LIBCXX` component today, not a base
  libc dependency).

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

**Update**: item 1 below is done -- `tests/cxx_frontend_test.cc`
(`add_crt_cxx_test(cxx_frontend_test ...)` in `tests/CMakeLists.txt`) is a
real C++ frontend compile/link/run probe, built and run via `ctest` on every
host the project is configured for (not Linux/macOS-only as originally
scoped), alongside `cxx_runtime_test` for the ABI hook surface itself.
Remaining recommended next work:

1. Add a Windows policy probe that records which C++ ABI hooks Clang emits for
   the selected target/profile.
2. **Done (2026-08-18):** `operator new/delete`, array forms, sized delete,
   and nothrow forms forward to the CRT allocator. `cxx_allocation_test`
   validates that boundary on every host.
3. **macOS complete:** Android libc++ and libc++abi build as static and shared
   libraries through the CRT wrappers, install into the sysroot/rootfs, and
   pass `crt-libcxx-smoke`. Use this runtime before enabling a real Skia link.
   Skia's CPU-raster archive still uses `std::string`, shared ownership,
   streams, and locale machinery even with GPU backends disabled. It must not
   be satisfied by silently linking the host libc++.
4. Build current AOSP LLVM libunwind from `toolchain/llvm-project`, then run
   the same imported-runtime smoke on Linux and Windows.
5. Start a separate Windows MSVC ABI bridge design with C ABI wrapper tests
   before allowing C++ object or exception interop across the bridge.

## Android LLVM Runtime Import

`crt-libcxx-fetch` currently fetches Android's `platform/external/libcxx` and
`platform/external/libcxxabi` repositories at one configurable
`CRT_LIBCXX_ANDROID_REF` (default `refs/heads/main`) into
`out/<preset>/external/llvm-runtimes/`. The project-owned metadata is under
`libstdc++/third_party/`; no upstream source is committed there. The retired
`platform/external/libunwind` repository is not fetched; current unwind work
uses AOSP `toolchain/llvm-project/libunwind`.

The CMake targets are deliberately staged:

```sh
cmake --build --preset <host-preset> --target crt-libcxx-fetch
cmake --build --preset <host-preset> --target crt-libcxx-configure
cmake --build --preset <host-preset> --target crt-libcxx-build
cmake --build --preset <host-preset> --target crt-libcxx-sysroot
cmake --build --preset <host-preset> --target crt-libcxx-smoke
```

`crt-libcxx-configure` and `crt-libcxx-build` always use `tools/crt-cc` and
`tools/crt-c++`, with exceptions/RTTI enabled only for the imported runtime.
The sysroot target stages headers and runtime libraries. The smoke target then
links and runs both static and shared forms of `tests/imported_libcxx_test.cc`
using only the staged C++ headers and runtime. `CRT_USE_IMPORTED_LIBCXX=ON`
makes `rootfs` and the Skia external
build depend on that staged runtime rather than the bootstrap `cxx_shared`
artifact. This mode is now verified on macOS; keep it host-explicit until the
Linux and Windows smoke targets pass.

The first import gate is source provenance and compiler mode. `tools/crt-c++`
now retains the bootstrap default of `-fno-exceptions -fno-rtti`, but an
external runtime build may explicitly set `CRT_CXX_ENABLE_EXCEPTIONS=1` and
`CRT_CXX_ENABLE_RTTI=1`. The next gate is to build libc++/libc++abi/libunwind
as one CRT static-and-shared set, then replace the bootstrap archive only after
standard-library and Skia link/run tests pass on Linux, macOS, and Windows.

The macOS build now completes both libc++abi and libc++ static/shared outputs.
It required the Bionic-main msun families recorded in the import manifest,
locale/time/wchar declarations, `nan*`, aligned allocation, and the
Bionic-shaped `<android/api-level.h>` host policy. `tools/crt-c++` also absorbs
legacy `-lpthread`/`-lrt` into libc, matching modern Bionic, so Darwin
libSystem symbols cannot preempt Bionic-shaped pthread objects.

Darwin's system libc++ re-exports strong C++ symbols, so a static libc++ archive
inside a normal macOS executable can be interposed by the system dylib. The
verified macOS execution shape therefore uses `CRT_CXX_RUNTIME_LINKAGE=shared`
and an rpath to the CRT sysroot. Static archives are still built and installed;
static link/run coverage remains a Linux/Windows gate.

Windows configure uses the installed CRT `mksh.exe` as CMake's compiler
launcher with `crt-cc`/`crt-c++` as `CMAKE_*_COMPILER_ARG1`, because native
Windows cannot execute their shebangs directly. Shared DLL and import-library
staging is prepared, but real Windows and Linux builds remain required before
cross-host completion is claimed. macOS `Availability.h` is still not a
Bionic public header.
