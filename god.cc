#include <unistd.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/reg.h>

#include <vector>
#include <string.h>
#include <iostream>
#include <cassert>
#include <thread>

const char* child_executable = "organism_runner";

std::vector<std::thread> g_tracing_threads;

void trace_thread(pid_t thread) {
  std::cout << "Tracing thread " << thread << std::endl;
  int wstatus;
  bool exited = false;
  bool expecting_syscall_return = false;
  int64_t awaited_syscall = -1;
  ptrace(PTRACE_SETOPTIONS, thread, NULL, PTRACE_O_TRACECLONE);
  bool expecting_clone_return = false;
  while (!exited) {
    waitpid(thread, &wstatus, 0);
    exited = WIFEXITED(wstatus);
    if (WIFSIGNALED(wstatus)) {
      std::cout << "Runner got signal " << WTERMSIG(wstatus) << std::endl;
    }
    if (WIFSTOPPED(wstatus)) {
      if (WSTOPSIG(wstatus) != SIGTRAP) {
        std::cout << "Runner stopped with signal \"" << strsignal(WSTOPSIG(wstatus)) << "\"" << std::endl;
      } else {
        int64_t orig_rax = ptrace(PTRACE_PEEKUSER, thread, 8 * ORIG_RAX, NULL);
        if (!expecting_syscall_return) {
          std::cout << "Sycall number: " << orig_rax << std::endl;
          if (orig_rax == 56) {
            assert(!expecting_clone_return);
            // Don't allow subprocesses. Only new threads.
            int64_t clone_flag = ptrace(PTRACE_PEEKUSER, thread, 8 * RDX, NULL);
            clone_flag |= CLONE_THREAD | CLONE_PTRACE;
            ptrace(PTRACE_POKEUSER, thread, 8 * RDX, clone_flag);
            std::cout << "Setting CLONE_THREAD and CLONE_PTRACE on new thread." << std::endl;
            expecting_clone_return = true;
          }
          if (orig_rax != 59) { // execve
            expecting_syscall_return = true;
            awaited_syscall = orig_rax;
          }
        } else {
          assert(orig_rax == awaited_syscall);
          int64_t rax = ptrace(PTRACE_PEEKUSER, thread, 8 * RAX, NULL);
          std::cout << "Sycall " << orig_rax << " returned: " << rax << std::endl;
          if (orig_rax == 56) {
            // Successful clone. Now we need to trace it too.
            std::cout << "Detected a clone. Tracing thread " << rax <<  "" << std::endl;
          }
          if (orig_rax == 56) {
            assert(expecting_clone_return);
            // TODO: Keep track of these threads.
            g_tracing_threads.push_back(std::thread(trace_thread, static_cast<pid_t>(rax)));
            expecting_clone_return = false;
          }
          expecting_syscall_return = false;
        }
      }
    } else {
      std::cout << "Unidentifed status " << wstatus << std::endl;
    }
    ptrace(PTRACE_SYSCALL, thread, NULL, NULL);
    // ptrace(PTRACE_CONT, thread, NULL, NULL);
  }
}

void run_god(pid_t organism_runner) {
  trace_thread(organism_runner);
}

int main(int argc, char ** argv) {
  (void)argc;
  (void)argv;

  // TODO: Kill child when the parent dies.
  pid_t organism_runner = fork();
  if (organism_runner == 0) {

    // Kill child when parent dies.
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);

    char * child_args[] = {strdup(child_executable), nullptr};
    std::cout << "Spawning child!" << std::endl;
    execvp(child_executable, child_args);
  } else {
    std::cout << "In parent!" << std::endl;
    run_god(organism_runner);
  }
  return 0;
}
