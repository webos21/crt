# Passed to WebKit's CMake as CMAKE_PROJECT_INCLUDE by tools/build_webkit_jsc.py on macOS. It runs right
# after project() and presents the CRT target to WebKit's platform logic as a Bionic/Linux-shaped POSIX
# platform instead of Darwin: WebKit's CMake only knows APPLE, Linux, Windows and Fuchsia, and its APPLE
# branches select Apple SDK code (Mach exceptions, Cocoa, libdispatch, host ICU) that would make the result
# run on libSystem rather than on the CRT runtime. The compiler side is the CRT wrapper's -U__APPLE__ plus
# -D__linux__=1 from the harness. Nothing in the WebKit tree is changed for this.
set(APPLE OFF)
set(CMAKE_SYSTEM_NAME Linux)

# Windows: the same presentation. The pinned WPE release tarball does not even contain the
# WIN32 sources (WTF's CMake names Source/WTF/wtf/text/win/StringWin.cpp, which is not in the
# archive), and WebKit's WIN32 branches assume windows.h and the Microsoft C runtime, which the CRT
# sysroot does not have by design. The compiler side is -U for every Windows macro clang predefines
# for *-w64-mingw32 and -D__linux__=1, from the harness.
if(WIN32)
  set(WIN32 OFF)
  set(MINGW OFF)
  set(MSVC OFF)
  set(UNIX ON)
endif()
