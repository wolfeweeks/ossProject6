#include <stdio.h>
#include "structs.h"

// Print time from clock
void print_clock(sclock_t* clock) {
  printf("%d:%d\n", clock->seconds, clock->nanoseconds);
}

// Increase clock time
void increment_clock(sclock_t* clock, unsigned int nanoseconds) {
  unsigned long long total_nanoseconds = clock->nanoseconds + nanoseconds;
  clock->seconds += total_nanoseconds / 1000000000;
  clock->nanoseconds = total_nanoseconds % 1000000000;
}

// Calculate difference between two times
unsigned int time_between_nano(sclock_t clock1, sclock_t clock2) {
  unsigned nanoseconds1 = clock1.seconds * 1000000000U + clock1.nanoseconds;
  unsigned nanoseconds2 = clock2.seconds * 1000000000U + clock2.nanoseconds;
  return nanoseconds1 - nanoseconds2;
}

// Initialize page tables
void initializePageTables(PageTable* pageTables) {
  int i, j;
  for (i = 0; i < 18; i++) {
    for (j = 0; j < 32; j++) {
      pageTables[i].pages[j].frame = -1;
    }
  }
}

// Initialize frame table
void initializeFrameTable(FrameTable* frameTable) {
  int i;
  for (i = 0; i < 256; i++) {
    frameTable->frames[i].occupied = false;
    frameTable->frames[i].reference_byte = 0;
    frameTable->frames[i].dirty_bit = 0;
  }
  frameTable->headIndex = 0;
}

// Print frame table
void printFrameTable(const FrameTable* frameTable, sclock_t* clock) {
  printf("Current memory layout at time %u:%u is:\n", clock->seconds, clock->nanoseconds);
  printf("              Occupied  Dirty Bit  Reference Bit\n");
  printf("----------------------------------------------\n");

  for (int i = 0; i < 256; i++) {
    const Frame* frame = &(frameTable->frames[i]);
    if (frame->occupied) {
      printf("%-13d %-9s %-9d %-13d\n", i, "Yes", frame->dirty_bit, frame->reference_byte);
    }
  }
}

// Get frame from address
int getFrameFromAddr(int address, PageTable* pageTables, int pageTableIndex) {
  int pageNumber = address / 1024;
  return pageTables[pageTableIndex].pages[pageNumber].frame;
}

// Reset page at a given frame
void resetPageAtFrame(int frameNumber, PageTable* pageTables) {
  int i, j;
  for (i = 0; i < 18; i++) {
    for (j = 0; j < 32; j++) {
      if (pageTables[i].pages[j].frame == frameNumber) {
        pageTables[i].pages[j].frame = -1;
        return;
      }
    }
  }
}

void removeProcessPages(FrameTable* frameTable, PageTable* pageTables, int pageTableIndex) {
  int i;
  for (i = 0; i < 32; i++) {
    int frame = pageTables[pageTableIndex].pages[i].frame;
    if (frame != -1) {
      // Mark the frame as unoccupied and reset its attributes
      frameTable->frames[frame].occupied = false;
      frameTable->frames[frame].dirty_bit = 0;
      frameTable->frames[frame].reference_byte = 0;

      // Update the page table to indicate the frame is no longer assigned
      pageTables[pageTableIndex].pages[i].frame = -1;
    }
  }
}

void replacePage(FrameTable* frameTable, int address, PageTable* pageTables, int pageTableIndex) {
  int pageNumber = address / 1024;

  int i;
  for (i = 0; i < 256; i++) {
    int index = frameTable->headIndex + i % 256;
    frameTable->headIndex = index;

    // Check if the frame is not already occupied
    if (!frameTable->frames[index].occupied) {
      // Assign the frame to the page in the page table
      pageTables[pageTableIndex].pages[pageNumber].frame = index;
      frameTable->frames[index].occupied = true;
      return;
    } else {
      // If the reference byte is not triggered, mark it
      if (frameTable->frames[index].reference_byte == 0) {
        frameTable->frames[index].reference_byte = 1;
      } else {
        // Reset the page assigned to the frame and assign the new page
        resetPageAtFrame(index, pageTables);
        pageTables[pageTableIndex].pages[pageNumber].frame = index;
        // printf("%d\n", index);
        frameTable->frames[index].reference_byte = 0;
        frameTable->frames[index].dirty_bit = 0;
      }
    }
  }
}
