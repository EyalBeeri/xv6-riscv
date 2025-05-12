#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAX_PETERSON_LOCKS 16

struct petersonlock {
  int active;          // Is this lock slot in use?
  int interested[2];         // Process 0 and 1's intent to enter critical section
  int turn;            // Whose turn is it to enter the critical section
  struct spinlock lk;  // Protects allocation/deallocation of the lock
};

struct {
  struct spinlock lock;
  struct petersonlock locks[MAX_PETERSON_LOCKS];
} ptable;

void
petersoninit(void)
{
  initlock(&ptable.lock, "petersontable");
  for(int i = 0; i < MAX_PETERSON_LOCKS; i++) {
    initlock(&ptable.locks[i].lk, "peterson");
    ptable.locks[i].active = 0;
    ptable.locks[i].interested[0] = 0;
    ptable.locks[i].interested[1] = 0;
    ptable.locks[i].turn = 0;
  }
}

int
peterson_create(void)
{
  int i;
  
  acquire(&ptable.lock);
  for(i = 0; i < MAX_PETERSON_LOCKS; i++) {
    if(ptable.locks[i].active == 0) {
      ptable.locks[i].active = 1;
      ptable.locks[i].interested[0] = 0;
      ptable.locks[i].interested[1] = 0;
      ptable.locks[i].turn = 0;
      release(&ptable.lock);
      return i;
    }
  }
  release(&ptable.lock);
  return -1;  // No free locks
}

int
peterson_acquire(int lock_id, int role)
{
  if(lock_id < 0 || lock_id >= MAX_PETERSON_LOCKS || role < 0 || role > 1)
    return -1;

  acquire(&ptable.lock);
  if(!ptable.locks[lock_id].active) {
    release(&ptable.lock);
    return -1;
  }
  release(&ptable.lock);

  struct petersonlock *lock = &ptable.locks[lock_id];
  int other = 1 - role;  // The other role

  // Indicate intent to enter critical section
  __sync_lock_test_and_set(&lock->interested[role], 1);
  __sync_synchronize();  // Memory barrier

  // Give priority to the other process
  __sync_lock_test_and_set(&lock->turn, other);
  __sync_synchronize();  // Memory barrier

  // Wait until the other process doesn't want to enter or it's our turn
  while(lock->interested[other] && lock->turn == other) {
    yield();  // Yield CPU instead of busy-waiting
    __sync_synchronize();  // Memory barrier before re-checking
  }

  return 0;
}

int
peterson_release(int lock_id, int role)
{
  if(lock_id < 0 || lock_id >= MAX_PETERSON_LOCKS || role < 0 || role > 1)
    return -1;

  acquire(&ptable.lock);
  if(!ptable.locks[lock_id].active) {
    release(&ptable.lock);
    return -1;
  }
  release(&ptable.lock);

  // Release the lock
  __sync_synchronize();  // Memory barrier
  __sync_lock_release(&ptable.locks[lock_id].interested[role]);
  
  return 0;
}

int
peterson_destroy(int lock_id)
{
  if(lock_id < 0 || lock_id >= MAX_PETERSON_LOCKS)
    return -1;
    
  acquire(&ptable.lock);
  if(!ptable.locks[lock_id].active) {
    release(&ptable.lock);
    return -1;
  }
  
  // Reset and mark as inactive
  ptable.locks[lock_id].active = 0;
  ptable.locks[lock_id].interested[0] = 0;
  ptable.locks[lock_id].interested[1] = 0;
  ptable.locks[lock_id].turn = 0;
  
  release(&ptable.lock);
  return 0;
}