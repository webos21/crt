/* Project-owned compatibility subset of <direct.h> -- NOT a real Windows
 * SDK/MSVC-CRT header.
 *
 * Skia's own src/ports/SkOSFile_stdio.cpp does `#include <direct.h>`
 * unconditionally under `#ifdef _WIN32` (immediately followed by
 * `#include <io.h>`, this same directory's own shim), for exactly one
 * function: `_mkdir(const char* path)` -- confirmed for real (2026-08-22):
 * `fatal error: 'direct.h' file not found` building SkOSFile_stdio.cpp,
 * the first time this project's own Windows GN/Skia build reached this
 * file (right after the imported libc++ recipe itself finished building
 * clean on Windows). Grepped the whole file for every other real
 * <direct.h>-family MSVC-CRT function (_getcwd/_chdir/_rmdir/_wmkdir/...)
 * -- zero hits, so only `_mkdir` itself is provided here, matching this
 * project's own established "exactly what's used, no more" shim
 * discipline (see ../windows.h's own file comment).
 *
 * `_mkdir` takes a single argument (no mode, unlike POSIX mkdir()) --
 * implemented here as a thin inline wrapper around this project's own
 * real mkdir(path, mode), picking a conventional 0777 (matching what
 * SkOSFile_stdio.cpp's own non-Windows branch, two lines below its
 * `_mkdir` call, already passes for every other platform) since Windows
 * itself has no real POSIX permission-bits concept for _mkdir to convey
 * in the first place.
 */
#ifndef CRT_WIN32_SHIM_DIRECT_H
#define CRT_WIN32_SHIM_DIRECT_H

#include <sys/stat.h>

static inline int _mkdir(const char* path) { return mkdir(path, 0777); }

#endif
