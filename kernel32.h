#ifndef KERNEL32_H
#define KERNEL32_H
#include "real32.h"

Float32 float32_floor(Context*, Float32);
Float32 float32_ceil(Context*, Float32);
Float32 float32_trunc(Context*, Float32);
Float32 float32_sqrt(Context*, Float32);
Float32 float32_abs(Context*, Float32);
Float32 float32_copysign(Context*, Float32, Float32);
Float32 float32_max(Context*, Float32, Float32);
Float32 float32_min(Context*, Float32, Float32);

#endif