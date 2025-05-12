#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

int
sys_peterson_create(void)
{
  return peterson_create();
}

int
sys_peterson_acquire(void)
{
  int lock_id, role;
  argint(0, &lock_id);
  argint(1, &role);
  
  if(lock_id < 0 || role < 0)
    return -1;
  
  return peterson_acquire(lock_id, role);
}

int
sys_peterson_release(void)
{
  int lock_id, role;
  argint(0, &lock_id);
  argint(1, &role);
  
  if(lock_id < 0 || role < 0)
    return -1;
  
  return peterson_release(lock_id, role);
}

int
sys_peterson_destroy(void)
{
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0)
    return -1;
  
  return peterson_destroy(lock_id);
}