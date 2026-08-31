# C++ Runtime

This document records the C++ runtime policy and current imported-runtime
status for CRT.

## Current Status

- The small in-tree `cxx`/`cxx_shared` libraries remain the default bootstrap
  ABI/allocation runtime when `CRT_USE_IMPORTED_LIBCXX=OFF`.
- The opt-in imported runtime uses one pinned AOSP
  `toolchain/llvm-project` revision for libc++, libc++abi, and libunwind source
  recipes. It builds and stages static/shared libc++ and libc++abi on Linux,
  macOS, and Windows.
- Linux and Windows build project-owned LLVM libunwind. macOS deliberately
  uses the libSystem unwinder and therefore excludes the libunwind recipe.
- `crt-libcxx-smoke` passes static and shared vector/string/RTTI/exception
  execution on all three hosts. Skia uses this imported runtime; host libc++ or
  GNU libstdc++ is not a runtime substitute.
- Windows CRT-targeted C++ uses the Itanium ABI/DWARF-CFI lane. A distinct
  MSVC-ABI DLL bridge remains a narrow future interoperability lane, not the
  core runtime ABI.

## Direction

The source directory is named `libstdc++/` because the original Android Bionic
tree historically used that directory for small C++ ABI support symbols. The
project policy is not to adopt GNU libstdc++ as the C++ standard library.

The intended stack is the same separation Android uses: Bionic's small
`libstdc++` ABI surface plus the separately maintained LLVM libc++, libc++abi,
and libunwind projects. It is not a plan to hand-write STL containers.

The stack is:

- project-owned C++ ABI bootstrap library, installed when the imported runtime
  is disabled;
- compiler-rt builtins for compiler-generated helper calls;
- project-built LLVM libunwind on Linux/Windows and libSystem unwind on macOS;
- imported libc++abi for the full Itanium C++ ABI surface;
- imported libc++ as the C++ standard library.

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
This bootstrap bridge does not make arbitrary MSVC-ABI C++ object or exception
interop safe. The imported Itanium lane does support exceptions/RTTI inside
CRT-targeted code; crossing into an unrelated MSVC-ABI DLL remains future work.

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
`-fno-rtti`. The imported Android LLVM libc++/libc++abi lane sets
`CRT_CXX_ENABLE_EXCEPTIONS=1` and `CRT_CXX_ENABLE_RTTI=1`; its static and
shared smoke verifies a real `std::runtime_error` throw/catch together with
`std::vector` and `std::string` on Linux, macOS, and Windows.

Linux and Windows use the project-built LLVM libunwind recipe. macOS resolves
`_Unwind_*` through the documented libSystem boundary. The retired
`platform/external/libunwind` repository is not used; the pinned source comes
from AOSP `toolchain/llvm-project`.

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
default and undefined under `-fdwarf-exceptions`), and `HISTORY.md`'s
2026-08-21 C++ runtime entries for the adoption trail.

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
that file's own top comment for the full empirical design story, but the short
version: the boundary-shim idea this section
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

The imported runtime build/stage/smoke milestone is complete. Remaining work
is narrower:

1. Add further focused standard-library behavior coverage where host
   adaptation is subtle. Windows `<filesystem>` UTF-32-`wchar_t`-to-UTF-16
   path handling, the item that used to head this list, is done -- see
   `HISTORY.md`'s 2026-08-31 entry for the full investigation (a real
   client/library ABI mismatch caused by an existing `<print>` patch's
   translation-unit-global `#undef`, plus several other real bugs found
   and fixed along the way).
2. Keep the Windows DWARF fault safety net covered and decide separately
   whether project-owned libunwind backtraces are worth adding; OS-native stack
   walkers cannot recover full call stacks through untabled DWARF-only frames.
3. Design the separate Windows MSVC ABI bridge with C-callable wrapper tests
   before allowing C++ objects, allocation ownership, RTTI, or exceptions to
   cross that boundary.
4. Establish export/version policy before treating imported C++ shared
   runtimes as ABI-stable distribution artifacts.

## Android LLVM Runtime Import

`crt-libcxx-fetch` reads the three project-owned recipes under
`libstdc++/third_party/{libcxx,libcxxabi,libunwind}/recipe.json`. They use one
pinned AOSP `toolchain/llvm-project` commit and sparse-check out only the
required source/CMake support trees under
`out/<preset>/external/llvm-runtimes/`; no upstream source is committed here.
The libunwind recipe targets Linux and Windows only because macOS uses
libSystem unwind.

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
makes `rootfs` and the Skia external build depend on that staged runtime rather
than the bootstrap `cxx_shared` artifact. This mode and both smoke linkage
shapes are verified on Linux, macOS, and Windows.

`tools/crt-c++` retains the bootstrap default of `-fno-exceptions -fno-rtti`,
while the external runtime build explicitly sets
`CRT_CXX_ENABLE_EXCEPTIONS=1` and `CRT_CXX_ENABLE_RTTI=1`. The imported set is
selected explicitly rather than silently replacing the bootstrap in every
ordinary build.

The macOS build now completes both libc++abi and libc++ static/shared outputs.
It required the Bionic-main msun families recorded in the import manifest,
locale/time/wchar declarations, `nan*`, aligned allocation, and the
Bionic-shaped `<android/api-level.h>` host policy. `tools/crt-c++` also absorbs
legacy `-lpthread`/`-lrt` into libc, matching modern Bionic, so Darwin
libSystem symbols cannot preempt Bionic-shaped pthread objects.

Darwin's system libc++ exports overlapping strong C++ symbols, so linkage order
and runtime paths remain deliberate. Both imported static and shared smoke
shapes now pass on macOS; neither is permission to mix host-libc++ objects into
the CRT C++ ABI lane.

Windows configure uses the installed CRT `mksh.exe` as CMake's compiler
launcher with `crt-cc`/`crt-c++` as `CMAKE_*_COMPILER_ARG1`, because native
Windows cannot execute their shebangs directly. Static/shared runtime and
import-library staging is verified on Windows and Linux as well as macOS.
macOS `Availability.h` remains a host-private concern, not a Bionic public
header.
