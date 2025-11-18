
#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>

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

/*
 * queue_init - Initializes a queue with a starting capacity
 *
 * @param q               Pointer to the queue
 * @param start_capacity  Number of elements the queue can hold
 *
 * Returns: 0 on success
 *          -1 on allocation failure
 */
int queue_init(struct queue *q, size_t start_capacity);

/*
 * queue_destroy - Frees all memory allocated for the queue
 *
 * @param q  Pointer to the queue
 *
 * Returns: void
 */
void queue_destroy(struct queue *q);

/*
 * queue_grow - Doubles the capacity of the queue when it becomes full
 *
 * @param q  Pointer to the queue
 *
 * Returns: 0 on success
 *          -1 on allocation failure
 */
int queue_grow(struct queue *q);

/*
 * queue_push - Adds a path to the end of the queue
 *
 * @param q     Pointer to the queue
 * @param path  String of the path to add
 *
 * Returns: 0 on success
 *          -1 on allocation failure
 */
int queue_push(struct queue *q, char *path);

/*
 * queue_pop - Removes and returns the path at the front of the queue
 *
 * @param q  Pointer to the queue
 *
 * Returns: Pointer to the removed path string
 *          NULL if the queue is empty
 */
char *queue_pop(struct queue *q);

/*
 * create_path - Adds a base path and a file name to create a complete path
 *
 * @param base  Base directory path
 * @param name  File or directory name to append
 *
 * Returns: String containing the full path
 *          NULL on allocation failure
 */

#endif
