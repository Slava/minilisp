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

double evaluate_op(char* op, double x, double y) {
  if (! strcmp(op, "+") || ! strcmp(op, "add"))
    return x + y;
  if (! strcmp(op, "-") || ! strcmp(op, "sub"))
    return x - y;
  if (! strcmp(op, "*") || ! strcmp(op, "mul"))
    return x * y;
  if (! strcmp(op, "/") || ! strcmp(op, "div"))
    return x / y;
  if (! strcmp(op, "%") || ! strcmp(op, "mod"))
    return (double)((long)x % (long)y);
  if (! strcmp(op, "^") || ! strcmp(op, "pow"))
    return pow(x, y);
  if (! strcmp(op, "min"))
    return fmin(x, y);
  if (! strcmp(op, "max"))
    return fmax(x, y);

  return 0.0;
}

double evaluate_ast(mpc_ast_t* ast) {
  // if tagged as number return it directly
  if (strstr(ast->tag, "number")) {
    return atof(ast->contents);
  }

  // this is an expression, evaluate it
  // ( operator ... )
  char* op = ast->children[1]->contents;
  int i_operand = 2;

  // the intermediate result
  double res = evaluate_ast(ast->children[i_operand++]);

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
      double result = evaluate_ast(r.output);
      printf("%lf\n", result);
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

