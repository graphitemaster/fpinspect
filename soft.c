#include "soft.h"

void context_init(Context* context) {
  context->exceptions = NULL;
  context->roundings = 0;
}

void context_free(Context* context) {
  array_free(context->exceptions);
}

bool context_raise(Context *context, Exception exception) {
  return array_push(context->exceptions, exception);
}