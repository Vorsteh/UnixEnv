#include "queue.h"
#include <stdlib.h>

int queue_init(struct queue *q, size_t start_capacity) {
  q->paths = calloc(start_capacity, sizeof(struct path_node));
  if (!q->paths)
    return -1;
  // Init all values
  q->capacity = start_capacity;
  q->head = 0;
  q->tail = 0;
  q->size = 0;
  return 0;
}

void queue_destroy(struct queue *q) {
  if (!q)
    return;
  // Loop through and free each path
  for (size_t i = 0; i < q->size; i++) {
    size_t idx = (q->head + i) % q->capacity;
    free(q->paths[idx].path);
  }
  // free whole array
  free(q->paths);
}

int queue_grow(struct queue *q) {
  // Allocate memory for temp queue
  size_t new_capacity = q->capacity ? q->capacity * 2 : 8;
  struct path_node *n = calloc(new_capacity, sizeof(struct path_node));
  if (!n)
    return -1;

  // Translate over the elements
  for (size_t i = 0; i < q->size; i++) {
    size_t idx = (q->head + i) % q->capacity;
    n[i] = q->paths[idx];
  }
  free(q->paths);

  // Updates queue values and sets the new path
  q->paths = n;
  q->capacity = new_capacity;
  q->head = 0;
  q->tail = q->size % new_capacity;
  return 0;
}

int queue_push(struct queue *q, char *path) {
  // Check if queue is full, if so grow
  if (q->size + 1 > q->capacity) {
    if (queue_grow(q) != 0)
      return -1;
  }

  // Adds new path to the tail and updates the tail position based on
  q->paths[q->tail].path = path;
  q->tail = (q->tail + 1) % q->capacity;
  q->size++;
  return 0;
}

char *queue_pop(struct queue *q) {
  if (q->size == 0)
    return NULL;

  // Gets the path in the head of the queue and updates value then returns path
  char *path = q->paths[q->head].path;
  q->paths[q->head].path = NULL;
  q->head = (q->head + 1) % q->capacity;
  q->size--;
  return path;
}
