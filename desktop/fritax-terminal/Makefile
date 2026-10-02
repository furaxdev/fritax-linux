# ============================================================
#  Fritax Terminal - le terminal maison de Fritax Linux
#    make native -> DRM/KMS + evdev (la distribution)
#    make x11    -> fenetre X11 (pour tester sur KDE/GNOME)
#    make test   -> tests du moteur VT
# ============================================================
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
LIBS_X11 := -lX11 -lutil
SRC      = src/vt.c src/screen.c
NATIVE   = src/drm.c src/input.c

all: native x11

native: fritax-terminal-native
fritax-terminal-native: src/main_drm.c $(SRC) $(NATIVE)
	$(CC) $(CFLAGS) -Isrc -o $@ src/main_drm.c $(SRC) $(NATIVE)

x11: fritax-terminal
fritax-terminal: src/main.c $(SRC)
	$(CC) $(CFLAGS) -Isrc -o $@ src/main.c $(SRC) $(LIBS_X11)

test: /tmp/fritax-vt-test
/tmp/fritax-vt-test: src/vt.c tests/test_vt.c
	$(CC) $(CFLAGS) -Isrc -o $@ src/vt.c tests/test_vt.c && $@

install-native: fritax-terminal-native
	install -Dm755 fritax-terminal-native $(DESTDIR)/usr/bin/fritax-terminal

clean:
	rm -f fritax-terminal fritax-terminal-native src/*.o
.PHONY: all native x11 test install-native clean
