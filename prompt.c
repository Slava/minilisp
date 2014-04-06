#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#ifndef __APPLE__
// is not required on Mac OS X, is required on Linux
#include <editline/history.h>
#endif

int main(int argc, char** argv) {

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

    // dumb reply
    printf("No you're a %s\n", input);

    // free retrived input
    free(input);
  }

  return 0;
}

