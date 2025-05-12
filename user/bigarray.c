#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ARRAYSIZE (1 << 16)  // 2^16
#define NUM_PROCESSES 4

// Making array global to avoid stack overflow
int array[ARRAYSIZE];

int main() {
  int pids[NUM_PROCESSES];
  int n, statuses[64];
  int i, quarter = ARRAYSIZE / NUM_PROCESSES;
  
  // Initialize array with consecutive integers
  for(i = 0; i < ARRAYSIZE; i++) {
    array[i] = i;
  }
  
  // Create child processes
  int ret = forkn(NUM_PROCESSES, pids);
  if(ret < 0) {
    printf("forkn failed\n");
    exit(1, "");
  }
  
  if(ret == 0) {
    // Parent process
    printf("Parent: created %d children with PIDs: ", NUM_PROCESSES);
    for(i = 0; i < NUM_PROCESSES; i++) {
      printf("%d ", pids[i]);
    }
    printf("\n");
    
    // Wait for all children to finish
    if(waitall(&n, statuses) < 0) {
      printf("waitall failed\n");
      exit(1, "");
    }
    
    // Check if we got all children back
    if(n != NUM_PROCESSES) {
      printf("waitall returned %d children, expected %d\n", n, NUM_PROCESSES);
      exit(1, "");
    }
    
    // Sum the results from all children
    long total_sum = 0;
    for(i = 0; i < n; i++) {
      total_sum += statuses[i];
    }
    
    printf("Parent: sum of all sums = %d\n", total_sum);
    exit(0, "Calculation complete");
  } else {
    // Child process - ret is the child number (1, 2, 3, 4)
    int child_num = ret;
    int start = (child_num - 1) * quarter;
    int end = start + quarter;
    
    // Calculate sum for this quarter
    int sum = 0;
    for(i = start; i < end; i++) {
      sum += array[i];
    }
    
    printf("Child %d: sum of quarter %d = %d\n", child_num, child_num, sum);
    
    // Exit with the sum as status
    exit(sum, "");
  }
}