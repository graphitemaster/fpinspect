#ifndef SOFT64_H
#define SOFT64_H
#include "soft.h"

static inline Uint64 float64_fract(Float64 a) {
  return a.bits & LIT64(0x000FFFFFFFFFFFFF);
}

static inline Sint16 float64_exp(Float64 a) {
  return (a.bits >> 52) & 0x7ff;
}

static inline Flag float64_sign(Float64 a) {
  return a.bits >> 63;
}

static inline Flag float64_is_nan(Float64 a) {
  return LIT64(0xFFE0000000000000) < (Uint64)(a.bits << 1);
}

static inline Flag float64_is_snan(Float64 a) {
  return (((a.bits >> 51) & 0xfff) == 0xffe)
    && (a.bits & LIT64(0x0007ffffffffffff));
}

// Pack sign, exponent, and significant into double-precision float.
static inline Float64 float64_pack(Flag sign, Sint16 exp, Uint64 sig) {
  return (Float64){(((Uint64)sign) << 63) + (((Uint64)exp) << 52) + sig};
}

static const Float64 FLOAT64_NAN = {LIT64(0xffffffffffffffff)};

// Conversion of float32 NaN to CanonicalNaN format.
CanonicalNaN float64_to_canonical_nan(Context*, Float64);

// Normalize subnormal.
Normal64 float64_normalize_subnormal(Uint64 sig);

// Build a float64 from sign, exponent, and significant with correct rounding.
Float64 float64_round_and_pack(Context *ctx, Flag sign, Sint32 exp, Uint64 sig);

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

#endif // FLOAT64_H