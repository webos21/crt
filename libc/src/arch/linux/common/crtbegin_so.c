/* crtbegin_so.o: the per-shared-object __dso_handle, the role Android Bionic's
 * (and glibc's) crtbegin_so.o plays. The Itanium C++ ABI registers every static
 * destructor with __cxa_atexit(destructor, object, &__dso_handle), so a shared
 * library that has any static-duration C++ object with a destructor (ICU,
 * JavaScriptCore, ...) references __dso_handle, and each DSO needs its own,
 * hidden one so a later __cxa_finalize(&__dso_handle) can tell the DSOs apart.
 *
 * libc.a already embeds one (cxa_atexit.c) in every image that links the archive,
 * but a shared-object link made by tools/crt-cc / tools/crt-c++ links libc.so, not
 * libc.a, so nothing defined it ("undefined reference to `__dso_handle'",
 * found building ICU's libicuuc.so). The wrappers add this object to every
 * Linux `-shared` link that is not building the CRT runtime itself; it must not
 * be combined with libc.a's copy in one image. */
__attribute__((visibility("hidden"))) void* __dso_handle = &__dso_handle;
