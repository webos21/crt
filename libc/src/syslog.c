#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

static const char* syslog_ident;
static int syslog_options;

void openlog(const char* ident, int option, int facility) {
  (void)facility;
  syslog_ident = ident;
  syslog_options = option;
}

void vsyslog(int priority, const char* format, va_list ap) {
  (void)priority;
  if ((syslog_options & LOG_PERROR) != 0 || isatty(STDERR_FILENO)) {
    if (syslog_ident != 0) {
      fprintf(stderr, "%s: ", syslog_ident);
    }
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
  }
}

void syslog(int priority, const char* format, ...) {
  va_list ap;

  va_start(ap, format);
  vsyslog(priority, format, ap);
  va_end(ap);
}

void closelog(void) {
  syslog_ident = 0;
  syslog_options = 0;
}
