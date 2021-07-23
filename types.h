#ifndef TYPES_H
#define TYPES_H
#include <stdint.h> // u?int{8,16,32,64}_t
#include <stdbool.h> // bool, true, false
#include <stddef.h> // size_t

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;

typedef bool Bool;

typedef size_t Size;

#endif // TYPES_H