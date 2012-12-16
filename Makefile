PREFIX=/opt/universal
CC = gcc
CFLAGS=-Wall -O0 -ggdb -I$(PREFIX)/include
LDFLAGS=-L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib -lzmq

all: zmq2 zmq3

zmq2: universal-pubsub universal-sub-pubsub

zmq3: universal-sub-xpubxsub universal-xpubxsub

universal-pubsub: universal-pubsub.c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

universal-sub-pubsub: universal-sub-pubsub.c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS) 

universal-xpubxsub: universal-xpubxsub.c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS) 

universal-sub-xpubxsub: universal-sub-xpubxsub.c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS) 

install: all
	mkdir -p $(PREFIX)/bin
	cp universal-pubsub universal-sub-pubsub universal-xpubxsub universal-sub-xpubxsub $(PREFIX)/bin

clean:
	rm -rf universal-pubsub universal-sub-pubsub universal-xpubxsub universal-sub-xpubxsub
