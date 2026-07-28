#ifndef CRT_ARPA_INET_H
#define CRT_ARPA_INET_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

in_addr_t inet_addr(const char* cp);
int inet_pton(int af, const char* src, void* dst);
const char* inet_ntop(int af, const void* src, char* dst, socklen_t size);

#ifdef __cplusplus
}
#endif

#endif
