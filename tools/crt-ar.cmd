@echo off
setlocal
if "%CRT_HOST_PYTHON%"=="" (
  py -3 "%~dp0crt-ar" %*
) else (
  "%CRT_HOST_PYTHON%" "%~dp0crt-ar" %*
)
exit /b %ERRORLEVEL%
