NAME=pdfpresenter

CFLAGS=-Wall -pedantic -g -std=c99
LDFLAGS=-lm

LIBS=cairo poppler-glib clutter-1.0
CFLAGS+=$(shell pkg-config --cflags $(LIBS))
LDFLAGS+=$(shell pkg-config --libs $(LIBS))

all: $(NAME)

$(NAME): main.c
	$(CC) -o $(NAME) $(CFLAGS) main.c $(LDFLAGS)
