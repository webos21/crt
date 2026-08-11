/* See ctors_begin.c and init_fini_array.c's own comments for the full
 * story. This translation unit contributes the LAST entry to both the
 * GNU-ABI (.ctors/.dtors) and MSVC-ABI (.CRT$XCZ/.CRT$XTZ) conventions.
 *
 * The GNU-ABI pair is link-order-sensitive: this object must be linked
 * after every other object/archive that could itself contribute a
 * .ctors/.dtors entry (the program's own objects, any static library it
 * links, and this project's own libc.a/libc.so, in that priority order
 * on tools/crt-cc's actual link line).
 *
 * The MSVC-ABI pair ("Z" sorts after the real entries' "U"/"X" group) is
 * NOT link-order-sensitive -- lld-link alphabetically sorts same-$-group
 * COFF sections regardless of link-command-line order (see
 * ctors_begin.c's comment for the empirical confirmation) -- but rides
 * along in this same "end" translation unit purely for convenience.
 */

__attribute__((section(".ctors"), used)) void (*__crt_ctors_end)(void) = (void (*)(void))0;
__attribute__((section(".dtors"), used)) void (*__crt_dtors_end)(void) = (void (*)(void))0;

__attribute__((section(".CRT$XCZ"), used)) void (*__crt_msvc_ctors_end)(void) = (void (*)(void))0;
__attribute__((section(".CRT$XTZ"), used)) void (*__crt_msvc_dtors_end)(void) = (void (*)(void))0;
