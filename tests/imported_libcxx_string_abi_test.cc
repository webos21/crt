#include <cstdio>
#include <string>

std::string crt_string_sdk_prefix(std::string value);
void crt_string_sdk_insert(std::string& value);
bool crt_string_sdk_parse();

extern "C" int main() {
  const size_t lengths[] = {0, 1, 15, 22, 23, 31, 64, 256};
  for (int pass = 0; pass < 100; ++pass) {
    for (size_t length : lengths) {
      std::string input(length, 'x');
      std::string expected = "floating-point value is too large: ";
      expected.append(input);
      std::string result = crt_string_sdk_prefix(input);
      if (result != expected) return 1;
      crt_string_sdk_insert(input);
      if (input != expected) return 2;
      result.insert(0, "error: ");
      expected.insert(0, "error: ");
      if (result != expected) return 3;
    }
    if (!crt_string_sdk_parse()) return 4;
  }
  std::puts("imported_libcxx_string_abi_test: ok");
  return 0;
}
