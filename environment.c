#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PER_ORGANISM_BUFFER_SIZE 256
#define FITNESS_WINDOW_SIZE 16
#define MAX_ERROR 200

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
  genome->size = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  genome->executable = (executable_t)mmap(NULL, genome->size + 1,
                                          PROT_EXEC | PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  fread(genome->executable, 1, genome->size, f);
  printf("Genome size: %ld\n", genome->size);
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

  // We don't want to ever get a zero result, as this would cause division by
  // zero in the scoring procedure.
  problem.a = (uint8_t)(rand() % 11) + 1;
  problem.b = (uint8_t)(rand() % 11) + 1;
  problem.c =
      (uint16_t)((uint16_t)problem.a + (uint16_t)problem.b) * (uint16_t)10;
  problem_t *buffer_problem = (problem_t *)buffer;
  buffer_problem->a = problem.a;
  buffer_problem->b = problem.b;
  return problem;
}

double score_buffer(const void *buffer, problem_t problem) {
  printf("Buffer: %d\n", ((problem_t *)buffer)->c);
  printf("Problem: %d\n", problem.c);

  return fabs((double)((problem_t *)buffer)->c - problem.c) / (double)problem.c;
}

// Circular array to store fitnesses.
typedef struct fitness_t {
  uint64_t epoch;
  double data[FITNESS_WINDOW_SIZE];
} fitness_t;

fitness_t *create_fitness() {
  fitness_t *fitness = malloc(sizeof(fitness_t));
  fitness->epoch = 0;
  memset(fitness->data, 0, FITNESS_WINDOW_SIZE * sizeof(double));
  return fitness;
}

void fitness_add(fitness_t *fitness, double datum) {
  fitness->data[fitness->epoch++ % FITNESS_WINDOW_SIZE] = datum;
}

double fitness_average(fitness_t *fitness) {
  double average = 0.0;
  size_t size = fitness->epoch < FITNESS_WINDOW_SIZE ? fitness->epoch
                                                     : FITNESS_WINDOW_SIZE;
  for (size_t i = 0; i < size; ++i) {
    // printf("Adding %f to average\n", fitness->data[i] / (double)size);
    average += fitness->data[i] / (double)size;
  }
  // printf("Returning average %f\n", average);
  return average;
}

typedef struct organism_t {
  genome_t *genome;
  problem_t problem;
  void *buffer;
  pthread_t tid;
  fitness_t *fitness;
} organism_t;

organism_t *create_organism(const char *genome_path) {
  organism_t *organism = malloc(sizeof(organism_t));
  organism->genome = load_genome(genome_path);
  organism->buffer = malloc(PER_ORGANISM_BUFFER_SIZE);
  memset(organism->buffer, 0, PER_ORGANISM_BUFFER_SIZE);
  organism->problem = setup_buffer(organism->buffer);
  organism->tid = (pthread_t)-1;
  organism->fitness = create_fitness();
  return organism;
}

void free_organism(organism_t *organism) {
  free_genome(organism->genome);
  free(organism->buffer);
  free(organism->fitness);
  free(organism);
}

__thread organism_t *g_organism = NULL;

void organism_signal_handler(int sig) {
  fprintf(stderr, "Caught signal %d in organism %p. Terminating.\n", sig,
          g_organism);
  pthread_exit(NULL);
}

// This function donates the calling thread to the execution of the organism.
void *run_organism(void *arg) {
  struct sigaction action;
  action.sa_handler = organism_signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGSEGV, &action, NULL);
  organism_t *organism = (organism_t *)arg;
  g_organism = organism;
  organism->genome->executable(organism->buffer);
  return NULL;
}

void spawn_organism(const char *genome_path) {
  organism_t *organism = create_organism(genome_path);
  // printf("Before: ");
  // print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);
  // printf("After: ");
  // print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);

  int ret = pthread_create(&(organism->tid), NULL, run_organism, organism);
  if (ret != 0) {
    fprintf(stderr, "Failed to spawn new thread for organism.\n");
    exit(1);
  }

  // TODO: Pull this up a level in the hierarchy and manage multiple organisms.
  for (size_t i = 0; i < 5; ++i) {
    usleep(500000);
    double accuracy_score = score_buffer(organism->buffer, organism->problem);
    fitness_add(organism->fitness, accuracy_score);
    printf("Current score: %f\n", accuracy_score);
    double average = fitness_average(organism->fitness);
    printf("Average score: %f\n", average);
    if (average > MAX_ERROR) {
      pthread_kill(organism->tid, SIGSEGV);
    }
    organism->problem = setup_buffer(organism->buffer);
  }

  free_organism(organism);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  srand(0);
  spawn_organism("progenitor_organism.o");
  return 0;
}
