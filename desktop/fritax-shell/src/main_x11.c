/* ============================================================
 *  FRITAX SHELL (X11) - meme bureau, affiche dans une fenetre.
 *  Sert a developper et tester le bureau sur une session
 *  existante (KDE, GNOME...). Le vrai bureau de la
 *  distribution est main_native.c (DRM/KMS, sans X11).
 * ============================================================ */
#define _GNU_SOURCE
#ifndef FX_NO_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>

#include "screen.h"
#include "ui.h"

#ifndef FX_NO_X11
static Display *dpy; static Window win; static GC gc; static XImage *img;
static Visual *vis; static int depth;
#endif
static int SW = 1366, SH = 768;
static FXScreen *fb, *clean;

static void render_ui(void) {
    fx_ui_render(clean);
    memcpy(fb->px, clean->px, (size_t)SW * SH * sizeof(uint32_t));
}

/* ---- mode verification : rendu dans un fichier PPM (sans X11) ---- */
static int render_to_file(const char *out, int w, int h, int launcher) {
    SW = w; SH = h;
    fb = fx_screen_new(w, h); clean = fx_screen_new(w, h);
    if (!fb || !clean) return 1;
    fx_ui_init(w, h, NULL, 0);          /* jeu de demonstration */
    fx_clear(clean, C_BG);
    fx_gradient_v(clean, 0, 0, w, h, C_BG, C_BG2);
    const char *fond = getenv("FRITAX_FOND");
    if (fond && *fond && fx_load_raw_rgb(clean, fond) == 0) printf("fond d'ecran : %s\n", fond);
    fx_wm_add_demo();                   /* fenetres d'exemple pour l'apercu */
    fx_ui_set_launcher(launcher);
    fx_wm_tick();
    render_ui();
    FILE *f = fopen(out, "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = fb->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255), (unsigned char)((p >> 8) & 255), (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("rendu : %s (%dx%d)\n", out, w, h);
    return 0;
}

int main(int argc, char **argv) {
    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-shell.log", home ? home : "/tmp");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--render") && i + 1 < argc) {
            int w = (i + 3 < argc) ? atoi(argv[i + 2]) : 1366;
            int h = (i + 3 < argc) ? atoi(argv[i + 3]) : 768;
            int launcher = 1;
            for (int k = 0; k < argc; k++) if (!strcmp(argv[k], "--barre")) launcher = 0;
            return render_to_file(argv[i + 1], w, h, launcher);
        }
        if (!strcmp(argv[i], "--size") && i + 2 < argc) { SW = atoi(argv[i + 1]); SH = atoi(argv[i + 2]); }
    }

#ifdef FX_NO_X11
    fprintf(stderr, "fritax-shell: compile sans X11 (mode rendu uniquement)\n");
    return 0;
#else
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "fritax-shell: pas d'affichage X\n"); return 1; }
    int sc = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, sc);
    depth = DefaultDepth(dpy, sc);
    if (DisplayWidth(dpy, sc) > 0) { SW = DisplayWidth(dpy, sc); SH = DisplayHeight(dpy, sc); }

    fb = fx_screen_new(SW, SH);
    clean = fx_screen_new(SW, SH);
    if (!fb || !clean) return 1;
    fx_clear(clean, C_BG);
    fx_gradient_v(clean, 0, 0, SW, SH, C_BG, C_BG2);
    fx_ui_init(SW, SH, lp, 1);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, sc), 0, 0, (unsigned)SW, (unsigned)SH, 0,
                              BlackPixel(dpy, sc), C_BG);
    XStoreName(dpy, win, "Fritax Bureau");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                          PointerMotionMask | StructureNotifyMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    Atom fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom st = XInternAtom(dpy, "_NET_WM_STATE", False);
    XChangeProperty(dpy, win, st, XA_ATOM, 32, PropModeReplace, (unsigned char *)&fs, 1);
    gc = XCreateGC(dpy, win, 0, NULL);
    XMapWindow(dpy, win);
    img = XCreateImage(dpy, vis, (unsigned)depth, ZPixmap, 0, (char *)fb->px, (unsigned)SW, (unsigned)SH, 32, 0);
    XFlush(dpy);

    int mouse_x = SW / 2, mouse_y = SH - 60, running = 1;
    unsigned last_slot = 999;
    while (running) {
        fx_ui_tick();                    /* lecture des terminaux (PTY) */
        memcpy(fb->px, clean->px, (size_t)SW * SH * sizeof(uint32_t));
        fx_ui_render(fb);
        fx_ui_draw_cursor(fb, mouse_x, mouse_y);
        if (img) XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)SW, (unsigned)SH);
        XFlush(dpy);
        if (fx_ui_wants_close()) break;

        fd_set rf; FD_ZERO(&rf);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &rf);
        struct timeval tv = { 30, 0 };
        if (select(xfd + 1, &rf, NULL, NULL, &tv) <= 0) { (void)last_slot; continue; }
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            switch (ev.type) {
            case ButtonPress:
                if (ev.xbutton.button == 4) fx_ui_wheel(ev.xbutton.x, ev.xbutton.y, 1);
                else if (ev.xbutton.button == 5) fx_ui_wheel(ev.xbutton.x, ev.xbutton.y, 0);
                else fx_ui_click(ev.xbutton.x, ev.xbutton.y);
                break;
            case ButtonRelease: fx_ui_release(); break;
            case MotionNotify: mouse_x = ev.xmotion.x; mouse_y = ev.xmotion.y; fx_ui_move(mouse_x, mouse_y); break;
            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                char buf[8]; int n = XLookupString(&ev.xkey, buf, sizeof buf, &ks, NULL);
                int ctrl = (ev.xkey.state & ControlMask) != 0;
                if (ctrl && (ks == XK_w || ks == XK_W)) fx_ui_key(UI_KEY_CLOSE, 0);
                else if (ctrl && (ks == XK_s || ks == XK_S)) fx_ui_key(UI_KEY_SAVE, 0);
                else if (ks == XK_Escape) fx_ui_key(UI_KEY_ESC, 0);
                else if (ks == XK_Super_L || ks == XK_Super_R) fx_ui_key(UI_KEY_SUPER, 0);
                else if (ks == XK_BackSpace) fx_ui_key(UI_KEY_BACKSPACE, 0);
                else if (ks == XK_Return || ks == XK_KP_Enter) fx_ui_key(UI_KEY_ENTER, 0);
                else if (ks == XK_Up) fx_ui_key(UI_KEY_UP, 0);
                else if (ks == XK_Down) fx_ui_key(UI_KEY_DOWN, 0);
                else if (ks == XK_Left) fx_ui_key(UI_KEY_LEFT, 0);
                else if (ks == XK_Right) fx_ui_key(UI_KEY_RIGHT, 0);
                else if (ks == XK_Tab) fx_ui_key(UI_KEY_TAB, 0);
                else if (n > 0) fx_ui_key(UI_KEY_NONE, (unsigned char)buf[0]);
                break; }
            case ClientMessage: if ((Atom)ev.xclient.data.l[0] == wm_delete) running = 0; break;
            default: break;
            }
        }
    }
    if (img) { img->data = NULL; XDestroyImage(img); }
    XCloseDisplay(dpy);
    return 0;
#endif
}
