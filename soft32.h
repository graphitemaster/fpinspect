#ifndef SOFT32_H
#define SOFT32_H
#include "soft.h"

typedef struct Float32 Float32;

struct Float32 {
  Uint32 bits;
};

// Arithmetic functions.
Float32 float32_add(Context*, Float32, Float32); // a + b
Float32 float32_sub(Context*, Float32, Float32); // a - b
Float32 float32_mul(Context*, Float32, Float32); // a * b
Float32 float32_div(Context*, Float32, Float32); // a / b

// Relational functions.
Flag float32_eq(Context*, Float32, Float32); // a == b
Flag float32_lte(Context*, Float32, Float32); // a <= b
Flag float32_lt(Context*, Float32, Float32); // a < b
Flag float32_ne(Context*, Float32, Float32); // a != b
Flag float32_gte(Context*, Float32, Float32); // a >= b
Flag float32_gt(Context*, Float32, Float32); // a > b

// Conversion functions.
Float32 float32_from_sint32(Context *ctx, Sint32 x);

// Kernel functions.
Float32 float32_floor(Context*, Float32);
Float32 float32_ceil(Context*, Float32);
Float32 float32_trunc(Context*, Float32);
Float32 float32_sqrt(Context*, Float32);
Float32 float32_abs(Context*, Float32);
Float32 float32_copysign(Context*, Float32, Float32);
Float32 float32_max(Context*, Float32, Float32);
Float32 float32_min(Context*, Float32, Float32);

// Needed temporarily for printing.
static inline float float32_cast(Float32 x) {
  union { Float32 s; float h; } u = {x};
  return u.h;
}

#endif