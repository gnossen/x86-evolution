#include <unistd.h>
#include <string.h>
#include <iostream>

const char* child_executable = "organism_runner";

int main(int argc, char ** argv) {
  (void)argc;
  (void)argv;
  pid_t child_pid = fork();
  if (child_pid == 0) {
    char * child_args[] = {strdup(child_executable), NULL};
    std::cout << "Spawning child!" << std::endl;
    execvp(child_executable, child_args);
  } else {
    std::cout << "In parent!" << std::endl;
    sleep(15);
  }
  return 0;
}
