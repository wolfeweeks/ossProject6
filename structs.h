#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdbool.h>

typedef struct {
  unsigned int seconds;
  unsigned int nanoseconds;
} sclock_t;

typedef struct {
  int frame;
} Page;

typedef struct {
  Page pages[32];
} PageTable;

typedef struct {
  bool occupied;
  int reference_byte;
  int dirty_bit;
} Frame;

typedef struct {
  Frame frames[256];
  int headIndex;
} FrameTable;

typedef struct {
  long msg_type;
  int pid;
  int address;
  bool isRead;
} MemoryRequest;

void print_clock(sclock_t* clock);
void increment_clock(sclock_t* clock, unsigned int nanoseconds);
unsigned int time_between_nano(sclock_t clock1, sclock_t clock2);

void initializePageTables(PageTable* pageTables);
void initializeFrameTable(FrameTable* frameTable);
void printFrameTable(const FrameTable* frameTable, sclock_t* clock);

int getFrameFromAddr(int address, PageTable* pageTables, int pageTableIndex);
void resetPageAtFrame(int frameNumber, PageTable* pageTables);
void removeProcessPages(FrameTable* frameTable, PageTable* pageTables, int pageTableIndex);
void replacePage(FrameTable* frameTable, int address, PageTable* pageTables, int pageTableIndex);

#endif /* STRUCTS_H */
