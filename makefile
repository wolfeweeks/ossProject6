CC = gcc
CFLAGS = -Wall -Wextra -g

# Define the executable names
OSS_EXEC = oss
USER_PROC_EXEC = user_proc

# Define the source files
OSS_SRC = oss.c shared_memory.c structs.c
USER_PROC_SRC = user_proc.c shared_memory.c structs.c

# Define the dependencies
OSS_DEPS = shared_memory.h structs.h
USER_PROC_DEPS = shared_memory.h structs.h

.PHONY: all clean

all: $(OSS_EXEC) $(USER_PROC_EXEC)

$(OSS_EXEC): $(OSS_SRC) $(OSS_DEPS)
	$(CC) $(CFLAGS) -o $@ $^

$(USER_PROC_EXEC): $(USER_PROC_SRC) $(USER_PROC_DEPS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OSS_EXEC) $(USER_PROC_EXEC)
