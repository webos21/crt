#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" int main() {
  std::vector<std::string> values;
  values.push_back("CRT");
  values.push_back("libc++");

  try {
    if (values.size() != 2 || values[0] + " " + values[1] != "CRT libc++") {
      return 1;
    }
    throw std::runtime_error("caught");
  } catch (const std::runtime_error& error) {
    if (std::string(error.what()) != "caught") return 2;
  }

  std::printf("imported_libcxx_test: ok\n");
  return 0;
}
