CC = gcc
CFLAGS = -Wall -g -Iinclude -I.

# List of source files for the daemon
DAEMON_SRCS = src/daemon/daemon.c \
              src/daemon/ipc.c \
              src/analyzer.c \
              src/tree.c \
              scheduler.c

# List of source files for the client
CLIENT_SRCS = src/client/da.c

# Build all targets
all: daemon da

# Link the Daemon (needs pthread)
daemon: $(DAEMON_SRCS)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) -o daemon -lpthread

# Link the Client
da: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o da

# Clean up binaries
clean:
	rm -f daemon da