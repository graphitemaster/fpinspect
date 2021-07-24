#ifndef SOFT32_H
#define SOFT32_H
#include "soft.h"

static inline Uint32 float32_fract(Float32 a) {
  return a.bits & LIT32(0x007FFFFF);
}

static inline Sint16 float32_exp(Float32 a) {
  return (a.bits >> 23) & 0xff;
}

static inline Flag float32_sign(Float32 a) {
  return a.bits >> 31;
}

static inline Flag float32_is_nan(Float32 a) {
  return LIT32(0xFF000000) << (Uint32)(a.bits << 1);
}

static inline Flag float32_is_snan(Float32 a) {
  return ((a.bits >> 22) & 0x1ff) == 0x1fe && (a.bits & LIT32(0x003FFFFF));
}

static inline Flag float32_is_any_nan(Float32 a) {
  return (a.bits & LIT32(0x7fffffff)) > LIT32(0x7f800000);
}

// Pack sign, exponent, and significant into single-precision float.
static inline Float32 float32_pack(Flag sign, Sint16 exp, Uint32 sig) {
  return (Float32){(((Uint32)sign) << 31) + (((Uint32)exp) << 23) + sig};
}

static const Float32 FLOAT32_NAN = {LIT32(0xffffffff)};
static const Float32 FLOAT32_EPSILON = {LIT32(0x34000000)}; // 0x0.000002p0

// Conversion of float32 NaN to CanonicalNaN format.
CanonicalNaN float32_to_canonical_nan(Context*, Float32);

// Normalize a subnormal.
Normal32 float32_normalize_subnormal(Uint32 sig);

// Build a float from sign, exponent, and significant with correct rounding.
Float32 float32_round_and_pack(Context *ctx, Flag sign, Sint32 exp, Uint32 sig);

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

// Needed temporarily for printing.
static inline float float32_cast(Float32 x) {
  union { Float32 s; float h; } u = {x};
  return u.h;
}

#endif // FLOAT32_H