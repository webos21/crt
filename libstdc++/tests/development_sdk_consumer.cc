#include <curl/curl.h>
#include <zlib.h>
#include <lzma.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <nlohmann/json.hpp>
#define CJSON_HIDE_SYMBOLS
#include <cJSON.h>
#include <mbedtls/platform.h>
#include <mbedtls/sha256.h>
#include <crt/platform.h>
#include <crt/dl.h>

static int image_seen(const crt_dl_image* image, size_t size, void* data) {
  if (size < sizeof(*image) || !image->path || !image->path[0]) return 1;
  ++*static_cast<int*>(data);
  return 0;
}

// xz (liblzma): the archive format of the Linux/macOS SDKs. A fixed 84-byte CRC64 stream of "crt-xz-fixture:ok\n";
// a flipped payload byte must be rejected by the integrity check, not decoded.
static bool xz_decodes(const unsigned char* data, size_t size, std::string& out) {
  lzma_stream stream = LZMA_STREAM_INIT;
  if (lzma_stream_decoder(&stream, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) return false;
  unsigned char buffer[256];
  stream.next_in = data; stream.avail_in = size;
  lzma_ret status = LZMA_OK;
  while (status == LZMA_OK) {
    stream.next_out = buffer; stream.avail_out = sizeof(buffer);
    status = lzma_code(&stream, LZMA_FINISH);
    out.append(reinterpret_cast<char*>(buffer), sizeof(buffer) - stream.avail_out);
  }
  lzma_end(&stream);
  return status == LZMA_STREAM_END;
}

static bool xz_utility() {
  static const unsigned char fixture[] = {
    0xfd,0x37,0x7a,0x58,0x5a,0x00,0x00,0x04,0xe6,0xd6,0xb4,0x46,0x04,0xc0,0x16,0x12,0x21,0x01,0x1c,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x6e,0xc1,0x55,0x89,0x01,0x00,0x11,0x63,0x72,0x74,0x2d,0x78,0x7a,0x2d,0x66,0x69,0x78,0x74,0x75,0x72,
    0x65,0x3a,0x6f,0x6b,0x0a,0x00,0x00,0x00,0x70,0x77,0xf8,0x60,0xc1,0x7f,0x33,0x91,0x00,0x01,0x32,0x12,0x12,0x90,0x4f,0x3e,
    0x1f,0xb6,0xf3,0x7d,0x01,0x00,0x00,0x00,0x00,0x04,0x59,0x5a};
  std::string plain;
  if (!xz_decodes(fixture, sizeof(fixture), plain) || plain != "crt-xz-fixture:ok\n") return false;
  unsigned char damaged[sizeof(fixture)];
  std::memcpy(damaged, fixture, sizeof(fixture));
  damaged[36] ^= 0x01;  // inside the compressed payload
  std::string rejected;
  return !xz_decodes(damaged, sizeof(damaged), rejected) && lzma_version_number() > 0;
}

// The consumer passes no platform-persona compile option. <curl/curl.h> and <mbedtls/platform.h> are included first on purpose: libc++'s
// std::filesystem must still agree with the packaged libc++ (a header parsed in the wrong persona gave wrong paths without a link
// error), the libcurl socket type must be the POSIX int the library was built with, and mbedtls_snprintf must link.
static bool platform_persona() {
  namespace fs = std::filesystem;
  fs::path file("dir/sub/file.txt");
  char text[32];
  mbedtls_snprintf(text, sizeof(text), "%d-%s", 7, "ok");
  return file.filename().string() == "file.txt" && file.parent_path().generic_string() == "dir/sub" &&
         (fs::path("a") / "b" / "c.txt").generic_string() == "a/b/c.txt" && file.extension().string() == ".txt" &&
         sizeof(curl_socket_t) == sizeof(int) && std::strcmp(text, "7-ok") == 0;
}

// std::fstream family opened with a std::filesystem::path. On Windows path::value_type is the packaged libc++'s wchar_t (4 bytes
// in this CRT) and the wide open has to reach the CRT's UTF-8 narrow file-name boundary; on Linux/macOS it is char. The same checks run
// on every OS: read, write and append through the path constructors and open(); a space, a Hangul and a supplementary-plane name;
// failure states and the preservation of an existing file. File names with a supplementary-plane character are created and removed
// through stdio, not std::filesystem: libc++'s own Windows directory operations are a separate item (TODO PAL-06).
static bool fstream_path_checks(std::string& why) {
  namespace fs = std::filesystem;
  auto fail = [&](const char* what) { why = what; return false; };
  auto slurp = [](const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
  };
  auto slurp_stdio = [](const std::string& u8, std::string& out) {
    FILE* file = std::fopen(u8.c_str(), "rb");
    if (!file) return false;
    char buffer[64];
    size_t count = std::fread(buffer, 1, sizeof buffer, file);
    std::fclose(file);
    out.assign(buffer, count);
    return true;
  };
  // "crt fstream 한글" (a space and Hangul syllables, UTF-8 spelled out, not source encoding)
  const fs::path dir = fs::u8path("crt fstream \xED\x95\x9C\xEA\xB8\x80");
  std::error_code ec;
  fs::remove_all(dir, ec);
  if (!fs::create_directory(dir)) return fail("create_directory with a space and a Hangul name");
  const fs::path names[] = {dir / "plain.txt", dir / "with space.txt", dir / fs::u8path("\xED\x95\x9C\xEA\xB8\x80.txt"),
                            dir / fs::u8path("a \xED\x95\x9C b.txt")};
  for (const fs::path& file : names) {
    std::string got;
    { std::ofstream out(file, std::ios::binary); if (!out) return fail("ofstream(path) write"); out << "one"; }
    if (!slurp(file, got) || got != "one") return fail("ifstream(path) read back after ofstream(path)");
    { std::ofstream out(file, std::ios::binary | std::ios::app); if (!out) return fail("ofstream(path, app)"); out << "+two"; }
    if (!slurp(file, got) || got != "one+two") return fail("append through ofstream(path, app)");
    { std::fstream io(file, std::ios::in | std::ios::out | std::ios::binary); if (!io) return fail("fstream(path, in|out)");
      io.seekp(0, std::ios::end); io << "+3"; }
    if (!slurp(file, got) || got != "one+two+3") return fail("fstream(path, in|out) must keep the existing content");
    std::ofstream late; late.open(file, std::ios::binary | std::ios::trunc);
    if (!late.is_open()) return fail("ofstream::open(path, trunc)");
    late << "new"; late.close();
    std::ifstream again; again.open(file, std::ios::binary);
    if (!again.is_open()) return fail("ifstream::open(path)");
    again.close();
    if (!slurp(file, got) || got != "new") return fail("open(path, trunc) then read");
    std::filebuf raw;
    if (!raw.open(file, std::ios::in | std::ios::binary)) return fail("filebuf::open(path)");
    if (raw.sgetc() != 'n') return fail("filebuf read");
    raw.close();
    if (fs::file_size(file) != 3) return fail("file_size through the same path");
  }
  int seen = 0;
  for (const fs::directory_entry& entry : fs::directory_iterator(dir)) { (void)entry; ++seen; }
  if (seen != 4) return fail("directory_iterator sees every file");

  // a supplementary-plane name (U+1F600) and a mixed one, opened through a path built from UTF-8: the stream open must reach the same
  // file stdio reaches with the UTF-8 name
  const std::string emoji = "crt-fstream-\xF0\x9F\x98\x80.txt", mixed = "crt a \xED\x95\x9C\xF0\x9F\x98\x80 b.txt";
  for (const std::string& u8 : {emoji, mixed}) {
    std::remove(u8.c_str());
    std::string got;
    { std::ofstream out(fs::u8path(u8), std::ios::binary); if (!out) return fail("ofstream(path) with a supplementary-plane name"); out << "wide"; }
    if (!slurp_stdio(u8, got) || got != "wide") return fail("stdio must read what ofstream(path) wrote under a supplementary-plane name");
    { std::ofstream out(fs::u8path(u8), std::ios::binary | std::ios::app); out << "+app"; }
    if (!slurp(fs::u8path(u8), got) || got != "wide+app") return fail("ifstream(path) with a supplementary-plane name");
    if (std::remove(u8.c_str()) != 0) return fail("remove the supplementary-plane file through stdio");
    std::ifstream gone(fs::u8path(u8));
    if (gone.is_open()) return fail("a removed supplementary-plane file must not open");
  }

  // failure states, and an existing file stays as it was
  const fs::path keep = dir / "keep.txt";
  { std::ofstream out(keep, std::ios::binary); out << "keep"; }
  std::ifstream missing(dir / "no such file.txt");
  if (missing.is_open() || !missing.fail()) return fail("ifstream on a missing path must fail");
  std::ofstream nodir(dir / "no such dir" / "x.txt");
  if (nodir.is_open() || !nodir.fail()) return fail("ofstream into a missing directory must fail");
  std::ofstream onto_dir(dir);
  if (onto_dir.is_open() || !onto_dir.fail()) return fail("ofstream onto a directory must fail");
  std::ofstream bad_mode(keep, std::ios::trunc | std::ios::app);   // out|trunc|app has no fopen mode: refused, and the file is not truncated
  if (bad_mode.is_open()) return fail("an invalid open mode must be refused");
  std::string got;
  if (!slurp(keep, got) || got != "keep") return fail("a failed open must preserve the existing file");
  std::ofstream already(keep, std::ios::binary | std::ios::app);
  already.open(keep, std::ios::binary);
  if (!already.fail()) return fail("open() on an open stream must set failbit");
  already.clear(); already.close();
  if (!slurp(keep, got) || got != "keep") return fail("the refused second open must preserve the file");
  // wide paths only exist where path::value_type is wide: a lone surrogate is not a valid file name and must not create one
  if (sizeof(fs::path::value_type) > 1) {
    std::wstring bad = L"lone-"; bad.push_back(static_cast<wchar_t>(0xD800)); bad += L".txt";
    std::ofstream lone(dir / fs::path(bad));
    if (lone.is_open()) return fail("a lone surrogate in a wide path must be refused");
  }
  fs::remove_all(dir, ec);
  if (fs::exists(dir)) return fail("cleanup");
  return true;
}

static bool utilities(const char* program) {
  if (!platform_persona()) return false;
  std::string fstream_why;
  if (!fstream_path_checks(fstream_why)) {
    std::cerr << "development-sdk: fstream path: " << fstream_why << '\n';
    return false;
  }
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
  return json_ok && xz_utility() && crt_get_platform_info()->arch[0] && crt_is_native_executable(program) == 1 &&
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
