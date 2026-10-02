# Fritax Tunnel - notre tunnel TCP avec mot de passe
CC     ?= cc
CFLAGS ?= -O2 -std=gnu11 -Wall
all: fritax-tunnel
fritax-tunnel: src/fritax-tunnel.c
	$(CC) $(CFLAGS) -o $@ src/fritax-tunnel.c
static: src/fritax-tunnel.c
	$(CC) $(CFLAGS) -static -o fritax-tunnel-static src/fritax-tunnel.c
clean:
	rm -f fritax-tunnel fritax-tunnel-static
.PHONY: all static clean
