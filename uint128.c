#include "uint128.h"

Uint128 uint128_mul64x64(Uint64 a, Uint64 b) {
  const Uint32 al = a;
  const Uint32 ah = a >> 32;
  const Uint32 bl = b;
  const Uint32 bh = b >> 32;
  Uint64 z0, z1;
  Uint64 ma, mb;
  z1 = (Uint64)al * bl;
  ma = (Uint64)al * bh;
  mb = (Uint64)ah * bl;
  z0 = (Uint64)ah * bh;
  ma += mb;
  z0 += ((Uint64)(ma < mb) << 32) + (ma >> 32);
  ma <<= 32;
  z1 += ma;
  z0 += z1 < ma;
  return (Uint128){z0, z1};
}

Uint64 uint128_div128x64(Uint128 a, Uint64 b) {
  if (b <= a.z0) {
    return LIT64(0xFFFFFFFFFFFFFFFF);
  }

  Uint64 b0 = b >> 32;
  Uint64 b1;

  Uint64 z = (b0 << 32 <= a.z0)
    ? LIT64(0xFFFFFFFF00000000) : (a.z0 / b0) << 32;

  Uint128 mul = uint128_mul64x64(b, z);
  Uint128 rem = uint128_sub(a, mul);

  while ((Sint64)rem.z0 < 0) {
    z -= LIT64(0x100000000);
    b1 = b << 32;
    rem = uint128_add(rem, (Uint128){b0, b1});
  }
  rem.z0 = (rem.z0 << 32) | (rem.z1 >> 32);

  z |= (b0 << 32 <= rem.z0) ? LIT32(0xffffffff) : rem.z0 / b0;

  return z;
}