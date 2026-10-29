#include <nlohmann/json.hpp>
#include <cstdio>
#include <stdexcept>

int main() {
  using nlohmann::json;
  json value = {{"integer", 18446744073709551615ULL}, {"utf8", "space \uD55C\uAE00"}, {"array", {true, nullptr, -1}}};
  if (json::parse(value.dump()) != value) return 1;
  try { (void)json::parse("{broken]"); return 2; }
  catch (const json::parse_error&) {}
  std::puts("json_roundtrip: ok");
}
