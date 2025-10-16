
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// STRUCTS
struct path_node {
  char *path;
};

struct queue {
  struct path_node *paths;
  size_t head;
  size_t tail;
  size_t capacity;
  size_t size;
};

int queue_init(struct queue *q, size_t start_capacity) {
  // Allocate memory for queue paths and check if successfull
  q->paths = calloc(start_capacity, sizeof(struct path_node));
  if (!q->paths)
    return -1;
  q->capacity = start_capacity;
  // Init all values to 0
  q->head = q->tail = q->size = 0;

  return 0;
}

void queue_destory(struct queue *q) {
  if (!q)
    return;

  // Loop through and free each path in logical order, circle back to head when
  // weve reached capacity
  for (size_t i = 0; i < q->size; i++) {
    size_t idx = (q->head + i) % q->capacity;
    free(q->paths[idx].path);
  }

  // Free the entire list
  free(q->paths);
}

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

  for (int i = optind; i < argc; i++) {
  }

  return EXIT_SUCCESS;
}
