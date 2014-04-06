#include <stdio.h>
#include <stdlib.h>

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
        number: /-?[0-9]+/ ; \
        operator: '+' | '-' | '*' | '/' | '%' | \
          \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; \
        expr: <number> | '(' <operator> <expr>+ ')' ; \
        program: /^/ <operator> <expr>+ /$/ ; \
      ", Number, Operator, Expr, Program);
}

void clean_grammar() {
  // undefine and delete the parsers
  mpc_cleanup(4, Number, Operator, Expr, Program);
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
      // print the AST
      mpc_ast_print(r.output);
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

