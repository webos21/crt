#ifndef CRT_PRIVATE_CRT_MACHO_SYMBOL_H
#define CRT_PRIVATE_CRT_MACHO_SYMBOL_H

/* Shared Mach-O export-trie symbol resolution engine for macOS.
 *
 * dyld's legacy NSAddImage()/NSLookupSymbolInImage() API cannot look up
 * symbols in dyld-shared-cache images (deprecated since Mac OS X 10.5, and
 * the shared cache -- used for every system library since macOS 11 "Big
 * Sur" -- stores exports as a compact "export trie" that API has no
 * knowledge of). This engine walks that trie directly instead, mirroring
 * dyld's own MachOLoaded::trieWalk()/findExportedSymbol()
 * (apple-oss-distributions/dyld, dyld3/MachOLoaded.cpp), reimplemented in C.
 * See docs/dynamic_loading.md for the full design writeup.
 *
 * This lives in libc (not libdl) because more than one CRT component needs
 * it: libdl's macOS dlsym() backend (libdl/src/arch/macos/dl_macos.c) for
 * general-purpose symbol lookup, and libc's own macOS signal delivery
 * backend (libc/src/arch/macos/common/signal_backend.c), which needs the
 * *real* libSystem sigaction()/sigprocmask() despite this libc defining
 * public symbols with those exact same names -- dlopen()/dlsym() are not an
 * option there, since libdl links against libc, and libc cannot link back
 * against libdl without a circular target dependency. libdl already depends
 * on libc for other reasons, so exposing this engine as a libc-private
 * header for libdl to also include has no such problem. */

/* Finds `image_name` (an exact dyld-reported path, or just a basename) among
 * dyld's currently loaded images. Returns an opaque handle usable with
 * __crt_macho_find_symbol_in_image(), or 0 if no loaded image matches. */
const void* __crt_macho_find_loaded_image(const char* image_name);

/* Looks up `symbol` (a plain C name; the leading underscore Mach-O symbol
 * tables use is added internally) in the image identified by `handle` (from
 * __crt_macho_find_loaded_image(), or any other 64-bit Mach-O header address
 * dyld has mapped, e.g. an NSAddImage() result). Follows re-exports (both an
 * individual trie node's REEXPORT flag, and falling through to every
 * LC_REEXPORT_DYLIB dependency when the symbol is absent from this image's
 * own trie entirely). Returns 0 if not found. */
void* __crt_macho_find_symbol_in_image(const void* handle, const char* symbol);

/* Convenience: __crt_macho_find_loaded_image() + __crt_macho_find_symbol_in_image()
 * in one call, for the common case of resolving one real symbol out of one
 * named system library. Returns 0 if the image is not loaded or does not
 * export the symbol. */
void* __crt_macho_find_symbol_in_loaded_image(const char* image_name, const char* symbol);

/* Scans every currently loaded dyld image in order, returning the first
 * match. Matches dlsym(RTLD_DEFAULT, ...) search semantics. */
void* __crt_macho_find_symbol_in_any_loaded_image(const char* symbol);

#endif
