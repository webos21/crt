#ifndef CRT_FEATURES_H
#define CRT_FEATURES_H

/* Synonym for <sys/cdefs.h>, for source compatibility with glibc -- matches
 * real Android Bionic's own include/features.h exactly (a one-line
 * #include, nothing else; verified against the real upstream source, not
 * guessed). Bionic deliberately does not define __GLIBC_PREREQ here (it is
 * not glibc), which is exactly the property LLVM libc++'s own <__config>
 * relies on: `#if defined(__linux__) #include <features.h> #if
 * defined(__GLIBC_PREREQ) ... #else _LIBCPP_GLIBC_PREREQ(a, b) 0 #endif`
 * unconditionally includes <features.h> on any __linux__ target (no glibc-
 * vs-not guard of its own), then falls back to the conservative "not
 * glibc" branch when __GLIBC_PREREQ is absent -- so providing this real,
 * minimal, Bionic-faithful header (rather than a full glibc <features.h>
 * with feature-test-macro machinery this project has no use for) is both
 * the correct porting-loop answer and sufficient to satisfy libc++. */
#include <sys/cdefs.h>

#endif
