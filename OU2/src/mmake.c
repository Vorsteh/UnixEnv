#include "../include/build.h"
#include "../include/parser.h"
#include <bits/getopt_core.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  FILE *file;
  char *filename = "mmakefile"; // [-f MAKEFILE]
  bool force_rebuild = false;   // [-B]
  bool silent = false;          // [-s]
  char **targets = NULL;        // [TARGET ...]

  int c;
  while ((c = getopt(argc, argv, "f:Bs")) != -1) {
    switch (c) {
    case 'f':
      filename = optarg;
      break;
    case 'B':
      force_rebuild = true;
      break;
    case 's':
      silent = true;
      break;
    default:
      fprintf(stderr, "Usage: mmake [-f MAKEFILE] [-B] [-s] [TARGET ...]\n");
      return EXIT_FAILURE;
    }
  }

  file = fopen(filename, "r");
  if (!file) {
    perror(filename);
    return EXIT_FAILURE;
  }

  makefile *mf = parse_makefile(file);
  fclose(file);

  if (!mf) {
    perror("Parse Makefile");
    return EXIT_FAILURE;
  }

  int num_targets = argc - optind;
  const char *target_name;

  if (num_targets > 0) {
    targets = &argv[optind];
    target_name = targets[0];
  } else {
    target_name = makefile_default_target(mf);
  }

  int build_status = build_target(target_name, mf, force_rebuild, silent);

  printf("%d", build_status);
  return EXIT_SUCCESS;
}
