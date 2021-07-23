#include "soft32.h"

typedef struct Normal32 Normal32;

struct Normal32 {
  Uint32 sig;
  Sint16 exp;
};

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

// When the result of evaluating something is not used the compiler will attempt
// to remove that dead code, even though in this case we want the evaluation
// of some expressions to happen to trigger exceptions.
static inline void float32_force_eval(Float32 x) {
  volatile Float32 y;
  y = x;
  (void)y; // Mark as used.
}

static const Float32 FLOAT32_NAN = {LIT32(0xffffffff)};

// Count leading zero bits.
static inline Sint8 count_leading_zeros_u32(Uint32 a) {
  return a == 0 ? 32 : __builtin_clz(a);
}

// Pack sign sign, exponent, and significant into single-precision float.
static inline Float32 float32_pack(Flag sign, Sint16 exp, Uint32 sig) {
  return (Float32){(((Uint32)sign) << 31) + (((Uint32)exp) << 23) + sig};
}

// Take two single-precision float values, one which must be NaN, and produce
// the correct NaN result, taking care to raise an invalid exception when either
// is a signaling NaN.
static Float32 float32_propagate_nan(Context *ctx, Float32 a, Float32 b) {
  const Flag a_is_nan = float32_is_nan(a);
  const Flag a_is_snan = float32_is_snan(a);
  const Flag b_is_nan = float32_is_nan(b);
  const Flag b_is_snan = float32_is_snan(b);
  a.bits |= LIT32(0x00400000);
  b.bits |= LIT32(0x00400000);
  if (a_is_snan | b_is_snan) {
    context_raise(ctx, EXCEPTION_INVALID);
  }
  if (a_is_nan) {
    return (a_is_snan & b_is_nan) ? b : a;
  }
  return b;
}

static Float32 float32_round_and_pack(Context *ctx, Flag sign, Sint32 exp, Uint32 sig) {
  const Round rounding_mode = ctx->round;
  const Flag round_nearest_even = rounding_mode == ROUND_NEAREST_EVEN;
  Sint8 round_increment = 0x40;
  if (!round_nearest_even) {
    if (rounding_mode == ROUND_TO_ZERO) {
      round_increment = 0;
    } else {
      round_increment = 0x7f;
      if (sign) {
        if (rounding_mode == ROUND_UP) {
          round_increment = 0;
        }
      } else {
        if (rounding_mode == ROUND_DOWN) {
          round_increment = 0;
        }
      }
    }
  }

  Sint8 round_bits = sig & 0x7f;

  if (round_bits) {
    ctx->roundings++;
  }
  
  if (0xfd <= (Uint16)exp) {
    if ((0xfd < exp) || ((exp == 0xfd) && ((Sint32)(sig + round_increment) < 0))) {
      context_raise(ctx, EXCEPTION_OVERFLOW | EXCEPTION_INEXACT);
      const Float32 pack = float32_pack(sign, 0xff, 0);
      return (Float32){pack.bits - (round_increment == 0 ? 0 : 1)};
    }
    if (exp < 0) {
      const Flag is_tiny = (ctx->tininess == TININESS_BEFORE_ROUNDING)
        || (exp < -1)
        || (sig + round_increment < LIT32(0x80000000));
      sig = rshr32(sig, -exp);
      exp = 0;
      round_bits = sig & 0x7f;
      if (is_tiny && round_bits) {
        context_raise(ctx, EXCEPTION_UNDERFLOW);
      }
    }
  }
  if (round_bits) {
    context_raise(ctx, EXCEPTION_INEXACT);
  }
  sig = (sig + round_increment) >> 7;
  sig &= ~(((round_bits ^ 0x40) == 0) & round_nearest_even);
  return float32_pack(sign, sig == 0 ? 0 : exp, sig);
}

static inline Float32 float32_normalize_round_and_pack(Context *ctx, Flag sign, Sint16 exp, Uint32 sig) {
  const Sint8 shift = count_leading_zeros_u32(sig) - 1;
  return float32_round_and_pack(ctx, sign, exp - shift, sig << shift);
}

static inline Normal32 float32_normalize_subnormal(Uint32 sig) {
  const Sint8 shift = count_leading_zeros_u32(sig) - 8;
  return (Normal32){sig << shift, 1 - shift};
}

static Float32 float32_add_sig(Context *ctx, Float32 a, Float32 b, Flag sign) {
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a) << 6;
  Uint32 b_sig = float32_fract(b) << 6;
  Sint16 exp_diff = a_exp - b_exp;

  Sint16 exp;
  Uint32 sig;
  if (0 < exp_diff) {
    if (a_exp == 0xff) {
      return a_sig ? float32_propagate_nan(ctx, a, b) : a;
    }
    if (b_exp == 0) {
      exp_diff--;
    } else {
      b_sig |= LIT32(0x20000000);
    }
    b_sig = rshr32(b_sig, exp_diff);
    exp = a_exp;
  } else if (exp_diff < 0) {
    if (b_exp == 0xff) {
      if (b_sig) {
        return float32_propagate_nan(ctx, a, b);
      }
      return float32_pack(sign, 0xff, 0);
    }
    if (a_exp == 0) {
      exp_diff++;
    } else {
      a_sig |= LIT32(0x20000000);
    }
    a_sig = rshr32(a_sig, -exp_diff);
    exp = b_exp;
  } else {
    if (a_exp == 0xff) {
      return (a_sig | b_sig) ? float32_propagate_nan(ctx, a, b) : a;
    }
    if (a_exp == 0) {
      return float32_pack(sign, 0, (a_sig + b_sig) >> 6);
    }
    sig = LIT32(0x40000000) + a_sig + b_sig;
    exp = a_exp;
    goto round_and_pack;
  }
  a_sig |= LIT32(0x20000000);
  sig = (a_sig + b_sig) << 1;
  exp--;
  if ((Sint32)sig < 0) {
    sig = a_sig + b_sig;
    exp++;
  }

round_and_pack:
  return float32_round_and_pack(ctx, sign, exp, sig);
}

static Float32 float32_sub_sig(Context *ctx, Float32 a, Float32 b, Flag sign) {
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a) << 7;
  Uint32 b_sig = float32_fract(b) << 7;
  Sint16 exp_diff = a_exp - b_exp;

  // Needed because goto crosses initialization.
  Sint16 exp;
  Uint32 sig;
  if (0 < exp_diff) {
    goto a_exp_bigger;
  }
  if (exp_diff < 0) {
    goto b_exp_bigger;
  }
  if (a_exp == 0xff) {
    if (a_sig | b_sig) {
      return float32_propagate_nan(ctx, a, b);
    }
    context_raise(ctx, EXCEPTION_INVALID);
    return FLOAT32_NAN;
  }
  if (a_exp == 0) {
    a_exp = 1;
    b_exp = 1;
  }
  if (b_sig < a_sig) {
    goto a_bigger;
  }
  if (a_sig < b_sig) {
    goto b_bigger;
  }
  return float32_pack(ctx->round == ROUND_DOWN, 0, 0);
b_exp_bigger:
  if (b_exp == 0xff) {
    return b_sig
      ? float32_propagate_nan(ctx, a, b)
      : float32_pack(sign ^ 1, 0xff, 0);
  }
  if (a_exp == 0) {
    exp_diff++;
  } else {
    a_sig |= LIT32(0x40000000);
  }
  a_sig = rshr32(a_sig, -exp_diff);
  b_sig |=  LIT32(0x40000000);
b_bigger:
  sig = b_sig - a_sig;
  exp = b_exp;
  sign ^= 1;
  goto normalize_round_and_pack;
a_exp_bigger:
  if (a_exp == 0xff) {
    return a_sig ? float32_propagate_nan(ctx, a, b) : a;
  }
  if (b_exp == 0) {
    exp_diff--;
  } else {
    b_sig |=  LIT32(0x40000000);
  }
  b_sig = rshr32(b_sig, exp_diff);
  a_sig |=  LIT32(0x40000000);
a_bigger:
  sig = a_sig - b_sig;
  exp = a_exp;
normalize_round_and_pack:
  exp--;
  return float32_normalize_round_and_pack(ctx, sign, exp, sig);
}

Float32 float32_add(Context *ctx, Float32 a, Float32 b) {
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  return a_sign == b_sign
    ? float32_add_sig(ctx, a, b, a_sign)
    : float32_sub_sig(ctx, a, b, a_sign);
}

Float32 float32_sub(Context *ctx, Float32 a, Float32 b) {
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  return a_sign == b_sign
    ? float32_sub_sig(ctx, a, b, a_sign)
    : float32_add_sig(ctx, a, b, a_sign);
}

Float32 float32_mul(Context *ctx, Float32 a, Float32 b) {
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a);
  Uint32 b_sig = float32_fract(b);
  Flag a_sign = float32_sign(a);
  Flag b_sign = float32_sign(b);
  Flag sign = a_sign ^ b_sign;
  if (a_exp == 0xff) {
    if (a_sig || (b_exp == 0xff && b_sig)) {
      return float32_propagate_nan(ctx, a, b);
    }
    if ((b_exp | b_sig) == 0) {
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT32_NAN;
    }
    return float32_pack(sign, 0xff, 0);
  }
  if (b_exp == 0xff) {
    if (b_sig) {
      return float32_propagate_nan(ctx, a, b);
    }
    if ((a_exp | a_sig) == 0) {
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT32_NAN;
    }
    return float32_pack(sign, 0xff, 0);
  }
  if (a_exp == 0) {
    if (a_sig == 0) {
      return float32_pack(sign, 0, 0);
    }
    const Normal32 n = float32_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  if (b_exp == 0) {
    if (b_sig == 0) {
      return float32_pack(sign, 0, 0);
      const Normal32 n = float32_normalize_subnormal(b_sig);
      b_exp = n.exp;
      b_sig = n.sig;
    }
  }
  Sint16 exp = a_exp + b_exp - 0x7f;
  a_sig = (a_sig | LIT32(0x00800000)) << 7;
  b_sig = (b_sig | LIT32(0x00800000)) << 8;
  // Compute with 64-bit mul, truncate to 32-bit.
  Uint32 sig = rshr64((Uint64)a_sig * b_sig, 32);
  if (0 <= (Sint32)(sig << 1)) {
    sig <<= 1;
    exp--;
  }
  return float32_round_and_pack(ctx, sign, exp, sig);
}

Float32 float32_div(Context *ctx, Float32 a, Float32 b) {
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a);
  Uint32 b_sig = float32_fract(b);
  Flag a_sign = float32_sign(a);
  Flag b_sign = float32_sign(b);
  Flag sign = a_sign ^ b_sign;
  if (a_exp == 0xff) {
    if (a_sig) {
      return float32_propagate_nan(ctx, a, b);
    }
    if (b_exp == 0xff) {
      if (b_sig) {
        return float32_propagate_nan(ctx, a, b);
      }
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT32_NAN;
    }
    return float32_pack(sign, 0xff, 0);
  }
  if (b_exp == 0xff) {
    return b_sig
      ? float32_propagate_nan(ctx, a, b)
      : float32_pack(sign, 0, 0);
  }
  if (b_exp == 0) {
    if (b_sig == 0) {
      if ((a_exp | a_sig) == 0) {
        context_raise(ctx, EXCEPTION_INVALID);
        return FLOAT32_NAN;
      }
      context_raise(ctx, EXCEPTION_DIVIDE_BY_ZERO);
      return float32_pack(sign, 0xff, 0);
    }
    const Normal32 n = float32_normalize_subnormal(b_sig);
    b_exp = n.exp;
    b_sig = n.sig;
  }
  if (a_exp == 0) {
    if (a_sig == 0) {
      return float32_pack(sign, 0, 0);
    }
    const Normal32 n = float32_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  Sint16 exp = a_exp - b_exp + 0x7d;
  a_sig = (a_sig | LIT32(0x00800000)) << 7;
  b_sig = (b_sig | LIT32(0x00800000)) << 8;
  if (b_sig <= a_sig + b_sig) {
    a_sig >>= 1;
    exp++;
  }
  // Compute with 64-bit divide, truncate to 32-bit.
  Uint32 sig = (((Uint64)a_sig) << 32) / b_sig;
  if ((sig & 0x3f) == 0) {
    sig |= (Uint64)b_sig * sig != ((Uint64)a_sig) << 32;
  }
  return float32_round_and_pack(ctx, sign, exp, sig);
}

// a == b
Flag float32_eq(Context *ctx, Float32 a, Float32 b) {
  if ((float32_exp(a) == 0xff && float32_fract(a)) ||
      (float32_exp(b) == 0xff && float32_fract(b)))
  {
    if (float32_is_snan(a) || float32_is_snan(b)) {
      context_raise(ctx, EXCEPTION_INVALID);
    }
    return 0;
  }
  return a.bits == b.bits || (Uint32)((a.bits | b.bits) << 1) == 0;
}

// a <= b
Flag float32_lte(Context *ctx, Float32 a, Float32 b) {
  if ((float32_exp(a) == 0xff && float32_fract(a)) ||
      (float32_exp(b) == 0xff && float32_fract(b)))
  {
    context_raise(ctx, EXCEPTION_INVALID);
    return 0;
  }

  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);

  if (a_sign != b_sign) {
    return a_sign || (Uint32)((a.bits | b.bits) << 1) == 0;
  }

  return a.bits == b.bits || (a_sign ^ (a.bits < b.bits));
}

// a < b
Flag float32_lt(Context *ctx, Float32 a, Float32 b) {
  if ((float32_exp(a) == 0xff && float32_fract(a)) ||
      (float32_exp(b) == 0xff && float32_fract(b)))
  {
    context_raise(ctx, EXCEPTION_INVALID);
    return 0;
  }

  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);

  if (a_sign != b_sign) {
    return a_sign && (Uint32)((a.bits | b.bits) << 1) != 0;
  }

  return a.bits != b.bits && (a_sign ^ (a.bits< b.bits));
}

// The others are implemented with a not on the flag. IEEE 754 requires
// these identities be held, so this is safe.
// a != b => !(a == b)
Flag float32_ne(Context *ctx, Float32 a, Float32 b) {
  return !float32_eq(ctx, a, b);
}

// a >= b => !(a < b)
Flag float32_gte(Context *ctx, Float32 a, Float32 b) {
  return !float32_lt(ctx, a, b);
}

// a > b  => !(a <= b)
Flag float32_gt(Context *ctx, Float32 a, Float32 b) {
  return !float32_lte(ctx, a, b);
}

Float32 float32_from_sint32(Context *ctx, Sint32 a) {
  if (a == 0) {
    return (Float32){0};
  }
  if (a == (Sint32)0x80000000) {
    return float32_pack(1, 0x9e, 0);
  }
  const Flag sign = a < 0;
  return float32_normalize_round_and_pack(ctx, sign, 0x9c, sign ? -a : a);
}

Flag float32_is_any_nan(Float32 a) {
  return (a.bits & LIT32(0x7fffffff)) > LIT32(0x7f800000);
}

// Kernel functions.
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