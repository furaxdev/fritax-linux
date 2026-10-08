/* ============================================================
 *  FRITAX SHELL (natif) - bureau de Fritax Linux
 *  DRM/KMS + evdev : aucun serveur graphique, aucune bibliotheque.
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <signal.h>

#include "screen.h"
#include "ui.h"
#include "drm.h"
#include "input.h"

static int cursor_x, cursor_y, btn_down;
static FXDrmBuf *bufs[2];
static FXDrm *drm;
static FXScreen *fb, *clean;
static int W, H, cur;

static void on_move(void *u, int dx, int dy) {
    (void)u;
    cursor_x += dx; cursor_y += dy;
    if (cursor_x < 0) cursor_x = 0; if (cursor_y < 0) cursor_y = 0;
    if (cursor_x > W - 1) cursor_x = W - 1; if (cursor_y > H - 1) cursor_y = H - 1;
    fx_ui_move(cursor_x, cursor_y);
}
static void on_button(void *u, int pressed) { (void)u; btn_down = pressed; if (pressed) fx_ui_click(cursor_x, cursor_y); else fx_ui_release(); }
static void on_key(void *u, int key, int ascii) {
    (void)u;
    /* la couche materielle (evdev) et l'interface ont chacune leurs codes : on traduit */
    switch (key) {
    case FXK_ESC:       fx_ui_key(UI_KEY_ESC, 0); break;
    case FXK_SUPER:     fx_ui_key(UI_KEY_SUPER, 0); break;
    case FXK_BACKSPACE: fx_ui_key(UI_KEY_BACKSPACE, ascii ? ascii : 0); break;
    case FXK_ENTER:     fx_ui_key(UI_KEY_ENTER, 0); break;
    case FXK_TAB:       fx_ui_key(UI_KEY_TAB, 0); break;
    default: break;
    }
    if (key == FXK_NONE && ascii == 19) { fx_ui_key(UI_KEY_SAVE, 0); return; }
    switch (key) {
    case FXK_TAB:       break;
    case FXK_UP:        fx_ui_key(UI_KEY_UP, 0); break;
    case FXK_DOWN:      fx_ui_key(UI_KEY_DOWN, 0); break;
    case FXK_LEFT:      fx_ui_key(UI_KEY_LEFT, 0); break;
    case FXK_RIGHT:     fx_ui_key(UI_KEY_RIGHT, 0); break;
    default:            fx_ui_key(UI_KEY_NONE, ascii); break;
    }
}
static void on_scroll(void *u, int up) { (void)u; fx_ui_wheel(cursor_x, cursor_y, up); }

/* La console texte (fbcon) continue de redessiner par-dessus notre image tant
   qu'elle est en mode texte. Sans la basculer en mode graphique, le bureau
   tourne bien mais rien n'apparait : on ne voit que le texte, et le programme
   reste bloque la sans rendre la main. D'ou ces en-tetes. */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <signal.h>

/* La console est passee en mode graphique pendant que le bureau tourne. Si on
   nous arrete (Echap, kill, fin de session), il FAUT la remettre en mode texte,
   sinon l'ecran reste fige sur la derniere image et on ne voit plus rien —
   ni l'invite, ni le journal. C'est ce qui donnait l'impression d'un blocage. */
static int console_tty = -1;

static void remettre_console(int sig) {
    (void)sig;
    if (console_tty >= 0) {
        ioctl(console_tty, KDSETMODE, KD_TEXT);
        close(console_tty);
        console_tty = -1;
    }
    _exit(0);
}

int main(int argc, char **argv) {
    const char *card = "/dev/dri/card0";
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--card") && i + 1 < argc) card = argv[++i];

    const char *home = getenv("HOME");
    char lp[512];
    snprintf(lp, sizeof lp, "%s/.fritax-shell.log", home ? home : "/tmp");
    fx_drm_log_path(lp); fx_inputs_log_path(lp);

    drm = fx_drm_open(card);
    if (!drm) { fprintf(stderr, "fritax-shell: pas d'affichage DRM (%s)\n", card); return 1; }
    W = fx_drm_width(drm); H = fx_drm_height(drm);
    fprintf(stderr, "fritax-shell: affichage ouvert sur %s, %dx%d\n", card, W, H);
    bufs[0] = fx_drm_buf_new(drm); bufs[1] = fx_drm_buf_new(drm);
    if (!bufs[0] || !bufs[1]) { fprintf(stderr, "fritax-shell: tampons indisponibles\n"); return 1; }
    fprintf(stderr, "fritax-shell: deux tampons prets\n");

    /* On passe la console en mode graphique : le noyau arrete de redessiner le
       texte, et notre image reste a l'ecran. On la remet en mode texte en
       sortant, sinon le shell de secours devient invisible. */
    signal(SIGINT, remettre_console);
    signal(SIGTERM, remettre_console);
    signal(SIGHUP, remettre_console);
    int tty0 = open("/dev/tty0", O_RDWR);
    console_tty = tty0;
    if (tty0 >= 0) {
        if (ioctl(tty0, KDSETMODE, KD_GRAPHICS) == 0)
            fprintf(stderr, "fritax-shell: console passee en mode graphique\n");
        else
            fprintf(stderr, "fritax-shell: console non basculable (l'image peut clignoter)\n");
    } else {
        fprintf(stderr, "fritax-shell: /dev/tty0 inaccessible (l'image peut clignoter)\n");
    }

    fb = fx_screen_new(W, H); clean = fx_screen_new(W, H);
    if (!fb || !clean) { fprintf(stderr, "fritax-shell: memoire insuffisante pour %dx%d\n", W, H); remettre_console(0); return 1; }
    fprintf(stderr, "fritax-shell: tampons memoire prets\n");
    fx_ui_init(W, H, lp, 1);
    fprintf(stderr, "fritax-shell: bureau initialise\n");

    /* fond : degrade Fritax, puis l'image par-dessus si elle existe */
    fx_gradient_v(clean, 0, 0, W, H, C_BG, C_BG2);
    /* choix retenu (Reglages), sinon le fond de la distribution */
    char wp[512];
    int garde = 0;
    if (home) {
        char conf[512];
        snprintf(conf, sizeof conf, "%s/.fritax-wallpaper", home);
        FILE *c2 = fopen(conf, "r");
        if (c2) {
            if (fgets(wp, sizeof wp, c2)) {
                char *nl = strchr(wp, '\n'); if (nl) *nl = 0;
                if (wp[0]) garde = (fx_load_raw_rgb(clean, wp) == 0);
            }
            fclose(c2);
        }
    }
    if (!garde) {
        if (home) { snprintf(wp, sizeof wp, "%s/.local/share/fritax/wallpaper.raw", home); fx_load_raw_rgb(clean, wp); }
        fx_load_raw_rgb(clean, "/usr/share/fritax/wallpaper.raw");
    }

    FXInputs *inputs = fx_inputs_open();
    fprintf(stderr, "fritax-shell: entrees ouvertes (%s)\n", inputs ? "ok" : "aucune");
    cursor_x = W / 2; cursor_y = H - 80;
    int premiere_image = 1;
    int echecs_affichage = 0;

    int running = 1;
    while (running) {
        extern void fx_ui_sync_children(void);
        fx_ui_sync_children();
        fx_ui_tick();                    /* lecture des terminaux (PTY) et des demandes */

        if (fx_ui_take_wallpaper_change()) {          /* fond d'ecran change dans Reglages */
            fx_gradient_v(clean, 0, 0, W, H, C_BG, C_BG2);
            fx_load_raw_rgb(clean, fx_ui_wallpaper_path());
            if (home) {
                char conf[512];
                snprintf(conf, sizeof conf, "%s/.fritax-wallpaper", home);
                FILE *c3 = fopen(conf, "w");
                if (c3) { fprintf(c3, "%s\n", fx_ui_wallpaper_path()); fclose(c3); }
            }
        }

        memcpy(fb->px, clean->px, (size_t)W * H * sizeof(uint32_t));
        fx_ui_render(fb);
        fx_ui_draw_cursor(fb, cursor_x, cursor_y);

        FXDrmBuf *b = bufs[cur];
        for (int y = 0; y < H; y++)
            memcpy((char *)b->map + (size_t)y * b->pitch, fb->px + (size_t)y * W, (size_t)W * 4);
        /* Si l'ecran refuse l'image, c'est qu'un autre programme le tient.
           On ne boucle pas : on sort, et la sortie propre remet la console en
           mode texte, ce qui laisse un ecran lisible et utilisable. */
        if (fx_drm_buf_flip(drm, b) != 0) {
            if (++echecs_affichage >= 3) {
                fprintf(stderr, "fritax-shell: affichage refuse, ecran occupe par "
                                "un autre programme — on rend la main\n");
                break;
            }
        } else {
            echecs_affichage = 0;
            if (premiere_image) {
                fprintf(stderr, "fritax-shell: premiere image affichee\n");
                premiere_image = 0;
            }
        }
        cur ^= 1;
        if (fx_ui_wants_close()) break;

        struct pollfd pfds[32];
        int n = fx_inputs_pollfds(inputs, pfds, 32);
        if (poll(pfds, (nfds_t)n, 50) > 0)
            for (int i = 0; i < n; i++) if (pfds[i].revents & POLLIN) fx_inputs_handle(inputs, i, on_move, on_button, on_key, on_scroll, NULL);
    }
    fx_inputs_close(inputs);
    if (tty0 >= 0) {
        ioctl(tty0, KDSETMODE, KD_TEXT);
        close(tty0);
    }
    fprintf(stderr, "fritax-shell: arret, console rendue au mode texte\n");
    return 0;
}
