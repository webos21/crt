#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define alloca(size) __builtin_alloca(size)

#ifdef __cplusplus
}
#endif
