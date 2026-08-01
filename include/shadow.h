#ifndef CRT_SHADOW_H
#define CRT_SHADOW_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct spwd {
  char* sp_namp;
  char* sp_pwdp;
  long sp_lstchg;
  long sp_min;
  long sp_max;
  long sp_warn;
  long sp_inact;
  long sp_expire;
  unsigned long sp_flag;
};

void setspent(void);
void endspent(void);
struct spwd* getspent(void);
struct spwd* getspnam(const char* name);
struct spwd* fgetspent(FILE* stream);
struct spwd* sgetspent(const char* string);
int putspent(const struct spwd* entry, FILE* stream);
int getspent_r(struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result);
int getspnam_r(const char* name, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result);
int sgetspent_r(const char* string, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result);
int fgetspent_r(FILE* stream, struct spwd* result_buf, char* buffer, size_t buflen, struct spwd** result);
int lckpwdf(void);
int ulckpwdf(void);

#ifdef __cplusplus
}
#endif

#endif
