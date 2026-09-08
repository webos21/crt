/* Half of a permanent regression test for
 * libc/src/arch/windows/common/pseudo_reloc.c's _pei386_runtime_relocator()
 * -- see that file's own comment for the full story.
 *
 * This translation unit is built as its own small DLL
 * (libpseudoreloc_regress.dll, via `tools/crt-cc -shared`) so that
 * windows_pseudo_reloc_consumer.c's reference to this global crosses a
 * real DLL boundary. Deliberately a PLAIN global with no
 * __declspec(dllexport)/(dllimport) annotation on either side -- exactly
 * the pattern that forces GNU ld/lld's auto-import + runtime
 * pseudo-relocation mechanism to kick in (confirmed the hard way: this
 * is the same pattern libffi's own ffi.h uses for its exported
 * ffi_type_* globals, which is what originally surfaced this gap -- see
 * HISTORY.md's dated entry).
 */

int windows_pseudo_reloc_regress_value = 424242;
