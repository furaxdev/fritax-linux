# ============================================================
#  Fritax Login - ecran de connexion de Fritax Linux
#    make native -> DRM/KMS + evdev (la distribution)
#    make x11    -> fenetre X11 (pour tester sur KDE/GNOME)
#    make apercu -> image PPM, sans aucun ecran (verification)
#    make test   -> tests du coeur
# ============================================================
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
TERM    := ../fritax-terminal/src
LIBS_X11 := -lX11
SHARED  := $(TERM)/screen.c $(TERM)/vt.c $(TERM)/appicon.c $(TERM)/window.c
PLATFORM := $(TERM)/drm.c $(TERM)/input.c

all: native

native: fritax-login-native
fritax-login-native: src/main_native.c src/login.c $(SHARED) $(PLATFORM)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_native.c src/login.c $(SHARED) $(PLATFORM)

x11: fritax-login
fritax-login: src/main_x11.c src/login.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_x11.c src/login.c $(SHARED) $(LIBS_X11)

apercu: fritax-login-apercu
fritax-login-apercu: src/main_x11.c src/login.c $(SHARED)
	$(CC) $(CFLAGS) -DFX_NO_X11 -Isrc -I$(TERM) -o $@ src/main_x11.c src/login.c $(SHARED)

test: fritax-login-test
fritax-login-test: tests/test_login.c src/login.c $(SHARED)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ tests/test_login.c src/login.c $(SHARED)
	./fritax-login-test

clean:
	rm -f fritax-login fritax-login-native fritax-login-apercu fritax-login-test
.PHONY: all native x11 apercu test clean
