#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PER_ORGANISM_BUFFER_SIZE 256

typedef void (*executable_t)(void *);

typedef struct genome_t {
  size_t size;
  executable_t executable;
} genome_t;

void print_buffer(void *buffer, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    printf("%02X ", ((uint8_t *)buffer)[i]);
  }
  printf("\n");
}

genome_t *load_genome(const char *executable_path) {
  genome_t *genome = malloc(sizeof(genome_t));
  FILE *f = fopen(executable_path, "r");
  if (f == NULL) {
    fprintf(stderr, "Failed to open file '%s'.\n", executable_path);
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  genome->size = ftell(f);
  fseek(f, 0, SEEK_SET);
  genome->executable = (executable_t)mmap(NULL, genome->size + 1,
                                          PROT_EXEC | PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  fread(genome->executable, 1, genome->size, f);
  printf("Genome size: %d\n", genome->size);
  print_buffer(genome->executable, genome->size);
  fclose(f);
  return genome;
}

void free_genome(genome_t *genome) {
  munmap(genome->executable, genome->size + 1);
  free(genome);
}

// The problem is to set c = 10 * (a + b).
typedef struct problem_t {
  uint8_t a;
  uint8_t b;
  uint16_t c;
} problem_t;

problem_t setup_buffer(void *buffer) {
  problem_t problem;
  problem.a = rand() % 12;
  problem.b = rand() % 12;
  problem.c = 10 * (problem.a + problem.b);
  problem_t *buffer_problem = (problem_t *)buffer;
  buffer_problem->a = problem.a;
  buffer_problem->b = problem.b;
  return problem;
}

uint16_t score_buffer(const void *buffer, problem_t problem) {
  // printf("Buffer: %d\n", ((problem_t *)buffer)->c);
  // printf("Problem: %d\n", problem.c);
  if (((problem_t *)buffer)->c >= problem.c) {
    return ((problem_t *)buffer)->c - problem.c;
  } else {
    return problem.c - ((problem_t *)buffer)->c;
  }
}

typedef struct organism_t {
  genome_t *genome;
  problem_t problem;
  void *buffer;
} organism_t;

organism_t *create_organism(const char *genome_path) {
  organism_t *organism = malloc(sizeof(organism_t));
  organism->genome = load_genome(genome_path);
  organism->buffer = malloc(PER_ORGANISM_BUFFER_SIZE);
  memset(organism->buffer, 0, PER_ORGANISM_BUFFER_SIZE);
  organism->problem = setup_buffer(organism->buffer);
  return organism;
}

void free_organism(organism_t *organism) {
  free_genome(organism->genome);
  free(organism->buffer);
  free(organism);
}

// This function donates the calling thread to the execution of the organism.
void *run_organism(void *arg) {
  // TODO: Handle segfaults.
  organism_t *organism = (organism_t *)arg;
  organism->genome->executable(organism->buffer);
  return NULL;
}

void spawn_organism(const char *genome_path) {
  organism_t *organism = create_organism(genome_path);
  // printf("Before: ");
  // print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);
  // printf("After: ");
  // print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);

  pthread_t tid;
  int ret = pthread_create(&tid, NULL, run_organism, organism);
  if (ret != 0) {
    fprintf(stderr, "Failed to spawn new thread for organism.\n");
    exit(1);
  }

  for (size_t i = 0; i < 10; ++i) {
    usleep(500000);

    // TODO: Do this scoring asynchronously.
    printf("Score: %d\n", score_buffer(organism->buffer, organism->problem));
    organism->problem = setup_buffer(organism->buffer);
  }
  free_organism(organism);
}

int main(int argc, char **argv) {
  srand(0);
  spawn_organism("progenitor_organism.o");
  return 0;
}
