#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include <sstream>
#include <string>
#include <utility>

// Cross the SDK/CRT translation-unit boundary in both directions, including
// short strings, heap strings, out-of-line insert, and destruction by caller.
std::string crt_string_sdk_prefix(std::string value) {
  return "floating-point value is too large: " + std::move(value);
}

void crt_string_sdk_insert(std::string& value) {
  value.insert(0, "floating-point value is too large: ");
}

bool crt_string_sdk_parse() {
  std::istringstream input(".0001");
  float value = 0;
  input >> value;
  return !input.fail() && value > 0.000099f && value < 0.000101f;
}
