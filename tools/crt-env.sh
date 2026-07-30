#!/bin/sh
#
# Source this file before running upstream configure/make builds against CRT.
#
# Usage:
#   . tools/crt-env.sh <preset> [target-os]
#
# Examples:
#   . tools/crt-env.sh linux-host-ninja-debug
#   . tools/crt-env.sh macos-host-ninja-debug
#   . tools/crt-env.sh windows-host-ninja-debug windows

crt_env_usage() {
  cat <<'EOF'
Usage:
  . tools/crt-env.sh <preset> [target-os]

Linux:
  . tools/crt-env.sh linux-host-ninja-debug

macOS:
  . tools/crt-env.sh macos-host-ninja-debug

Windows from Git Bash/MSYS:
  . tools/crt-env.sh windows-host-ninja-debug windows

Windows Developer Command Prompt:
  call tools\crt-env.cmd windows-host-ninja-debug

PowerShell without changing execution policy:
  cmd /k tools\crt-env.cmd windows-host-ninja-debug
EOF
}

if [ $# -eq 0 ]; then
  crt_env_usage
  return 2 2>/dev/null || exit 2
fi

if [ "${BASH_SOURCE:-}" ]; then
  _crt_env_script="${BASH_SOURCE}"
elif [ -n "${ZSH_VERSION:-}" ]; then
  _crt_env_script="$0"
else
  _crt_env_script="$0"
fi

case "$_crt_env_script" in
  *'
'*) _crt_env_script=$(printf '%s\n' "$_crt_env_script" | tail -n 1) ;;
esac
case "$_crt_env_script" in
  "~/"*) _crt_env_script="$HOME/${_crt_env_script#~/}" ;;
esac

if [ -f "$_crt_env_script" ] || [ -d "$_crt_env_script" ]; then
  _crt_env_path="$_crt_env_script"
elif command -v -- "$_crt_env_script" >/dev/null 2>&1; then
  _crt_env_path=$(command -v -- "$_crt_env_script")
elif [ -f "./$_crt_env_script" ]; then
  _crt_env_path="./$_crt_env_script"
else
  _crt_env_path="$_crt_env_script"
fi

_crt_env_dir=$(CDPATH= cd -- "$(dirname -- "$_crt_env_path")" 2>/dev/null && pwd)
if [ -z "$_crt_env_dir" ]; then
  _crt_env_dir=$(pwd)
fi

_crt_env_root=""
_crt_env_search="$_crt_env_dir"
while [ "$_crt_env_search" != "/" ]; do
  if [ -f "$_crt_env_search/CMakeLists.txt" ] && [ -f "$_crt_env_search/tools/crt-env.sh" ]; then
    _crt_env_root="$_crt_env_search"
    break
  fi
  _crt_env_search=$(dirname -- "$_crt_env_search")
done

if [ -z "$_crt_env_root" ]; then
  echo "crt-env.sh: could not locate CRT repository root from $_crt_env_script" >&2
  return 2 2>/dev/null || exit 2
fi

_crt_env_preset="${1:-}"
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
export CPPFLAGS="-I$PORT_PREFIX/include${CRT_EXTRA_CPPFLAGS:+ $CRT_EXTRA_CPPFLAGS}"
export LDFLAGS="-L$PORT_PREFIX/lib${CRT_EXTRA_LDFLAGS:+ $CRT_EXTRA_LDFLAGS}"
export PKG_CONFIG_LIBDIR="$PORT_PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
export PATH="$_crt_env_root/tools:$PATH"

mkdir -p "$_crt_env_root/out/$_crt_env_preset/port-tests/src" \
  "$PORT_PREFIX/include" \
  "$PORT_PREFIX/lib/pkgconfig"

echo "CRT_SYSROOT=$CRT_SYSROOT"
echo "CRT_TARGET_OS=$CRT_TARGET_OS"
echo "PORT_PREFIX=$PORT_PREFIX"
