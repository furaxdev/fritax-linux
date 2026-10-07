# Fritax Moniteur - moniteur systeme de la distribution Fritax
#    make native -> DRM/KMS + evdev (la distribution)
#    make x11    -> fenetre X11 (pour tester sur KDE/GNOME)
#    make render -> image PPM, sans aucun ecran (verification)
#    make test   -> test du module de lecture /proc sur la machine
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
TERM    := ../fritax-terminal/src
LIBS_X11 := -lX11
ZIG     ?= /home/furax/tools/zig-x86_64-linux-0.16.0/zig
SHARED   := $(TERM)/screen.c $(TERM)/window.c $(TERM)/appicon.c $(TERM)/vt.c
PLATFORM := $(TERM)/drm.c $(TERM)/input.c

all: native x11

native: fritax-moniteur-native
fritax-moniteur-native: src/main_native.c src/app.c src/proc.c $(SHARED) $(PLATFORM)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_native.c src/app.c src/proc.c $(SHARED) $(PLATFORM)

x11: fritax-moniteur
fritax-moniteur: src/main_x11.c src/app.c src/proc.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c src/proc.c $(SHARED) $(LIBS_X11)

render: fritax-moniteur-render
fritax-moniteur-render: src/main_x11.c src/app.c src/proc.c $(SHARED)
	$(CC) $(CFLAGS) -DFX_NO_X11 -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c src/proc.c $(SHARED)

test: fritax-moniteur-test
fritax-moniteur-test: test_proc.c src/proc.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ test_proc.c src/proc.c src/app.c $(SHARED)
	./fritax-moniteur-test

# compilation locale avec zig (le bac a sable n'a pas de gcc)
zig: render
	$(ZIG) cc $(CFLAGS) -Isrc -I$(TERM) -o fritax-moniteur-native src/main_native.c src/app.c src/proc.c $(SHARED) $(PLATFORM)

clean:
	rm -f fritax-moniteur fritax-moniteur-native fritax-moniteur-render fritax-moniteur-test
.PHONY: all native x11 render test zig clean
