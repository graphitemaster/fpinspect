#ifndef EVAL_H
#define EVAL_H
#include "soft32.h"
#include "soft64.h"

typedef struct Expression Expression;

Bool expr_parse(Expression**, const char*);
Float32 expr_eval32(Context*, Expression*);
Float64 expr_eval64(Context*, Expression*);
void expr_free(Expression*);
void expr_print(FILE*, Expression*);

#endif // EVAL_H