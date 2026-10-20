CC      ?= cc
CFLAGS  := -Wall -Wextra -std=c11 -g
LDFLAGS :=

TARGETS := server client

.PHONY: all clean run

all: $(TARGETS)

server: server.o
	$(CC) $(CFLAGS) -o $@ server.o $(LDFLAGS)

client: client.o
	$(CC) $(CFLAGS) -o $@ client.o $(LDFLAGS)

%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./server & SERVER_PID=$$$$; \
	sleep 0.3; \
	./client; \
	kill $$SERVER_PID 2>/dev/null || true

clean:
	rm -f $(TARGETS) *.o
	rm -f /tmp/unix_dgram_server.sock /tmp/unix_dgram_client.sock
