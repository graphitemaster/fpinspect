#include "float32.h"
#include "float64.h"

void context_init(Context* context) {
  context->exceptions = NULL;
  context->operations = NULL;
  context->roundings = 0;
}

void context_free(Context* context) {
  array_free(context->exceptions);
  array_free(context->operations);
}

void context_copy(Context* dst, const Context *src) {
  context_init(dst);
  dst->round = src->round;
  dst->tininess = src->tininess;
}

bool context_raise(Context *context, Exception exception) {
  return array_push(context->exceptions, exception);
}

static Float32 canonical_nan_to_float32(CanonicalNaN nan) {
  return (Float32){(((Uint32)nan.sign) << 31) | LIT32(0x7FC00000) | (nan.hi >> 41)};
}

static Float64 canonical_nan_to_float64(CanonicalNaN nan) {
  return (Float64){(((Uint64)nan.sign) << 63) | LIT64(0x7FF8000000000000) | (nan.hi >> 12)};
}

Float32 float64_to_float32(Context *ctx, Float64 a) {
  Uint64 a_sig = float64_fract(a);
  Sint16 a_exp = float64_exp(a);
  Flag a_sign = float64_sign(a);
  if (a_exp == 0x7ff) {
    return a_sig 
      ? canonical_nan_to_float32(float64_to_canonical_nan(ctx, a))
      : float32_pack(a_sign, 0xff, 0);
  }
  a_sig = rshr64(a_sig, 22);
  Uint32 sig = a_sig;
  if (a_exp || sig) {
    sig |= LIT32(0x40000000);
    a_exp -= 0x381;
  }
  return float32_round_and_pack(ctx, a_sign, a_exp, sig);
}

Float64 float32_to_float64(Context *ctx, Float32 a) {
  Uint32 a_sig = float32_fract(a);
  Sint16 a_exp = float32_exp(a);
  Flag a_sign = float32_sign(a);
  if (a_exp == 0xff) {
    return a_sig 
      ? canonical_nan_to_float64(float32_to_canonical_nan(ctx, a))
      : float64_pack(a_sign, 0x7ff, 0);
  }
  if (a_exp == 0) {
    if (a_sig == 0) {
      return float64_pack(a_sign, 0, 0);
    }
    Normal32 normal = float32_normalize_subnormal(a_sig);
    a_exp = normal.exp;
    a_sig = normal.sig;
    a_exp--;
  }
  return float64_pack(a_sign, a_exp + 0x380, (Uint64)a_sig << 29);
}