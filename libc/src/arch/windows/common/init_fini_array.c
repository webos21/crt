/* Runs constructors/destructors for the executable's own static link --
 * the Windows/PE equivalent of
 * libc/src/arch/linux/common/init_fini_array.c; see that file's own
 * comment for why this exists at all (found and fixed for Linux first
 * while porting xz/liblzma, porting/recipes/xz.json; confirmed Windows
 * had the identical gap by direct testing before this file existed --
 * see HISTORY.md's dated entries and TODO.md's ".init_array"/
 * ".fini_array" item for the full story).
 *
 * Unlike Linux, this can't rely on automatic linker-provided boundary
 * symbols (COFF/PE has no __start_SECNAME/__stop_SECNAME equivalent).
 * Worse, on Windows this project actually has TWO, entirely separate
 * constructor/destructor conventions in play depending on how the
 * calling code itself was compiled -- see ctors_begin.c's own comment
 * for the full empirical story:
 *
 *  - GNU/MinGW ABI (tools/crt-cc's --target=*-w64-mingw32 builds, used
 *    for third-party ports like xz/liblzma): a plain, unnamed-group
 *    ".ctors"/".dtors" section per function, merged in strict
 *    link-command-line order -- bracketed by __crt_ctors_begin/end and
 *    __crt_dtors_begin/end (ctors_begin.o placed right after crt1.o,
 *    ctors_end.o placed at the very end of the link line -- see
 *    tools/crt-cc's own comment).
 *
 *  - MSVC ABI (this project's own CMake-native builds -- libc itself,
 *    tests/, shell/ -- compiled by plain clang with no explicit
 *    --target): the fixed section names ".CRT$XCU"/".CRT$XTX", merged
 *    in alphabetical "$"-suffix order regardless of link-command-line
 *    order -- bracketed by __crt_msvc_ctors_begin/end and
 *    __crt_msvc_dtors_begin/end (".CRT$XCA"/".CRT$XTA" sort before,
 *    ".CRT$XCZ"/".CRT$XTZ" sort after, so no special link placement is
 *    needed for this pair at all).
 *
 * Both are walked unconditionally: a given final executable link can
 * contain code compiled under either ABI (this project's own libc/crt1
 * plus a third-party port's sources), so a constructor/destructor
 * anywhere needs to run regardless of which convention produced it. An
 * executable that happens to have nothing in one of the two regions
 * just walks an empty (single-slot, begin==end) range there -- a no-op,
 * not an error.
 *
 * Each walk loop stops by *pointer identity* against its own region's
 * end marker address, not by inspecting a sentinel value -- deliberately
 * safer against a stray real function pointer bit pattern ever
 * coinciding with a chosen "magic" value.
 */

typedef void (*crt_init_func)(void);

extern crt_init_func __crt_ctors_begin;
extern crt_init_func __crt_ctors_end;
extern crt_init_func __crt_dtors_begin;
extern crt_init_func __crt_dtors_end;

extern crt_init_func __crt_msvc_ctors_begin;
extern crt_init_func __crt_msvc_ctors_end;
extern crt_init_func __crt_msvc_dtors_begin;
extern crt_init_func __crt_msvc_dtors_end;

static void run_ctor_range(crt_init_func* begin, crt_init_func* end) {
  crt_init_func* p = begin;

  for (; p != end; ++p) {
    if (*p != 0) {
      (*p)();
    }
  }
}

static void run_dtor_range(crt_init_func* begin, crt_init_func* end) {
  crt_init_func* p = end;

  /* Destructors run in the reverse of construction order, matching
   * every other libc's convention (glibc, musl, Bionic) and this
   * project's own Linux implementation. */
  while (p != begin) {
    --p;
    if (*p != 0) {
      (*p)();
    }
  }
}

void __crt_run_init_array(void) {
  run_ctor_range(&__crt_ctors_begin, &__crt_ctors_end);
  run_ctor_range(&__crt_msvc_ctors_begin, &__crt_msvc_ctors_end);
}

void __crt_run_fini_array(void) {
  run_dtor_range(&__crt_dtors_begin, &__crt_dtors_end);
  run_dtor_range(&__crt_msvc_dtors_begin, &__crt_msvc_dtors_end);
}
