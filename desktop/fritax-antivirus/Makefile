# Fritax Antivirus - scanner de signatures ecrit a la main
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra
ZIG     ?= /home/furax/tools/zig-x86_64-linux-0.16.0/zig

all: fritax-av

fritax-av: src/av.c
	$(CC) $(CFLAGS) -o $@ src/av.c

# compilation locale avec zig (le bac a sable n'a pas de gcc)
zig: src/av.c
	$(ZIG) cc $(CFLAGS) -o fritax-av src/av.c

clean:
	rm -f fritax-av
.PHONY: all zig clean
