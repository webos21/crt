#include <filesystem>
#include <type_traits>

#include "editor_probe.h"

static_assert(std::is_same_v<std::filesystem::path::value_type, char>);

int main() {
  const std::filesystem::path path("editor");
  return path.native() == "editor" && editor_probe() == EDITOR_PROBE_VALUE ? 0 : 1;
}
