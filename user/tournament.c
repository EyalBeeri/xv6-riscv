#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int processes = 16;  // Default to 16 processes
  
  // Parse command line argument if provided
  if (argc > 1) {
    processes = atoi(argv[1]);
    if (!processes || (processes & (processes - 1)) || processes > 16) {
      printf("Number of processes must be a power of 2 and <= 16\n");
      exit(1);
    }
  }
  
  // Create tournament tree
  int id = tournament_create(processes);
  if (id < 0) {
    printf("Tournament creation failed\n");
    exit(1);
  }
  
  // Try to acquire the lock
  if (tournament_acquire() < 0) {
    printf("Process %d failed to acquire tournament lock\n", id);
    exit(1);
  }
  
  // Critical section
  printf("PID %d (Tournament ID %d) entered the critical section\n", getpid(), id);
  sleep(1);  // Hold for a moment to demonstrate mutual exclusion
  
  // Release the lock
  if (tournament_release() < 0) {
    printf("Process %d failed to release tournament lock\n", id);
    exit(1);
  }
  
  exit(0);
}