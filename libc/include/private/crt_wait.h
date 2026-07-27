#ifndef CRT_PRIVATE_WAIT_H
#define CRT_PRIVATE_WAIT_H

int __crt_wait32(int* addr, int expected);
int __crt_wake32_one(int* addr);
int __crt_wake32_all(int* addr);

#endif
