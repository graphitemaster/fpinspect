#include "kernel32.h"
#include "soft64.h"

// When the result of evaluating something is not used the compiler will attempt
// to remove that dead code, even though in this case we want the evaluation
// of some expressions to happen to trigger exceptions.
static inline void float32_force_eval(Float32 x) {
  volatile Float32 y;
  y = x;
  (void)y; // Mark as used.
}

Float32 float32_floor(Context *ctx, Float32 x) {
  static const Float32 HUGE = {LIT32(0x7b800000)}; // 0x1p120f
  const Sint16 e = float32_exp(x) - 0x7f;
  if (e >= 23) {
    return x;
  }
  if (e >= 0) {
    const Uint32 m = LIT32(0x007fffff) >> e;
    if ((x.bits & m) == 0) {
      return x;
    }
    float32_force_eval(float32_add(ctx, x, HUGE)); 
    if (x.bits >> 31) {
      x.bits += m;
    }
    x.bits &= ~m;
  } else {
    float32_force_eval(float32_add(ctx, x, HUGE));
    if (x.bits >> 31 == 0) {
      x.bits = 0;
    } else if (x.bits << 1) {
      x.bits = LIT32(0xbf800000); // -1.0
    }
  }
  return x;
}

Float32 float32_ceil(Context *ctx, Float32 x) {
  static const Float32 HUGE = {LIT32(0x7b800000)}; // 0x1p120f
  const Sint16 e = float32_exp(x) - 0x7f;
  if (e >= 23) {
    return x;
  }
  if (e >= 0) {
    const Uint32 m = LIT32(0x007fffff) >> e;
    if ((x.bits & m) == 0) {
      return x;
    }
    float32_force_eval(float32_add(ctx, x, HUGE));
    if (x.bits >> 31 == 0) {
      x.bits += m;
    }
    x.bits &= ~m;
  } else {
    float32_force_eval(float32_add(ctx, x, HUGE));
    if (x.bits >> 31) {
      x.bits = LIT32(0x80000000); // -0.0
    } else if (x.bits << 1) {
      x.bits = LIT32(0x3f800000); // 1.0
    }
  }
  return x;
}

Float32 float32_trunc(Context *ctx, Float32 x) {
  static const Float32 HUGE = {LIT32(0x7b800000)}; // 0x1p120f
  Sint16 e = float32_exp(x) - 0x7f + 9;
  if (e >= 23 + 9) {
    return x;
  }
  if (e < 9) {
    e = 1;
  }
  const Uint32 m = -1u >> e;
  if ((x.bits & m) == 0) {
    return x;
  }
  float32_force_eval(float32_add(ctx, x, HUGE));
  x.bits &= ~m;
  return x;
}

// 32-bit multiplication without truncation.
static inline Uint32 mul32(Uint32 a, Uint32 b) {
  return (Uint64)a*b >> 32;
}

// Computes (x-x) / (x-x) to correctly raise an invalid exception and compute
// correct exceptional value of NaN, sNaN, +Inf, or -Inf for given x.
static Float32 float32_invalid(Context *ctx, Float32 x) {
  const Float32 sub = float32_sub(ctx, x, x);
  return float32_div(ctx, sub, sub);
}

Float32 float32_sqrt(Context *ctx, Float32 x) {
  // if x in [1,2): i = (Sint32)(64*x);
  // if x in [2,4): i = (Sint32)(32*x-64);
  // TABLE[i]*2^-16 is estimating 1/sqrt(x) with small relative error:
  // |TABLE[i]*0x1p-16*sqrt(x) - 1| < -0x1.fdp-9 < 2^-8
  static const Uint16 TABLE[128] = {
    0xb451, 0xb2f0, 0xb196, 0xb044, 0xaef9, 0xadb6, 0xac79, 0xab43,
    0xaa14, 0xa8eb, 0xa7c8, 0xa6aa, 0xa592, 0xa480, 0xa373, 0xa26b,
    0xa168, 0xa06a, 0x9f70, 0x9e7b, 0x9d8a, 0x9c9d, 0x9bb5, 0x9ad1,
    0x99f0, 0x9913, 0x983a, 0x9765, 0x9693, 0x95c4, 0x94f8, 0x9430,
    0x936b, 0x92a9, 0x91ea, 0x912e, 0x9075, 0x8fbe, 0x8f0a, 0x8e59,
    0x8daa, 0x8cfe, 0x8c54, 0x8bac, 0x8b07, 0x8a64, 0x89c4, 0x8925,
    0x8889, 0x87ee, 0x8756, 0x86c0, 0x862b, 0x8599, 0x8508, 0x8479,
    0x83ec, 0x8361, 0x82d8, 0x8250, 0x81c9, 0x8145, 0x80c2, 0x8040,
    0xff02, 0xfd0e, 0xfb25, 0xf947, 0xf773, 0xf5aa, 0xf3ea, 0xf234,
    0xf087, 0xeee3, 0xed47, 0xebb3, 0xea27, 0xe8a3, 0xe727, 0xe5b2,
    0xe443, 0xe2dc, 0xe17a, 0xe020, 0xdecb, 0xdd7d, 0xdc34, 0xdaf1,
    0xd9b3, 0xd87b, 0xd748, 0xd61a, 0xd4f1, 0xd3cd, 0xd2ad, 0xd192,
    0xd07b, 0xcf69, 0xce5b, 0xcd51, 0xcc4a, 0xcb48, 0xca4a, 0xc94f,
    0xc858, 0xc764, 0xc674, 0xc587, 0xc49d, 0xc3b7, 0xc2d4, 0xc1f4,
    0xc116, 0xc03c, 0xbf65, 0xbe90, 0xbdbe, 0xbcef, 0xbc23, 0xbb59,
    0xba91, 0xb9cc, 0xb90a, 0xb84a, 0xb78c, 0xb6d0, 0xb617, 0xb560,
  };

  Uint32 ix = x.bits;

  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    // x < 0x1p-126, inf, or nan.
    if (ix * 2 == 0) {
      return x;
    }
    if (ix == LIT32(0x7f800000)) {
      return x;
    }
    if (ix > LIT32(0x7f800000)) {
      return float32_invalid(ctx, x);
    }
    // is subnormal, normalize it.
    const Float32 n = float32_mul(ctx, x, (Float32){LIT32(0x4b000000)}); // 0x1p23f
    ix = n.bits;
    ix -= 23 << 23;
  }

  // x = 4^e m; with int e and m in [1, 4).
  Uint32 even = ix & LIT32(0x00800000);
  Uint32 m1 = (ix << 8) | LIT32(0x80000000);
  Uint32 m0 = (ix << 7) & LIT32(0x7fffffff);
  Uint32 m = even ? m0 : m1;

  // 2^e is exponent part.
  Uint32 ey = ix >> 1;
  ey += LIT32(0x3f800000) >> 1;
  ey &= LIT32(0x7f800000);

  // Compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 iterations.
  static const Uint32 THREE = LIT32(0xc0000000);
  const Uint32 i = (ix >> 17) % 128;
  Uint32 r, s, d, u;
  r = (Uint32)TABLE[i] << 16;
  // |r*sqrt(m) - 1| < 0x1p-8
  s = mul32(m, r);
  // |s/sqrt(m) - 1| < 0x1p-8
  d = mul32(s, r);
  u = THREE - d;
  r = mul32(r, u) << 1;
  // |r*sqrt(m) - 1| < 0x1.7bp-16
  s = mul32(s, u) << 1;
  // |s/sqrt(m) - 1| < 0x1.7bp-16
  d = mul32(s, r);
  u = THREE - d;
  s = mul32(s, u);
  // -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31
  s = (s - 1) >> 6;
  // s < sqrt(m) < s + 0x1.08p-23

  // Compute nearest rounded result.
  const Uint32 d0 = (m << 16) - s*s;
  const Uint32 d1 = s - d0;
  const Uint32 d2 = d1 + s + 1;
  s += d1 >> 31;
  s &= LIT32(0x007fffff);
  s |= ey;

  const Float32 y = {s};

  // Handle rounding and inexact exceptions.
  const Float32 t = {(d2 == 0 ? 0 : LIT32(0x01000000)) | ((d1 ^ d2) & LIT32(0x80000000))};

  return float32_add(ctx, y, t);
}

Float32 float32_abs(Context *ctx, Float32 x) {
  (void)ctx;
  x.bits &= 0x7fffffff;
  return x;
}

Float32 float32_copysign(Context *ctx, Float32 x, Float32 y) {
  (void)ctx;
  x.bits &= LIT32(0x7fffffff); // abs
  x.bits |= y.bits & LIT32(0x80000000); // copy sign bit
  return x;
}

Float32 float32_max(Context *ctx, Float32 x, Float32 y) {
  if (float32_is_any_nan(x)) {
    return y;
  }
  if (float32_is_any_nan(y)) {
    return x;
  }

  // Handle signed zeros.
  const Flag sign_x = float32_sign(x);
  const Flag sign_y = float32_sign(y);
  if (sign_x != sign_y) {
    return sign_x ? y : x;
  }

  // IEEE makes it clear min and max should both use lt relational operation.
  return float32_lt(ctx, x, y) ? y : x;
}

Float32 float32_min(Context *ctx, Float32 x, Float32 y) {
  if (float32_is_any_nan(x)) {
    return y;
  }
  if (float32_is_any_nan(y)) {
    return x;
  }

  // Handle signed zeros.
  const Flag sign_x = float32_sign(x);
  const Flag sign_y = float32_sign(y);
  if (sign_x != sign_y) {
    return sign_x ? x : y;
  }

  return float32_lt(ctx, x, y) ? x : y;
}

Float32 float32_cosd(Context *ctx, Float64 x) {
  static const Float64 C0 = {LIT64(0xbfdffffffd0c5e81)};
  static const Float64 C1 = {LIT64(0x3fa55553e1053a42)};
  static const Float64 C2 = {LIT64(0xbf56c087e80f1e27)};
  static const Float64 C3 = {LIT64(0x3ef99342e0ee5069)};

  Float64 z = float64_mul(ctx, x, x);
  Float64 w = float64_mul(ctx, z, z);
  Float64 r = float64_add(ctx, C2, float64_mul(ctx, z, C3));

  // ((1.0+(z*C0)) + (w*C1)) + ((w*z)*r)
  return float64_to_float32(
    ctx,
    float64_add(ctx,
      float64_add(ctx,
        float64_add(ctx,
          (Float64){LIT64(0x3ff0000000000000)},
          float64_mul(ctx, z, C0)),
        float64_mul(ctx, w, C1)),
      float64_mul(ctx, float64_mul(ctx, w, z), r)));
}