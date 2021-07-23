#ifndef SOFT_H
#define SOFT_H
#include "types.h"

typedef Sint8 Flag;

typedef struct Float32 Float32;
typedef struct Float64 Float64;

typedef enum Round Round;
typedef enum Exception Exception;
typedef enum Tininess Tininess;

typedef struct Context Context;

struct Float32 {
  Uint32 bits;
};

struct Float64 {
  Uint32 bits[2];
};

enum Round {
  ROUND_NEAREST_EVEN,
  ROUND_TO_ZERO,
  ROUND_DOWN,
  ROUND_UP
};

enum Exception {
  EXCEPTION_INVALID        = 1 << 0,
  EXCEPTION_DENORMAL       = 1 << 1,
  EXCEPTION_DIVIDE_BY_ZERO = 1 << 2,
  EXCEPTION_OVERFLOW       = 1 << 3,
  EXCEPTION_UNDERFLOW      = 1 << 4,
  EXCEPTION_INEXACT        = 1 << 5
};

enum Tininess {
  TININESS_AFTER_ROUNDING,
  TININESS_BEFORE_ROUNDING
};

struct Context {
  Round round;
  Exception exception;
  Tininess tininess;
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

#endif // SOFT_H