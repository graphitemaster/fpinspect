#ifndef UINT128_H
#define UINT128_H
#include "types.h"

typedef struct Uint128 Uint128;

struct Uint128 {
  Uint64 z0;
  Uint64 z1;
};

// Multiplies two 64-bit integers to obtain a 128-bit product.
Uint128 uint128_mul64x64(Uint64 a, Uint64 b);

// Calculate approximation to the 64-bit integer quotient obtained by dividing
// 64-bit b into the 128-bit a. The divisor b must be at least 2^63.
Uint64 uint128_div128x64(Uint128 a, Uint64 b);

// Subtraction is modulo 2^128
static inline Uint128 uint128_sub(Uint128 a, Uint128 b) {
  const Uint64 z1 = a.z1 - b.z1;
  return (Uint128){a.z0 - b.z0 - z1, z1};
}

// Addition is modulo 2^128
static inline Uint128 uint128_add(Uint128 a, Uint128 b) {
  const Uint64 z1 = a.z1 + b.z1;
  return (Uint128){a.z0 + b.z0 + (z1 < a.z1), z1};
}

#endif // UINT128_H