#ifndef SOFT_H
#define SOFT_H
#include "array.h"

typedef Sint8 Flag;

typedef enum Round Round;
typedef enum Exception Exception;
typedef enum Tininess Tininess;

typedef struct Context Context;

typedef struct Float32 Float32;
typedef struct Float64 Float64;
typedef struct Normal32 Normal32;
typedef struct Normal64 Normal64;

typedef struct CanonicalNaN CanonicalNaN;

struct Float32 {
  Uint32 bits;
};

struct Float64 {
  Uint64 bits;
};

struct Normal32 {
  Uint32 sig;
  Sint16 exp;
};

struct Normal64 {
  Uint64 sig;
  Sint16 exp;
};

// Canonical NaN format for conversion between NaNs in different precisions.
struct CanonicalNaN {
  Flag sign;
  Uint64 hi;
  Uint64 lo;
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
  Size roundings;
  ARRAY(Exception) exceptions;
  Tininess tininess;
};

void context_init(Context* context);
void context_free(Context* context);
bool context_raise(Context *context, Exception exception);

// Special right shifts where the least significant bit of result is set when
// any non-zero bits are shifted off.
static inline Uint32 rshr32(Uint32 a, Sint16 count) {
  if (count == 0) {
    return a;
  } else if (count < 32) {
    return (a >> count) | ((a << ((-count) & 31)) != 0);
  }
  return a != 0 ? 1 : 0;
}

static inline Uint64 rshr64(Uint64 a, Sint16 count) {
  if (count == 0) {
    return a;
  } else if (count < 64) {
    return (a >> count) | ((a << ((-count) & 63)) != 0);
  }
  return a != 0 ? 1 : 0;
}

Float32 float64_to_float32(Context*, Float64);
Float64 float32_to_float64(Context*, Float32);

#endif // SOFT_H