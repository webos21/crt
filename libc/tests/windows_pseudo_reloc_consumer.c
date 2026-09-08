/* Other half of the _pei386_runtime_relocator() regression test -- see
 * windows_pseudo_reloc_dll.c and libc/src/arch/windows/common/
 * pseudo_reloc.c's own comments for the full story.
 *
 * Links against libpseudoreloc_regress.dll.a (the import library for
 * windows_pseudo_reloc_dll.c's DLL) and takes the address of its plain,
 * non-dllimport-annotated global -- but, critically, stores that
 * address as a STATIC-DURATION AGGREGATE INITIALIZER
 * (windows_pseudo_reloc_regress_holder below), not a plain local
 * `int* p = &value;` assignment. This distinction turned out to matter
 * a great deal, confirmed the hard way while building this test: a
 * simple runtime pointer assignment compiles to an actual LOAD
 * instruction that can legitimately go through the normal, real
 * __imp_-prefixed import stub (which the OS loader itself populates
 * correctly regardless of pseudo-relocation, so testing that shape
 * produced an empty pseudo-reloc list and passed vacuously, catching
 * nothing). An aggregate initializer, by contrast, must be literal
 * embedded bytes -- data can't "be" a load instruction -- so the
 * compiler has no choice but to bake in a placeholder address value
 * needing a genuine runtime pseudo-relocation fixup, exactly matching
 * libffi's own `ffi_type* args[2] = { &ffi_type_sint, ... }` pattern
 * that originally surfaced this whole gap.
 *
 * Without a working _pei386_runtime_relocator() linked into and called
 * by this project's own Windows crt1 startup, this either fails to
 * link at all ("output image has runtime pseudo relocations, but the
 * function _pei386_runtime_relocator is missing") or, if some other
 * mechanism papers over that, reads back the wrong value (the
 * compile-time placeholder -- the IAT slot's own address -- never
 * having been fixed up to the real cross-DLL address). Confirmed both
 * failure modes directly while root-causing this. */

#include <stdio.h>

extern int windows_pseudo_reloc_regress_value;

static int* windows_pseudo_reloc_regress_holder[1] = {&windows_pseudo_reloc_regress_value};

int main(void) {
  int* p = windows_pseudo_reloc_regress_holder[0];

  if (*p == 424242) {
    printf("windows_pseudo_reloc_test: ok\n");
    return 0;
  }
  printf("windows_pseudo_reloc_test: FAIL got=%d\n", *p);
  return 1;
}
