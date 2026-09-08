#include <unistd.h>

int main(void) {
  static const char message[] = "Hello from freestanding CRT\n";
  write(1, message, sizeof(message) - 1);
  return 0;
}
