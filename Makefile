NAME=presentpdf

CFLAGS=-Wall -Werror -pedantic -g -std=c99
LDFLAGS=-lm

LIBS=cairo poppler-glib clutter-1.0
CFLAGS+=$(shell pkg-config --cflags $(LIBS))
LDFLAGS+=$(shell pkg-config --libs $(LIBS))
PREFIX?=/usr/local

all: $(NAME)

$(NAME): main.c
	$(CC) -o $(NAME) $(CFLAGS) main.c $(LDFLAGS)

.PHONY: clean install uninstall

clean:
	rm -f $(NAME)

install: $(NAME)
	install -D -s $(NAME) $(PREFIX)/bin/$(NAME)
	install -D $(NAME).desktop $(PREFIX)/share/applications/$(NAME).desktop

uninstall:
	rm -f $(PREFIX)/bin/$(NAME)
	rm -rf $(PREFIX)/share/applications/$(NAME).desktop
