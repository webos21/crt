// A catch handler must resume with the callee-saved registers its frame had before the call that threw. On Win64 that
// includes XMM6-XMM15, which clang happily keeps live across a call (it hoists a zeroed XMM6 to initialise
// std::basic_string). The pinned libunwind copied only the general registers into its cursor, so every catch that
// unwound through frames without CFI for the vector registers restored indeterminate XMM6-XMM15; the first visible
// symptom in crt-cli was a std::wstring whose size byte came from stack garbage (Windows/x64 concurrent
// `sdk install` SIGSEGV). Exercised through libc++ frames as well as through user frames, on every linkage.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#if defined(_WIN64) && defined(__x86_64__)
#define CRT_EH_VECTOR_TEST 1
static void write_vector(uint64_t a, uint64_t b) {
    asm volatile("movq %0, %%xmm6\n\tmovq %1, %%xmm7" : : "r"(a), "r"(b) : "xmm6", "xmm7");
}
static void read_vector(uint64_t* a, uint64_t* b) {
    asm volatile("movq %%xmm6, %0\n\tmovq %%xmm7, %1" : "=r"(*a), "=r"(*b));
}
#endif

#ifdef CRT_EH_VECTOR_TEST
namespace fs = std::filesystem;

__attribute__((noinline)) static void throw_from_user_frame() { throw std::runtime_error("user frame"); }
__attribute__((noinline)) static void throw_through_libcxx() {
    // stoi throws std::invalid_argument from inside libc++'s own frames.
    (void)std::stoi(std::string("not a number"));
}
__attribute__((noinline)) static void throw_after_stream_open(const fs::path& path) {
    std::ifstream in(path.string());
    if (!in) throw std::runtime_error("missing file");
}

template <class Thrower>
static int check(const char* what, Thrower thrower) {
    int lost = 0;
    for (int i = 0; i < 200; ++i) {
        const uint64_t a = 0x5a5a5a5a00000000ull + static_cast<uint64_t>(i);
        const uint64_t b = 0xa5a5a5a500000000ull + static_cast<uint64_t>(i);
        write_vector(a, b);
        try {
            thrower();
        } catch (const std::exception&) {
        }
        uint64_t ra = 0, rb = 0;
        read_vector(&ra, &rb);
        if (ra != a || rb != b) ++lost;
    }
    if (lost) std::fprintf(stderr, "%s: callee-saved vector registers lost in %d of 200 catches\n", what, lost);
    return lost;
}
#endif

extern "C" int main() {
#ifdef CRT_EH_VECTOR_TEST
    int lost = 0;
    lost += check("user frame", [] { throw_from_user_frame(); });
    lost += check("through libc++", [] { throw_through_libcxx(); });
    const fs::path missing = fs::path("crt-eh-registers-test-missing-directory") / "file";
    lost += check("after stream open", [&] { throw_after_stream_open(missing); });
    if (lost) return 1;
    std::puts("imported_libcxx_eh_registers_test: ok");
#else
    std::puts("imported_libcxx_eh_registers_test: ok (checked on Windows/x86_64 only)");
#endif
    return 0;
}
