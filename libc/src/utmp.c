#include <errno.h>
#include <utmp.h>
#include <utmpx.h>

int utmpname(const char* path) {
  (void)path;
  return __set_errno(ENOTSUP);
}

void setutent(void) {
}

struct utmp* getutent(void) {
  return 0;
}

struct utmp* pututline(const struct utmp* entry) {
  (void)entry;
  return 0;
}

void endutent(void) {
}

void setutxent(void) {
  setutent();
}

struct utmpx* getutxent(void) {
  return 0;
}

struct utmpx* getutxid(const struct utmpx* entry) {
  (void)entry;
  return getutxent();
}

struct utmpx* getutxline(const struct utmpx* entry) {
  (void)entry;
  return getutxent();
}

struct utmpx* pututxline(const struct utmpx* entry) {
  (void)entry;
  return 0;
}

void endutxent(void) {
  endutent();
}
