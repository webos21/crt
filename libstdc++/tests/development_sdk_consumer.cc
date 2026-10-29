#include <curl/curl.h>
#include <zlib.h>
#include <cstring>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#define CJSON_HIDE_SYMBOLS
#include <cJSON.h>
#include <mbedtls/sha256.h>
#include <crt/platform.h>
#include <crt/dl.h>

static int image_seen(const crt_dl_image* image, size_t size, void* data) {
  if (size < sizeof(*image) || !image->path || !image->path[0]) return 1;
  ++*static_cast<int*>(data);
  return 0;
}

static bool utilities(const char* program) {
  nlohmann::json value = {{"integer", 18446744073709551615ULL}, {"text", "space \uD55C\uAE00"}};
  if (nlohmann::json::parse(value.dump()) != value) return false;
  cJSON* parsed = cJSON_Parse("{\"value\":42}");
  if (!parsed) return false;
  bool json_ok = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(parsed, "value")) == 42;
  cJSON_Delete(parsed);
  unsigned char digest[32];
  const unsigned char expected[] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                                    0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
  if (mbedtls_sha256(reinterpret_cast<const unsigned char*>("abc"), 3, digest, 0) || std::memcmp(digest, expected, 32)) return false;
  char home[65536]; int images = 0;
  return json_ok && crt_get_platform_info()->arch[0] && crt_is_native_executable(program) == 1 &&
         crt_get_user_home(home, sizeof(home)) == 0 && crt_dl_iterate_images(image_seen, &images) == 0 && images > 0;
}

static size_t receive(char* data, size_t size, size_t count, void* context) {
  static_cast<std::string*>(context)->append(data, size * count);
  return size * count;
}

extern "C" int main(int argc, char** argv) {
  if (argc != 4 || std::strncmp(argv[1], "https://", 8) != 0) return 2;
  if (!utilities(argv[0])) return 9;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 3;
  CURL* request = curl_easy_init();
  if (!request) return 4;
  std::string body;
  curl_easy_setopt(request, CURLOPT_URL, argv[1]);
  curl_easy_setopt(request, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(request, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(request, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(request, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, receive);
  curl_easy_setopt(request, CURLOPT_WRITEDATA, &body);
  if (std::strcmp(argv[2], "-") != 0) curl_easy_setopt(request, CURLOPT_CAINFO, argv[2]);
  CURLcode result = curl_easy_perform(request);
  curl_easy_cleanup(request);
  curl_global_cleanup();
  if (std::strcmp(argv[3], "reject") == 0) {
    if (result == CURLE_OK) return 5;
    std::cout << "development-sdk: rejected\n";
    return 0;
  }
  if (result != CURLE_OK) return 6;
  unsigned char plain[128];
  uLongf length = sizeof plain;
  if (uncompress(plain, &length, reinterpret_cast<const Bytef*>(body.data()), body.size()) != Z_OK) return 7;
  if (std::string(reinterpret_cast<char*>(plain), length) != "crt-development-sdk") return 8;
  std::cout << "development-sdk: HTTPS+inflate ok\n";
  return 0;
}
