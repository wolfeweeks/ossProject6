/**
 * @file oss.c
 * @author Wolfe Weeks
 * @date 2023-05-12
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <errno.h>

#include "shared_memory.h"
#include "structs.h"

sclock_t* sclock;
PageTable* pageTables;
FrameTable* frameTable;

int msgqid;


// This function assigns a process ID (pid) to the first available slot in the process control block (pcb).
void assignProcess(int* pcb, int pid) {
  int i;
  // Iterate over the pcb array
  for (i = 0; i < 18; i++) {
    // If the current slot is unoccupied (-1), assign it the pid
    if (pcb[i] == -1) {
      pcb[i] = pid;
      return;
    };
  }
  return;
}

// This function clears the process ID (pid) from the process control block (pcb).
void clearProcess(int* pcb, int pid) {
  int i;
  // Iterate over the pcb array
  for (i = 0; i < 18; i++) {
    // If the current slot holds the pid, clear it (-1)
    if (pcb[i] == pid) {
      pcb[i] = -1;
      return;
    }
  }
  return;
}

// This function returns the index of a given process ID (pid) in the process control block (pcb), or -1 if it is not found.
int findProcessIndex(int* pcb, int pid) {
  int i;
  // Iterate over the pcb array
  for (i = 0; i < 18; i++) {
    // If the pid is found, return its index
    if (pcb[i] == pid) return i;
  }
  return -1;
}

// This function prints all process IDs (pid) in the process control block (pcb).
void printPIDs(int* pcb) {
  int i;
  // Iterate over the pcb array
  for (i = 0; i < 18; i++) {
    printf("%-5d ", pcb[i]); // Print each PID with a width of 5 characters
    if ((i + 1) % 18 == 0) {
      printf("\n"); // Print a new line after every 18 PIDs
    }
  }
}

// This function pushes a process ID (pid) to the end of a queue.
void pushToQueue(int* queue, int pid) {
  int i;
  // Iterate over the queue array
  for (i = 0; i < 18; i++) {
    // If the current slot is unoccupied (-1), push the pid to it
    if (queue[i] == -1) {
      queue[i] = pid;
      break;
    }
  }
}

// This function removes and returns the process ID (pid) from the front of a queue.
int popQueue(int* queue) {
  int front = queue[0];

  int i;
  // Shift all elements of the queue to the left
  for (i = 0; i < 17; i++) {
    queue[i] = queue[i + 1];
  }
  queue[17] = -1;

  return front;
}

// This function checks if a queue is empty and returns true if it is, or false if it isn't.
bool queueIsEmpty(int* queue) {
  if (queue[0] != -1) return false;
  return true;
}

// Function to push new blocked time into the blockedTimes array
void pushToBlockedTimes(sclock_t* blockedTimes, int seconds, int nano) {
  int i;
  // Loop through the array
  for (i = 0; i < 18; i++) {
    // If this index is not occupied (signified by seconds == -1)
    if (blockedTimes[i].seconds == -1) {
      // Create a new sclock_t object with the specified time
      sclock_t t;
      t.seconds = seconds;
      t.nanoseconds = nano;
      // Put the new object into the array at the current index
      blockedTimes[i] = t;
      // Stop the loop after adding the new time
      break;
    }
  }
}

// Function to pop the first blocked time from the blockedTimes array
void popBlockedTimes(sclock_t* blockedTimes) {
  int i;
  // Shift all elements in the array one position to the left
  for (i = 0; i < 17; i++) {
    blockedTimes[i] = blockedTimes[i + 1];
  }
  // Put a placeholder sclock_t object at the last index to signify it's not occupied
  sclock_t t;
  t.seconds = -1;
  t.nanoseconds = 0;
  blockedTimes[17] = t;
}

// Function to clean up system resources before exiting the program
void clearEverything() {
  // Delete the message queue
  if (msgctl(msgqid, IPC_RMID, NULL) == -1) {
    perror("msgctl failed");
    exit(1);
  }

  // Detach and destroy shared memory blocks
  detach_memory_block((void*)sclock);
  destroy_memory_block("oss.c");
  detach_memory_block((void*)pageTables);
  destroy_memory_block("user_proc.c");
  detach_memory_block((void*)frameTable);
  destroy_memory_block("structs.c");
}

// Function to handle alarm signal (SIGALRM)
void handle_alarm(int signum) {
  printf("\nTerminating due to 2 seconds timeout.\n");
  // Clean up system resources
  clearEverything();
  // Send termination signal to all processes in the current process group
  kill(0, SIGTERM);
  // Exit the current process
  exit(0);
}

// Function to handle interrupt signal (SIGINT, triggered by CTRL-C)
void handle_interrupt(int signum) {
  printf("\nTerminating due to CTRL-C.\n");
  // Clean up system resources
  clearEverything();
  // Send termination signal to all processes in the current process group
  kill(0, SIGTERM);
  // Exit the current process
  exit(0);
}

int main(int argc, char const* argv[]) {
  // Make the main process the group leader
  setpgid(0, 0);

  // Set up signal handling for SIGALRM and SIGINT
  signal(SIGALRM, handle_alarm);
  signal(SIGINT, handle_interrupt);

  // Set alarm for 2 seconds
  alarm(2);

  // Attach shared memory block for logical clock
  sclock = (sclock_t*)attach_memory_block("oss.c", sizeof(sclock_t));
  sclock->seconds = 0;
  sclock->nanoseconds = 0;

  // Attach shared memory block for page tables
  pageTables = (PageTable*)attach_memory_block("user_proc.c", sizeof(PageTable) * 18);
  initializePageTables(pageTables);

  // Attach shared memory block for frame table
  frameTable = (FrameTable*)attach_memory_block("structs.c", sizeof(FrameTable));
  initializeFrameTable(frameTable);

  int max_processes = 18;
  int total_processes = 100;

  int created_children = 0;
  int running_children = 0;

  sclock_t previousLaunchTime;
  previousLaunchTime.nanoseconds = 0;
  previousLaunchTime.seconds = 0;

  // Generate random gap between process launches
  srand(time(0));
  int randGap = (rand() % (500000000 - 1000000 + 1)) + 1000000;

  // Initialize PCB array with -1
  int pcb[18];
  int i;
  for (i = 0; i < 18; i++) {
    pcb[i] = -1;
  }

  // Initialize queue array with -1
  int queue[18];
  for (i = 0; i < 18; i++) {
    queue[i] = -1;
  }

  // Initialize blocked times array with -1
  sclock_t blockedTimes[18];
  for (i = 0; i < 18; i++) {
    sclock_t t;
    t.seconds = -1;
    t.nanoseconds = 0;
    blockedTimes[i] = t;
  }

  // Create a message queue using IPC_CREAT flag
  key_t key = ftok(".", 'm');
  msgqid = msgget(key, IPC_CREAT | 0666);
  if (msgqid == -1) {
    perror("msgget");
    exit(1);
  }

  // Initialize page tables
  initializePageTables(pageTables);

  // Initialize frame table
  initializeFrameTable(frameTable);

  sclock_t previousPCBPrint;
  previousPCBPrint.seconds = 0;
  previousPCBPrint.nanoseconds = 0;

  MemoryRequest request;

  while (true) {
    // Check if all processes have been created and no processes are running
    if (created_children >= total_processes && running_children == 0) {
      break;
    }

    // Print frame table if specified time has elapsed
    if (time_between_nano(*sclock, previousPCBPrint) >= 500000000) {
      printFrameTable(frameTable, sclock);
      previousPCBPrint.seconds = sclock->seconds;
      previousPCBPrint.nanoseconds = sclock->nanoseconds;
    }

    // Check if the queue is not empty
    if (!queueIsEmpty(queue)) {
      for (i = 0; i < 18; i++) {
        if (blockedTimes[i].seconds == -1) break;

        // Check if process has waited long enough in the queue
        int timeBetween = time_between_nano(*sclock, blockedTimes[i]);
        if (timeBetween >= 14000000) {
          int pid = popQueue(queue);
          popBlockedTimes(blockedTimes);

          request.msg_type = pid;

          // Send message to process
          if (msgsnd(msgqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
          }
        }
      }
    }

    // Receive message from the message queue
    if (msgrcv(msgqid, &request, sizeof(MemoryRequest) - sizeof(long), 1, IPC_NOWAIT) == -1) {
      if (errno != ENOMSG) { // Ignore ENOMSG errors
        perror("msgrcv failed");
        exit(1);
      }
    } else {
      request.msg_type = request.pid;
      assignProcess(pcb, request.pid);
      printf("Process %d requesting %s of address %d at time %u:%u\n", request.pid, request.isRead ? "read" : "write", request.address, sclock->seconds, sclock->nanoseconds);

      int frameNumber = getFrameFromAddr(request.address, pageTables, findProcessIndex(pcb, request.pid));
      if (frameNumber == -1) {
        printf("Address %d is not in a frame, pagefault\n", request.address);

        pushToQueue(queue, request.pid);
        pushToBlockedTimes(blockedTimes, sclock->seconds, sclock->nanoseconds);

        replacePage(frameTable, request.address, pageTables, findProcessIndex(pcb, request.pid));
      } else {
        printf("Address %d in frame %d ", request.address, frameNumber);

        increment_clock(sclock, 100);

        if (request.isRead) {
          printf("Giving data to Process %d at time %u:%u\n", request.pid, sclock->seconds, sclock->nanoseconds);

          // Send message to process
          if (msgsnd(msgqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
          }
        } else {
          printf("writing data to frame at time %u:%u\n", sclock->seconds, sclock->nanoseconds);

          frameTable->frames[frameNumber].dirty_bit = 1;
          printf("Dirty bit of frame %d set, adding additional time to the clock\n", frameNumber);

          if (msgsnd(msgqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
          }
        }
      }
    }

    increment_clock(sclock, 5000);
    unsigned int elapsedNano = time_between_nano(*sclock, previousLaunchTime);

    if (elapsedNano >= randGap) {
      // Check if there is room to create a new process
      if (running_children < max_processes && created_children < total_processes) {
        previousLaunchTime.seconds = sclock->seconds;
        previousLaunchTime.nanoseconds = sclock->nanoseconds;

        // Fork a new process.
        pid_t pid = fork();

        // If fork failed.
        if (pid < 0) {
          printf("Fork failed!\n");
          exit(1);
        }// If this is the child process.
        else if (pid == 0) {
          // Set the process group ID of the child process to that of the parent
          setpgid(0, getppid());
          printf("Process %d launched at %u:%u\n", created_children, sclock->seconds, sclock->nanoseconds);

          // Execute the user process
          execl("./user_proc", "./user_proc", NULL);
          exit(1);
        }
        // If this is the parent process.
        else {
          created_children++;
          running_children++;
          randGap = (rand() % (500000000 - 1000000 + 1)) + 1000000;
          previousLaunchTime.nanoseconds = sclock->nanoseconds;
          previousLaunchTime.seconds = sclock->seconds;
        }
      } else {
        // Check if any child processes have finished
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
          printFrameTable(frameTable, sclock);
          removeProcessPages(frameTable, pageTables, findProcessIndex(pcb, pid));
          clearProcess(pcb, pid);
          printf("Process %d terminated\n", pid);
          running_children--;
        }
      }
    }
  }

  // Clear allocated resources
  clearEverything();
  return 0;
}
