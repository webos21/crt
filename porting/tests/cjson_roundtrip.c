#include <cJSON.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  const char* end = 0;
  cJSON* value = cJSON_ParseWithOpts("{\"text\":\"space \\uD55C\\uAE00\",\"number\":42,\"ok\":true}", &end, 1);
  cJSON* again;
  char* encoded;
  if (!value || !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(value, "ok")) ||
      cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(value, "number")) != 42) return 1;
  encoded = cJSON_PrintUnformatted(value);
  if (!encoded) return 2;
  again = cJSON_Parse(encoded);
  if (!again || strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(again, "text")),
                        cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(value, "text")))) return 3;
  cJSON_free(encoded);
  cJSON_Delete(again);
  cJSON_Delete(value);
  value = cJSON_ParseWithOpts("{\"broken\":]", &end, 1);
  if (value != 0) return 4;
  puts("cjson_roundtrip: ok");
  return 0;
}
