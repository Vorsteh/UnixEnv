
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  int num_threads = 1;

  int c;
  while ((c = getopt(argc, argv, "f:")) != -1) {
    switch (c) {
    case 'j': {
      // Convert and check that arguemnts are valid
      char *end;
      long value = strtol(optarg, &end, 10);
      if (end == optarg || value <= 0) {
        fprintf(stderr, "Invalid thread count for -j: %s\n", optarg);
        return EXIT_FAILURE;
      }

      num_threads = (int)value;
      break;
    }
    default:
      fprintf(stderr, "Usage: %s [-j antal_trådar] fil ...\n", argv[0]);
    }
  }

  // Check that usage is correct
  if (optind >= argc) {
    fprintf(stderr, "Usage: %s [-j antal_trådar] fil ...\n", argv[0]);
    return EXIT_FAILURE;
  }

  for (int i = optind; i < argc; ++i) {
  }

  return EXIT_SUCCESS;
}
