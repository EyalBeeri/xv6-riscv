#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ALLOC_SIZE (20 * 1024) // 20KB

int
main(int argc, char *argv[])
{
  // (a) Print initial memory usage
  int initial_size = memsize();
  printf("Initial memory size: %d bytes\n", initial_size);
  
  // (b) Allocate 20KB of memory
  char *mem = malloc(ALLOC_SIZE);

  // (c) Print memory usage after allocation
  int after_alloc_size = memsize();
  printf("Memory size after allocation: %d bytes\n", after_alloc_size);
  printf("Difference: %d bytes\n", after_alloc_size - initial_size);
  
  // (d) Free the allocated memory
  free(mem);
  
  // (e) Print memory usage after freeing
  int after_free_size = memsize();
  printf("Memory size after free: %d bytes\n", after_free_size);
  printf("Difference from initial: %d bytes\n", after_free_size - initial_size);
  
  exit(0, "");
}