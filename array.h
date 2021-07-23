#ifndef ARRAY_H
#define ARRAY_H
#include "types.h"

typedef struct Array Array;

struct Array {
  Size size;
  Size capacity;
};

#define ARRAY(T) T*

#define array_meta(array) \
  ((Array*)(((Uint8*)(array)) - sizeof(Array)))

// grow [array] by [expand] elements.
#define array_try_grow(array, expand) \
  (((!(array) || array_meta(array)->size + (expand) >= array_meta(array)->capacity)) \
    ? array_grow(((void **)&(array)), (expand), sizeof(*(array))) \
    : true)

// push [value] into [array]
#define array_push(array, value) \
  (array_try_grow((array), 1) \
    ? ((array)[array_meta(array)->size++] = (value), true) \
    : false)

// free [array]
#define array_free(array) \
  ((void)((array) ? (array_delete((void*)(array)), (array) = 0) : 0))

// size of [array]
#define array_size(array) \
  ((array) ? array_meta(array)->size : 0)

Bool array_grow(void**, Size, Size);
void array_delete(void*);

#endif // ARRAY_H