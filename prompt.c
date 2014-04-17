#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <editline/readline.h>

#ifndef __APPLE__
// is not required on Mac OS X, is required on Linux
#include <editline/history.h>
#endif

#include "mpc.h"

mpc_parser_t *Number, *Operator, *Expr, *Program;
void define_grammar() {
  Number = mpc_new("number");
  Operator = mpc_new("operator");
  Expr = mpc_new("expr");
  Program = mpc_new("program");

  // define the grammar for this language
  mpca_lang(MPC_LANG_DEFAULT,
      " \
        number: /-?[0-9]+(\\.[0-9]+)?/ ; \
        operator: '+' | '-' | '*' | '/' | '%' | '^' | \
          \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"pow\" | \
          \"min\" | \"max\" ; \
        expr: <number> | '(' <operator> <expr>+ ')' ; \
        program: /^/ <operator> <expr>+ /$/ ; \
      ", Number, Operator, Expr, Program);
}

void clean_grammar() {
  // undefine and delete the parsers
  mpc_cleanup(4, Number, Operator, Expr, Program);
}

typedef struct {
  int type;
  double num;
  int err;
} lval;

// possible types of lval
enum { LVAL_NUM, LVAL_ERR };

// possible error types of lval
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// number factory
lval lval_num(double x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

// error factory
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%lf", v.num);
      break;
    case LVAL_ERR:
      switch (v.err) {
        case LERR_DIV_ZERO:
          printf("Error: Division By Zero!");
          break;
        case LERR_BAD_OP:
          printf("Error: Invalid Operator!");
          break;
        case LERR_BAD_NUM:
          printf("Error: Invalid Number!");
          break;
      }

      break;
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval evaluate_op(char* op, lval x, lval y) {
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
  if (! strcmp(op, "%") || ! strcmp(op, "mod"))
    return lval_num((double)((long)x.num % (long)y.num));
  if (! strcmp(op, "^") || ! strcmp(op, "pow"))
    return lval_num(pow(x.num, y.num));
  if (! strcmp(op, "min"))
    return lval_num(fmin(x.num, y.num));
  if (! strcmp(op, "max"))
    return lval_num(fmax(x.num, y.num));

  return lval_err(LERR_BAD_OP);
}

lval evaluate_ast(mpc_ast_t* ast) {
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
      lval result = evaluate_ast(r.output);
      lval_println(result);
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

