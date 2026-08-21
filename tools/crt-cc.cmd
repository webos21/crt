@echo off
rem Native-Windows-executable launcher for tools/crt-cc (a #!/bin/sh
rem script CreateProcess cannot run directly). Exists specifically so
rem CMake can be pointed at a single, plain, directly-executable
rem CMAKE_C_COMPILER path -- unlike the mksh.exe + CMAKE_C_COMPILER_ARG1
rem trick tools/crt-libcxx-build.py otherwise uses (which works for the
rem initial compiler *identification* probe but not reliably for every
rem later CMake-driven TryCompile, confirmed for real: CMAKE_CXX_COMPILER_
rem ARG1 silently failed to reach the "Detecting CXX compiler ABI info"
rem TryCompile even though the exact same mechanism worked for C).
rem CRT_MKSH_EXE must be set by the caller to the CRT rootfs's mksh.exe.
if "%CRT_MKSH_EXE%"=="" (
  echo crt-cc.cmd: CRT_MKSH_EXE is not set 1>&2
  exit /b 1
)
"%CRT_MKSH_EXE%" "%~dp0crt-cc" %*
exit /b %ERRORLEVEL%
