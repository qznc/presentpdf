CFLAGS=-Wall -pedantic -g -std=c99
LDFLAGS=-lm

LIBS=cairo poppler-glib
CFLAGS+=$(shell pkg-config --cflags $(LIBS))
LDFLAGS+=$(shell pkg-config --libs $(LIBS))

all: test

test: main.c
	$(CC) -o $@ $(CFLAGS) main.c $(LDFLAGS)
