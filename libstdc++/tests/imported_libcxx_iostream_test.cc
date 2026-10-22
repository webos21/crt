#include <iostream>
#include <sstream>
#include <string>

/* A plain include-only test does not pull the static stream definitions in.
 * Use them before main as well as inside it, then require buffered destructor
 * output to survive normal process exit. Do not add an explicit ios_base::Init
 * or a forced flush that would hide a broken CRT startup/lifetime boundary. */
static bool initialized;
struct StreamLifetime {
  StreamLifetime() {
    std::cout << "iostream-before-main\n";
    initialized = std::cout.good();
  }
  ~StreamLifetime() {
    std::cout << "iostream-after-main\n";
  }
};
static StreamLifetime lifetime;

extern "C" int main() {
  std::istringstream input("17 CRT");
  std::ostringstream output;
  int value = 0;
  std::string word;
  input >> value >> word;
  output << word << ':' << value;
  if (!initialized || output.str() != "CRT:17" || !std::cin.good()) return 1;
  std::cout << "iostream-main:" << output.str() << '\n';
  std::cerr << "iostream-cerr\n";
  std::clog << "iostream-clog\n";
  return std::cout.good() && std::cerr.good() && std::clog.good() ? 0 : 2;
}
