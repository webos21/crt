/* See init_fini_array.c's own comment for the full story. This
 * translation unit's only job is to contribute the FIRST entry to BOTH
 * of the two, entirely separate constructor/destructor conventions this
 * project's Windows builds can produce, depending on how the calling
 * code itself was compiled:
 *
 *  - GNU/MinGW ABI (tools/crt-cc's own --target=*-w64-mingw32 builds,
 *    used for third-party autoconf/configure ports like xz/liblzma):
 *    clang emits a plain, unnamed-group ".ctors"/".dtors" section per
 *    __attribute__((constructor))/((destructor)) function -- confirmed
 *    via llvm-objdump -h against that exact target. COFF has no
 *    automatic linker-provided boundary symbols for a plain section (no
 *    __start_SECNAME/__stop_SECNAME equivalent), and lld-link merges
 *    same-named plain sections from different object files strictly in
 *    link-command-line order -- the same reason real mingw-w64 crt0
 *    ships dedicated crtbegin.o/crtend.o objects. tools/crt-cc places
 *    this object immediately after crt1.o (before anything a program or
 *    the libraries it links might itself contribute to .ctors/.dtors)
 *    so its own slot is always first; ctors_end.c sits last on the link
 *    line for the same reason in the other direction.
 *
 *  - MSVC ABI (this project's own CMake-native builds -- libc itself,
 *    tests/, shell/ -- compiled by plain clang with no explicit
 *    --target, which defaults to *-pc-windows-msvc; see the top-level
 *    CMakeLists.txt's own comment on why nothing here needs the
 *    GNU-compatible target crt-cc uses): clang instead emits the fixed,
 *    hardcoded section names ".CRT$XCU" (constructors) / ".CRT$XTX"
 *    (destructors) -- confirmed empirically against this exact
 *    toolchain by compiling a constructor/destructor probe with no
 *    --target override and inspecting the object with llvm-objdump -h.
 *    Unlike the GNU case, "$"-suffixed COFF section names ARE
 *    alphabetically sorted by lld-link when merging same-group
 *    ("CRT$X*") sections from different object files, REGARDLESS of
 *    link-command-line order -- confirmed empirically: three objects
 *    contributing to ".CRT$XCA"/".CRT$XCU"/".CRT$XCZ" respectively,
 *    deliberately linked in reverse (Z, U, A) order, still produced a
 *    final merged section with A's data first, U's in the middle, Z's
 *    last. So ".CRT$XCA"/".CRT$XTA" (this file, sorts before the "U"/
 *    "X" real entries) and ".CRT$XCZ"/".CRT$XTZ" (ctors_end.c, sorts
 *    after) bracket that convention reliably without needing any
 *    special link-line placement at all -- they only ride along in the
 *    same "begin" group as the position-sensitive GNU sentinels above
 *    purely for file-organization convenience.
 *
 * Both conventions are walked (see init_fini_array.c) so that a
 * constructor/destructor anywhere in the final link -- whether from
 * this project's own native code or from a crt-cc-built third-party
 * port -- actually runs, regardless of which ABI compiled it.
 *
 * Every marker's own value is never read (see init_fini_array.c: each
 * walk loop stops via *pointer identity* against its end marker's
 * address, not by inspecting any sentinel value stored here) -- 0 is
 * used only so that a walk which accidentally started one slot too
 * early wouldn't call through an uninitialized/garbage pointer.
 */

__attribute__((section(".ctors"), used)) void (*__crt_ctors_begin)(void) = (void (*)(void))0;
__attribute__((section(".dtors"), used)) void (*__crt_dtors_begin)(void) = (void (*)(void))0;

__attribute__((section(".CRT$XCA"), used)) void (*__crt_msvc_ctors_begin)(void) = (void (*)(void))0;
__attribute__((section(".CRT$XTA"), used)) void (*__crt_msvc_dtors_begin)(void) = (void (*)(void))0;
