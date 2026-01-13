CC = gcc
# Adaugam -Iinclude pentru headerele din folderul include
# Adaugam -I. pentru scheduler.h care e in radacina
CFLAGS = -Wall -g -Iinclude -I.

# LISTA SURSE DAEMON
# Atentie: Conform imaginii, analyzer.c si tree.c sunt in src/daemon/
DAEMON_SRCS = src/daemon/daemon.c \
              src/daemon/ipc.c \
              src/analyzer.c \
              src/tree.c \
              scheduler.c

# LISTA SURSE CLIENT
CLIENT_SRCS = src/client/da.c

# REGULI DE COMPILARE
all: daemon da

# Compilare Daemon (Server)
# NOTA: Linia de sub 'daemon:' TREBUIE sa inceapa cu un TAB, nu spatii!
daemon: $(DAEMON_SRCS)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) -o daemon -lpthread

# Compilare Client (da)
da: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o da

clean:
	rm -f daemon da