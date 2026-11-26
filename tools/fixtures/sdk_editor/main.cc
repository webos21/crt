#include <filesystem>
#include <type_traits>

#include "editor_probe.h"

#if defined(CRT_TARGET_OS_WINDOWS)
// The packaged Windows libc++ keeps the native wide path persona (CRT builds wchar_t as a 4-byte int).
static_assert(std::is_same_v<std::filesystem::path::value_type, wchar_t>);
#define EDITOR_PROBE_PATH L"editor"
#else
static_assert(std::is_same_v<std::filesystem::path::value_type, char>);
#define EDITOR_PROBE_PATH "editor"
#endif

int main() {
  const std::filesystem::path path("editor");
  return path.native() == EDITOR_PROBE_PATH && editor_probe() == EDITOR_PROBE_VALUE ? 0 : 1;
}
