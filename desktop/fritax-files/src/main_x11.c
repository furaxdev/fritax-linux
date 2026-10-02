/* Fritax Fichiers - moteur X11 (test) + mode apercu sans ecran */
#define _GNU_SOURCE
#ifndef FX_NO_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "screen.h"
#include "app.h"

#ifndef FX_NO_X11
static Display *dpy; static Window win; static GC gc; static XImage *img;
static Visual *vis; static int depth;
#endif
static int W = 1366, H = 768;
static FXScreen *fb;

static int render_to_file(const char *out, int w, int h) {
    W = w; H = h;
    fb = fx_screen_new(w, h);
    if (!fb) return 1;
    fx_files_init(w, h, NULL, NULL);
    fx_files_demo();
    fx_files_draw(fb);
    FILE *f = fopen(out, "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = fb->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255), (unsigned char)((p >> 8) & 255), (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("apercu ecrit : %s (%dx%d)\n", out, w, h);
    return 0;
}

int main(int argc, char **argv) {
    const char *start = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--render") && i + 1 < argc) {
            int w = (i + 3 < argc) ? atoi(argv[i + 2]) : 1366;
            int h = (i + 3 < argc) ? atoi(argv[i + 3]) : 768;
            return render_to_file(argv[i + 1], w, h);
        }
        if (!strcmp(argv[i], "--path") && i + 1 < argc) start = argv[++i];
    }
#ifdef FX_NO_X11
    fprintf(stderr, "fritax-files: compile sans X11 (apercu seulement)\n");
    return 0;
#else
    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-files.log", home ? home : "/tmp");
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "fritax-files: pas d'affichage X\n"); return 1; }
    int sc = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, sc); depth = DefaultDepth(dpy, sc);
    if (DisplayWidth(dpy, sc) > 0) { W = DisplayWidth(dpy, sc); H = DisplayHeight(dpy, sc); }
    fb = fx_screen_new(W, H);
    if (!fb) return 1;
    fx_files_init(W, H, start, lp);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, sc), 0, 0, (unsigned)W, (unsigned)H, 0, BlackPixel(dpy, sc), 0);
    XStoreName(dpy, win, "Fichiers - Fritax");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask | StructureNotifyMask);
    Atom del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &del, 1);
    Atom fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom st = XInternAtom(dpy, "_NET_WM_STATE", False);
    XChangeProperty(dpy, win, st, XA_ATOM, 32, PropModeReplace, (unsigned char *)&fs, 1);
    gc = XCreateGC(dpy, win, 0, NULL);
    XMapWindow(dpy, win);
    img = XCreateImage(dpy, vis, (unsigned)depth, ZPixmap, 0, (char *)fb->px, (unsigned)W, (unsigned)H, 32, 0);
    XFlush(dpy);

    int running = 1;
    while (running) {
        fx_files_draw(fb);
        if (img) XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)W, (unsigned)H);
        XFlush(dpy);
        fd_set rf; FD_ZERO(&rf);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &rf);
        struct timeval tv = { 1, 0 };
        if (select(xfd + 1, &rf, NULL, NULL, &tv) <= 0) continue;
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            switch (ev.type) {
            case ButtonPress: if (fx_files_click(ev.xbutton.x, ev.xbutton.y)) running = 0; break;
            case MotionNotify: fx_files_move(ev.xmotion.x, ev.xmotion.y); break;
            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) { running = 0; break; }
                if (ks == XK_Up) fx_files_key(FXF_UP);
                else if (ks == XK_Down) fx_files_key(FXF_DOWN);
                else if (ks == XK_Return || ks == XK_KP_Enter) fx_files_key(FXF_ENTER);
                else if (ks == XK_BackSpace) fx_files_key(FXF_BACK);
                else if (ks == XK_h) fx_files_key(FXF_HOME);
                break; }
            case ButtonRelease: break;
            case ClientMessage: if ((Atom)ev.xclient.data.l[0] == del) running = 0; break;
            default: break;
            }
        }
    }
    if (img) { img->data = NULL; XDestroyImage(img); }
    XCloseDisplay(dpy);
    return 0;
#endif
}
