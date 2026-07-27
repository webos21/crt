int main(void);
void __crt_sys_exit(int status);

void mainCRTStartup(void) {
  __crt_sys_exit(main());
}
