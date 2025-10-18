
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
#include <time.h>
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
};

int queue_init(struct queue *q, size_t start_capacity);
void queue_destroy(struct queue *q);
int queue_grow(struct queue *q);
int queue_push(struct queue *q, char *path);
char *queue_pop(struct queue *q);
char *create_path(const char *base, const char *name);
void *calculate_path_size(void *arg);
int create_threads(pthread_t *threads, int num_threads, struct state *s);
int process_path(const char *path, int num_threads);

long long get_file_size(const char *path);
void destroy_resources(struct state *s);
int setup_state(struct state *s);
void shutdown_threads(struct state *s);

int main(int argc, char *argv[]) {

  int num_threads = 1;

  int c;
  while ((c = getopt(argc, argv, "j:")) != -1) {
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

  int exit_status = EXIT_SUCCESS;
  for (int i = optind; i < argc; i++) {
    if (process_path(argv[i], num_threads) != 0) {
      // Set exit status but continue with all files
      exit_status = EXIT_FAILURE;
      continue;
    }
  }

  return exit_status;
}

int queue_init(struct queue *q, size_t start_capacity) {
  // Allocate memory for queue paths and check if successfull
  q->paths = calloc(start_capacity, sizeof(struct path_node));
  if (!q->paths)
    return -1;
  q->capacity = start_capacity;
  // Init all values to 0
  q->head = 0;
  q->tail = 0;
  q->size = 0;

  return 0;
}

void queue_destroy(struct queue *q) {
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

  // Calculate needed length of paths joined
  size_t total_len = base_len + name_len + 2;

  char *final_path = malloc(total_len);
  if (!final_path)
    return NULL;

  // If base length is valid and ends with a '/' we canjust add them directly
  // else we add '/' ourselves
  if (base_len > 0 && base[base_len - 1] == '/')
    snprintf(final_path, total_len, "%s%s", base, name);
  else
    snprintf(final_path, total_len, "%s/%s", base, name);

  return final_path;
}

void *calculate_path_size(void *arg) {
  struct state *s = (struct state *)arg;

  while (true) {
    // Wait for work and shutdown if flag is active
    pthread_mutex_lock(&s->mutex);
    while (s->queue.size == 0 && !s->shutdown && s->pending_dirs > 0) {
      pthread_cond_wait(&s->cond, &s->mutex);
    }

    if (s->queue.size == 0 && s->pending_dirs == 0) {
      pthread_mutex_unlock(&s->mutex);
      break;
    }

    if (s->shutdown) {
      pthread_mutex_unlock(&s->mutex);
      break;
    }

    // Get first entry in queue
    char *path = queue_pop(&s->queue);
    pthread_mutex_unlock(&s->mutex);

    // Skip this queue entry
    if (!path)
      continue;

    DIR *dir = opendir(path);
    if (!dir) {
      perror("opendir");
      pthread_mutex_lock(&s->mutex);
      s->pending_dirs--;
      if (s->pending_dirs == 0)
        pthread_cond_broadcast(&s->cond);
      free(path);
      // Continue with other directories
      continue;
    }

    // Create dirent struct
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
      const char *ent_name = ent->d_name;
      if (strcmp(ent_name, ".") == 0 || strcmp(ent_name, "..") == 0) {
        continue;
      }

      char *full_name = create_path(path, ent_name);
      if (!full_name) {
        fprintf(stderr, "mdu: merory allocation failed");
        continue;
      }

      // Get file size and add to total_blocks if not its a directory and we
      // continue
      long long size = get_file_size(full_name);

      if (size > 0) {
        s->total_blocks += size;
        free(full_name);
        continue;
      }

      // Add the directory to the queue
      pthread_mutex_lock(&s->mutex);
      if (queue_push(&s->queue, full_name) != 0) {
        fprintf(stderr, "mdu: failed to add %s to queue", full_name);
        free(full_name);
        s->pending_dirs--;
      } else {
        s->pending_dirs++;
        pthread_cond_signal(&s->cond);
      }
      pthread_mutex_unlock(&s->mutex);
    }

    closedir(dir);

    pthread_mutex_lock(&s->mutex);

    s->pending_dirs--;
    if (s->pending_dirs == 0)
      pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    free(path);
  }

  return NULL;
}

long long get_file_size(const char *path) {
  struct stat st;
  if (lstat(path, &st) != 0) {
    perror("lstat");
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    return -1;
  }

  return (long long)st.st_blocks;
}

void destroy_resources(struct state *s) {
  // Destroyes all resouces in state
  if (!s)
    return;
  if (s->queue.paths)
    queue_destroy(&s->queue);

  pthread_mutex_destroy(&s->mutex);
  pthread_cond_destroy(&s->cond);
}

int setup_state(struct state *s) {
  int err;

  // Setup mutex
  if ((err = pthread_mutex_init(&s->mutex, NULL)) != 0) {
    fprintf(stderr, "pthread_mutex_init: %s\n", strerror(err));
    return EXIT_FAILURE;
  }

  // Setup cond
  if ((err = pthread_cond_init(&s->cond, NULL)) != 0) {
    fprintf(stderr, "pthread_cond_init: %s\n", strerror(err));
    pthread_mutex_destroy(&s->mutex);
    return EXIT_FAILURE;
  }

  // Setup queue
  if (queue_init(&s->queue, 16) != 0) {
    fprintf(stderr, "queue_init failed\n");
    destroy_resources(s);
    return EXIT_FAILURE;
  }

  s->total_blocks = 0;
  s->pending_dirs = 0;

  return 0;
}

void shutdown_threads(struct state *s) {
  pthread_mutex_lock(&s->mutex);
  s->shutdown = 1;
  pthread_cond_broadcast(&s->cond);
  pthread_mutex_unlock(&s->mutex);
}

int process_path(const char *path, int num_threads) {

  long long size = get_file_size(path);

  // If it is a normal file we will print the size and return. If not we
  // continue with the program meaning it is a directory
  if (size > 0) {
    printf("%lld\t%s\n", size, path);
    return 0;
  }

  struct state s;
  if (setup_state(&s) != 0)
    return EXIT_FAILURE;

  struct stat st;
  if (lstat(path, &st) != 0) {
    fprintf(stderr, "mdu: lstat failed for: %s", path);
    destroy_resources(&s);
    return EXIT_FAILURE;
  }

  char *start_path = strdup(path);
  if (!start_path) {
    fprintf(stderr, "mdu: strdup failed\n");
    destroy_resources(&s);
    return EXIT_FAILURE;
  }

  if (queue_push(&s.queue, start_path) != 0) {
    fprintf(stderr, "mdu: couldnt push to queue\n");
    free(start_path);
    destroy_resources(&s);
    return EXIT_FAILURE;
  }

  s.pending_dirs = 1;

  // Create Threads
  pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
  if (!threads) {
    perror("calloc");
    destroy_resources(&s);

    return EXIT_FAILURE;
  }

  int threads_created = 0;
  for (int i = 0; i < num_threads; i++) {
    int ret = pthread_create(&threads[i], NULL, calculate_path_size, &s);
    if (ret != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(ret));
      shutdown_threads(&s);
      for (int j = 0; j < threads_created; j++) {
        pthread_join(threads[j], NULL);
      }
      destroy_resources(&s);
      free(threads);
      return EXIT_FAILURE;
    }
    threads_created++;
  }

  // Cleanup
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }
  free(threads);

  printf("%lld\t%s\n", s.total_blocks, path);

  destroy_resources(&s);

  return 0;
}
