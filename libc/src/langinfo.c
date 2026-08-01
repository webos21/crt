#include <langinfo.h>

char* nl_langinfo(nl_item item) {
  if (item == CODESET) {
    return "UTF-8";
  }
  return "";
}
