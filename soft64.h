#ifndef SOFT64_H
#define SOFT64_H
#include "soft.h"

typedef struct Float64 Float64;

struct Float64 {
  Uint64 bits;
};

// Arithmetic functions.
Float64 float64_add(Context*, Float64, Float64); // a + b
Float64 float64_sub(Context*, Float64, Float64); // a - b
Float64 float64_mul(Context*, Float64, Float64); // a * b
Float64 float64_div(Context*, Float64, Float64); // a / b

// Needed temporarily for printing.
static inline double float64_cast(Float64 x) {
  union { Float64 s; double h; } u = {x};
  return u.h;
}

#endif // SOFT64_H