/* ============================================================
 *  FRITAX FICHIERS (natif) - DRM/KMS + evdev, aucun serveur graphique
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
    fx_files_move(cx, cy);
}
static void on_button(void *u, int pressed) { (void)u; if (pressed && fx_files_click(cx, cy)) quit = 1; }
static void on_key(void *u, int key, int ascii) {
    (void)u; (void)ascii;
    if (key == FXK_ESC) quit = 1;
    else if (key == FXK_UP) fx_files_key(FXF_UP);
    else if (key == FXK_DOWN) fx_files_key(FXF_DOWN);
    else if (key == FXK_ENTER) fx_files_key(FXF_ENTER);
    else if (key == FXK_BACKSPACE) fx_files_key(FXF_BACK);
}
static void on_scroll(void *u, int up) { (void)u; fx_files_scroll(up); }

int main(int argc, char **argv) {
    const char *card = "/dev/dri/card0", *start = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--card") && i + 1 < argc) card = argv[++i];
        else if (!strcmp(argv[i], "--path") && i + 1 < argc) start = argv[++i];
    }
    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-files.log", home ? home : "/tmp");
    fx_drm_log_path(lp); fx_inputs_log_path(lp);

    drm = fx_drm_open(card);
    if (!drm) { fprintf(stderr, "fritax-files: pas d'affichage DRM (%s)\n", card); return 1; }
    W = fx_drm_width(drm); H = fx_drm_height(drm);
    bufs[0] = fx_drm_buf_new(drm); bufs[1] = fx_drm_buf_new(drm);
    if (!bufs[0] || !bufs[1]) return 1;
    fb = fx_screen_new(W, H);
    if (!fb) return 1;
    fx_files_init(W, H, start, lp);
    FXInputs *in = fx_inputs_open();
    cx = W / 2; cy = H / 2;

    while (!quit) {
        fx_files_draw(fb);
        FXDrmBuf *b = bufs[cur];
        for (int y = 0; y < H; y++)
            memcpy((char *)b->map + (size_t)y * b->pitch, fb->px + (size_t)y * W, (size_t)W * 4);
        fx_drm_buf_flip(drm, b);
        cur ^= 1;
        struct pollfd pfds[32];
        int n = fx_inputs_pollfds(in, pfds, 32);
        if (poll(pfds, (nfds_t)n, 800) > 0)
            for (int i = 0; i < n; i++) if (pfds[i].revents & POLLIN) fx_inputs_handle(in, i, on_move, on_button, on_key, on_scroll, NULL);
    }
    fx_inputs_close(in);
    return 0;
}
