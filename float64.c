#include "float64.h"
#include "uint128.h"

// Count leading zero bits.
static inline Sint8 count_leading_zeros_u64(Uint64 a) {
  return a == 0 ? 64 : __builtin_clzl(a);
}

// Take two double-precision float values, one which must be NaN, and produce
// the correct NaN result, taking care to raise an invalid exception when either
// is a signaling NaN.
static Float64 float64_propagate_nan(Context *ctx, Float64 a, Float64 b) {
  const Flag a_is_nan = float64_is_nan(a);
  const Flag a_is_snan = float64_is_snan(a);
  const Flag b_is_nan = float64_is_nan(b);
  const Flag b_is_snan = float64_is_snan(b);
  a.bits |= LIT64(0x0008000000000000);
  b.bits |= LIT64(0x0008000000000000);
  if (a_is_snan | b_is_snan) {
    context_raise(ctx, EXCEPTION_INVALID);
  }
  if (a_is_nan) {
    return (a_is_snan & b_is_nan) ? b : a;
  }
  return b;
}

CanonicalNaN float64_to_canonical_nan(Context* ctx, Float64 a) {
  if (float64_is_snan(a)) {
    context_raise(ctx, EXCEPTION_INVALID);
  }
  CanonicalNaN nan;
  nan.sign = a.bits >> 63;
  nan.lo = 0;
  nan.hi = a.bits << 12; 
  return nan;
}

Float64 float64_round_and_pack(Context *ctx, Flag sign, Sint32 exp, Uint64 sig) {
  const Round rounding_mode = ctx->round;
  const Flag round_nearest_even = rounding_mode == ROUND_NEAREST_EVEN;
  Sint16 round_increment = 0x200;
  if (!round_nearest_even) {
    if (rounding_mode == ROUND_TO_ZERO) {
      round_increment = 0;
    } else {
      round_increment = 0x3ff;
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

  Sint16 round_bits = sig & 0x3ff;

  if (round_bits) {
    ctx->roundings++;
  }
  
  if (0x7fd <= (Uint16)exp) {
    if ((0x7fd < exp) || ((exp == 0x7fd) && ((Sint64)(sig + round_increment) < 0))) {
      context_raise(ctx, EXCEPTION_OVERFLOW | EXCEPTION_INEXACT);
      const Float64 pack = float64_pack(sign, 0x7ff, 0);
      return (Float64){pack.bits - (round_increment == 0 ? 0 : 1)};
    }
    if (exp < 0) {
      const Flag is_tiny = (ctx->tininess == TININESS_BEFORE_ROUNDING)
        || (exp < -1)
        || (sig + round_increment < LIT64(0x8000000000000000));
      sig = rshr64(sig, -exp);
      exp = 0;
      round_bits = sig & 0x3ff;
      if (is_tiny && round_bits) {
        context_raise(ctx, EXCEPTION_UNDERFLOW);
      }
    }
  }
  if (round_bits) {
    context_raise(ctx, EXCEPTION_INEXACT);
  }
  sig = (sig + round_increment) >> 10;
  sig &= ~(((round_bits ^ 0x200) == 0) & round_nearest_even);
  return float64_pack(sign, sig == 0 ? 0 : exp, sig);
}

static inline Float64 float64_normalize_round_and_pack(Context *ctx, Flag sign, Sint16 exp, Uint64 sig) {
  const Sint8 shift = count_leading_zeros_u64(sig) - 1;
  return float64_round_and_pack(ctx, sign, exp - shift, sig << shift);
}

Normal64 float64_normalize_subnormal(Uint64 sig) {
  const Sint8 shift = count_leading_zeros_u64(sig) - 11;
  return (Normal64){sig << shift, 1 - shift};
}

static Float64 float64_add_sig(Context *ctx, Float64 a, Float64 b, Flag sign) {
  Sint16 a_exp = float64_exp(a);
  Sint16 b_exp = float64_exp(b);
  Uint64 a_sig = float64_fract(a) << 9;
  Uint64 b_sig = float64_fract(b) << 9;
  Sint16 exp_diff = a_exp - b_exp;

  Sint16 exp;
  Uint64 sig;
  if (0 < exp_diff) {
    if (a_exp == 0x7ff) {
      return a_sig ? float64_propagate_nan(ctx, a, b) : a;
    }
    if (b_exp == 0) {
      exp_diff--;
    } else {
      b_sig |= LIT64(0x2000000000000000);
    }
    b_sig = rshr64(b_sig, exp_diff);
    exp = a_exp;
  } else if (exp_diff < 0) {
    if (b_exp == 0x7ff) {
      return b_sig
        ? float64_propagate_nan(ctx, a, b)
        : float64_pack(sign, 0x7ff, 0);
    }
    if (a_exp == 0) {
      exp_diff++;
    } else {
      a_sig |= LIT64(0x2000000000000000);
    }
    a_sig = rshr64(a_sig, -exp_diff);
    exp = b_exp;
  } else {
    if (a_exp == 0x7ff) {
      return (a_sig | b_sig) ? float64_propagate_nan(ctx, a, b) : a;
    }
    if (a_exp == 0) {
      return float64_pack(sign, 0, (a_sig + b_sig) >> 9);
    }
    sig = LIT64(0x4000000000000000) + a_sig + b_sig;
    exp = a_exp;
    goto round_and_pack;
  }
  a_sig |= LIT64(0x2000000000000000);
  sig = (a_sig + b_sig) << 1;
  exp--;
  if ((Sint64)sig < 0) {
    sig = a_sig + b_sig;
    exp++;
  }
round_and_pack:
  return float64_round_and_pack(ctx, sign, exp, sig);
}

static Float64 float64_sub_sig(Context *ctx, Float64 a, Float64 b, Flag sign) {
  Sint16 a_exp = float64_exp(a);
  Sint16 b_exp = float64_exp(b);
  Uint64 a_sig = float64_fract(a) << 10;
  Uint64 b_sig = float64_fract(b) << 10;
  Sint16 exp_diff = a_exp - b_exp;

  // Needed because goto crosses initialization.
  Sint16 exp;
  Uint64 sig;
  if (0 < exp_diff) {
    goto a_exp_bigger;
  }
  if (exp_diff < 0) {
    goto b_exp_bigger;
  }
  if (a_exp == 0x7ff) {
    if (a_sig | b_sig) {
      return float64_propagate_nan(ctx, a, b);
    }
    context_raise(ctx, EXCEPTION_INVALID);
    return FLOAT64_NAN;
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
  return float64_pack(ctx->round == ROUND_DOWN, 0, 0);
b_exp_bigger:
  if (b_exp == 0x7ff) {
    return b_sig
      ? float64_propagate_nan(ctx, a, b)
      : float64_pack(sign ^ 1, 0xff, 0);
  }
  if (a_exp == 0) {
    exp_diff++;
  } else {
    a_sig |= LIT64(0x4000000000000000);
  }
  a_sig = rshr64(a_sig, -exp_diff);
  b_sig |= LIT64(0x4000000000000000);
b_bigger:
  sig = b_sig - a_sig;
  exp = b_exp;
  sign ^= 1;
  goto normalize_round_and_pack;
a_exp_bigger:
  if (a_exp == 0x7ff) {
    return a_sig ? float64_propagate_nan(ctx, a, b) : a;
  }
  if (b_exp == 0) {
    exp_diff--;
  } else {
    b_sig |= LIT64(0x4000000000000000);
  }
  b_sig = rshr64(b_sig, exp_diff);
  a_sig |= LIT64(0x4000000000000000);
a_bigger:
  sig = a_sig - b_sig;
  exp = a_exp;
normalize_round_and_pack:
  exp--;
  return float64_normalize_round_and_pack(ctx, sign, exp, sig);
}

Float64 float64_add(Context *ctx, Float64 a, Float64 b) {
  const Flag a_sign = float64_sign(a);
  const Flag b_sign = float64_sign(b);
  return a_sign == b_sign
    ? float64_add_sig(ctx, a, b, a_sign)
    : float64_sub_sig(ctx, a, b, b_sign);
}

Float64 float64_sub(Context *ctx, Float64 a, Float64 b) {
  const Flag a_sign = float64_sign(a);
  const Flag b_sign = float64_sign(b);
  return a_sign == b_sign
    ? float64_sub_sig(ctx, a, b, a_sign)
    : float64_add_sig(ctx, a, b, a_sign);
}

Float64 float64_mul(Context *ctx, Float64 a, Float64 b) {
  Sint16 a_exp = float64_exp(a);
  Sint16 b_exp = float64_exp(b);
  Uint64 a_sig = float64_fract(a);
  Uint64 b_sig = float64_fract(b);
  Flag a_sign = float64_sign(a);
  Flag b_sign = float64_sign(b);
  Flag sign = a_sign ^ b_sign;
  if (a_exp == 0x7ff) {
    if (a_sig || (b_exp == 0x7ff && b_sig)) {
      return float64_propagate_nan(ctx, a, b);
    }
    if ((b_exp | b_sig) == 0) {
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT64_NAN;
    }
    return float64_pack(sign, 0x7ff, 0);
  }
  if (b_exp == 0x7ff) {
    if (b_sig) {
      return float64_propagate_nan(ctx, a, b);
    }
    if ((a_exp | a_sig) == 0) {
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT64_NAN;
    }
    return float64_pack(sign, 0x7ff, 0);
  }
  if (a_exp == 0) {
    if (a_sig == 0) {
      return float64_pack(sign, 0, 0);
    }
    const Normal64 n = float64_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  if (b_exp == 0) {
    if (b_sig == 0) {
      return float64_pack(sign, 0, 0);
      const Normal64 n = float64_normalize_subnormal(b_sig);
      b_exp = n.exp;
      b_sig = n.sig;
    }
  }
  Sint16 exp = a_exp + b_exp - 0x3ff;
  a_sig = (a_sig | LIT64(0x0010000000000000)) << 10;
  b_sig = (b_sig | LIT64(0x0010000000000000)) << 11;

  // Compute with 128-bit mul, truncate to 64-bit.
  Uint128 mul = uint128_mul64x64(a_sig, b_sig);
  mul.z0 |= mul.z1 != 0;
  if (0 <= (Sint64)(mul.z0 << 1)) {
    mul.z0 <<= 1;
    exp--;
  }
  return float64_round_and_pack(ctx, sign, exp, mul.z0);
}

Float64 float64_div(Context *ctx, Float64 a, Float64 b) {
  Sint16 a_exp = float64_exp(a);
  Sint16 b_exp = float64_exp(b);
  Uint64 a_sig = float64_fract(a);
  Uint64 b_sig = float64_fract(b);
  Flag a_sign = float64_sign(a);
  Flag b_sign = float64_sign(b);
  Flag sign = a_sign ^ b_sign;
  if (a_exp == 0x7ff) {
    if (a_sig) {
      return float64_propagate_nan(ctx, a, b);
    }
    if (b_exp == 0x7ff) {
      if (b_sig) {
        return float64_propagate_nan(ctx, a, b);
      }
      context_raise(ctx, EXCEPTION_INVALID);
      return FLOAT64_NAN;
    }
    return float64_pack(sign, 0xff, 0);
  }
  if (b_exp == 0x7ff) {
    return b_sig
      ? float64_propagate_nan(ctx, a, b)
      : float64_pack(sign, 0, 0);
  }
  if (b_exp == 0) {
    if (b_sig == 0) {
      if ((a_exp | a_sig) == 0) {
        context_raise(ctx, EXCEPTION_INVALID);
        return FLOAT64_NAN;
      }
      context_raise(ctx, EXCEPTION_DIVIDE_BY_ZERO);
      return float64_pack(sign, 0xff, 0);
    }
    const Normal64 n = float64_normalize_subnormal(b_sig);
    b_exp = n.exp;
    b_sig = n.sig;
  }
  if (a_exp == 0) {
    if (a_sig == 0) {
      return float64_pack(sign, 0, 0);
    }
    const Normal64 n = float64_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  Sint16 exp = a_exp - b_exp + 0x7d;
  a_sig = (a_sig | LIT64(0x0010000000000000)) << 10;
  b_sig = (b_sig | LIT64(0x0010000000000000)) << 11;
  if (b_sig <= a_sig + a_sig) {
    a_sig >>= 1;
    exp++;
  }

  Uint64 sig = uint128_div128x64((Uint128){a_sig, 0}, b_sig);
  if ((sig & 0x1ff) <= 2) {
    Uint128 term = uint128_mul64x64(b_sig, sig);
    Uint128 rem = uint128_sub((Uint128){a_sig, 0}, term);
    while ((Sint64)rem.z0 < 0) {
      sig--;
      rem = uint128_add(rem, (Uint128){0, b_sig});
    }
    sig |= rem.z1 != 0;
  }

  return float64_round_and_pack(ctx, sign, exp, sig);
}