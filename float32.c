#include "float32.h"

// Count leading zero bits.
static inline Sint8 count_leading_zeros_u32(Uint32 a) {
  return a == 0 ? 32 : __builtin_clz(a);
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

CanonicalNaN float32_to_canonical_nan(Context* ctx, Float32 a) {
  if (float32_is_snan(a)) {
    context_raise(ctx, EXCEPTION_INVALID);
  }
  CanonicalNaN nan;
  nan.sign = a.bits >> 31;
  nan.lo = 0;
  nan.hi = (Uint64)a.bits << 41;
  return nan;
}

Float32 float32_round_and_pack(Context *ctx, Flag sign, Sint32 exp, Uint32 sig) {
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

Normal32 float32_normalize_subnormal(Uint32 sig) {
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
  array_push(ctx->operations, OPERATION_ADD);
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  return a_sign == b_sign
    ? float32_add_sig(ctx, a, b, a_sign)
    : float32_sub_sig(ctx, a, b, a_sign);
}

Float32 float32_sub(Context *ctx, Float32 a, Float32 b) {
  array_push(ctx->operations, OPERATION_SUB);
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  return a_sign == b_sign
    ? float32_sub_sig(ctx, a, b, a_sign)
    : float32_add_sig(ctx, a, b, a_sign);
}

Float32 float32_mul(Context *ctx, Float32 a, Float32 b) {
  array_push(ctx->operations, OPERATION_MUL);
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a);
  Uint32 b_sig = float32_fract(b);
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  const Flag sign = a_sign ^ b_sign;
  Uint32 mag_bits = 0;
  if (a_exp == 0xff) {
    if (a_sig || (b_exp == 0xff && b_sig)) goto propagate_nan;
    mag_bits = b_exp | b_sig;
    goto infinity;
  }
  if (b_exp == 0xff) {
    if (b_sig) goto propagate_nan;
    mag_bits = a_exp | a_sig;
    goto infinity;
  }
  if (a_exp == 0) {
    if (a_sig == 0) goto zero;
    const Normal32 n = float32_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  if (b_exp == 0) {
    if (b_sig == 0) goto zero;
    const Normal32 n = float32_normalize_subnormal(b_sig);
    b_exp = n.exp;
    b_sig = n.sig;
  }
  Sint16 exp = a_exp + b_exp - 0x7f;
  a_sig = (a_sig | LIT32(0x00800000)) << 7;
  b_sig = (b_sig | LIT32(0x00800000)) << 8;

  // Compute with 64-bit mul, truncate to 32-bit.
  Uint32 sig = rshr64((Uint64)a_sig * b_sig, 32);
  if (sig < LIT32(0x40000000)) {
    exp--;
    sig <<= 1;
  }
  return float32_round_and_pack(ctx, sign, exp, sig);
propagate_nan:
  return float32_propagate_nan(ctx, a, b);
infinity:
  if (!mag_bits) {
    context_raise(ctx, EXCEPTION_INVALID);
    return FLOAT32_NAN;
  } else {
    return float32_pack(sign, 0xff, 0);
  }
zero:
  return float32_pack(sign, 0, 0);
}

Float32 float32_div(Context *ctx, Float32 a, Float32 b) {
  array_push(ctx->operations, OPERATION_DIV);
  Sint16 a_exp = float32_exp(a);
  Sint16 b_exp = float32_exp(b);
  Uint32 a_sig = float32_fract(a);
  Uint32 b_sig = float32_fract(b);
  const Flag a_sign = float32_sign(a);
  const Flag b_sign = float32_sign(b);
  const Flag sign = a_sign ^ b_sign;
  if (a_exp == 0xff) {
    if (a_sig) goto propagate_nan;
    if (b_exp == 0xff) {
      if (b_sig) goto propagate_nan;
      goto invalid;
    }
    goto infinity;
  }
  if (b_exp == 0xff) {
    if (b_sig) goto propagate_nan;
    goto zero;
  }
  if (b_exp == 0) {
    if (b_sig == 0) {
      if ((a_exp | a_sig) == 0) goto invalid;
      context_raise(ctx, EXCEPTION_INFINITE);
      goto infinity;
    }
    const Normal32 n = float32_normalize_subnormal(b_sig);
    b_exp = n.exp;
    b_sig = n.sig;
  }
  if (a_exp == 0) {
    if (a_sig == 0) goto zero;
    const Normal32 n = float32_normalize_subnormal(a_sig);
    a_exp = n.exp;
    a_sig = n.sig;
  }
  Sint16 exp = a_exp - b_exp + 0x7e;
  a_sig = (a_sig | LIT32(0x00800000));
  b_sig = (b_sig | LIT32(0x00800000));
  // Use 64-bit divide for 32-bit significand.
  Uint64 a_sig_64;
  if (a_sig < b_sig) {
    exp--;
    a_sig_64 = (Uint64)a_sig << 31;
  } else {
    a_sig_64 = (Uint64)a_sig << 30;
  }
  Uint32 sig = a_sig_64 / b_sig;
  if (!(sig & 0x3f)) {
    sig |= ((Uint64)b_sig * sig != a_sig_64);
  }
  return float32_round_and_pack(ctx, sign, exp, sig);
propagate_nan:
  return float32_propagate_nan(ctx, a, b);
invalid:
  context_raise(ctx, EXCEPTION_INVALID);
  return FLOAT32_NAN;
infinity:
  return float32_pack(sign, 0xff, 0);
zero:
  return float32_pack(sign, 0, 0);
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