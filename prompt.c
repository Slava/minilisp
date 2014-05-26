#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>

#ifndef __APPLE__
// is not required on Mac OS X, is required on Linux
#include <editline/history.h>
#endif

#include "mpc.h"

mpc_parser_t *Number, *Symbol, *Sexpr, *Qexpr, *Expr, *Program;
void define_grammar() {
  Number = mpc_new("number");
  Symbol = mpc_new("symbol");
  Sexpr = mpc_new("sexpr");
  Qexpr = mpc_new("qexpr");
  Expr = mpc_new("expr");
  Program = mpc_new("program");

  // define the grammar for this language
  mpca_lang(MPC_LANG_DEFAULT,
      " \
        number: /-?[0-9]+(\\.[0-9]+)?/ ; \
        symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
        sexpr: '(' <expr>* ')' ; \
        qexpr: '{' <expr>* '}' ; \
        expr: <number> | <symbol> | <sexpr> | <qexpr> ; \
        program: /^/ <expr>* /$/ ; \
      ", Number, Symbol, Sexpr, Qexpr, Expr, Program);
}

void clean_grammar() {
  // undefine and delete the parsers
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Program);
}

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval* (*lbuiltin)(lenv*, lval*);

struct lval {
  int type;
  double num;

  // Error and symbol are represented by strings
  char *err;
  char *sym;
  // Function is represented by a function pointer
  lbuiltin fun;

  // A list of lval and the number of elements in the list
  int count;
  struct lval** cell;
};

// possible types of lval
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

// number factory
lval *lval_num(double x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->count = 0;
  v->num = x;
  return v;
}

// error factory
lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->count = 0;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

// symbol factory
lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->count = 0;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

// function factory
lval *lval_fun(lbuiltin f) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->count = 0;
  v->fun = f;
  return v;
}

// s-expr factory
lval *lval_sexpr() {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// q-expr factory
lval *lval_qexpr() {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_FUN: break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      for (int i = 0; i < v->count; i++)
        lval_del(v->cell[i]);

      free(v->cell);
      break;
  }

  free(v);
}

// add a new element x to v's list
lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i];

  // shift the array inplace removing a reference to i-th element
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count-i-1));

  // decrease the count
  v->count--;

  // reallocate the memory used (as we removed one element)
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval *lval_join(lval *x, lval *y) {
  // one by one put elements from y to x
  while (y->count > 0)
    x = lval_add(x, lval_pop(y, 0));
  lval_del(y);
  return x;
}

// deep copy of the value passed in
lval *lval_copy(lval *x) {
  lval *c = malloc(sizeof(lval));
  c->type = x->type;

  switch (x->type) {
    case LVAL_NUM:
      c->num = x->num; break;
    case LVAL_ERR:
      c->err = malloc(strlen(x->err) + 1);
      strcpy(c->err, x->err);
      break;
    case LVAL_SYM:
      c->sym = malloc(strlen(x->sym) + 1);
      strcpy(c->sym, x->sym);
      break;
    case LVAL_FUN:
      c->fun = x->fun; break;
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      c->count = x->count;
      c->cell = malloc(sizeof(lval*) * c->count);
      for (int i = 0; i < x->count; i++)
        c->cell[i] = lval_copy(x->cell[i]);
      break;
  }

  return c;
}

lval *lval_read_num(mpc_ast_t *t) {
  double x;
  int read = sscanf(t->contents, "%lf", &x);
  return read > 0 ? lval_num(x) : lval_err("Invalid number.");
}

lval *lval_read(mpc_ast_t* t) {
  // for atoms, like numbers or symbols, just create a val of this type
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  // otherwise it is either root or sexpr for s-expression
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  // recursively add valid expressions of s-expression
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

void lval_print(lval*);
void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  // print the whole list
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    // no whitespace after the last element in the list
    if (i != v->count - 1)
      putchar(' ');
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      printf("%lf", v->num); break;
    case LVAL_ERR:
      printf("Error: %s", v->err); break;
    case LVAL_SYM:
      printf("%s", v->sym); break;
    case LVAL_FUN:
      printf("<function>"); break;
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR:
      lval_expr_print(v, '{', '}'); break;
  }
}


void lval_println(lval *v) { lval_print(v); putchar('\n'); }

#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }
lval *builtin_head(lval *args) {
  LASSERT(args, args->count == 1, "HEAD was passed incorrect number of arguments.");

  lval* list = args->cell[0];
  LASSERT(args, list->type == LVAL_QEXPR, "HEAD was passed incorrect type.");

  LASSERT(args, list->count != 0, "HEAD was passed empty list ({}).");

  // take frees the original args list
  list = lval_take(args, 0);

  // remove the rest
  while (list->count > 1)
    lval_pop(list, 1);

  return list;
}

lval *builtin_tail(lval *args) {
  LASSERT(args, args->count == 1, "TAIL was passed incorrect number of arguments.");

  lval* list = args->cell[0];
  LASSERT(args, list->type == LVAL_QEXPR, "TAIL was passed incorrect type.");
  LASSERT(args, list->count != 0, "TAIL was passed an empty list ({}).");

  // take frees the original args list
  list = lval_take(args, 0);

  // remove the head
  lval_del(lval_pop(list, 0));

  return list;
}

lval *builtin_list(lval *args) {
  args->type = LVAL_QEXPR;
  return args;
}

lval *lval_eval(lval *);
lval *builtin_eval(lval *args) {
  LASSERT(args, args->count == 1, "EVAL was passed incorrect number of arguments.");
  LASSERT(args, args->cell[0]->type == LVAL_QEXPR, "EVAL was passed incorrect type.");

  lval *list = lval_take(args, 0);
  list->type = LVAL_SEXPR;
  return lval_eval(list);
}

lval *builtin_join(lval *args) {
  LASSERT(args, args->count != 0, "JOIN was passed 0 arguments.");
  for (int i = 0; i < args->count; i++)
    LASSERT(args, args->cell[i]->type == LVAL_QEXPR, "JOIN was passed incorrect type.");

  lval *res = lval_pop(args, 0);
  while (args->count > 0)
    res = lval_join(res, lval_pop(args, 0));

  lval_del(args);
  return res;
}

lval *builtin_cons(lval *args) {
  LASSERT(args, args->count == 2, "CONS was passed incorrect number of arguments.");
  LASSERT(args, args->cell[1]->type == LVAL_QEXPR, "CONS was passed incorrect type.");

  lval *val = lval_pop(args, 0);
  lval *list = lval_pop(args, 0);
  lval *res = lval_qexpr();
  lval_del(args);

  return lval_join(lval_add(res, val), list);
}

lval *builtin_len(lval *args) {
  LASSERT(args, args->count == 1, "LEN was passed incorrect number of arguments.");
  LASSERT(args, args->cell[0]->type == LVAL_QEXPR, "LEn was passed incorrect type.");

  lval *res = lval_num(args->cell[0]->count);
  lval_del(args);

  return res;
}

lval *evaluate_op(char* op, lval *x, lval *y) {
  // if either of operands is an error, return it
  if (x->type == LVAL_ERR) return x;
  if (y->type == LVAL_ERR) return y;


  if (! strcmp(op, "+") || ! strcmp(op, "add"))
    return lval_num(x->num + y->num);
  if (! strcmp(op, "-") || ! strcmp(op, "sub"))
    return lval_num(x->num - y->num);
  if (! strcmp(op, "*") || ! strcmp(op, "mul"))
    return lval_num(x->num * y->num);
  if (! strcmp(op, "/") || ! strcmp(op, "div")) {
    // restrict the division by zero, even for doubles for now
    if (y->num == 0.0)
      return lval_err("Division by zero when trying to to divide.");
    return lval_num(x->num / y->num);
  }
  if (! strcmp(op, "%") || ! strcmp(op, "mod")) {
    if (y->num == 0.0)
      return lval_err("Division by zero when trying to take mod.");
    return lval_num((double)((long)x->num % (long)y->num));
  }
  if (! strcmp(op, "^") || ! strcmp(op, "pow"))
    return lval_num(pow(x->num, y->num));
  if (! strcmp(op, "min"))
    return lval_num(fmin(x->num, y->num));
  if (! strcmp(op, "max"))
    return lval_num(fmax(x->num, y->num));

  return lval_err("Bad operator.");
}

lval *builtin_op(lval *args, char *op) {
  // all arguments should be numbers
  for (int i = 0; i < args->count; i++)
    if (args->cell[i]->type != LVAL_NUM) {
      lval_del(args);
      return lval_err("Cannot operate on non-numbers.");
    }

  // use the first argument as the base
  lval *res = lval_pop(args, 0);

  // Special case for unary minus:
  if (args->count == 0 && strcmp("-", op) == 0)
    res->num = -res->num;

  while (args->count > 0) {
    lval *x = lval_pop(args, 0);
    lval *newRes = evaluate_op(op, res, x);
    lval_del(res);
    lval_del(x);

    res = newRes;
    if (res->type == LVAL_ERR)
      break;
  }

  lval_del(args);
  return res;
}

lval *builtin(lval *args, char *func) {
  if (! strcmp(func, "list")) return builtin_list(args);
  if (! strcmp(func, "head")) return builtin_head(args);
  if (! strcmp(func, "tail")) return builtin_tail(args);
  if (! strcmp(func, "eval")) return builtin_eval(args);
  if (! strcmp(func, "join")) return builtin_join(args);
  if (! strcmp(func, "cons")) return builtin_cons(args);
  if (! strcmp(func, "len")) return builtin_len(args);
  return builtin_op(args, func);
}

lval *lval_eval_sexpr(lval *sexpr) {
  // evaluate all the children first
  for (int i = 0; i < sexpr->count; i++)
    sexpr->cell[i] = lval_eval(sexpr->cell[i]);

  // check if any of the children evaluations returned an error
  for (int i = 0; i < sexpr->count; i++)
    if (sexpr->cell[i]->type == LVAL_ERR)
      return lval_take(sexpr, i);

  // an empty expression is resulted into an empty expression:
  // () -> ()
  if (sexpr->count == 0)
    return sexpr;

  // expression with a single children is resulted into this children
  // (6) -> 6
  if (sexpr->count == 1)
    return lval_take(sexpr, 0);

  // (sym arg arg arg ...)
  // first child should be a symbol
  lval *op = lval_pop(sexpr, 0);

  if (op->type != LVAL_SYM) {
    lval_del(op);
    lval_del(sexpr);
    return lval_err("S-expression doesn't start with a symbol.");
  }

  lval *result = builtin(sexpr, op->sym);
  lval_del(op);
  return result;
}

lval *lval_eval(lval *v) {
  // S-expression should be evaluated
  if (v->type == LVAL_SEXPR)
    return lval_eval_sexpr(v);
  // evaluate to itself
  return v;
}

void start_repl() {
  printf("Minilisp Version 0.0.1\n");
  printf("Press Ctrl+c to Exit\n\n");

  // REPL loop
  while (1) {
    // prompt and read the input
    char* input = readline("minilisp> ");

    // break if EOF
    if (! input)
      break;

    // add input to history
    add_history(input);

    // attempt to parse the user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Program, &r)) {
      // print the result of evaluation
      lval *x = lval_read(r.output);
      x = lval_eval(x);
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      // print the error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    // free retrived input
    free(input);
  }
}

int main(int argc, char** argv) {
  define_grammar();
  start_repl();
  clean_grammar();
  return 0;
}

