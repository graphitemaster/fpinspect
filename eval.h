#ifndef EVAL_H
#define EVAL_H
#include "soft.h"

typedef struct Expression Expression;

Bool expr_parse(Expression**, const char*);
Float32 expr_eval(Context*, Expression*);
void expr_free(Expression*);
void expr_print(FILE*, Expression*);

#endif // EVAL_H