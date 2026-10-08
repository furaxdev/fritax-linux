/* ============================================================
 *  FRITAX LOGIN (natif) - DRM/KMS + evdev, aucun serveur graphique.
 *  Apres une connexion reussie : le bureau Fritax est lance.
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

#include "screen.h"
#include "login.h"
#include "drm.h"
#include "input.h"

static int cx, cy, W, H, cur;
static FXDrm *drm;
static FXDrmBuf *bufs[2];
static FXScreen *fb;
static FXLogin log_;
static int quit, connecte;
static long ticks;

static void on_move(void *u, int dx, int dy) {
    (void)u;
    cx += dx; cy += dy;
    if (cx < 0) cx = 0; if (cy < 0) cy = 0;
    if (cx > W - 1) cx = W - 1; if (cy > H - 1) cy = H - 1;
}
static void on_button(void *u, int pressed) { (void)u; if (pressed) fx_login_souris(&log_, cx, cy, 1); }
static void on_key(void *u, int key, int ascii) { (void)u; fx_login_touche(&log_, key, ascii); }
static void on_scroll(void *u, int up) { (void)u; (void)up; }

int main(int argc, char **argv) {
    const char *card = "/dev/dri/card0";
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--card") && i + 1 < argc) card = argv[++i];

    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-login.log", home ? home : "/tmp");
    fx_drm_log_path(lp); fx_inputs_log_path(lp);

    drm = fx_drm_open(card);
    if (!drm) { fprintf(stderr, "fritax-login: pas d'affichage DRM (%s)\n", card); return 1; }
    W = fx_drm_width(drm); H = fx_drm_height(drm);
    bufs[0] = fx_drm_buf_new(drm); bufs[1] = fx_drm_buf_new(drm);
    if (!bufs[0] || !bufs[1]) return 1;
    fb = fx_screen_new(W, H);
    if (!fb) return 1;

    fx_login_init(&log_, W, H);
    fx_login_souris(&log_, W / 2, H / 2, 0);
    FXInputs *in = fx_inputs_open();
    cx = W / 2; cy = H / 2;

    int echecs_affichage = 0;
    while (!quit) {
        log_.curseur = ((ticks / 5) % 2) ? 1 : 0;      /* clignotement du curseur */
        ticks++;
        fx_login_dessine(&log_, fb);
        FXDrmBuf *b = bufs[cur];
        for (int y = 0; y < H; y++)
            memcpy((char *)b->map + (size_t)y * b->pitch, fb->px + (size_t)y * W, (size_t)W * 4);

        /* Si l'ecran nous refuse l'image (un autre programme le tient), on
           sort en erreur au bout de trois essais : le surveillant du demarrage
           peut alors passer la main au bureau au lieu de boucler sans fin. */
        if (fx_drm_buf_flip(drm, b) != 0) {
            if (++echecs_affichage >= 3) {
                fprintf(stderr, "fritax-login: l'affichage est refuse, ecran occupe\n");
                fx_inputs_close(in);
                fx_drm_close(drm);
                return 2;
            }
        } else {
            echecs_affichage = 0;
        }
        cur ^= 1;

        if (log_.etat == FXL_CONNECTE) { connecte = 1; break; }

        struct pollfd pfds[32];
        int n = fx_inputs_pollfds(in, pfds, 32);
        if (poll(pfds, (nfds_t)n, 400) > 0)
            for (int i = 0; i < n; i++)
                if (pfds[i].revents & POLLIN)
                    fx_inputs_handle(in, i, on_move, on_button, on_key, on_scroll, NULL);
    }
    fx_inputs_close(in);
    if (connecte) {
        execlp(fx_login_session(), fx_login_session(), (char *)NULL);   /* le bureau prend la main */
        fprintf(stderr, "fritax-login: impossible de lancer %s\n", fx_login_session());
        return 1;
    }
    return 0;
}
