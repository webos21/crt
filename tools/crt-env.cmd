@echo off

if "%~1"=="" (
  echo Usage:
  echo   call tools\crt-env.cmd ^<preset^> [target-os]
  echo.
  echo Windows Developer Command Prompt:
  echo   call tools\crt-env.cmd windows-host-ninja-debug
  echo.
  echo PowerShell without changing execution policy:
  echo   cmd /k tools\crt-env.cmd windows-host-ninja-debug
  echo.
  echo Git Bash/MSYS:
  echo   . tools/crt-env.sh windows-host-ninja-debug windows
  exit /b 2
)

set "CRT_ENV_PRESET=%~1"
set "CRT_ENV_TARGET_OS=%~2"

if "%CRT_ENV_TARGET_OS%"=="" (
  echo %CRT_ENV_PRESET% | findstr /b /c:"linux-" >nul
  if not errorlevel 1 set "CRT_ENV_TARGET_OS=linux"
)
if "%CRT_ENV_TARGET_OS%"=="" (
  echo %CRT_ENV_PRESET% | findstr /b /c:"macos-" >nul
  if not errorlevel 1 set "CRT_ENV_TARGET_OS=macos"
)
if "%CRT_ENV_TARGET_OS%"=="" (
  echo %CRT_ENV_PRESET% | findstr /b /c:"windows-" >nul
  if not errorlevel 1 set "CRT_ENV_TARGET_OS=windows"
)
if "%CRT_ENV_TARGET_OS%"=="" (
  echo crt-env.cmd: pass target-os for preset %CRT_ENV_PRESET%
  exit /b 2
)

set "CRT_ENV_TOOLS_DIR=%~dp0"
for %%I in ("%CRT_ENV_TOOLS_DIR%..") do set "CRT_ENV_ROOT=%%~fI"

set "CRT_SYSROOT=%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\sysroot"
set "CRT_TARGET_OS=%CRT_ENV_TARGET_OS%"
set "CC=%CRT_ENV_ROOT%\tools\crt-cc"
set "CXX=%CRT_ENV_ROOT%\tools\crt-c++"
if not defined AR set "AR=llvm-ar"
if not defined RANLIB set "RANLIB=llvm-ranlib"
if not defined STRIP set "STRIP=llvm-strip"
set "PORT_PREFIX=%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\port-tests\install"
if defined CPPFLAGS (
  set "CPPFLAGS=-I%PORT_PREFIX%\include %CPPFLAGS%"
) else (
  set "CPPFLAGS=-I%PORT_PREFIX%\include"
)
if defined LDFLAGS (
  set "LDFLAGS=-L%PORT_PREFIX%\lib %LDFLAGS%"
) else (
  set "LDFLAGS=-L%PORT_PREFIX%\lib"
)
set "PKG_CONFIG_LIBDIR=%PORT_PREFIX%\lib\pkgconfig"
set "PKG_CONFIG_PATH=%PKG_CONFIG_LIBDIR%"
set "PATH=%CRT_ENV_ROOT%\tools;%PATH%"

if not exist "%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\port-tests\src" mkdir "%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\port-tests\src"
if not exist "%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\port-tests\install" mkdir "%CRT_ENV_ROOT%\out\%CRT_ENV_PRESET%\port-tests\install"

echo CRT_SYSROOT=%CRT_SYSROOT%
echo CRT_TARGET_OS=%CRT_TARGET_OS%
echo PORT_PREFIX=%PORT_PREFIX%

set "CRT_ENV_PRESET="
set "CRT_ENV_TARGET_OS="
set "CRT_ENV_TOOLS_DIR="
set "CRT_ENV_ROOT="
