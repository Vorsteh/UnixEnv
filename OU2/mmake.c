#include "build.h"
#include "parser.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  FILE *file;
  char *filename = "mmakefile"; // [-f MAKEFILE]
  bool force_rebuild = false;   // [-B]
  bool silent = false;          // [-s]

  // Gather data from cmd line arguments
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

  // Open file and check that it worked
  file = fopen(filename, "r");
  if (!file) {
    perror(filename);
    return EXIT_FAILURE;
  }

  // Parse the file into a makefile struct
  makefile *mf = parse_makefile(file);
  fclose(file);

  if (!mf) {
    perror("mmakefile");
    return EXIT_FAILURE;
  }

  int num_targets = argc - optind;
  const char *target_name;

  // If any targets are given build each target
  if (num_targets > 0) {
    for (int i = optind; i < argc; i++) {
      if (build_target(argv[i], mf, force_rebuild, silent) != EXIT_SUCCESS) {
        makefile_del(mf);
        return EXIT_FAILURE;
      }
    }
  } else {
    // If no targets are given the target name is just the default target so we
    // build that
    target_name = makefile_default_target(mf);
    if (build_target(target_name, mf, force_rebuild, silent) != EXIT_SUCCESS) {
      makefile_del(mf);
      perror("build_target");
      return EXIT_FAILURE;
    }
  }

  // Cleanup memory from prase_makefile
  makefile_del(mf);
  return EXIT_SUCCESS;
}
