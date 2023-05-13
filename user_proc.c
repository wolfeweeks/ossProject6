/**
 * @file user_proc.c
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

int main(int argc, char const* argv[]) {
  srand(time(NULL) ^ getpid());

  int msgqid;
  key_t key = ftok(".", 'm');
  msgqid = msgget(key, IPC_CREAT | 0666);
  if (msgqid == -1) {
    perror("msgget");
    exit(1);
  }

  int termInterval = rand() % 201 + 900; // Random termination interval between 900 and 1100
  int requestsSinceLastCheck = 0; // Counter to keep track of the number of memory requests since the last termination interval check

  while (true) {
    if (requestsSinceLastCheck >= termInterval) {
      int shouldTerm = rand() % 2; // Randomly decide whether to terminate or continue
      if (shouldTerm) {
        return 0; // Terminate the process
      } else {
        int termInterval = rand() % 201 + 900; // Generate a new termination interval for the next round
        requestsSinceLastCheck = 0; // Reset the counter
      }
    }

    int addr = rand() % 32 * 1024 + rand() % 1024; // Generate a random memory address within the accessible range

    MemoryRequest request;
    request.msg_type = 1;
    request.pid = getpid();
    request.address = addr;
    request.isRead = rand() % 100 < 75 ? true : false; // Randomly determine whether it's a read or write request

    if (msgsnd(msgqid, &request, sizeof(request) - sizeof(long), 0) == -1) {
      perror("msgsnd");
      exit(1);
    }
    requestsSinceLastCheck++;

    if (msgrcv(msgqid, &request, sizeof(MemoryRequest) - sizeof(long), getpid(), 0) == -1) {
      if (errno != ENOMSG) { // Ignore ENOMSG errors
        perror("msgrcv failed");
        exit(1);
      }
    }
  }

  return 0;
}