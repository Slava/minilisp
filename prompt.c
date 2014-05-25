#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>

#ifndef __APPLE__
// is not required on Mac OS X, is required on Linux
#include <editline/history.h>
#endif

#include "mpc.h"

mpc_parser_t *Number, *Symbol, *Sexpr, *Expr, *Program;
void define_grammar() {
  Number = mpc_new("number");
  Symbol = mpc_new("symbol");
  Sexpr = mpc_new("sexpr");
  Expr = mpc_new("expr");
  Program = mpc_new("program");

  // define the grammar for this language
  mpca_lang(MPC_LANG_DEFAULT,
      " \
        number: /-?[0-9]+(\\.[0-9]+)?/ ; \
        symbol: '+' | '-' | '*' | '/' | '%' | '^' | \
          \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"pow\" | \
          \"min\" | \"max\" ; \
        sexpr: '(' <expr>* ')' ; \
        expr: <number> | <symbol> | <sexpr> ; \
        program: /^/ <expr>* /$/ ; \
      ", Number, Symbol, Sexpr, Expr, Program);
}

void clean_grammar() {
  // undefine and delete the parsers
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Program);
}

typedef struct lval {
  int type;
  double num;

  // Error and symbol are represented by strings
  char *err;
  char *sym;

  // A list of lval and the number of elements in the list
  int count;
  struct lval** cell;
} lval;

// possible types of lval
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

// number factory
lval *lval_num(double x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// error factory
lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

// symbol factory
lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
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

void lval_del(lval *v) {
  switch (v->type) {
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_SEXPR:
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

lval *lval_read_num(mpc_ast_t *t) {
  double x;
  int read = sscanf(t->contents, "%lf", &x);
  return read > 0 ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  // for atoms, like numbers or symbols, just create a val of this type
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  // otherwise it is either root or sexpr for s-expression
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }

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
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')'); break;
  }
}


void lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval evaluate_op(char* op, lval x, lval y) {
  /*
  // if either of operands is an error, return it
  if (x.type == LVAL_ERR) return x;
  if (y.type == LVAL_ERR) return y;


  if (! strcmp(op, "+") || ! strcmp(op, "add"))
    return lval_num(x.num + y.num);
  if (! strcmp(op, "-") || ! strcmp(op, "sub"))
    return lval_num(x.num - y.num);
  if (! strcmp(op, "*") || ! strcmp(op, "mul"))
    return lval_num(x.num * y.num);
  if (! strcmp(op, "/") || ! strcmp(op, "div")) {
    // restrict the division by zero, even for doubles for now
    if (y.num == 0.0)
      return lval_err(LERR_DIV_ZERO);
    return lval_num(x.num / y.num);
  }
  if (! strcmp(op, "%") || ! strcmp(op, "mod")) {
    if (y.num == 0.0)
      return lval_err(LERR_DIV_ZERO);
    return lval_num((double)((long)x.num % (long)y.num));
  }
  if (! strcmp(op, "^") || ! strcmp(op, "pow"))
    return lval_num(pow(x.num, y.num));
  if (! strcmp(op, "min"))
    return lval_num(fmin(x.num, y.num));
  if (! strcmp(op, "max"))
    return lval_num(fmax(x.num, y.num));

  return lval_err(LERR_BAD_OP);
  */
}

lval evaluate_ast(mpc_ast_t* ast) {
  /*
  // if tagged as number return it directly
  if (strstr(ast->tag, "number")) {
    return lval_num(atof(ast->contents));
  }

  // this is an expression, evaluate it
  // ( operator ... )
  char* op = ast->children[1]->contents;
  int i_operand = 2;

  // the intermediate result
  lval res = evaluate_ast(ast->children[i_operand++]);

  while (strstr(ast->children[i_operand]->tag, "expr")) {
    res = evaluate_op(op, res, evaluate_ast(ast->children[i_operand]));
    i_operand++;
  }

  return res;
  */
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

