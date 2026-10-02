# Fritax Fichiers - gestionnaire de fichiers
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
TERM    := ../fritax-terminal/src
LIBS_X11 := -lX11
SHARED   := $(TERM)/screen.c $(TERM)/window.c $(TERM)/appicon.c $(TERM)/vt.c
PLATFORM := $(TERM)/drm.c $(TERM)/input.c

all: native x11

native: fritax-files-native
fritax-files-native: src/main_native.c src/app.c $(SHARED) $(PLATFORM)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_native.c src/app.c $(SHARED) $(PLATFORM)

x11: fritax-files
fritax-files: src/main_x11.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c $(SHARED) $(LIBS_X11)

render: fritax-files-render
fritax-files-render: src/main_x11.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -DFX_NO_X11 -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c $(SHARED)

clean:
	rm -f fritax-files fritax-files-native fritax-files-render
.PHONY: all native x11 render clean
