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

int crtDllMainCRTStartup(void* module, unsigned long reason, void* reserved) {
  (void)module;
  (void)reserved;
  if (reason == 1 /* DLL_PROCESS_ATTACH */) {
    _pei386_runtime_relocator();
  }
  return 1;
}
