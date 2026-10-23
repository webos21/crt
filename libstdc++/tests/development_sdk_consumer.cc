#include <curl/curl.h>
#include <zlib.h>
#include <cstring>
#include <iostream>
#include <string>

static size_t receive(char* data, size_t size, size_t count, void* context) {
  static_cast<std::string*>(context)->append(data, size * count);
  return size * count;
}

extern "C" int main(int argc, char** argv) {
  if (argc != 4 || std::strncmp(argv[1], "https://", 8) != 0) return 2;
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
