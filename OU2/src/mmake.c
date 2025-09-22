#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

  int a = argc;
  char **b = argv;
  printf("%d\n", a);
  printf("%s\n", *b);
  return EXIT_SUCCESS;
}
