#include <stdio.h> // printf
#include <float.h> // DBL_DIG
#include <stdlib.h> // atoi

#include "eval.h"

static int usage(const char *app) {
  fprintf(stderr, "%s [OPTION]... [EXPRESSION]\n", app);
  fprintf(stderr, "-r   rounding mode\n");
  fprintf(stderr, "      0 - nearest even [default]\n");
  fprintf(stderr, "      1 - to zero\n");
  fprintf(stderr, "      2 - down\n");
  fprintf(stderr, "      3 - up\n");
  fprintf(stderr, "-t   tininess detection mode\n");
  fprintf(stderr, "      0 - before rounding [default]\n");
  fprintf(stderr, "      1 - after rounding\n");
  return 1;
}

int main(int argc, char **argv) {
  argc--;
  argv++;
  if (argc == 0) {
    return usage(argv[-1]);
  }

  Context c;
  c.round = ROUND_NEAREST_EVEN;
  c.tininess = TININESS_BEFORE_ROUNDING;
  context_init(&c);

  // Parse some command line options.
  if (argv[0][0] == '-') {
    if (argv[0][1] == 'r') {
      int round = atoi(argv[1]);
      if (round < 0 || round > 3) {
        return usage(argv[-1]);
      }
      argv += 2; // skip -r %d
      argc -= 2;
      c.round = round;
    } else if (argv[0][1] == 't') {
      int tiny = atoi(argv[1]);
      if (tiny < 0 || tiny > 1) {
        return usage(argv[-1]);
      }
      argv += 2; // skip -t %d
      argc -= 2;
      c.tininess = tiny;
    } else {
      return usage(argv[-1]);
    }
  }

  if (argc == 0) {
    return usage(argv[-1]);
  }

  Expression *e;
  if (!expr_parse(&e, argv[0])) {
    return 2;
  }

  const Float32 result = expr_eval32(&c, e);
  expr_print(stdout, e);
  printf("\n\t= %.*f\n", DBL_DIG - 1, float32_cast(result));
  expr_free(e);

  context_free(&c);

  return 0;
}