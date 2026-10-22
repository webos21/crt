/* Defined by libc/src/arch/windows/common/pseudo_reloc.c
 * (crt1_pseudo_reloc OBJECT library). crt1.c's own mainCRTStartup() calls
 * this as the very first thing at process startup for an EXECUTABLE; a
 * DLL has no equivalent "process startup" moment of its own, so this is
 * the DLL analogue -- the OS calls this entry point with
 * reason == DLL_PROCESS_ATTACH (1) exactly once, before any of this
 * DLL's own exported functions (or any other code that might touch a
 * GNU-ld-auto-imported data reference crossing into this DLL) can run.
 * See that file's own comment for the full pseudo-relocation story --
 * this was not needed until a real cross-DLL auto-imported data
 * reference first appeared in this project's own build (libc++.dll
 * importing from libunwind.dll), confirmed via lld-link's own refusal to
 * produce the image at all: "output image has runtime pseudo
 * relocations, but the function _pei386_runtime_relocator is missing".
 */
void _pei386_runtime_relocator(void);
/* Defined by libc/src/arch/windows/common/dwarf_unwind_safety_net.c
 * (crt1_dwarf_safety_net OBJECT library). See that file's own top
 * comment for the full story: a process-wide vectored-exception safety
 * net for hardware faults inside DWARF-CFI-compiled (-fdwarf-exceptions)
 * CRT/libc++ code, needed here too since libc++.dll/libc++abi.dll/
 * libunwind.dll are themselves DLLs whose own DLL_PROCESS_ATTACH is the
 * only "process/module startup" moment they get, same reasoning as
 * _pei386_runtime_relocator() above. AddVectoredExceptionHandler() is
 * process-wide, not per-module, so registering it more than once (e.g.
 * once from the main executable's own crt1.c AND once from a DLL it
 * loads) is harmless -- each registration just adds one more handler to
 * the same list, and this file's own RtlLookupFunctionEntry() gate makes
 * every extra invocation of the same handler a fast, side-effect-free
 * no-op once the fault has already been resolved (or, for another
 * genuine gap, redundant-but-correct). */
void _crt_install_dwarf_unwind_safety_net(void);

/* Clang emits a reference to _fltused for x86_64 COFF objects that use floating point; an
 * executable gets the definition from compiler_abi.c (libc.a), a DLL built with this entry
 * file gets it here (found linking JavaScriptCore.dll). Weak, so libc.dll, which also links
 * compiler_abi.o, does not conflict. */
#if defined(__x86_64__) || defined(_M_X64)
__attribute__((weak)) int _fltused = 0;
#endif

/* This DLL's __dso_handle, as crt1.c's for an executable: the identity a C++ object's registration
 * of its destructor names (__cxa_thread_atexit's third argument). Weak, so libc++abi.a's shim wins
 * where that archive is linked statically. libWebKit.dll was the first DLL to need it. */
__attribute__((weak)) void* __dso_handle = &__dso_handle;

/* Static constructors and destructors of THIS DLL. An executable's crt1 walks the regions bracketed by
 * ctors_begin.o/ctors_end.o (".ctors" of the GNU ABI, ".CRT$XCU" of the MSVC one; init_fini_array.c);
 * a DLL has no such moment, so its global C++ objects were never constructed and the first use of a
 * namespace-scope std::map crashed (found by libcrtweb.dll, Web Tranche 4). The tools/crt-cc and
 * tools/crt-c++ shared links now bracket the DLL's own contributions with the same marker objects.
 * Weak references: a DLL linked without them (libc.dll itself, a link by other means) has nothing to
 * walk, which is the old behaviour. */
typedef void (*crt_init_func)(void);
extern crt_init_func __crt_ctors_begin __attribute__((weak));
extern crt_init_func __crt_ctors_end __attribute__((weak));
extern crt_init_func __crt_dtors_begin __attribute__((weak));
extern crt_init_func __crt_dtors_end __attribute__((weak));
extern crt_init_func __crt_msvc_ctors_begin __attribute__((weak));
extern crt_init_func __crt_msvc_ctors_end __attribute__((weak));
extern crt_init_func __crt_msvc_dtors_begin __attribute__((weak));
extern crt_init_func __crt_msvc_dtors_end __attribute__((weak));

static void dll_run_forward(crt_init_func* begin, crt_init_func* end) {
  crt_init_func* p;

  if (begin == 0 || end == 0) {
    return;
  }
  for (p = begin; p != end; ++p) {
    if (*p != 0) {
      (*p)();
    }
  }
}

static void dll_run_reverse(crt_init_func* begin, crt_init_func* end) {
  crt_init_func* p = end;

  if (begin == 0 || end == 0) {
    return;
  }
  while (p != begin) {
    --p;
    if (*p != 0) {
      (*p)();
    }
  }
}

int crtDllMainCRTStartup(void* module, unsigned long reason, void* reserved) {
  (void)module;
  if (reason == 1 /* DLL_PROCESS_ATTACH */) {
    _pei386_runtime_relocator();
    _crt_install_dwarf_unwind_safety_net();
    /* Same complete GNU priority range/order as executable startup. */
    dll_run_reverse(&__crt_ctors_begin, &__crt_ctors_end);
    dll_run_forward(&__crt_msvc_ctors_begin, &__crt_msvc_ctors_end);
  } else if (reason == 0 /* DLL_PROCESS_DETACH */ && reserved == 0) {
    /* FreeLibrary(): destructors run. At process exit (reserved != 0) the other DLLs the objects
     * depend on may already be gone, as for every other DLL; the executable's exit() has run its own. */
    dll_run_forward(&__crt_dtors_begin, &__crt_dtors_end);
    dll_run_reverse(&__crt_msvc_dtors_begin, &__crt_msvc_dtors_end);
  }
  return 1;
}
