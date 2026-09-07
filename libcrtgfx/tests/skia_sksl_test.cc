#include "include/core/SkString.h"
#include "include/effects/SkRuntimeEffect.h"

#include <stdio.h>
#include <string.h>

extern "C" int main() {
  for (int iteration = 0; iteration < 20; ++iteration) {
    auto valid = SkRuntimeEffect::MakeForShader(SkString(
        "half4 main(float2 p) { return half4(0.25, 0.5, 0.75, 1); }"));
    if (!valid.effect) {
      printf("crtgfx_skia_sksl_test: valid shader: %s\n", valid.errorText.c_str());
      return 1;
    }
    auto too_large = SkRuntimeEffect::MakeForShader(SkString(
        "half4 main(float2 p) { float x = "
        "999999999999999999999999999999999999999999999999.0; "
        "return half4(x); }"));
    if (too_large.effect || !strstr(too_large.errorText.c_str(), "too large")) {
      printf("crtgfx_skia_sksl_test: overflow diagnostic: %s\n", too_large.errorText.c_str());
      return 2;
    }
    auto unknown = SkRuntimeEffect::MakeForShader(SkString(
        "half4 main(float2 p) { return nonexistent_identifier_for_diagnostic; }"));
    if (unknown.effect || !strstr(unknown.errorText.c_str(), "unknown identifier")) {
      printf("crtgfx_skia_sksl_test: identifier diagnostic: %s\n", unknown.errorText.c_str());
      return 3;
    }
  }
  puts("crtgfx_skia_sksl_test: ok");
  return 0;
}
