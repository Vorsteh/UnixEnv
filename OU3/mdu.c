/**
 * Prints the output of files and folders with multi threading compability
 *
 * @file mdu.c
 * @author Emil Jönsson @c24ejn
 * @version 1.1
 */

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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

struct state {
  struct queue queue;
  size_t pending_dirs;
  long long total_blocks;

  pthread_mutex_t mutex;
  pthread_cond_t cond;

  int shutdown;
  int error;
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
char *create_path(const char *base, const char *name);

/*
 * calculate_path_size - Thread function that processes directories and files
 *                       to calculate their total disk usage
 *
 * @param arg  Pointer to a struct state containing shared data and
 * synchronization primitives
 *
 * Returns: NULL
 */
void *calculate_path_size(void *arg);

/*
 * get_file_size - Returns the size of a file
 *
 * @param path  Path to the file or directory
 *
 * Returns: File size in blocks
 *          0 if the file cannot be accessed
 */
long long get_file_size(const char *path);

/*
 * destroy_resources - Cleans up allocated memory, mutexes, and condition
 * variables
 *
 * @param s  Pointer to the program state
 *
 */
void destroy_resources(struct state *s);

/*
 * setup_state - Initializes all fields in the program state struct,
 *
 * @param s  Pointer to the program state
 *
 * Returns: 0 on success
 *          -1 on failure
 */
int setup_state(struct state *s);

/*
 * shutdown_threads - Joins all threads and ensures all directories have been
 *                    processed before exiting
 *
 * @param s  Pointer to the program state
 *
 * Returns: Nothing
 */
void shutdown_threads(struct state *s);

/*
 * process_path - Main function that sets up the environment, creates the
 * threads, and begins traversing given path and calculates size
 *
 * @param path         Root directory
 * @param num_threads  Number of threads to use
 *
 * Returns: 0 on success
 *          -1 on initialization or thread creation failure
 */
int process_path(const char *path, int num_threads);

int main(int argc, char *argv[]) {
  int num_threads = 1;
  int c;

  // Look for potential -j flag and sets amount of threads based on flag
  while ((c = getopt(argc, argv, "j:")) != -1) {
    switch (c) {
    case 'j': {
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
      fprintf(stderr, "Usage: %s {FILE}\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Usage: %s {FILE}\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Starts processing provided path arguemnts after the -j flag
  //  Sets exit status if failed instead of exiting because we cant to continue
  //  with other files
  int exit_status = EXIT_SUCCESS;
  for (int i = optind; i < argc; i++) {
    if (process_path(argv[i], num_threads) != 0) {
      exit_status = EXIT_FAILURE;
    }
  }

  return exit_status;
}

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

char *create_path(const char *base, const char *name) {
  // Get lenths of path strings and add capability for extra characters
  size_t base_len = strlen(base);
  size_t name_len = strlen(name);
  size_t total_len = base_len + name_len + 2;

  char *final_path = malloc(total_len);
  if (!final_path)
    return NULL;

  // If it ends with '/' just join the strings if not add '/' inbetween
  if (base_len > 0 && base[base_len - 1] == '/')
    snprintf(final_path, total_len, "%s%s", base, name);
  else
    snprintf(final_path, total_len, "%s/%s", base, name);

  return final_path;
}

long long get_file_size(const char *path) {
  struct stat st;

  if (lstat(path, &st) == 0) {
    return (long long)st.st_blocks;
  }
  return 0;
}

void *calculate_path_size(void *arg) {
  struct state *s = (struct state *)arg;

  while (true) {

    // Wait for work
    pthread_mutex_lock(&s->mutex);
    while (s->queue.size == 0 && !s->shutdown && s->pending_dirs > 0)
      pthread_cond_wait(&s->cond, &s->mutex);

    if (s->shutdown || (s->queue.size == 0 && s->pending_dirs == 0)) {
      pthread_mutex_unlock(&s->mutex);
      break;
    }

    char *path = queue_pop(&s->queue);
    pthread_mutex_unlock(&s->mutex);

    if (!path)
      continue;

    struct stat st;
    if (lstat(path, &st) != 0) {
      fprintf(stderr, "du: cannot read '%s': %s\n", path, strerror(errno));
      pthread_mutex_lock(&s->mutex);
      s->error = 1;
      s->pending_dirs--;
      if (s->pending_dirs == 0)
        pthread_cond_broadcast(&s->cond);
      pthread_mutex_unlock(&s->mutex);
      free(path);
      continue;
    }

    // Regular file or special
    if (!S_ISDIR(st.st_mode)) {
      pthread_mutex_lock(&s->mutex);
      s->total_blocks += st.st_blocks;
      pthread_mutex_unlock(&s->mutex);
      free(path);
      continue;
    }

    // Open directory
    DIR *dir = opendir(path);
    if (!dir) {
      struct stat st_dir;
      // If we cant open direcotry still add size of dir then set variable and
      // continue to next path
      if (lstat(path, &st_dir) == 0) {
        pthread_mutex_lock(&s->mutex);
        s->total_blocks += st_dir.st_blocks;
        pthread_mutex_unlock(&s->mutex);
      }
      fprintf(stderr, "du: cannot read directory '%s': %s\n", path,
              strerror(errno));
      pthread_mutex_lock(&s->mutex);
      s->error = 1;
      s->pending_dirs--;
      if (s->pending_dirs == 0)
        pthread_cond_broadcast(&s->cond);
      pthread_mutex_unlock(&s->mutex);
      free(path);
      continue;
    }

    // Add size of dir
    struct stat st_dir;
    if (lstat(path, &st_dir) == 0) {
      pthread_mutex_lock(&s->mutex);
      s->total_blocks += st_dir.st_blocks;
      pthread_mutex_unlock(&s->mutex);
    }

    // Loop through each entry inside direcotry
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
      const char *name = ent->d_name;
      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      char *full_path = create_path(path, name);
      if (!full_path)
        continue;

      if (lstat(full_path, &st) != 0) {
        perror("lstat");
        free(full_path);
        continue;
      }

      // If entry is a directory we add it to the queue
      if (S_ISDIR(st.st_mode)) {
        pthread_mutex_lock(&s->mutex);
        if (queue_push(&s->queue, full_path) != 0) {
          fprintf(stderr, "mdu: failed to add %s to queue\n", full_path);
          free(full_path);
          s->pending_dirs--;
        } else {
          s->pending_dirs++;
          pthread_cond_signal(&s->cond);
        }
        pthread_mutex_unlock(&s->mutex);
      } else {
        pthread_mutex_lock(&s->mutex);
        s->total_blocks += st.st_blocks;
        pthread_mutex_unlock(&s->mutex);
        free(full_path);
      }
    }

    closedir(dir);

    // Boradcast that theres no more dirs to do
    pthread_mutex_lock(&s->mutex);
    s->pending_dirs--;
    if (s->pending_dirs == 0)
      pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    free(path);
  }

  return NULL;
}

// Resource management
void destroy_resources(struct state *s) {
  if (!s)
    return;
  if (s->queue.paths)
    queue_destroy(&s->queue);
  pthread_mutex_destroy(&s->mutex);
  pthread_cond_destroy(&s->cond);
}

int setup_state(struct state *s) {
  int err;
  // Init mutex
  if ((err = pthread_mutex_init(&s->mutex, NULL)) != 0) {
    fprintf(stderr, "pthread_mutex_init: %s\n", strerror(err));
    return EXIT_FAILURE;
  }

  // Init cond
  if ((err = pthread_cond_init(&s->cond, NULL)) != 0) {
    fprintf(stderr, "pthread_cond_init: %s\n", strerror(err));
    pthread_mutex_destroy(&s->mutex);
    return EXIT_FAILURE;
  }
  // Init queue
  if (queue_init(&s->queue, 16) != 0) {
    fprintf(stderr, "queue_init failed\n");
    destroy_resources(s);
    return EXIT_FAILURE;
  }

  // Set values
  s->total_blocks = 0;
  s->pending_dirs = 0;
  s->shutdown = 0;
  s->error = 0;
  return 0;
}

void shutdown_threads(struct state *s) {
  pthread_mutex_lock(&s->mutex);
  s->shutdown = 1;
  pthread_cond_broadcast(&s->cond);
  pthread_mutex_unlock(&s->mutex);
}

int process_path(const char *path, int num_threads) {
  struct stat st;
  if (lstat(path, &st) != 0) {
    fprintf(stderr, "du: cannot access '%s': %s\n", path, strerror(errno));
    return EXIT_FAILURE;
  }

  // Not a directory so we print size
  if (!S_ISDIR(st.st_mode)) {
    printf("%lld\t%s\n", (long long)st.st_blocks, path);
    return 0;
  }

  struct state s;
  if (setup_state(&s) != 0)
    return EXIT_FAILURE;

  char *start_path = strdup(path);
  if (!start_path) {
    fprintf(stderr, "mdu: strdup failed\n");
    destroy_resources(&s);
    return EXIT_FAILURE;
  }

  // Add path to queue
  if (queue_push(&s.queue, start_path) != 0) {
    fprintf(stderr, "mdu: couldn't push to queue\n");
    free(start_path);
    destroy_resources(&s);
    return EXIT_FAILURE;
  }
  s.pending_dirs = 1;

  // Allocate memory for number of wanterd threads
  pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
  if (!threads) {
    perror("calloc");
    destroy_resources(&s);
    return EXIT_FAILURE;
  }

  // Sets up each thread and on fail we join the threads and shutdown
  for (int i = 0; i < num_threads; i++) {
    int ret = pthread_create(&threads[i], NULL, calculate_path_size, &s);
    if (ret != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(ret));
      shutdown_threads(&s);
      for (int j = 0; j < i; j++)
        pthread_join(threads[j], NULL);
      destroy_resources(&s);
      free(threads);
      return EXIT_FAILURE;
    }
  }

  // Add threads before freeing memory
  for (int i = 0; i < num_threads; i++)
    pthread_join(threads[i], NULL);

  free(threads);

  printf("%lld\t%s\n", s.total_blocks, path);
  destroy_resources(&s);

  return s.error ? EXIT_FAILURE : 0;
}
