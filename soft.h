#ifndef SOFT_H
#define SOFT_H
#include "array.h"

typedef Sint8 Flag;

typedef enum Round Round;
typedef enum Exception Exception;
typedef enum Tininess Tininess;

typedef struct Context Context;

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

#endif // SOFT_H