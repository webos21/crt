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

int crtDllMainCRTStartup(void* module, unsigned long reason, void* reserved) {
  (void)module;
  (void)reserved;
  if (reason == 1 /* DLL_PROCESS_ATTACH */) {
    _pei386_runtime_relocator();
    _crt_install_dwarf_unwind_safety_net();
  }
  return 1;
}
