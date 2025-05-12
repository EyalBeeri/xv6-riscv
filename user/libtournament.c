#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Global variables for tournament state
static int *locks = 0;            // Array of Peterson locks
static int process_index = -1;    // Process index in tournament (0 to N-1)
static int num_processes = 0;     // Total number of processes
static int num_levels = 0;        // Number of levels in tree
static int *held_locks = 0;       // Track which locks this process holds

// Calculate log2 of n (assumes n is a power of 2)
static int log2(int n) {
  int result = 0;
  while (n > 1) {
    n >>= 1;
    result++;
  }
  return result;
}

// Check if n is a power of 2
static int is_power_of_2(int n) {
  return n > 0 && (n & (n - 1)) == 0;
}

int tournament_create(int processes) {
  // Validate input
  if (!is_power_of_2(processes) || processes > 16 || processes < 2)
    return -1;
  
  // Initialize globals
  num_processes = processes;
  num_levels = log2(processes);
  
  // Calculate total number of locks needed (processes-1 for a binary tree)
  int total_locks = processes - 1;
  
  // Allocate memory for locks
  locks = malloc(total_locks * sizeof(int));
  if (!locks)
    return -1;
  
  // Create all Peterson locks
  for (int i = 0; i < total_locks; i++) {
    locks[i] = peterson_create();
    if (locks[i] < 0) {
      // Lock creation failed
      for (int j = 0; j < i; j++) {
        peterson_destroy(locks[j]);
      }
      free(locks);
      locks = 0;
      return -1;
    }
  }
  
  // Allocate memory to track held locks
  held_locks = malloc(num_levels * sizeof(int));
  if (!held_locks) {
    for (int i = 0; i < total_locks; i++) {
      peterson_destroy(locks[i]);
    }
    free(locks);
    locks = 0;
    return -1;
  }
  
  // Initialize held_locks to indicate no locks are held
  for (int i = 0; i < num_levels; i++) {
    held_locks[i] = -1;
  }
  
  // Fork processes and assign indices
  process_index = 0;  // Parent gets index 0
  for (int i = 1; i < processes; i++) {
    int pid = fork();
    if (pid < 0) {
      // Fork failed, but we don't clean up as per requirements
      return -1;
    } else if (pid == 0) {
      // Child process
      process_index = i;
      break;  // Child breaks out of loop
    }
  }
  
  return process_index;
}

int tournament_acquire(void) {
  if (process_index < 0 || !locks || !held_locks)
    return -1;
  
  // Acquire locks from bottom to top (leaf to root)
  for (int l = num_levels - 1; l >= 0; l--) {
    // Calculate role for this level
    int bit_pos = num_levels - 1 - l;
    int role = (process_index & (1 << bit_pos)) >> bit_pos;
    
    // Calculate lock index for this level
    int lock_idx = process_index >> (num_levels - l);
    
    // Calculate array index for the lock
    int array_idx = lock_idx + (1 << l) - 1;
    
    // Acquire the lock
    if (peterson_acquire(locks[array_idx], role) < 0) {
      // Failed to acquire lock, release any held locks
      for (int i = num_levels - 1; i > l; i--) {
        if (held_locks[i] >= 0) {
          int rel_bit_pos = num_levels - 1 - i;
          int rel_role = (process_index & (1 << rel_bit_pos)) >> rel_bit_pos;
          peterson_release(locks[held_locks[i]], rel_role);
          held_locks[i] = -1;
        }
      }
      return -1;
    }
    
    // Record that we've acquired this lock
    held_locks[l] = array_idx;
  }
  
  return 0;
}

int tournament_release(void) {
  if (process_index < 0 || !locks || !held_locks)
    return -1;
  
  // Release locks from top to bottom (root to leaf)
  for (int l = 0; l < num_levels; l++) {
    if (held_locks[l] < 0)
      continue;  // Skip locks we don't hold
    
    // Get lock array index
    int array_idx = held_locks[l];
    
    // Calculate role for this level
    int bit_pos = num_levels - 1 - l;
    int role = (process_index & (1 << bit_pos)) >> bit_pos;
    
    // Release the lock
    if (peterson_release(locks[array_idx], role) < 0) {
      return -1;
    }
    
    held_locks[l] = -1;  // Mark as released
  }
  
  return 0;
}