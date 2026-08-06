/* Empty stub. Upstream NetBSD/Bionic regerror.c and regfree.c include
 * "namespace.h" unconditionally (regcomp.c and regexec.c guard it behind
 * `#ifndef LIBHACK`, which netbsd-compat.h defines, so they never reach
 * it at all). On real NetBSD this renames public symbols to their
 * internal `_foo` aliases for libc's own use; this project has no such
 * renaming scheme, so an empty header satisfies the #include with no
 * effect. See netbsd-compat.h for the rest of the portability shim. */
#ifndef CRT_REGEX_NAMESPACE_H
#define CRT_REGEX_NAMESPACE_H
#endif
