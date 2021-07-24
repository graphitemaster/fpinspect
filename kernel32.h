#ifndef KERNEL32_H
#define KERNEL32_H
#include "float32.h"

Float32 float32_floor(Context*, Float32);
Float32 float32_ceil(Context*, Float32);
Float32 float32_trunc(Context*, Float32);
Float32 float32_sqrt(Context*, Float32);
Float32 float32_abs(Context*, Float32);
Float32 float32_copysign(Context*, Float32, Float32);
Float32 float32_max(Context*, Float32, Float32);
Float32 float32_min(Context*, Float32, Float32);

Float32 float32_cosd(Context *ctx, Float64 x); // cos approximation for test.

#endif