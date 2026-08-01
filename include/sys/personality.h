#ifndef CRT_SYS_PERSONALITY_H
#define CRT_SYS_PERSONALITY_H

#define PER_LINUX 0x0000
#define PER_LINUX32 0x0008

#ifdef __cplusplus
extern "C" {
#endif

int personality(unsigned long persona);

#ifdef __cplusplus
}
#endif

#endif
