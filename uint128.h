#ifndef UINT128_H
#define UINT128_H
#include "types.h"

typedef struct Uint128 Uint128;

struct Uint128 {
  Uint64 z0;
  Uint64 z1;
};

Uint128 uint128_mul64(Uint64 a, Uint64 b);

#endif // UINT128_H