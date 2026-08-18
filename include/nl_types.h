#ifndef CRT_NL_TYPES_H
#define CRT_NL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nl_catd;
typedef int nl_item;

#define NL_SETD 1
#define NL_CAT_LOCALE 1

nl_catd catopen(const char* name, int flag);
char* catgets(nl_catd catalog, int set_number, int message_number, const char* message);
int catclose(nl_catd catalog);

#ifdef __cplusplus
}
#endif

#endif
