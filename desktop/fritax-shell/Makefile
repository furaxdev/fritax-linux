# ============================================================
#  Fritax Shell - le bureau de Fritax Linux
#    make native -> DRM/KMS + evdev (la distribution)
#    make x11    -> fenetre X11 (pour tester sur KDE/GNOME)
#    make render -> image de verification (sans ecran)
# ============================================================
CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall
TERM    := ../fritax-terminal/src
LIBS_X11 := -lX11
LIBS    := -lutil                                   # forkpty (notre terminal natif)
CORE    := src/ui.c src/wm.c src/ipc.c $(TERM)/screen.c $(TERM)/window.c $(TERM)/appicon.c $(TERM)/vt.c
PLATFORM:= $(TERM)/drm.c $(TERM)/input.c

all: native x11

native: fritax-shell-native
fritax-shell-native: src/main_native.c $(CORE) $(PLATFORM)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_native.c $(CORE) $(PLATFORM) $(LIBS)

x11: fritax-shell
fritax-shell: src/main_x11.c $(CORE)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ src/main_x11.c $(CORE) $(LIBS_X11) $(LIBS)

render: fritax-render
fritax-render: src/main_x11.c $(CORE)
	$(CC) $(CFLAGS) -DFX_NO_X11 -Isrc -I$(TERM) -o $@ src/main_x11.c $(CORE) $(LIBS)

test: fritax-test-wm
fritax-test-wm: tests/test_wm.c src/wm.c src/ui.c $(CORE)
	$(CC) $(CFLAGS) -Isrc -I$(TERM) -o $@ tests/test_wm.c src/wm.c src/ui.c $(CORE) $(LIBS)
	./$@

clean:
	rm -f fritax-shell fritax-shell-native fritax-render fritax-test-wm
.PHONY: all native x11 render test clean
