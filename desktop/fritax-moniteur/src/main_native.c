/* ============================================================
 *  FRITAX MONITEUR (natif) - DRM/KMS + evdev, aucun serveur graphique
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <linux/input.h>

#include "screen.h"
#include "app.h"
#include "drm.h"
#include "input.h"

static int cx, cy, W, H, cur;
static FXDrm *drm;
static FXDrmBuf *bufs[2];
static FXScreen *fb;
static int quit;

static void on_move(void *u, int dx, int dy) {
    (void)u;
    cx += dx; cy += dy;
    if (cx < 0) cx = 0; if (cy < 0) cy = 0;
    if (cx > W - 1) cx = W - 1; if (cy > H - 1) cy = H - 1;
    fx_mon_move(cx, cy);
}
static void on_button(void *u, int pressed) { (void)u; if (pressed && fx_mon_click(cx, cy)) quit = 1; }
static void on_key(void *u, int key, int ascii) {
    (void)u;
    if (key == FXK_ESC) quit = 1;
    else if (key == FXK_NONE && (ascii == 'r' || ascii == 'R')) fx_mon_key(FXM_RAFRAICHIR);
}

int main(int argc, char **argv) {
    const char *card = "/dev/dri/card0";
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--card") && i + 1 < argc) card = argv[++i];

    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-moniteur.log", home ? home : "/tmp");
    fx_drm_log_path(lp); fx_inputs_log_path(lp);

    drm = fx_drm_open(card);
    if (!drm) { fprintf(stderr, "fritax-moniteur: pas d'affichage DRM (%s)\n", card); return 1; }
    W = fx_drm_width(drm); H = fx_drm_height(drm);
    bufs[0] = fx_drm_buf_new(drm); bufs[1] = fx_drm_buf_new(drm);
    if (!bufs[0] || !bufs[1]) return 1;
    fb = fx_screen_new(W, H);
    if (!fb) return 1;
    fx_mon_init(W, H, lp);
    FXInputs *in = fx_inputs_open();
    cx = W / 2; cy = H / 2;

    while (!quit) {
        fx_mon_rafraichir();        /* relit /proc au plus une fois par seconde */
        fx_mon_draw(fb);
        FXDrmBuf *b = bufs[cur];
        for (int y = 0; y < H; y++)
            memcpy((char *)b->map + (size_t)y * b->pitch, fb->px + (size_t)y * W, (size_t)W * 4);
        fx_drm_buf_flip(drm, b);
        cur ^= 1;
        struct pollfd pfds[32];
        int n = fx_inputs_pollfds(in, pfds, 32);
        if (poll(pfds, (nfds_t)n, 800) > 0)
            for (int i = 0; i < n; i++)
                if (pfds[i].revents & POLLIN)
                    fx_inputs_handle(in, i, on_move, on_button, on_key, NULL, NULL);
    }
    fx_inputs_close(in);
    return 0;
}
