#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdbool.h>

#define IPC_RESULT_ERROR (-1)

// Function declarations
int get_shared_block(char* filename, int size);
void* attach_memory_block(char* filename, int size);
bool detach_memory_block(void* block);
bool destroy_memory_block(char* filename);

#endif /* SHARED_MEMORY_H */
