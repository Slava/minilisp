#include <stdio.h>

// a user limitted input
static char input[2048];

int main(int argc, char** argv) {

  printf("Minilisp Version 0.0.1\n");
  printf("Press Ctrl+c to Exit\n\n");

  // REPL loop
  while (1) {
    // prompt
    fputs("minilisp> ", stdout);

    // read the user input or break if no input is available
    if (! fgets(input, 2048, stdin))
      break;

    // dumb reply
    printf("No you're a %s", input);
  }

  return 0;
}

