#ifndef EVAL_H
#define EVAL_H
#include "real32.h"

typedef struct Expression Expression;

Bool expr_parse(Expression**, const char*);
Real32 expr_eval32(Context*, Expression*);
void expr_free(Expression*);
void expr_print(FILE*, Expression*);

#endif // EVAL_H