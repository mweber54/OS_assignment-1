CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread -D_DEFAULT_SOURCE
LDFLAGS=-pthread -lrt

all: producer consumer

producer: producer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

consumer: consumer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f producer consumer
