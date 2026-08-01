#ifndef CRT_UTMPX_H
#define CRT_UTMPX_H

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMPTY 0
#define RUN_LVL 1
#define BOOT_TIME 2
#define NEW_TIME 3
#define OLD_TIME 4
#define INIT_PROCESS 5
#define LOGIN_PROCESS 6
#define USER_PROCESS 7
#define DEAD_PROCESS 8
#define ACCOUNTING 9

struct utmpx {
  short ut_type;
  pid_t ut_pid;
  char ut_line[32];
  char ut_id[4];
  char ut_user[32];
  char ut_host[256];
  struct {
    short e_termination;
    short e_exit;
  } ut_exit;
  long ut_session;
  struct timeval ut_tv;
  int32_t ut_addr_v6[4];
  char unused[20];
};

void setutxent(void);
struct utmpx* getutxent(void);
struct utmpx* getutxid(const struct utmpx* entry);
struct utmpx* getutxline(const struct utmpx* entry);
struct utmpx* pututxline(const struct utmpx* entry);
void endutxent(void);

#ifdef __cplusplus
}
#endif

#endif
