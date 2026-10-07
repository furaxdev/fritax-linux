# ============================================================
#  Fritax Calculatrice - la calculatrice de Fritax Linux
#    make native -> DRM/KMS + evdev (la distribution)
#    make x11    -> fenetre X11 (pour tester sur KDE/GNOME)
#    make render -> image PPM, sans aucun ecran (apercu)
#    make test   -> test du moteur de calcul
#    make local  -> tout ci-dessus compile avec zig (pas de gcc)
# ============================================================
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
# compilateur local : zig sert de cc dans le bac a sable (pas de gcc)
ZIG     ?= /home/furax/tools/zig-x86_64-linux-0.16.0/zig
TERM    := ../fritax-terminal/src
LIBS_X11 := -lX11
SHARED   := $(TERM)/screen.c $(TERM)/window.c $(TERM)/appicon.c $(TERM)/vt.c
PLATFORM := $(TERM)/drm.c $(TERM)/input.c

all: native x11

native: fritax-calculatrice-native
fritax-calculatrice-native: src/main_native.c src/app.c $(SHARED) $(PLATFORM)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_native.c src/app.c $(SHARED) $(PLATFORM)

x11: fritax-calculatrice
fritax-calculatrice: src/main_x11.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c $(SHARED) $(LIBS_X11)

render: fritax-calculatrice-render
fritax-calculatrice-render: src/main_x11.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -DFX_NO_X11 -Isrc -I$(TERM) -o $@ src/main_x11.c src/app.c $(SHARED)

test: fritax-calculatrice-test
fritax-calculatrice-test: test_calcul.c src/app.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ test_calcul.c src/app.c $(SHARED)
	./fritax-calculatrice-test

# compilation locale complete avec zig cc (aucun gcc requis)
local:
	$(MAKE) CC="$(ZIG) cc" all test

clean:
	rm -f fritax-calculatrice fritax-calculatrice-native fritax-calculatrice-render fritax-calculatrice-test
.PHONY: all native x11 render test local clean
