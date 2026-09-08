@echo off
rem See tools/crt-cc.cmd's own comment -- same reasoning, for tools/crt-c++.
if "%CRT_MKSH_EXE%"=="" (
  echo crt-c++.cmd: CRT_MKSH_EXE is not set 1>&2
  exit /b 1
)
setlocal
set "PATH=/system/bin:/bin:/usr/bin"
"%CRT_MKSH_EXE%" "%~dp0crt-c++" %*
set "CRT_WRAPPER_STATUS=%ERRORLEVEL%"
endlocal & exit /b %CRT_WRAPPER_STATUS%
