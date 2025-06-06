#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *message = "Hello daddy";

void
print_size(char *label, int pid)
{
  uint64 size = (uint64)sbrk(0);  // Change from int to uint64
  printf("%s (pid %d): Size is %d bytes\n", label, pid, (int)size);
}

void
test_shared_memory(int unmap_in_child)
{
  int pid;
  void *shared_mem;
  void *new_mem;
  uint64 original_size;
  int parent_pid = getpid();  // Get parent PID before forking
  
  // Allocate memory in parent
  shared_mem = malloc(4096);
  if(shared_mem == 0) {
    printf("Failed to allocate memory\n");
    exit(1);
  }
  
  // Record original size before forking
  original_size = (uint64)sbrk(0);
  print_size("Parent before fork", getpid());
  
  pid = fork();
  if(pid < 0) {
    printf("Fork failed\n");
    exit(1);
  }
  
  if(pid == 0) {
    // Child process
    uint64 child_original_size = (uint64)sbrk(0);
    print_size("Child before mapping", getpid());
    
    // Map memory from parent using stored parent_pid instead of getppid()
    uint64 addr = map_shared_pages(parent_pid, shared_mem, 4096);
    if(addr == 0) {
      printf("Child: mapping failed\n");
      exit(1);
    }
    
    print_size("Child after mapping", getpid());
    
    // Write to shared memory
    char *shared_buf = (char *)addr;
    strcpy(shared_buf, message);
    
    if(unmap_in_child) {
      // Unmap shared memory
      if(unmap_shared_pages((void *)addr, 4096) < 0) {
        printf("Child: unmapping failed\n");
        exit(1);
      }
      
      print_size("Child after unmapping", getpid());
      
      // Try to allocate new memory
      new_mem = malloc(2048);
      if(new_mem == 0) {
        printf("Child: failed to allocate new memory\n");
        exit(1);
      }
      
      print_size("Child after malloc", getpid());
      free(new_mem);
      
      // Verify that size is back to original
      if((uint64)sbrk(0) != child_original_size) {
        printf("Child: size not restored to original after cleanup\n");
      } else {
        printf("Child: size successfully restored to original\n");
      }
    } else {
      printf("Child: not unmapping shared memory before exit\n");
    }
    
    exit(0);
  } else {
    // Parent process
    sleep(10); // Give child time to write
    
    // Read from shared memory
    printf("Parent: shared memory contains: %s\n", (char*)shared_mem);
    
    wait(0);
    
    if(!unmap_in_child) {
      // If child didn't unmap, parent should still be able to access the memory
      printf("Parent: after child exit, shared memory contains: %s\n", (char*)shared_mem);
    }
    
    free(shared_mem);
    
    // Verify that size is back to original
    if((uint64)sbrk(0) != original_size) {
      printf("Parent: size not restored to original after cleanup\n");
    } else {
      printf("Parent: size successfully restored to original\n");
    }
  }
}

int
main(int argc, char *argv[])
{
  printf("\n=== Test with child unmapping shared memory ===\n");
  test_shared_memory(1);
  
  sleep(50);
  
  printf("\n=== Test with child NOT unmapping shared memory ===\n");
  test_shared_memory(0);
  
  exit(0);
}