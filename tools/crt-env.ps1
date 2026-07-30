param(
  [string]$Preset = "",
  [string]$TargetOS = ""
)

# Dot-source this file before running CRT porting commands from PowerShell.
#
# Usage:
#   . .\tools\crt-env.ps1 -Preset windows-host-ninja-debug
#
# For Autoconf configure scripts on Windows, Git Bash or MSYS2 is usually still
# needed. This file provides the same CRT environment for PowerShell-driven
# tools and for processes launched from PowerShell.

$ScriptPath = if ($PSCommandPath) { $PSCommandPath } else { $MyInvocation.MyCommand.Path }
$ToolsDir = Split-Path -Parent $ScriptPath
$RepoRoot = Split-Path -Parent $ToolsDir

if ([string]::IsNullOrWhiteSpace($Preset)) {
  Write-Host @"
Usage:
  . .\tools\crt-env.ps1 -Preset <preset> [-TargetOS windows]

Windows PowerShell:
  . .\tools\crt-env.ps1 -Preset windows-host-ninja-debug

PowerShell without changing execution policy:
  cmd /k tools\crt-env.cmd windows-host-ninja-debug

Windows Developer Command Prompt:
  call tools\crt-env.cmd windows-host-ninja-debug

Git Bash/MSYS:
  . tools/crt-env.sh windows-host-ninja-debug windows

Linux:
  . tools/crt-env.sh linux-host-ninja-debug

macOS:
  . tools/crt-env.sh macos-host-ninja-debug
"@
  return
}

if ([string]::IsNullOrWhiteSpace($TargetOS)) {
  if ($Preset.StartsWith("linux-")) {
    $TargetOS = "linux"
  } elseif ($Preset.StartsWith("macos-")) {
    $TargetOS = "macos"
  } elseif ($Preset.StartsWith("windows-")) {
    $TargetOS = "windows"
  } else {
    throw "Pass -TargetOS for preset '$Preset'."
  }
}

$env:CRT_SYSROOT = Join-Path $RepoRoot "out\$Preset\sysroot"
$env:CRT_TARGET_OS = $TargetOS
$env:CC = Join-Path $RepoRoot "tools\crt-cc"
$env:CXX = Join-Path $RepoRoot "tools\crt-c++"
if (-not $env:AR) { $env:AR = "llvm-ar" }
if (-not $env:RANLIB) { $env:RANLIB = "llvm-ranlib" }
if (-not $env:STRIP) { $env:STRIP = "llvm-strip" }
$env:PORT_PREFIX = Join-Path $RepoRoot "out\$Preset\port-tests\install"
$env:CPPFLAGS = "-I$($env:PORT_PREFIX)\include" + $(if ($env:CRT_EXTRA_CPPFLAGS) { " $($env:CRT_EXTRA_CPPFLAGS)" } else { "" })
$env:LDFLAGS = "-L$($env:PORT_PREFIX)\lib" + $(if ($env:CRT_EXTRA_LDFLAGS) { " $($env:CRT_EXTRA_LDFLAGS)" } else { "" })
$env:PKG_CONFIG_LIBDIR = Join-Path $env:PORT_PREFIX "lib\pkgconfig"
$env:PKG_CONFIG_PATH = $env:PKG_CONFIG_LIBDIR
$env:PATH = "$ToolsDir;$($env:PATH)"

New-Item -ItemType Directory -Force -Path (Join-Path $RepoRoot "out\$Preset\port-tests\src") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $env:PORT_PREFIX "include") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $env:PORT_PREFIX "lib\pkgconfig") | Out-Null

Write-Host "CRT_SYSROOT=$($env:CRT_SYSROOT)"
Write-Host "CRT_TARGET_OS=$($env:CRT_TARGET_OS)"
Write-Host "PORT_PREFIX=$($env:PORT_PREFIX)"
