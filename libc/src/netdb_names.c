/* Name-service helpers that need no resolver: getnameinfo() (numeric host, service by the built-in
 * table), getservbyname()/getservbyport() (the table), inet_ntoa(). Bionic reads /etc/services and
 * asks the resolver for PTR records; CRT has no /etc/services on Windows and no reverse-DNS client,
 * so the host is always numeric: NI_NAMEREQD fails with EAI_NONAME, as it does for an address with
 * no PTR record. First consumer: OpenSSL's BIO_ADDR (Web Tranche 3B, milestone B2). */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

struct crt_service {
  const char* name;
  const char* alias;
  unsigned short port;
  const char* proto;
};

static const struct crt_service services[] = {
    {"echo", 0, 7, "tcp"},        {"echo", 0, 7, "udp"},       {"ftp-data", 0, 20, "tcp"},
    {"ftp", 0, 21, "tcp"},        {"ssh", 0, 22, "tcp"},       {"telnet", 0, 23, "tcp"},
    {"smtp", "mail", 25, "tcp"},  {"domain", 0, 53, "tcp"},    {"domain", 0, 53, "udp"},
    {"tftp", 0, 69, "udp"},       {"http", "www", 80, "tcp"},  {"pop3", "pop-3", 110, "tcp"},
    {"ntp", 0, 123, "udp"},       {"imap", "imap2", 143, "tcp"}, {"snmp", 0, 161, "udp"},
    {"ldap", 0, 389, "tcp"},      {"https", 0, 443, "tcp"},    {"https", 0, 443, "udp"},
    {"submission", 0, 587, "tcp"}, {"ldaps", 0, 636, "tcp"},   {"imaps", 0, 993, "tcp"},
    {"pop3s", 0, 995, "tcp"},
};

static struct servent result_entry;
static char* result_aliases[2];
static char result_name[32];
static char result_alias[32];
static char result_proto[8];

static struct servent* fill(const struct crt_service* service) {
  size_t aliases = 0;

  strncpy(result_name, service->name, sizeof result_name - 1);
  strncpy(result_proto, service->proto, sizeof result_proto - 1);
  result_entry.s_name = result_name;
  result_entry.s_proto = result_proto;
  result_entry.s_port = (int)htons(service->port);
  if (service->alias != 0) {
    strncpy(result_alias, service->alias, sizeof result_alias - 1);
    result_aliases[aliases++] = result_alias;
  }
  result_aliases[aliases] = 0;
  result_entry.s_aliases = result_aliases;
  return &result_entry;
}

struct servent* getservbyname(const char* name, const char* proto) {
  size_t i;

  if (name == 0) {
    return 0;
  }
  for (i = 0; i < sizeof services / sizeof services[0]; ++i) {
    if ((proto == 0 || strcmp(proto, services[i].proto) == 0) &&
        (strcmp(name, services[i].name) == 0 || (services[i].alias != 0 && strcmp(name, services[i].alias) == 0))) {
      return fill(&services[i]);
    }
  }
  return 0;
}

struct servent* getservbyport(int port, const char* proto) {
  size_t i;

  for (i = 0; i < sizeof services / sizeof services[0]; ++i) {
    if ((int)htons(services[i].port) == port && (proto == 0 || strcmp(proto, services[i].proto) == 0)) {
      return fill(&services[i]);
    }
  }
  return 0;
}

char* inet_ntoa(struct in_addr in) {
  static char text[INET_ADDRSTRLEN];
  const unsigned char* bytes = (const unsigned char*)&in.s_addr;

  snprintf(text, sizeof text, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
  return text;
}

int getnameinfo(const struct sockaddr* addr, socklen_t addrlen, char* host, socklen_t hostlen, char* serv,
                socklen_t servlen, int flags) {
  unsigned short port;
  const void* address;
  int family;

  if (addr == 0 || addrlen < (socklen_t)sizeof(sa_family_t)) {
    return EAI_FAMILY;
  }
  family = addr->sa_family;
  if (family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
    const struct sockaddr_in* in = (const struct sockaddr_in*)addr;
    address = &in->sin_addr;
    port = ntohs(in->sin_port);
  } else if (family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
    const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)addr;
    address = &in6->sin6_addr;
    port = ntohs(in6->sin6_port);
  } else {
    return EAI_FAMILY;
  }
  if (host == 0 && serv == 0) {
    return EAI_NONAME;
  }
  if (host != 0 && hostlen > 0) {
    if ((flags & NI_NAMEREQD) != 0) {
      return EAI_NONAME; /* no reverse-DNS client */
    }
    if (inet_ntop(family, address, host, hostlen) == 0) {
      return EAI_OVERFLOW;
    }
  }
  if (serv != 0 && servlen > 0) {
    const struct servent* entry = 0;
    int written;

    if ((flags & NI_NUMERICSERV) == 0) {
      entry = getservbyport((int)htons(port), (flags & NI_DGRAM) != 0 ? "udp" : "tcp");
    }
    written = entry != 0 ? snprintf(serv, servlen, "%s", entry->s_name) : snprintf(serv, servlen, "%u", port);
    if (written < 0 || (socklen_t)written >= servlen) {
      return EAI_OVERFLOW;
    }
  }
  return 0;
}
