#include <errno.h>
#include <nl_types.h>

nl_catd catopen(const char* name, int flag) {
  (void)name;
  (void)flag;
  errno = ENOENT;
  return (nl_catd)-1;
}

char* catgets(nl_catd catalog, int set_number, int message_number, const char* message) {
  (void)catalog;
  (void)set_number;
  (void)message_number;
  return (char*)message;
}

int catclose(nl_catd catalog) {
  if (catalog == (nl_catd)-1) {
    errno = EBADF;
    return -1;
  }
  return 0;
}
