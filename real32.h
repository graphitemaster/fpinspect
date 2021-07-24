#ifndef REAL32_H
#define REAL32_H
#include "float32.h"
#include "kernel32.h"

// Accumulative error accounting.
//
// The idea here is arithmetic results of soft float will always be close to
// the correct value +- 0.5 * EPSILON * value.
//
// That is:
//   err(a+b) = err(a) + err(b) + EPSILON * abs(a+b)
//
// The error result of an elementary floating-point operation does not exceed
// and is close to abs(result) * EPSILON.
typedef struct Real32 Real32;

struct Real32 {
  Float32 value;
  Float32 eps;
};

// Cannot use FLOAT32_ZERO here as it would be a non-const initializer in C.
#define REAL32_NAN        (Real32){FLOAT32_NAN,       {0}} //  NaN
#define REAL32_EPSILON    (Real32){FLOAT32_EPSILON,   {0}} //  0x0.000002p0
#define REAL32_ZERO       (Real32){FLOAT32_ZERO,      {0}} //  0.0
#define REAL32_HALF       (Real32){FLOAT32_HALF,      {0}} //  0.5
#define REAL32_ONE        (Real32){FLOAT32_ONE,       {0}} //  1.0
#define REAL32_MINUS_ONE  (Real32){FLOAT32_MINUS_ONE, {0}} // -1.0

Real32 real32_add(Context *ctx, Real32 a, Real32 b);
Real32 real32_sub(Context *ctx, Real32 a, Real32 b);
Real32 real32_mul(Context *ctx, Real32 a, Real32 b);
Real32 real32_div(Context *ctx, Real32 a, Real32 b);

#define REAL32_WRAP1_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a)
#define REAL32_WRAP2_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a, Real32 b)
#define REAL32_WRAP_RELATION_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a, Real32 b)

// Operations that cannot produce errors.
// 1. Truncation.
REAL32_WRAP1_NO_ERROR(floor);
REAL32_WRAP1_NO_ERROR(ceil);
REAL32_WRAP1_NO_ERROR(trunc);
// 2. Absolute.
REAL32_WRAP1_NO_ERROR(abs);
// 3. Sign bit inspection.
REAL32_WRAP2_NO_ERROR(copysign);
REAL32_WRAP2_NO_ERROR(max);
REAL32_WRAP2_NO_ERROR(min);
// 4. Relational operators.
REAL32_WRAP_RELATION_NO_ERROR(eq);
REAL32_WRAP_RELATION_NO_ERROR(lte);
REAL32_WRAP_RELATION_NO_ERROR(lt);
REAL32_WRAP_RELATION_NO_ERROR(ne);
REAL32_WRAP_RELATION_NO_ERROR(gte);
REAL32_WRAP_RELATION_NO_ERROR(gt);

#undef REAL32_WRAP_RELATION_NO_ERROR
#undef REAL32_WRAP2_NO_ERROR
#undef REAL32_WRAP1_NO_ERROR

Real32 real32_sqrt(Context*, Real32);

#endif // ERROR_H