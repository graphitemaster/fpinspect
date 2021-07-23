#include "uint128.h"

Uint128 uint128_mul64(Uint64 a, Uint64 b) {
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