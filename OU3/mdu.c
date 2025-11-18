/**
 * Calculates and prints the disk usage of files and folders with multi
 * threading compability
 *
 * @file mdu.c
 * @author Emil Jönsson @c24ejn
 * @version 1.2
 */

#include "queue.h"
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

struct state {
  struct queue queue;
  size_t pending;
  long long total_size;

  pthread_mutex_t mutex;
  pthread_cond_t cond;

  int shutdown;
  int error;
};

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

char *create_path(const char *base, const char *name) {
  // Get lenths of path strings and add capability for extra characters
  size_t base_len = strlen(base);
  size_t name_len = strlen(name);
  size_t total_len = base_len + name_len + 2;

  char *final_path = malloc(total_len);
  if (!final_path) {
    fprintf(stderr, "mdu: malloc failed: %s\n", strerror(errno));
    return NULL;
  }

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

/*
 * process_directory - Processes a single directory and its contents
 *
 * @param s    Program state
 * @param path Path to the directory to process
 */
static void process_directory(struct state *s, const char *path) {
  DIR *dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "mdu: cannot read directory '%s': %s\n", path,
            strerror(errno));
    pthread_mutex_lock(&s->mutex);
    s->error = 1;
    pthread_mutex_unlock(&s->mutex);
    return;
  }

  // Process each directory entry
  errno = 0;
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    const char *name = ent->d_name;

    // Skip . and ..
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    char *full_path = create_path(path, name);
    if (!full_path)
      continue;

    struct stat st;
    if (lstat(full_path, &st) != 0) {
      fprintf(stderr, "mdu: cannot access '%s': %s\n", full_path,
              strerror(errno));
      free(full_path);
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      // Add directory to queue for processing
      pthread_mutex_lock(&s->mutex);
      if (queue_push(&s->queue, full_path) != 0) {
        fprintf(stderr, "mdu: failed to add %s to queue\n", full_path);
        free(full_path);
        s->pending--;
      } else {
        s->pending++;
        pthread_cond_signal(&s->cond);
      }
      pthread_mutex_unlock(&s->mutex);
    } else {
      // Add file size to total
      pthread_mutex_lock(&s->mutex);
      s->total_size += st.st_blocks;
      pthread_mutex_unlock(&s->mutex);
      free(full_path);
    }
  }

  // Check for readdir errors
  if (errno != 0) {
    fprintf(stderr, "mdu: readdir failed in '%s': %s\n", path, strerror(errno));
    pthread_mutex_lock(&s->mutex);
    s->error = 1;
    pthread_mutex_unlock(&s->mutex);
  }

  closedir(dir);
}

/*
 * calculate_path_size - Thread function that processes directories and files
 *                       to calculate their total disk usage
 *
 * @param arg  Pointer to a struct state containing shared data and
 *             synchronization primitives
 *
 * Returns: NULL
 */
void *calculate_path_size(void *arg) {
  struct state *s = (struct state *)arg;

  while (true) {
    // Wait for work or shutdown
    pthread_mutex_lock(&s->mutex);
    while (s->queue.size == 0 && !s->shutdown && s->pending > 0)
      pthread_cond_wait(&s->cond, &s->mutex);

    if (s->shutdown || (s->queue.size == 0 && s->pending == 0)) {
      pthread_mutex_unlock(&s->mutex);
      break;
    }

    char *path = queue_pop(&s->queue);
    pthread_mutex_unlock(&s->mutex);

    if (!path)
      continue;

    // Get file info
    struct stat st;
    if (lstat(path, &st) != 0) {
      fprintf(stderr, "mdu: cannot read '%s': %s\n", path, strerror(errno));
      pthread_mutex_lock(&s->mutex);
      s->error = 1;
      s->pending--;
      if (s->pending == 0)
        pthread_cond_broadcast(&s->cond);
      pthread_mutex_unlock(&s->mutex);
      free(path);
      continue;
    }

    // Add the item itself to total size (file or directory)
    pthread_mutex_lock(&s->mutex);
    s->total_size += st.st_blocks;
    pthread_mutex_unlock(&s->mutex);

    // If it's a directory, process its contents
    if (S_ISDIR(st.st_mode)) {
      process_directory(s, path);
    }

    // Decrement pending count and notify if done
    pthread_mutex_lock(&s->mutex);
    s->pending--;
    if (s->pending == 0)
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
  s->total_size = 0;
  s->pending = 0;
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
  s.pending = 1;

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

  printf("%lld\t%s\n", s.total_size, path);
  destroy_resources(&s);

  return s.error ? EXIT_FAILURE : 0;
}
