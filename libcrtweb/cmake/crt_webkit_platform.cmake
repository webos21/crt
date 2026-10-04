# Passed to WebKit's CMake as CMAKE_PROJECT_INCLUDE by tools/build_webkit_jsc.py on macOS. It runs right
# after project() and presents the CRT target to WebKit's platform logic as a Bionic/Linux-shaped POSIX
# platform instead of Darwin: WebKit's CMake only knows APPLE, Linux, Windows and Fuchsia, and its APPLE
# branches select Apple SDK code (Mach exceptions, Cocoa, libdispatch, host ICU) that would make the result
# run on libSystem rather than on the CRT runtime. The compiler side is the CRT wrapper's -U__APPLE__ plus
# -D__linux__=1 from the harness. Nothing in the WebKit tree is changed for this.
set(APPLE OFF)
set(CMAKE_SYSTEM_NAME Linux)
