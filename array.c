#include <stdlib.h> // malloc, realloc, free.

#include "array.h"

Bool array_grow(void **array, Size expand, Size type_size) {
  Array *meta = array_meta(*array);
  Size count = 0;
  void *data = NULL;

  if (*array) {
    count = 2 * meta->capacity + expand;
    data = realloc(meta, type_size * count + sizeof *meta);
    if (!data) {
      return false;
    }
  } else {
    count = expand + 1;
    data = malloc(type_size * count + sizeof *meta);
    if (!data) {
      return false;
    }
    ((Array*)data)->size = 0;
  }

  meta = (Array*)data;
  meta->capacity = count;

  *array = meta + 1;

  return true;
}

void array_delete(void *array) {
  free(array_meta(array));
}