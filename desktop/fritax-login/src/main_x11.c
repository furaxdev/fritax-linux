/* ============================================================
 *  FRITAX LOGIN - moteur X11 (test sur PC) + apercu sans ecran.
 *  FX_NO_X11 : ecrit une image PPM au lieu d'ouvrir une fenetre.
 * ============================================================ */
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
#include "login.h"
#include "input.h"      /* FXK_* : necessaire pour piloter le coeur depuis X11 */

#ifndef FX_NO_X11
static Display *dpy; static Window win; static GC gc; static XImage *img;
static Visual *vis; static int depth;
#endif
static int W = 1366, H = 768;
static FXScreen *fb;
static FXLogin log_;

static int render_to_file(const char *out, int w, int h) {
    W = w; H = h;
    fb = fx_screen_new(w, h);
    if (!fb) return 1;
    fx_login_init(&log_, w, h);
    fx_login_dessine(&log_, fb);
    FILE *f = fopen(out, "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = fb->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255),
                                 (unsigned char)((p >> 8) & 255),
                                 (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("apercu ecrit : %s (%dx%d)\n", out, w, h);
    return 0;
}

/* apercu d'un etat precis (pour verifier visuellement) */
static int render_etat(const char *out, int w, int h, const char *mdp, int refus) {
    W = w; H = h;
    fb = fx_screen_new(w, h);
    if (!fb) return 1;
    fx_login_init(&log_, w, h);
    if (mdp) strncpy(log_.motdepasse, mdp, sizeof(log_.motdepasse) - 1);
    if (refus) fx_login_touche(&log_, FXK_ENTER, 0);
    fx_login_dessine(&log_, fb);
    FILE *f = fopen(out, "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = fb->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255),
                                 (unsigned char)((p >> 8) & 255),
                                 (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("apercu ecrit : %s (%s)\n", out, refus ? "mot de passe refuse" : "saisie");
    return 0;
}

#ifndef FX_NO_X11
static unsigned long xc(uint32_t c) {
    unsigned long r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    return (r << 16) | (g << 8) | b;
}
static void present(void) {
    for (int y = 0; y < H; y++) {
        char *dst = img->data + y * img->bytes_per_line;
        uint32_t *src = fb->px + (size_t)y * W;
        for (int x = 0; x < W; x++) {
            uint32_t p = src[x];
            dst[x*4+0] = (p >> 16) & 255; dst[x*4+1] = (p >> 8) & 255;
            dst[x*4+2] = p & 255; dst[x*4+3] = 0;
        }
    }
    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
    XFlush(dpy);
}
#endif

int main(int argc, char **argv) {
    const char *out = NULL, *etat = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--render") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--mdp") && i + 1 < argc) etat = argv[++i];
        else if (!strcmp(argv[i], "--taille") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
    }
    if (out) {
        int taille = (argc > 5 && strstr(argv[1], "--taille"));
        return render_etat(out, W, H, etat, taille && argc > 6 ? atoi(argv[6]) : 0);
    }
#ifdef FX_NO_X11
    fprintf(stderr, "mode apercu : utilisez --render fichier.ppm\n");
    return 1;
#else
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "pas d'affichage X11\n"); return 1; }
    int scr = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, scr); depth = DefaultDepth(dpy, scr);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, W, H, 0,
                              BlackPixel(dpy, scr), BlackPixel(dpy, scr));
    XStoreName(dpy, win, "Fritax Linux - connexion");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask);
    XMapWindow(dpy, win);
    gc = XCreateGC(dpy, win, 0, NULL);
    img = XCreateImage(dpy, vis, depth, ZPixmap, 0, calloc(W * H * 4, 1), W, H, 32, 0);
    fb = fx_screen_new(W, H);
    fx_login_init(&log_, W, H);
    int quit = 0;
    while (!quit) {
        present();
        if (log_.etat == FXL_CONNECTE) { printf("connexion OK -> %s\n", fx_login_session()); quit = 1; }
        fd_set rs; FD_ZERO(&rs); FD_SET(ConnectionNumber(dpy), &rs);
        struct timeval tv = { 0, 120000 };
        if (select(ConnectionNumber(dpy) + 1, &rs, NULL, NULL, &tv) > 0) {
            while (XPending(dpy)) {
                XEvent e; XNextEvent(dpy, &e);
                if (e.type == KeyPress) {
                    KeySym k = XLookupKeysym(&e.xkey, 0);
                    if (k == XK_Escape) fx_login_touche(&log_, FXK_ESC, 0);
                    else if (k == XK_Return || k == XK_KP_Enter) fx_login_touche(&log_, FXK_ENTER, 0);
                    else if (k == XK_Tab) fx_login_touche(&log_, FXK_TAB, 0);
                    else if (k == XK_BackSpace) fx_login_touche(&log_, FXK_BACKSPACE, 0);
                    else {
                        char b[8]; int n = XLookupString(&e.xkey, b, sizeof b, NULL, NULL);
                        if (n == 1 && b[0] >= 32) fx_login_touche(&log_, FXK_NONE, (unsigned char)b[0]);
                    }
                } else if (e.type == ButtonPress || e.type == MotionNotify) {
                    fx_login_souris(&log_, e.xbutton.x, e.xbutton.y, 0);
                }
            }
        }
    }
    return 0;
#endif
}
