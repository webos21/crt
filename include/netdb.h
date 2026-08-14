#ifndef CRT_NETDB_H
#define CRT_NETDB_H

#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct addrinfo {
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  socklen_t ai_addrlen;
  char* ai_canonname;
  struct sockaddr* ai_addr;
  struct addrinfo* ai_next;
};

struct hostent {
  char* h_name;
  char** h_aliases;
  int h_addrtype;
  int h_length;
  char** h_addr_list;
};

#define h_addr h_addr_list[0]

int* __get_h_errno(void);

#define HOST_NOT_FOUND 1
#define TRY_AGAIN 2
#define NO_RECOVERY 3
#define NO_DATA 4

#define AI_PASSIVE 0x00000001
#define AI_CANONNAME 0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_NUMERICSERV 0x00000008
#define AI_ALL 0x00000100
#define AI_V4MAPPED_CFG 0x00000200
#define AI_ADDRCONFIG 0x00000400
#define AI_V4MAPPED 0x00000800
#define AI_DEFAULT (AI_V4MAPPED_CFG | AI_ADDRCONFIG)
#define AI_MASK \
  (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG)

#define EAI_ADDRFAMILY 1
#define EAI_AGAIN 2
#define EAI_BADFLAGS 3
#define EAI_FAIL 4
#define EAI_FAMILY 5
#define EAI_MEMORY 6
#define EAI_NODATA 7
#define EAI_NONAME 8
#define EAI_SERVICE 9
#define EAI_SOCKTYPE 10
#define EAI_SYSTEM 11
#define EAI_BADHINTS 12
#define EAI_PROTOCOL 13
#define EAI_OVERFLOW 14
#define EAI_MAX 15

#define h_errno (*__get_h_errno())

int getaddrinfo(
    const char* node,
    const char* service,
    const struct addrinfo* hints,
    struct addrinfo** res);
void freeaddrinfo(struct addrinfo* res);
const char* gai_strerror(int errcode);
struct hostent* gethostbyname(const char* name);
const char* hstrerror(int err);

#ifdef __cplusplus
}
#endif

#endif
