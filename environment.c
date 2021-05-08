#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef void (*executable_t)(uint32_t *);

typedef struct genome_t {
  size_t size;
  executable_t executable;
} genome_t;

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
  for (size_t i = 0; i < genome->size; ++i) {
    printf("%02X ", ((uint8_t *)genome->executable)[i]);
  }
  printf("\n");
  fclose(f);
  return genome;
}

void free_genome(genome_t *genome) {
  munmap(genome->executable, genome->size + 1);
  free(genome);
}

int main(int argc, char **argv) {
  genome_t *genome = load_genome("progenitor_organism.o");
  int32_t to_modify = 0x0;
  printf("Before: %llx\n", to_modify);
  genome->executable(&to_modify);
  printf("After: %llx\n", to_modify);
  free_genome(genome);
  return 0;
}
