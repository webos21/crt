#include <errno.h>
#include <shadow.h>

void setspent(void) {
}

void endspent(void) {
}

struct spwd* getspent(void) {
  return 0;
}

struct spwd* getspnam(const char* name) {
  (void)name;
  return 0;
}

struct spwd* fgetspent(FILE* stream) {
  (void)stream;
  return 0;
}

struct spwd* sgetspent(const char* string) {
  (void)string;
  return 0;
}

int putspent(const struct spwd* entry, FILE* stream) {
  (void)entry;
  (void)stream;
  return __set_errno(ENOTSUP);
}

int getspent_r(struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result) {
  (void)result_buf;
  (void)buffer;
  (void)buflen;
  if (result != 0) *result = 0;
  return ENOENT;
}

int getspnam_r(const char* name, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result) {
  (void)name;
  (void)result_buf;
  (void)buffer;
  (void)buflen;
  if (result != 0) *result = 0;
  return ENOENT;
}

int sgetspent_r(const char* string, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result) {
  (void)string;
  (void)result_buf;
  (void)buffer;
  (void)buflen;
  if (result != 0) *result = 0;
  return ENOENT;
}

int fgetspent_r(FILE* stream, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result) {
  (void)stream;
  (void)result_buf;
  (void)buffer;
  (void)buflen;
  if (result != 0) *result = 0;
  return ENOENT;
}

int lckpwdf(void) {
  return 0;
}

int ulckpwdf(void) {
  return 0;
}
