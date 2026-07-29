#!/bin/sh
#
# Source this file before running upstream configure/make builds against CRT.
#
# Usage:
#   . tools/crt-env.sh [preset] [target-os]
#
# Examples:
#   . tools/crt-env.sh linux-host-ninja-debug
#   . tools/crt-env.sh macos-host-ninja-debug
#   . tools/crt-env.sh windows-host-ninja-debug windows

if [ "${BASH_SOURCE:-}" ]; then
  _crt_env_script="${BASH_SOURCE}"
elif [ -n "${ZSH_VERSION:-}" ]; then
  eval '_crt_env_script="${(%):-%x}"'
else
  _crt_env_script="$0"
fi

_crt_env_dir=$(CDPATH= cd -- "$(dirname -- "$_crt_env_script")" && pwd)
_crt_env_root=$(CDPATH= cd -- "$_crt_env_dir/.." && pwd)

_crt_env_preset="${1:-}"
if [ -z "$_crt_env_preset" ]; then
  case "$(uname -s)" in
    Darwin) _crt_env_preset="macos-host-ninja-debug" ;;
    Linux) _crt_env_preset="linux-host-ninja-debug" ;;
    MINGW*|MSYS*|CYGWIN*) _crt_env_preset="windows-host-ninja-debug" ;;
    *)
      echo "crt-env.sh: pass a preset name for this host" >&2
      return 2 2>/dev/null || exit 2
      ;;
  esac
fi

_crt_env_target_os="${2:-}"
if [ -z "$_crt_env_target_os" ]; then
  case "$_crt_env_preset" in
    linux-*) _crt_env_target_os="linux" ;;
    macos-*) _crt_env_target_os="macos" ;;
    windows-*) _crt_env_target_os="windows" ;;
    *)
      echo "crt-env.sh: pass target-os for preset $_crt_env_preset" >&2
      return 2 2>/dev/null || exit 2
      ;;
  esac
fi

export CRT_SYSROOT="$_crt_env_root/out/$_crt_env_preset/sysroot"
export CRT_TARGET_OS="$_crt_env_target_os"
export CC="$_crt_env_root/tools/crt-cc"
export CXX="$_crt_env_root/tools/crt-c++"
export AR="${AR:-ar}"
export RANLIB="${RANLIB:-ranlib}"
export STRIP="${STRIP:-strip}"
export PORT_PREFIX="$_crt_env_root/out/$_crt_env_preset/port-tests/install"
export CPPFLAGS="-I$PORT_PREFIX/include${CPPFLAGS:+ $CPPFLAGS}"
export LDFLAGS="-L$PORT_PREFIX/lib${LDFLAGS:+ $LDFLAGS}"
export PKG_CONFIG_LIBDIR="$PORT_PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
export PATH="$_crt_env_root/tools:$PATH"

mkdir -p "$_crt_env_root/out/$_crt_env_preset/port-tests/src" "$PORT_PREFIX"

echo "CRT_SYSROOT=$CRT_SYSROOT"
echo "CRT_TARGET_OS=$CRT_TARGET_OS"
echo "PORT_PREFIX=$PORT_PREFIX"
