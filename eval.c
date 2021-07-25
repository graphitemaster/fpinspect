#include <stdlib.h> // calloc, free
#include <string.h> // strchr
#include <stdio.h> // fprintf, stderr

#include "eval.h"

typedef struct Parser Parser;
typedef struct Expression Expression;

struct Expression {
  enum {
    EXPR_VALUE,
    EXPR_CONST,
    EXPR_FUNC1, EXPR_FUNC2,
    EXPR_EQ, EXPR_LTE, EXPR_LT,
    EXPR_NE, EXPR_GTE, EXPR_GT,
    EXPR_ADD, EXPR_SUB, EXPR_MUL, EXPR_DIV,
    EXPR_LAST
  } type;
  Real32 value;
  union {
    Size constant;
    enum {
      // EXPR_FUNC1
      FUNC_FLOOR,
      FUNC_CEIL,
      FUNC_TRUNC,
      FUNC_SQRT,
      FUNC_ABS,
      // EXPR_FUNC2
      FUNC_MIN,
      FUNC_MAX,
      FUNC_COPYSIGN
    } func;
  };
  Expression* params[2];
};

static const struct {
  const char *identifier;
  const Real32 value;
} CONSTANTS[] = {
  { "e",    {{LIT32(0x402df854)}, {0}} },
  { "pi",   {{LIT32(0x40490fdb)}, {0}} },
  { "phi",  {{LIT32(0x3fcf1bbd)}, {0}} },
  { "fmin", {{LIT32(0x00800000)}, {0}} }, // FLT_MIN
  { "fmax", {{LIT32(0x7f7fffff)}, {0}} }, // FLT_MAX
};

static const struct {
  const char *match;
  Uint32 func;
} FUNCS1[] = {
  { "floor", FUNC_FLOOR },
  { "ceil",  FUNC_CEIL  },
  { "trunc", FUNC_TRUNC },
  { "sqrt",  FUNC_SQRT  },
  { "abs",   FUNC_ABS   }
};

static const struct {
  const char *match;
  Uint32 func;
} FUNCS2[] = {
  { "min",      FUNC_MIN      },
  { "max",      FUNC_MAX      },
  { "copysign", FUNC_COPYSIGN }
};

#define ARRAY_COUNT(x) \
  (sizeof (x) / sizeof (*(x)))

static const char* func1_name(Uint32 func) {
  for (Size i = 0; i < ARRAY_COUNT(FUNCS1); i++) {
    if (FUNCS1[i].func == func) {
      return FUNCS1[i].match;
    }
  }
  return NULL;
}

static const char *func2_name(Uint32 func) {
  for (Size i = 0; i < ARRAY_COUNT(FUNCS2); i++) {
    if (FUNCS2[i].func == func) {
      return FUNCS2[i].match;
    }
  }
  return NULL;
}

// This is cheating for now until we implement an accurate strtof, strtod, etc.
static Real32 real32_from_string(const char *string, char **next) {
  union { float f; Float32 s; } u = {strtof(string, next)};
  return (Real32){u.s, {0}};
}

static Bool is_identifier(int ch) {
  return ((unsigned)ch - '0' <= 9u)
      || ((unsigned)ch - 'a' <= 25u)
      || ((unsigned)ch - 'A' <= 25u)
      || ch == '_';
}

static bool match(const char *s, const char *prefix) {
  Size i = 0;
  for (; prefix[i]; i++) {
    if (prefix[i] != s[i]) {
      return false;
    }
  }
  return !is_identifier(s[i]); // Should be terminated identifier.
}

struct Parser {
  Sint32 level;
  char *s;
};

#define ALU(fp, op) \
    fprintf(fp, "("); \
    expr_print(fp, expression->params[0]); \
    fprintf(fp, " %s ", op); \
    expr_print(fp, expression->params[1]); \
    fprintf(fp, ")"); \
    break

void expr_print(FILE *fp, Expression *expression) {
  switch (expression->type) {
  case EXPR_VALUE:
    fprintf(fp, "%f", float32_cast(expression->value.value));
    break;
  case EXPR_CONST:
    fprintf(fp, "%s", CONSTANTS[expression->constant].identifier);
    break;
  case EXPR_FUNC1:
    fprintf(fp, "%s(", func1_name(expression->func));
    expr_print(fp, expression->params[0]);
    fprintf(fp, ")");
    break;
  case EXPR_FUNC2:
    fprintf(fp, "%s(", func2_name(expression->func));
    expr_print(fp, expression->params[0]);
    fprintf(fp, ", ");
    expr_print(fp, expression->params[1]);
    fprintf(fp, ")");
    break;
  case EXPR_ADD: ALU(fp, "+");
  case EXPR_SUB: ALU(fp, "-");
  case EXPR_MUL: ALU(fp, "*");
  case EXPR_DIV: ALU(fp, "/");
  default:
    break;
  }
}

static Real32 eval_func1_32(Context *ctx, Uint32 func, Real32 a) {
  switch (func) {
  case FUNC_FLOOR:
    return real32_floor(ctx, a);
  case FUNC_CEIL:
    return real32_ceil(ctx, a);
  case FUNC_TRUNC:
    return real32_trunc(ctx, a);
  case FUNC_SQRT:
    return real32_sqrt(ctx, a);
  case FUNC_ABS:
    return real32_abs(ctx, a);
  }
  return (Real32){FLOAT32_ZERO, {0}};
}

static Real32 eval_func2_32(Context *ctx, Uint32 func, Real32 a, Real32 b) {
  switch (func) {
  case FUNC_MIN:
    return real32_min(ctx, a, b);
  case FUNC_MAX:
    return real32_max(ctx, a, b);
  case FUNC_COPYSIGN:
    return real32_copysign(ctx, a, b);
  }
  return (Real32){FLOAT32_ZERO, {0}};
}

Real32 expr_eval32(Context *ctx, Expression *expression) {
  if (!expression) {
    return REAL32_ZERO;
  }

  Real32 a = expr_eval32(ctx, expression->params[0]);
  Real32 b = expr_eval32(ctx, expression->params[1]);

  Real32 result = REAL32_ZERO;

  switch (expression->type) {
  /****/ case EXPR_VALUE: result = expression->value;
  break; case EXPR_CONST: result = CONSTANTS[expression->constant].value;
  break; case EXPR_FUNC1: result = eval_func1_32(ctx, expression->func, a);
  break; case EXPR_FUNC2: result = eval_func2_32(ctx, expression->func, a, b);
  break; case EXPR_EQ:    result = real32_eq(ctx, a, b);
  break; case EXPR_LTE:   result = real32_lte(ctx, a, b);
  break; case EXPR_LT:    result = real32_lt(ctx, a, b);
  break; case EXPR_NE:    result = real32_ne(ctx, a, b);
  break; case EXPR_GTE:   result = real32_gte(ctx, a, b);
  break; case EXPR_GT:    result = real32_gt(ctx, a, b);
  break; case EXPR_ADD:   result = real32_add(ctx, a, b);
  break; case EXPR_SUB:   result = real32_sub(ctx, a, b);
  break; case EXPR_MUL:   result = real32_mul(ctx, a, b);
  break; case EXPR_DIV:   result = real32_div(ctx, a, b);
  break; case EXPR_LAST:  // Empty.
  break;
  }

  Size n_operations = array_size(ctx->operations);
  Size n_exceptions = array_size(ctx->exceptions);

  for (Size i = 0; i < n_exceptions; i++) {
    Exception exception = ctx->exceptions[i];
    fprintf(stderr, "Exception: %zu (%zu roundings) ", i, ctx->roundings);
    Bool flag = false;
    if (exception & EXCEPTION_INVALID) {
      fprintf(stderr, "%sINVALID", flag ? "|" : ""), flag = true;
    }
    if (exception & EXCEPTION_INFINITE) {
      fprintf(stderr, "%sINFINITE", flag ? "|" : ""), flag = true;
    }
    if (exception & EXCEPTION_OVERFLOW) {
      fprintf(stderr, "%sOVERFLOW", flag ? "|" : ""), flag = true;
    }
    if (exception & EXCEPTION_UNDERFLOW) {
      fprintf(stderr, "%sUNDERFLOW", flag ? "|" : ""), flag = true;
    }
    if (exception & EXCEPTION_INEXACT) {
      fprintf(stderr, "%sINEXACT", flag ? "|" : ""), flag = true;
    }
    fprintf(stderr, " ");
    expr_print(stderr, expression);
    fprintf(stderr, "\n");
  }
  if (n_operations && n_exceptions) {
    fprintf(stderr, "  Trace (%zu operations) ", n_operations);
    Bool hit = false;
    for (Size i = 0; i < n_operations; i++) {
      Operation operation = ctx->operations[i];
      switch (operation) {
      case OPERATION_ADD: fprintf(stderr, "%sADD", hit ? " " : ""), hit = true; break;
      case OPERATION_SUB: fprintf(stderr, "%sSUB", hit ? " " : ""), hit = true; break;
      case OPERATION_MUL: fprintf(stderr, "%sMUL", hit ? " " : ""), hit = true; break;
      case OPERATION_DIV: fprintf(stderr, "%sDIV", hit ? " " : ""), hit = true; break;
      }
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
  }

  return result;
}

static Expression *create(int type, Expression *e0, Expression *e1) {
  Expression *e = calloc(1, sizeof *e);
  if (!e) {
    return NULL;
  }
  e->type = type;
  e->value = REAL32_ONE;
  e->params[0] = e0;
  e->params[1] = e1;
  return e;
}

static Bool parse_expr(Expression **e, Parser *p);
static Bool parse_primary(Expression **e, Parser *p, Flag sign) {
  Expression *d = calloc(1, sizeof *d);
  if (!d) {
    return false;
  }

  char *next = p->s;
  char *s0 = p->s;
  d->value = real32_from_string(sign ? p->s - 1 : p->s, &next);
  if (next != p->s) {
    d->type = EXPR_VALUE;
    p->s = next;
    *e = d;
    return true;
  }

  d->value = REAL32_ONE;

  for (Size i = 0; i < sizeof CONSTANTS / sizeof *CONSTANTS; i++) {
    if (!match(p->s, CONSTANTS[i].identifier)) {
      continue;
    }
    p->s += strlen(CONSTANTS[i].identifier);
    d->type = EXPR_CONST;
    d->constant = i;
    *e = d;
    return true;
  }

  p->s = strchr(p->s, '(');
  if (!p->s) {
    fprintf(stderr, "Undefined constant or missing '(' in '%s'\n", s0);
    p->s = next;
    expr_free(d);
    return false;
  }

  p->s++; // '('
  if (*next == '(') {
    expr_free(d);
    if (!parse_expr(&d, p)) {
      return false;
    }
    if (*p->s != ')') {
      fprintf(stderr, "Missing ')' in '%s'\n", s0);
      expr_free(d);
      return false;
    }
    p->s++; // ')'
    *e = d;
    return true;
  }
  if (!parse_expr(&d->params[0], p)) {
    expr_free(d);
    return false;
  }
  if (*p->s == ',') {
    p->s++; // ','
    parse_expr(&d->params[1], p); // ignore?
  }
  if (*p->s != ')') {
    fprintf(stderr, "Missing ')' or too many arguments in '%s'\n", s0);
    expr_free(d);
    return false;
  }
  p->s++; // ')'

  for (Size i = 0; i < ARRAY_COUNT(FUNCS1); i++) {
    if (match(next, FUNCS1[i].match)) {
      d->type = EXPR_FUNC1;
      d->func = FUNCS1[i].func;
      *e = d;
      return true;
    }
  }

  for (Size i = 0; i < ARRAY_COUNT(FUNCS2); i++) {
    if (match(next, FUNCS2[i].match)) {
      d->type = EXPR_FUNC2;
      d->func = FUNCS2[i].func;
      *e = d;
      return true;
    }
  }

  fprintf(stderr, "Unknown identifier '%s'\n", s0);
  expr_free(d);

  return false;
}

static Bool parse_top(Expression **e, Parser *p) {
  Flag sign = false;
  if (*p->s == '+') p->s++; // skip unary '+'
  else if (*p->s == '-') p->s++, sign = true; // skip unary '-'
  return parse_primary(e, p, sign);
}

static Bool parse_factor(Expression **e, Parser *p) {
  Expression *e0;
  if (!parse_top(&e0, p)) {
    return false;
  }
  // TODO(dweiler): Handle other operations here.
  *e = e0;
  return true;
}

static Bool parse_term(Expression **e, Parser *p) {
  Expression *e0, *e1, *e2;
  if (!parse_factor(&e0, p)) {
    return false;
  }
  while (*p->s == '*' || *p->s == '/') {
    int ch = *p->s++;
    e1 = e0;
    if (!parse_factor(&e2, p)) {
      expr_free(e1);
      return false;
    }
    e0 = create(ch == '*' ? EXPR_MUL : EXPR_DIV, e1, e2);
    if (!e0) {
      expr_free(e1);
      expr_free(e2);
      return false;
    }
  }
  *e = e0;
  return true;
}

static Bool parse_subexpr(Expression **e, Parser *p) {
  Expression *e0, *e1, *e2;
  if (!parse_term(&e0, p)) {
    return false;
  }
  while (*p->s == '+' || *p->s == '-') {
    int ch = *p->s++;
    e1 = e0;
    if (!parse_term(&e2, p)) {
      expr_free(e1);
      return false;
    }
    e0 = create(ch == '+' ? EXPR_ADD : EXPR_SUB, e1, e2);
    if (!e0) {
      expr_free(e1);
      expr_free(e2);
      return false;
    }
  }
  *e = e0;
  return true;
}

static Bool parse_expr(Expression **e, Parser *p) {
  Expression *e0, *e1, *e2;
  if (!parse_subexpr(&e0, p)) {
    return false;
  }
  while (*p->s == ';') {
    p->s++;
    e1 = e0;
    if (!parse_subexpr(&e2, p)) {
      expr_free(e1);
      return false;
    }
    e0 = create(EXPR_LAST, e1, e2);
    if (!e0) {
      expr_free(e1);
      expr_free(e2);
      return false;
    }
  }
  *e = e0;
  return true;
}

static Bool parse_verify(Expression *expression) {
  if (!expression) {
    return false;
  }
  switch (expression->type) {
  case EXPR_VALUE: // fallthrough
  case EXPR_CONST:
    return true;
  case EXPR_FUNC1:
    return parse_verify(expression->params[0]) && !expression->params[1];
  default:
    return parse_verify(expression->params[0]) && parse_verify(expression->params[1]);
  }
}

Bool expr_parse(Expression **expression, const char *string) {
  Parser p = { 0 };
  char *w = malloc(strlen(string) + 1);
  char *wp = w;
  const char *s0 = string;

  if (!w) {
    return false;
  }

  while (*string) {
    if (*string != ' ') {
      *wp++ = *string;
    }
    string++;
  }
  *wp++ = '\0';

  p.s = w;

  Expression *e = NULL;
  if (!parse_expr(&e, &p)) {
    free(w);
    return false;
  }

  if (*p.s) {
    expr_free(e);
    free(w);
    fprintf(stderr, "Unexpected end of expression '%s'\n", s0);
    return false;
  }

  if (!parse_verify(e)) {
    expr_free(e);
    free(w);
    return false;
  }

  free(w);

  *expression = e;
  return true;
}

void expr_free(Expression *expression) {
  if (expression) {
    expr_free(expression->params[0]);
    expr_free(expression->params[1]);
    free(expression);
  }
}