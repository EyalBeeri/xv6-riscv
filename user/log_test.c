#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_CHILDREN 4
#define BUFFER_SIZE 4096
#define MAX_MESSAGES 30

// Header format: 16 bits child index, 16 bits message length
typedef uint header_t;

// Helper function to create a header from child index and message length
header_t make_header(uint16 child_idx, uint16 msg_len) {
    return ((uint)child_idx << 16) | msg_len;
}

// Helper functions to extract child index and message length from header
uint16 get_child_idx(header_t header) {
    return (uint16)(header >> 16);
}

uint16 get_msg_len(header_t header) {
    return (uint16)(header & 0xFFFF);
}

// Helper function to align address to 4-byte boundary
uint64 align_addr(uint64 addr) {
    return (addr + 3) & ~3;
}

// Function for child processes to write messages to the shared buffer
void write_messages(header_t *buf_start, int child_idx) {
    uint64 addr = (uint64)buf_start;
    uint64 buf_end = addr + BUFFER_SIZE;
    char message[100];
    
    // First child writes more messages to test buffer overflow
    int max_messages = (child_idx == 0) ? MAX_MESSAGES * 2 : MAX_MESSAGES;
    
    // Each child writes multiple messages
    for (int msg_num = 0; msg_num < max_messages; msg_num++) {
        // Vary message length based on message number and child index
        int extra_len = (msg_num + child_idx) % 20;
        
        // Create message with child index and message number
        char base_msg[50];
        strcpy(base_msg, "Message from child ");
        base_msg[18] = '0' + child_idx;
        base_msg[19] = ':';
        base_msg[20] = ' ';
        base_msg[21] = '#';
        base_msg[22] = '0' + (msg_num / 10);
        base_msg[23] = '0' + (msg_num % 10);
        base_msg[24] = '\0';
        
        // Copy base message
        strcpy(message, base_msg);
        
        // Add extra characters to vary message length
        int base_len = strlen(message);
        for (int i = 0; i < extra_len && base_len + i + 1 < sizeof(message); i++) {
            message[base_len + i] = 'a' + (i % 26);
        }
        message[base_len + extra_len] = '\0';
        
        uint16 msg_len = strlen(message);
        header_t new_header = make_header(child_idx, msg_len);
        
        // Try to find an empty slot (max 100 attempts)
        int attempts = 0;
        int max_attempts = 100;
        
        while (attempts < max_attempts && addr + sizeof(header_t) + msg_len < buf_end) {
            header_t *header_ptr = (header_t*)addr;
            
            // Try to atomically claim this slot
            if (__sync_val_compare_and_swap(header_ptr, 0, new_header) == 0) {
                // Successfully claimed the slot, write the message
                char *msg_ptr = (char*)(addr + sizeof(header_t));
                memcpy(msg_ptr, message, msg_len);
                
                // Advance to next potential header location (aligned)
                addr = align_addr(addr + sizeof(header_t) + msg_len);
                break;
            }
            
            // Slot was already taken, skip to next potential header
            uint16 existing_len = get_msg_len(*header_ptr);
            addr = align_addr(addr + sizeof(header_t) + existing_len);
            attempts++;
        }
        
        // If we've reached the end of the buffer, stop writing
        if (addr + sizeof(header_t) >= buf_end) {
            printf("Child %d: buffer full after writing %d messages\n", child_idx, msg_num);
            break;
        }
        
        // If we've tried too many times, wrap around to the beginning
        if (attempts >= max_attempts) {
            addr = (uint64)buf_start;
        }
        
        // Small delay to allow other children to write
        for (volatile int i = 0; i < 1000; i++);
    }
}

// Function for parent to read messages from the shared buffer
void read_messages(header_t *buf_start) {
    uint64 addr = (uint64)buf_start;
    uint64 buf_end = addr + BUFFER_SIZE;
    int msg_count = 0;
    int child_count[NUM_CHILDREN] = {0};
    
    printf("\nParent reading messages from shared buffer:\n");
    printf("--------------------------------------\n");
    
    while (addr + sizeof(header_t) < buf_end) {
        header_t *header_ptr = (header_t*)addr;
        header_t header = *header_ptr;
        
        if (header == 0) {
            // Skip empty slots
            addr += sizeof(header_t);
            continue;
        }
        
        uint16 child_idx = get_child_idx(header);
        uint16 msg_len = get_msg_len(header);
        
        // Read the message
        if (addr + sizeof(header_t) + msg_len <= buf_end) {
            char message[256];
            if (msg_len < sizeof(message)) {
                memcpy(message, (char*)(addr + sizeof(header_t)), msg_len);
                message[msg_len] = '\0';
                printf("Message %d: [Child %d] %s\n", msg_count, child_idx, message);
                msg_count++;
                
                if (child_idx < NUM_CHILDREN) {
                    child_count[child_idx]++;
                }
            }
        }
        
        // Move to next header (aligned)
        addr = align_addr(addr + sizeof(header_t) + msg_len);
    }
    
    printf("--------------------------------------\n");
    printf("Parent finished reading %d messages\n", msg_count);
    
    // Print message count per child
    for (int i = 0; i < NUM_CHILDREN; i++) {
        printf("  Child %d: %d messages\n", i, child_count[i]);
    }
}

int main() {
    void *shared_buf;
    int parent_pid = getpid();
    
    // Allocate shared memory buffer in parent
    shared_buf = malloc(BUFFER_SIZE);
    if (shared_buf == 0) {
        printf("Failed to allocate memory\n");
        exit(1);
    }
    
    // Initialize the buffer with zeros
    memset(shared_buf, 0, BUFFER_SIZE);
    
    printf("Parent process %d created shared buffer at %p\n", parent_pid, shared_buf);
    
    // Fork child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("Fork failed for child %d\n", i);
            exit(1);
        }
        
        if (pid == 0) {
            // Child process
            printf("Child %d started with pid %d\n", i, getpid());
            
            // Map shared memory from parent
            uint64 addr = map_shared_pages(parent_pid, shared_buf, BUFFER_SIZE);
            if (addr == 0) {
                printf("Child %d: mapping failed\n", i);
                exit(1);
            }
            
            printf("Child %d mapped shared buffer at %p\n", i, (void*)addr);
            
            // Write messages to the shared buffer
            write_messages((header_t*)addr, i);
            
            printf("Child %d finished writing messages\n", i);
            exit(0);
        }
    }
    
    // Parent waits a bit to allow children to start writing
    sleep(10);
    
    // Read messages a few times to show progress
    for (int i = 0; i < 3; i++) {
        read_messages((header_t*)shared_buf);
        sleep(10);
    }
    
    // Wait for all children to exit
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(0);
    }
    
    // Final read of all messages
    read_messages((header_t*)shared_buf);
    
    // Clean up
    free(shared_buf);
    
    return 0;
}