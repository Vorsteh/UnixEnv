
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

int queue_grow(struct queue *q) {
  // Set new_capcaity to either be double the old or default it to 8 if old
  // capacity was 0
  size_t new_capcaity;
  if (q->capacity != 0)
    new_capcaity = q->capacity * 2;
  else
    new_capcaity = 8;

  // Create array of path_node
  struct path_node *n = calloc(new_capcaity, sizeof(struct path_node));
  if (!n)
    return -1;

  // Loop through old paths and add them to new array in logical order
  for (size_t i = 0; i < q->size; i++) {
    size_t idx = (q->head + i) % q->capacity;
    n[i] = q->paths[idx];
  }

  // Free old paths and set all new data
  free(q->paths);
  q->paths = n;
  q->capacity = new_capcaity;
  q->head = 0;
  q->tail = q->size % new_capcaity;

  return 0;
}

int queue_push(struct queue *q, char *path) {
  // Check if queue is full, if so make queue larger
  if (q->size + 1 > q->capacity) {
    if (queue_grow(q) != 0)
      return -1;
  }

  // Add path to queue paths and update tail and size
  q->paths[q->tail].path = path;
  q->tail = (q->tail + 1) % q->capacity;
  q->size++;

  return 0;
}

char *queue_pop(struct queue *q) {
  if (q->size == 0)
    return NULL;

  // Save path of head and set it to NULL then update head position and size
  char *path = q->paths[q->head].path;
  q->paths[q->head].path = NULL;
  q->head = (q->head + 1) % q->capacity;
  q->size--;

  return path;
}

char *create_path(const char *base, const char *name) {
  size_t base_len = strlen(base);
  size_t name_len = strlen(name);

  size_t total_len = base_len + name_len + 2;

  char *final_path = malloc(total_len);
  if (!final_path)
    return NULL;
  if (base_len > 0 && base[base_len - 1] == '/')
    snprintf(final_path, total_len, "%s%s", base, name);
  else
    snprintf(final_path, total_len, "%s/%s", base, name);

  return final_path;
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
