# fritax-open : demander une fenetre au bureau
CC     ?= cc
CFLAGS ?= -O2 -std=gnu11 -Wall
SHELL_SRC := ../fritax-shell/src

all: fritax-open
fritax-open: src/open.c $(SHELL_SRC)/ipcclient.c
	$(CC) $(CFLAGS) -Isrc -I$(SHELL_SRC) -o $@ src/open.c $(SHELL_SRC)/ipcclient.c
clean:
	rm -f fritax-open
.PHONY: all clean
