int main(void);
void __crt_env_set_initial(char** envp);
void exit(int status);

void mainCRTStartup(void) {
  __crt_env_set_initial(0);
  exit(main());
}
