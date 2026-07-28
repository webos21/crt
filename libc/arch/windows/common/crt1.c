int main(void);
void __crt_env_init(char** envp);
void exit(int status);

void mainCRTStartup(void) {
  __crt_env_init(0);
  exit(main());
}
