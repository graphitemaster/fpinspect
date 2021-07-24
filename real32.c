#include "real32.h"
#include "kernel32.h"

// When calculating error we don't want to muddy the value context. Use a copy
// of it with the same rounding and tininess mode ignoring everything else.
static inline Context eps_ctx(const Context *ctx) {
  Context c;
  context_copy(&c, ctx);
  return c;
}

Real32 real32_add(Context *ctx, Real32 a, Real32 b) {
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

Real32 real32_sub(Context *ctx, Real32 a, Real32 b) {
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

Real32 real32_mul(Context *ctx, Real32 a, Real32 b) {
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
Real32 real32_div(Context *ctx, Real32 a, Real32 b) {
  Context ec = eps_ctx(ctx);
  Real32 r;
  r.value = float32_div(ctx, a.value, b.value);
  
  const Float32 abs_b = float32_abs(&ec, b.value);
  const Float32 abs_r = float32_abs(&ec, r.value);
  Float32 e = 
    float32_div(
      &ec,
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

Real32 real32_sqrt(Context *ctx, Real32 x) {
  Context ec = eps_ctx(ctx);

  // Calculate error.
  Float32 d;
  // Assume non-negative input.
  if (float32_gte(&ec, x.value, FLOAT32_ZERO)) {
    const Float32 r = float32_sqrt(&ec, x.value);
    // if x > 10.0 * err(x)
    const Float32 err = float32_mul(&ec, float32_from_sint32(&ec, 10), x.eps);
    if (float32_gt(&ec, x.value, err)) {
      // 0.5 * (err(x) / r)
      d = float32_mul(&ec, FLOAT32_HALF, float32_div(&ec, x.eps, r));
    } else {
      // if x > err(x)
      if (float32_gt(&ec, x.value, x.eps)) {
        // r - sqrt(x - err(x))
        d = float32_sub(&ec, r, float32_sqrt(&ec, float32_sub(&ec, x.value, x.eps)));
      } else {
        // max(r, sqrt(x + err(x)) - r)
        d = float32_max(&ec, r, float32_sub(&ec, float32_sqrt(&ec, float32_add(&ec, x.value, x.eps)), r));
      }
    }
    // d += EPSILON * abs(r)
    d = float32_add(&ec, d, float32_mul(&ec, FLOAT32_EPSILON, float32_abs(&ec, r)));
  } else {
    // Assume negative input.
    if (float32_lt(&ec, x.value, float32_mul(&ec, x.eps, FLOAT32_MINUS_ONE))) {
      d = FLOAT32_NAN;
    } else {
      // Assume zero input.
      d = float32_sqrt(&ec, x.eps);
    }
  }

  return (Real32){float32_sqrt(ctx, x.value), d};
}

// Operations that cannot generate error.
#define REAL32_WRAP1_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a) { \
    return (Real32){float32_ ## name(ctx, a.value), {0}}; \
  }

#define REAL32_WRAP2_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a, Real32 b) { \
    return (Real32){float32_ ## name(ctx, a.value, b.value), {0}}; \
  }

#define REAL32_WRAP_RELATION_NO_ERROR(name) \
  Real32 real32_ ## name(Context *ctx, Real32 a, Real32 b) { \
    return float32_ ## name(ctx, a.value, b.value) ? REAL32_ONE : REAL32_ZERO; \
  }

REAL32_WRAP1_NO_ERROR(floor)
REAL32_WRAP1_NO_ERROR(ceil)
REAL32_WRAP1_NO_ERROR(trunc)

REAL32_WRAP1_NO_ERROR(abs)

REAL32_WRAP2_NO_ERROR(copysign)
REAL32_WRAP2_NO_ERROR(max)
REAL32_WRAP2_NO_ERROR(min)

REAL32_WRAP_RELATION_NO_ERROR(eq)
REAL32_WRAP_RELATION_NO_ERROR(lte)
REAL32_WRAP_RELATION_NO_ERROR(lt)
REAL32_WRAP_RELATION_NO_ERROR(ne)
REAL32_WRAP_RELATION_NO_ERROR(gte)
REAL32_WRAP_RELATION_NO_ERROR(gt)