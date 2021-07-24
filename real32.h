#ifndef REAL32_H
#define REAL32_H
#include "soft32.h"
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

inline Real32 real32(Float32 value) {
  Real32 result;
  result.value = value;
  result.eps = (Float32){0};
  return result;
}

// When calculating error we don't want to muddy the context of the value.
static inline Context eps_ctx(const Context *ctx) {
  Context c;
  context_copy(&c, ctx);
  return c;
}

static inline Real32 real32_add(Context *ctx, Real32 a, Real32 b) {
  Context ec = eps_ctx(ctx);
  Real32 r;
  r.value = float32_add(ctx, a.value, b.value);
  r.eps = 
    float32_add(
      &ec,
      // err(a) + err(b)
      float32_add(&ec, a.eps, b.eps),
      // EPSILON * abs(value)
      float32_mul(&ec, FLOAT32_EPSILON, float32_abs(&ec, r.value)));
  return r;
}

static inline Real32 real32_sub(Context *ctx, Real32 a, Real32 b) {
  Context ec = eps_ctx(ctx);
  Real32 r;
  r.value = float32_sub(ctx, a.value, b.value);
  r.eps = 
    float32_add(
      &ec,
      // err(a) + err(b)
      float32_add(&ec, a.eps, b.eps),
      // EPSILON * abs(value)
      float32_mul(&ec, FLOAT32_EPSILON, float32_abs(&ec, r.value)));
  return r;
}

static inline Real32 real32_mul(Context *ctx, Real32 a, Real32 b) {
  Context ec = eps_ctx(ctx);
  Real32 r;
  r.value = float32_mul(ctx, a.value, b.value);
  r.eps = float32_add(
    &ec,
    float32_add(
      &ec,
      float32_add(
        &ec,
        // err(a) * abs(b)
        float32_mul(&ec, a.eps, float32_abs(&ec, b.value)),
        // err(b) * abs(a)
        float32_mul(&ec, b.eps, float32_abs(&ec, a.value))),
      // err(a) * err(b)
      float32_mul(&ec, a.eps, b.eps)),
    // EPSILON * abs(value)
    float32_mul(&ec, FLOAT32_EPSILON, float32_abs(&ec, r.value)));
  return r;
}

// Calculating division error is non-trivial when the divisor is inaccurate,
// use the following to recover inaccuracies for inaccurate divisor
// r^2(-x) - r*x + 0 = 0
static inline Real32 real32_div(Context *ctx, Real32 a, Real32 b) {
  Context ec = eps_ctx(ctx);
  Real32 r;
  r.value = float32_div(ctx, a.value, b.value);
  
  const Float32 abs_b = float32_abs(&ec, b.value);
  const Float32 abs_r = float32_abs(&ec, r.value);
  Float32 e = 
    float32_div(
      &ec,
      // eps(a) + (?)
      float32_add(
        &ec,
        a.eps,
        // abs(r) * eps(b)
        float32_mul(&ec, abs_r, b.eps)),
      abs_b);
  
  // Use more accurate for inaccurate divisors.
  static const Float32 EPS = {LIT32(0x3c23d70a)}; // 0.01f
  if (float32_gt(&ec, b.eps, float32_mul(&ec, EPS, abs_b))) {
    const Float32 r = float32_div(&ec, b.eps, b.value);
    // e = e * (1 + (1 + r) * r)
    e = float32_mul(
      &ec,
      e,
      // 1 + (1 + r) * r
      float32_add(
        &ec,
        float32_from_sint32(&ec, 1),
        // (1 + r) * r
        float32_mul(
          &ec,
          // 1 + r
          float32_add(
            &ec,
            float32_from_sint32(&ec, 1),
            r),
          r)));
  }

  r.eps = 
    // e + (EPSILON * abs(value))
    float32_add(
      &ec,
      e,
      // EPSILON * abs(value)
      float32_mul(&ec, FLOAT32_EPSILON, float32_abs(&ec, r.value)));
  
  return r;
}

#endif // ERROR_H