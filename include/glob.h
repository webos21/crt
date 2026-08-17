#ifndef CRT_GLOB_H
#define CRT_GLOB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t gl_pathc;
  char** gl_pathv;
  size_t gl_offs;
} glob_t;

/* Behavior flags (POSIX-mandated subset; GLOB_BRACE/GLOB_TILDE and other
 * shell-ism extensions some BSD/glibc implementations add are not
 * implemented -- no identified consumer needs them). */
#define GLOB_APPEND 0x0001
#define GLOB_DOOFFS 0x0002
#define GLOB_ERR 0x0004
#define GLOB_MARK 0x0008
#define GLOB_NOCHECK 0x0010
#define GLOB_NOSORT 0x0020
#define GLOB_NOESCAPE 0x0040

/* Return values. */
#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3
#define GLOB_NOSYS 4

int glob(
    const char* pattern, int flags, int (*errfunc)(const char* epath, int eerrno),
    glob_t* pglob);
void globfree(glob_t* pglob);

#ifdef __cplusplus
}
#endif

#endif
