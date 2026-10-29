#include <crt/platform.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(CRT_TARGET_OS_LINUX)
#define CRT_PLATFORM_OS "linux"
#define CRT_IMAGE_FORMAT "elf"
#define CRT_EXE_SUFFIX ""
#define CRT_SHARED_PREFIX "lib"
#define CRT_SHARED_SUFFIX ".so"
#elif defined(CRT_TARGET_OS_MACOS)
#define CRT_PLATFORM_OS "macos"
#define CRT_IMAGE_FORMAT "macho"
#define CRT_EXE_SUFFIX ""
#define CRT_SHARED_PREFIX "lib"
#define CRT_SHARED_SUFFIX ".dylib"
#elif defined(CRT_TARGET_OS_WINDOWS)
#define CRT_PLATFORM_OS "windows"
#define CRT_IMAGE_FORMAT "pe"
#define CRT_EXE_SUFFIX ".exe"
#define CRT_SHARED_PREFIX ""
#define CRT_SHARED_SUFFIX ".dll"
#else
#error Unsupported CRT platform
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define CRT_PLATFORM_ARCH "aarch64"
#define CRT_ELF_MACHINE 183
#define CRT_MACH_CPU 0x0100000cU
#define CRT_PE_MACHINE 0xaa64
#elif defined(__x86_64__) || defined(_M_X64)
#define CRT_PLATFORM_ARCH "x86_64"
#define CRT_ELF_MACHINE 62
#define CRT_MACH_CPU 0x01000007U
#define CRT_PE_MACHINE 0x8664
#else
#error Unsupported CRT architecture
#endif

const struct crt_platform_info* crt_get_platform_info(void) {
  static const struct crt_platform_info info = {
      CRT_PLATFORM_OS, CRT_PLATFORM_ARCH, CRT_IMAGE_FORMAT, CRT_EXE_SUFFIX,
      CRT_SHARED_PREFIX, CRT_SHARED_SUFFIX};
  return &info;
}

int crt_get_user_home(char* buffer, size_t capacity) {
  const char* home;
  size_t length;
  if (buffer == 0 || capacity == 0) return __set_errno(EINVAL);
#if defined(CRT_TARGET_OS_WINDOWS)
  home = getenv("USERPROFILE");
#else
  home = getenv("HOME");
#endif
  if (home == 0 || home[0] == 0) return __set_errno(ENOENT);
#if defined(CRT_TARGET_OS_WINDOWS)
  if (!((home[0] == '/' || home[0] == '\\') ||
        (((home[0] >= 'A' && home[0] <= 'Z') || (home[0] >= 'a' && home[0] <= 'z')) &&
         home[1] == ':' && (home[2] == '/' || home[2] == '\\')))) return __set_errno(EINVAL);
#else
  if (home[0] != '/') return __set_errno(EINVAL);
#endif
  length = strlen(home);
  if (length >= capacity) return __set_errno(ERANGE);
  memcpy(buffer, home, length + 1);
  return 0;
}

static uint32_t le32(const unsigned char* p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

#if defined(CRT_TARGET_OS_MACOS)
static uint32_t be32(const unsigned char* p) {
  return (uint32_t)p[3] | (uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24;
}
static int macho_header(const unsigned char* p) {
  return le32(p) == 0xfeedfacfU && le32(p + 4) == CRT_MACH_CPU && le32(p + 12) == 2;
}
#endif

int crt_is_native_executable(const char* path) {
  FILE* input;
  unsigned char header[64];
  size_t length;
  int result = 0;
  if (path == 0 || path[0] == 0) return __set_errno(EINVAL);
  input = fopen(path, "rb");
  if (input == 0) return -1;
  length = fread(header, 1, sizeof(header), input);
  if (ferror(input)) { fclose(input); return -1; }
#if defined(CRT_TARGET_OS_LINUX)
  if (length == sizeof(header) && memcmp(header, "\177ELF", 4) == 0 &&
      header[4] == 2 && header[5] == 1 && header[6] == 1 &&
      (header[16] == 2 || header[16] == 3) && header[17] == 0 &&
      (le32(header + 16) >> 16) == CRT_ELF_MACHINE) result = 1;
#elif defined(CRT_TARGET_OS_MACOS)
  if (length >= 32 && macho_header(header)) result = 1;
  else if (length >= 8 && (be32(header) == 0xcafebabeU || be32(header) == 0xcafebabfU)) {
    unsigned int i, count = be32(header + 4);
    size_t stride = be32(header) == 0xcafebabfU ? 32 : 20;
    /* Bound malformed universal headers; the loader still validates the image. */
    for (i = 0; i < count && i < 128; ++i) {
      unsigned char entry[32];
      uint32_t offset;
      if (fseek(input, (long)(8 + i * stride), SEEK_SET) != 0 ||
          fread(entry, 1, stride, input) != stride) break;
      if (be32(entry) != CRT_MACH_CPU) continue;
      if (stride == 32 && be32(entry + 8) != 0) continue;
      offset = be32(entry + (stride == 32 ? 12 : 8));
      if (fseek(input, (long)offset, SEEK_SET) == 0 && fread(header, 1, 32, input) == 32 &&
          macho_header(header)) { result = 1; break; }
    }
  }
#elif defined(CRT_TARGET_OS_WINDOWS)
  if (length == sizeof(header) && header[0] == 'M' && header[1] == 'Z') {
    uint32_t offset = le32(header + 60);
    if (offset <= 0x7fffffffU && fseek(input, (long)offset, SEEK_SET) == 0 &&
        fread(header, 1, 24, input) == 24 && memcmp(header, "PE\0\0", 4) == 0 &&
        (header[4] | (unsigned int)header[5] << 8) == CRT_PE_MACHINE &&
        (header[22] & 2) && !(header[23] & 0x20)) result = 1;
  }
#endif
  if (ferror(input)) result = -1;
  if (fclose(input) != 0) result = -1;
  return result;
}
