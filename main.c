#include <stdio.h> // printf
#include <float.h> // DBL_DIG

#include "eval.h"

int main(int argc, char **argv) {
  argc--;
  argv++;
  if (argc == 0) {
    return 1;
  }

  Expression *e;
  if (!expr_parse(&e, argv[0])) {
    return 2;
  }

  const Float32 r = expr_eval(e);
  expr_print(stdout, e);
  printf("\n\t= %.*f\n", DBL_DIG - 1, *(float *)&r);
  expr_free(e);

  return 0;
}