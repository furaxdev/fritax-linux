/* ============================================================
 *  FRITAX TERMINAL (natif) - le terminal de la distribution
 *  Il ne s'occupe plus de X11 : il affiche directement sur
 *  l'ecran via DRM/KMS et lit le clavier par evdev.
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <locale.h>

#include "vt.h"
#include "screen.h"
#include "drm.h"
#include "input.h"

#define PAD 6
#define BLINK_MS 500

static VT *vt;
static FXScreen *fb;
static FXDrm *drm;
static FXDrmBuf *bufs[2];
static int cur, W, H, cols, rows;
static pid_t child;
static int pty_fd = -1;
static int dirty_all = 1;
static unsigned char dirty_rows[1024];

static void pty_write(const char *s, size_t n) {
    if (pty_fd < 0) return;
    size_t off = 0;
    while (off < n) { ssize_t w = write(pty_fd, s + off, n - off); if (w <= 0) return; off += (size_t)w; }
}

static void cb_dirty(void *u, int row) {
    (void)u;
    if (row < 0) dirty_all = 1;
    else if (row < (int)sizeof dirty_rows) dirty_rows[row] = 1;
}
static void cb_reply(void *u, const char *s, size_t n) { (void)u; pty_write(s, n); }
static void cb_bell(void *u) { (void)u; }
static void cb_title(void *u, const char *s, size_t n) { (void)u; (void)s; (void)n; }

static void present(void) {
    FXDrmBuf *b = bufs[cur];
    for (int y = 0; y < H; y++)
        memcpy((char *)b->map + (size_t)y * b->pitch, fb->px + (size_t)y * W, (size_t)W * 4);
    fx_drm_buf_flip(drm, b);
    cur ^= 1;
}

static void render_all(void) {
    fx_clear(fb, 0x0D1422);
    fx_render_vt(vt, fb, 0, rows - 1, PAD, 0xD633FF, 0x0D1422);
    dirty_all = 0;
    memset(dirty_rows, 0, sizeof dirty_rows);
    present();
}

static void render_dirty(void) {
    if (dirty_all) { render_all(); return; }
    int any = 0;
    for (int i = 0; i < rows; i++) {
        if (!dirty_rows[i]) continue;
        any = 1;
        for (int c = 0; c < cols; c++) {
            VTCell cell = vt_cell(vt, i, c);
            uint32_t bg = fx_vt_color(vt, cell.bg, 1, 255, 255);
            uint32_t fg = fx_vt_color(vt, cell.fg, 0, 255, 255);
            if (cell.attr & VT_REVERSE) { uint32_t t = bg; bg = fg; fg = t; }
            fx_fill_rect(fb, PAD + c * FONT_W, PAD + i * FONT_H, FONT_W, FONT_H, bg);
            if (cell.ch > 32) fx_draw_glyph(fb, PAD + c * FONT_W, PAD + i * FONT_H, cell.ch, fg);
        }
    }
    if (!any) return;
    fx_render_vt(vt, fb, 0, rows - 1, PAD, 0xD633FF, 0x0D1422);   /* redessine proprement (curseur) */
    present();
    memset(dirty_rows, 0, sizeof dirty_rows);
}

/* ---- clavier -> sequences envoyees au shell ---- */
static void send_key(int key, int ascii) {
    const char *seq = NULL;
    switch (key) {
    case FXK_UP: seq = "\033[A"; break;
    case FXK_DOWN: seq = "\033[B"; break;
    case FXK_RIGHT: seq = "\033[C"; break;
    case FXK_LEFT: seq = "\033[D"; break;
    case FXK_PAGEUP: seq = "\033[5~"; break;
    case FXK_PAGEDOWN: seq = "\033[6~"; break;
    case FXK_ESC: seq = "\033"; break;
    case FXK_BACKSPACE: seq = "\177"; break;
    case FXK_ENTER: seq = "\r"; break;
    case FXK_TAB: seq = "\t"; break;
    default: break;
    }
    if (seq) { pty_write(seq, strlen(seq)); return; }
    if (ascii > 0) {
        unsigned char b = (unsigned char)ascii;
        if (ascii < 128) pty_write((char *)&b, 1);
        else {                                   /* Latin-1 -> UTF-8 (accents) */
            char u[2] = { (char)(0xC0 | (b >> 6)), (char)(0x80 | (b & 0x3F)) };
            pty_write(u, 2);
        }
    }
}

static void on_move(void *u, int dx, int dy) { (void)u; (void)dx; (void)dy; }
static void on_button(void *u, int p) { (void)u; (void)p; }
static void on_key(void *u, int key, int ascii) { (void)u; send_key(key, ascii); }
static void on_scroll(void *u, int up) { (void)u; vt_scroll_view(vt, up ? 3 : -3); }

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    const char *card = "/dev/dri/card0";
    const char *cmd = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--card") && i + 1 < argc) card = argv[++i];
        else if ((!strcmp(argv[i], "-e") || !strcmp(argv[i], "--exec")) && i + 1 < argc) cmd = argv[++i];
        else if (!strcmp(argv[i], "--version")) { printf("Fritax Terminal 1.0 (Nova) - natif DRM/KMS\n"); return 0; }
    }
    char *shell = getenv("SHELL");
    if (!shell || !*shell || access(shell, X_OK)) shell = "/bin/bash";
    if (access(shell, X_OK)) shell = "/bin/sh";

    drm = fx_drm_open(card);
    if (!drm) { fprintf(stderr, "fritax-terminal: pas d'affichage DRM (%s)\n", card); return 1; }
    W = fx_drm_width(drm); H = fx_drm_height(drm);
    bufs[0] = fx_drm_buf_new(drm); bufs[1] = fx_drm_buf_new(drm);

    cols = (W - PAD * 2) / FONT_W;
    rows = (H - PAD * 2) / FONT_H;
    fb = fx_screen_new(W, H);

    VTHooks hooks = { cb_dirty, cb_reply, cb_bell, cb_title };
    vt = vt_new(cols, rows, VT_SCROLLBACK, hooks, NULL);
    uint8_t pal[16][3];
    fx_default_palette(pal);
    vt_set_palette(vt, pal);

    struct winsize ws = { 0, 0, 0, 0 };
    ws.ws_col = (unsigned short)cols; ws.ws_row = (unsigned short)rows;
    child = forkpty(&pty_fd, NULL, NULL, &ws);
    if (child < 0) { perror("forkpty"); return 1; }
    if (child == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "fritax-terminal", 1);
        if (cmd) execl(shell, shell, "-c", cmd, (char *)NULL);
        else execl(shell, shell, (char *)NULL);
        _exit(127);
    }

    FXInputs *inputs = fx_inputs_open();
    render_all();

    int running = 1;
    while (running) {
        struct pollfd pfds[33];
        int n = fx_inputs_pollfds(inputs, pfds, 32);
        pfds[n].fd = pty_fd; pfds[n].events = POLLIN; n++;
        int r = poll(pfds, (nfds_t)n, BLINK_MS);

        if (r == 0) {                       /* clignotement du curseur */
            vt_tick(vt);
            if (dirty_all || 1) { if (dirty_all) render_all(); else { fx_render_vt(vt, fb, vt_cursor_y(vt), vt_cursor_y(vt), PAD, 0xD633FF, 0x0D1422); present(); } }
            continue;
        }
        if (r < 0) { if (errno == EINTR) continue; break; }

        if (pfds[n - 1].revents & POLLIN) {
            char buf[8192];
            ssize_t got = read(pty_fd, buf, sizeof buf);
            if (got > 0) { vt_feed(vt, buf, (size_t)got); render_dirty(); }
            else {
                int st = 0;
                if (waitpid(child, &st, WNOHANG) > 0) { running = 0; break; }
            }
        }
        for (int i = 0; i < n - 1; i++)
            if (pfds[i].revents & POLLIN) fx_inputs_handle(inputs, i, on_move, on_button, on_key, on_scroll, NULL);
    }
    fx_inputs_close(inputs);
    return 0;
}
