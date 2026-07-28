int main(void);
void exit(int status);

void mainCRTStartup(void) {
  exit(main());
}
