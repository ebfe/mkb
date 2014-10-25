VERSION = $(shell git describe --always --dirty)
PREFIX = $(HOME)

CC = cc

CPPFLAGS = -D_DEFAULT_SOURCE -DVERSION=\"$(VERSION)\"

CFLAGS ?= -g -Os
CFLAGS += -std=c99 -pedantic
CFLAGS += -Wall 

LDFLAGS = 

# libX11
CFLAGS += $(shell pkg-config --cflags x11)
LDFLAGS += $(shell pkg-config --libs x11)

all: mkb 

mkb: mkb.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

tags:
	ctags -R .

clean:
	@-rm mkb *.o *~ core tags

install: all
	install -s -t $(DESTDIR)$(PREFIX)/bin -m 0755 mkb

.PHONY: all tags clean
