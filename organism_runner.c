#define _GNU_SOURCE

#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>

#define PER_ORGANISM_BUFFER_SIZE 256

const int g_resumable_signals[] = {
    SIGABRT, SIGALRM, SIGBUS, SIGCHLD, SIGCONT, SIGFPE, SIGHUP, SIGILL, SIGINT,
    /* _not_ SIGKILL */
    SIGPIPE, SIGPOLL, SIGQUIT, SIGSEGV, SIGSTOP, SIGSYS,
    /* _not_ SIGTRAP. We want this for debugging */
};

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

typedef struct organism_t {
  genome_t *genome;
  void *buffer;
  pthread_t tid;
} organism_t;

organism_t *create_organism(const char *genome_path) {
  organism_t *organism = malloc(sizeof(organism_t));
  organism->genome = load_genome(genome_path);
  organism->buffer = malloc(PER_ORGANISM_BUFFER_SIZE);
  memset(organism->buffer, 0, PER_ORGANISM_BUFFER_SIZE);
  // TODO: Set up problem in God process.
  organism->tid = (pthread_t)-1;
  return organism;
}

void free_organism(organism_t *organism) {
  free_genome(organism->genome);
  free(organism->buffer);
  free(organism);
}

__thread organism_t *g_organism = NULL;

void organism_kill_handler(int sig) {
  (void)sig;
  // TODO: No stdlib function calls in a signal handler.
  fprintf(stderr, "Organism %p terminated by environment.\n", g_organism);
  // TODO: May have to clear all other signal handlers before exiting the
  // thread.
  for (size_t i = 0; i < sizeof(g_resumable_signals); ++i) {
    signal(g_resumable_signals[i], SIG_IGN);
  }
  pthread_exit(NULL);
}

void organism_signal_handler(int cause, siginfo_t *info, void *uap) {
  (void)cause;
  (void)info;
  ucontext_t *context = uap;
  // printf("Signal '%s' with RIP 0x%llx (executable + 0x%llx)\n",
  // strsignal(cause), context->uc_mcontext.gregs[REG_RIP],
  // context->uc_mcontext.gregs[REG_RIP] - (long long
  // int)g_organism->genome->executable);
  // Seek to the next nop.
  int found_nop = 0;
  uint8_t *byte;
  for (byte = (uint8_t *)context->uc_mcontext.gregs[REG_RIP] + 1;
       byte >= (uint8_t *)context->uc_mcontext.gregs[REG_RIP] &&
       byte < (uint8_t *)((uint64_t)context->uc_mcontext.gregs[REG_RIP] +
                          g_organism->genome->size);
       ++byte) {
    if (*byte == 0x90) {
      found_nop = 1;
      break;
    }
  }
  if (found_nop) {
    context->uc_mcontext.gregs[REG_RIP] = (greg_t)byte;
  } else {
    context->uc_mcontext.gregs[REG_RIP] =
        (greg_t)g_organism->genome->executable;
  }
  // printf("Set RIP to 0x%llx (executable + 0x%llx)\n",
  // context->uc_mcontext.gregs[REG_RIP], context->uc_mcontext.gregs[REG_RIP] -
  // (long long int)g_organism->genome->executable);
}

// This function donates the calling thread to the execution of the organism.
void *run_organism(void *arg) {
  struct sigaction kill_action;
  kill_action.sa_handler = organism_kill_handler;
  sigemptyset(&kill_action.sa_mask);
  kill_action.sa_flags = 0;
  sigaction(SIGTERM, &kill_action, NULL);
  sigaction(SIGKILL, &kill_action, NULL);

  struct sigaction general_action;
  general_action.sa_sigaction = organism_signal_handler;
  sigemptyset(&general_action.sa_mask);
  general_action.sa_flags = SA_SIGINFO;
  for (size_t i = 0; i < sizeof(g_resumable_signals); ++i) {
    sigaction(g_resumable_signals[i], &general_action, NULL);
  }

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

  sleep(2);

  // // TODO: Pull this up a level in the hierarchy and manage multiple organisms.
  for (size_t i = 0; i < 5; ++i) {
    usleep(500000);
    print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);
  }

  pthread_exit(NULL);

  // pthread_kill(organism->tid, SIGTERM);

  // for (size_t i = 0; i < 5; ++i) {
  //   usleep(500000);
  //   print_buffer(organism->buffer, PER_ORGANISM_BUFFER_SIZE);
  // }

  // free_organism(organism);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  srand(0);
  spawn_organism("progenitor_organism.o");
  return 0;
}
