/* ============================================================
 *  FRITAX WM - implementation
 * ============================================================ */
#define _GNU_SOURCE
#include "wm.h"
#include "ui.h"        /* les touches UI_KEY_* */
#include "appicon.h"
#include "screen.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <stdarg.h>
#include <strings.h>

/* les fonds d'ecran proposes dans Reglages (dossier surchargeable par FRITAX_FONDS) */
#define NFONDS 6                    /* Nuit, Aurore, Graphite, Ocean, Foret, Neon */
static char fonds[NFONDS][512], fonds_mini[NFONDS][512];
static void init_fonds(void) {
    static int fait = 0;
    if (fait) return;
    fait = 1;
    const char *d = getenv("FRITAX_FONDS");
    if (!d || !*d) d = "/usr/share/fritax";
    for (int i = 0; i < NFONDS; i++) {
        snprintf(fonds[i], sizeof fonds[i], "%s/fond-%d.fx", d, i + 1);
        snprintf(fonds_mini[i], sizeof fonds_mini[i], "%s/fond-%d-mini.fx", d, i + 1);
    }
}
static int fond_choisi = 0, fond_change = 0;

#define MAXENTS 256
#define INPUT_MAX 512

typedef struct { char name[128]; int is_dir; long long size; } FEnt;

typedef struct {
    int used, kind, x, y, w, h;
    int maxed, minimized, z;
    int mx, my, mw, mh;   /* etat avant agrandissement */
    char title[64];
    /* terminal */
    VT *vt;
    int pty;
    pid_t child;
    /* fichiers */
    char cwd[512];
    FEnt ents[MAXENTS];
    int nents, sel, fscroll;
    /* editeur */
    char text[4096];
    int tlen, tcur;
    char fpath[512];
    int modifie;
    /* divers */
    int accent_idx;
} Win;

static Win wins[FX_WM_MAX];
static int SW, SH, ztop, focus = -1, drag_win = -1, drag_dx, drag_dy, resizing;
static int res_win = -1, res_w0, res_h0, res_x0, res_y0;
static FILE *LOG;
static int accent_sel = 0;

static const uint32_t ACCENTS[] = { 0x0067C0, 0x0078D4, 0x00838F, 0x2D7D46, 0xC239B3, 0xC42B1C };
#define NACCENTS ((int)(sizeof ACCENTS / sizeof ACCENTS[0]))

/* Couleurs Fluent (Windows 11) : surfaces claires, texte presque noir,
   un seul accent bleu. L'ancienne palette etait bleu nuit avec du violet,
   du cyan et du rose partout : c'est ce qu'on quitte en 1.2.
   Les valeurs sont celles de Windows 11 : fond de fenetre blanc pur,
   barre de titre tres pale, texte #1A1A1A, accent #0067C0. */
static const uint32_t W_VIOLET = 0x0067C0;          /* l'accent, desormais bleu */
static const uint32_t W_TITLE = 0xF9F9F9, W_BODY = 0xFFFFFF, W_FG = 0x1A1A1A,
                      W_DIM = 0x5E5E5E, W_SEL = 0xE8E8E8, W_TOOLBAR = 0xF3F3F3,
                      W_FOLDER = 0xE8A33D, W_FILE = 0x4A4A4A;

static void init_fonds(void);

static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(LOG, fmt, ap); va_end(ap); fputc('\n', LOG); fflush(LOG);
}

void fx_wm_init(int screen_w, int screen_h, const char *log_path) {
    init_fonds();
    SW = screen_w; SH = screen_h;
    if (log_path) LOG = fopen(log_path, "w");
    for (int i = 0; i < FX_WM_MAX; i++) wins[i].used = 0;
    focus = -1; drag_win = -1; ztop = 0;
    logf_("Fritax WM pret (%dx%d)", SW, SH);
}

static void term_reply(void *user, const char *s, size_t n) {
    Win *w = user;
    if (w->pty >= 0) { ssize_t r = write(w->pty, s, n); (void)r; }
}

/* ---------- ouverture ---------- */
static int slot_free(void) {
    for (int i = 0; i < FX_WM_MAX; i++) if (!wins[i].used) return i;
    return -1;
}

static void load_cwd(Win *w, const char *p) {
    w->nents = 0; w->sel = 0; w->fscroll = 0;
    DIR *d = opendir(p);
    if (!d) return;
    snprintf(w->cwd, sizeof w->cwd, "%s", p);
    struct dirent *e;
    while ((e = readdir(d)) && w->nents < MAXENTS) {
        if (!strcmp(e->d_name, ".")) continue;
        FEnt *t = &w->ents[w->nents];
        snprintf(t->name, sizeof t->name, "%s", e->d_name);
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", p, e->d_name);
        struct stat st;
        if (lstat(full, &st) == 0) { t->is_dir = S_ISDIR(st.st_mode); t->size = (long long)st.st_size; }
        w->nents++;
    }
    closedir(d);
    /* dossiers d'abord, puis alphabetique */
    for (int i = 0; i < w->nents; i++)
        for (int j = i + 1; j < w->nents; j++) {
            int sw = 0, a = i, b = j;
            if (w->ents[a].is_dir != w->ents[b].is_dir) sw = w->ents[a].is_dir < w->ents[b].is_dir;
            else sw = strcasecmp(w->ents[a].name, w->ents[b].name) > 0;
            if (sw) { FEnt t = w->ents[i]; w->ents[i] = w->ents[j]; w->ents[j] = t; }
        }
}

static void terminal_start(Win *w) {
    struct winsize ws = { 0, 0, 0, 0 };
    int cols = (w->w - 16) / FONT_W, rows = (w->h - FX_TITLEBAR_H - 12) / FONT_H;
    if (cols < 20) cols = 20; if (rows < 6) rows = 6;
    ws.ws_col = (unsigned short)cols; ws.ws_row = (unsigned short)rows;
    VTHooks h = { 0 };
    h.on_reply = term_reply;
    w->vt = vt_new(cols, rows, VT_SCROLLBACK, h, w);
    if (!w->vt) return;
    uint8_t pal[16][3];
    fx_default_palette(pal);
    pal[4][0] = 0x5B; pal[4][1] = 0xC8; pal[4][2] = 0xFF;    /* bleu Fritax */
    pal[5][0] = 0xD6; pal[5][1] = 0x33; pal[5][2] = 0xFF;    /* violet Fritax */
    vt_set_palette(w->vt, pal);

    int master = -1;
    pid_t p = forkpty(&master, NULL, NULL, &ws);
    if (p == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("PS1", "\033[1;35mfritax\033[0m:\033[1;36m$PWD\033[0m$ ", 1);
        execl("/bin/sh", "sh", "-i", (char *)NULL);
        _exit(127);
    }
    if (p < 0) { w->pty = -1; return; }
    w->child = p; w->pty = master;
    int fl = fcntl(master, F_GETFL, 0);
    fcntl(master, F_SETFL, fl | O_NONBLOCK);
    logf_("terminal : pty %d, pid %d, %dx%d", master, (int)p, cols, rows);
}

int fx_wm_open(int kind, const char *title) {
    int i = slot_free();
    if (i < 0) { logf_("plus de place pour une fenetre"); return -1; }
    Win *w = &wins[i];
    memset(w, 0, sizeof *w);
    w->used = 1; w->kind = kind; w->pty = -1; w->minimized = 0; w->maxed = 0;
    w->w = kind == FX_APP_TERMINAL ? 720 : 620;
    w->h = kind == FX_APP_TERMINAL ? 420 : 460;
    if (w->w > SW - 80) w->w = SW - 80;
    if (w->h > SH - 140) w->h = SH - 140;
    /* decalage en cascade */
    int n = 0;
    for (int k = 0; k < FX_WM_MAX; k++) if (wins[k].used) n++;
    w->x = 60 + (n % 5) * 46;
    w->y = 60 + (n % 5) * 40;
    snprintf(w->title, sizeof w->title, "%s", title ? title : "Fenetre");
    w->z = ++ztop;
    focus = i;
    const char *home = getenv("HOME");
    if (kind == FX_APP_TERMINAL) terminal_start(w);
    else if (kind == FX_APP_FILES) load_cwd(w, home ? home : "/");
    else if (kind == FX_APP_EDITOR) {
        const char *h = getenv("HOME");
        snprintf(w->fpath, sizeof w->fpath, "%s/document-fritax.txt", h ? h : "/tmp");
        snprintf(w->text, sizeof w->text,
                 "Bloc-notes Fritax\n\nTape ton texte ici, puis Ctrl+S pour enregistrer.\n"
                 "Fichier : %s\n", w->fpath);
        w->tlen = (int)strlen(w->text); w->tcur = w->tlen; w->modifie = 1;
    }
    logf_("fenetre %d ouverte : %s (%d,%d %dx%d)", i, w->title, w->x, w->y, w->w, w->h);
    return i;
}

void fx_wm_close_all(void) {
    for (int i = 0; i < FX_WM_MAX; i++) {
        if (!wins[i].used) continue;
        if (wins[i].pty >= 0) { close(wins[i].pty); wins[i].pty = -1; }
        if (wins[i].vt) { vt_free(wins[i].vt); wins[i].vt = NULL; }
        wins[i].used = 0;
    }
    focus = -1;
}

void fx_wm_add_demo(void) {
    int t = fx_wm_open(FX_APP_TERMINAL, "Terminal");
    if (t >= 0 && wins[t].vt) {
        static const char *script =
            "\033[1;35mfritax\033[0m:\033[1;36m~\033[0m$ uname -srm\r\n"
            "Linux 6.12.9-fritax x86_64\r\n"
            "\r\n"
            "\033[1;35mfritax\033[0m:\033[1;36m~\033[0m$ ls\r\n"
            "Bureau   Documents   Images   Musique\r\n"
            "Telechargements   fritax-1.0-nova.iso\r\n"
            "\r\n"
            "\033[1;35mfritax\033[0m:\033[1;36m~\033[0m$ cat /etc/os-release\r\n"
            "NAME=\"Fritax Linux\"\r\n"
            "VERSION=\"1.0 (Nova)\"\r\n"
            "ID=fritax\r\n"
            "\r\n"
            "\033[1;35mfritax\033[0m:\033[1;36m~\033[0m$ free -m\r\n"
            "               total   used   free\r\n"
            "Mem:            3808   1464    687\r\n"
            "\r\n"
            "\033[1;35mfritax\033[0m:\033[1;36m~\033[0m$ \r\n";
        vt_feed(wins[t].vt, script, strlen(script));
        wins[t].x = 170; wins[t].y = 70; wins[t].w = 720; wins[t].h = 400;
    }
    int f = fx_wm_open(FX_APP_FILES, "Fichiers");
    if (f >= 0) {
        wins[f].x = SW - 640; wins[f].y = 300; wins[f].w = 560; wins[f].h = 380;
        wins[f].nents = 0;
        static const char *dirs[] = { "Bureau", "Documents", "Images", "Musique", "T\351l\351chargements", "Videos" };
        for (unsigned i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
            snprintf(wins[f].ents[wins[f].nents].name, 128, "%s", dirs[i]);
            wins[f].ents[wins[f].nents].is_dir = 1; wins[f].nents++;
        }
        snprintf(wins[f].ents[wins[f].nents].name, 128, "fritax-1.0-nova.iso");
        wins[f].ents[wins[f].nents].size = 742391808LL; wins[f].nents++;
        snprintf(wins[f].ents[wins[f].nents].name, 128, "notes.txt");
        wins[f].ents[wins[f].nents].size = 2048; wins[f].nents++;
        snprintf(wins[f].cwd, sizeof wins[f].cwd, "/home/furaxb");
    }
    int r = fx_wm_open(FX_APP_SETTINGS, "R\351glages");
    if (r >= 0) { wins[r].x = 300; wins[r].y = 210; wins[r].w = 500; wins[r].h = 400; }
    focus = r >= 0 ? r : (f >= 0 ? f : t);
}

/* ---------- dessin ---------- */
static void draw_terminal(Win *w, FXScreen *s, int cx, int cy, int cw, int ch) {
    fx_fill_rect(s, cx, cy, cw, ch, 0x0A0F1A);
    if (!w->vt) { fx_draw_text(s, cx + 8, cy + 8, "terminal indisponible", W_DIM); return; }
    int cols = vt_cols(w->vt), rows = vt_rows(w->vt);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int px = cx + 6 + c * FONT_W, py = cy + 4 + r * FONT_H;
            if (px + FONT_W > cx + cw || py + FONT_H > cy + ch) continue;
            VTCell cell = vt_cell(w->vt, r, c);
            int fg = cell.fg, bg = cell.bg;
            if (cell.attr & VT_REVERSE) { int t = fg; fg = bg; bg = t; }
            if ((cell.attr & VT_BOLD) && fg < 8) fg += 8;
            uint32_t cbg = fx_vt_color(w->vt, bg, 1, 0, 0);
            if (bg) fx_fill_rect(s, px, py, FONT_W, FONT_H, cbg);
            if (cell.ch && cell.ch != ' ')
                fx_draw_glyph(s, px, py, cell.ch, fx_vt_color(w->vt, fg, 0, 7, 0));
        }
    }
    /* curseur */
    if (vt_cursor_visible(w->vt)) {
        int px = cx + 6 + vt_cursor_x(w->vt) * FONT_W, py = cy + 4 + vt_cursor_y(w->vt) * FONT_H;
        if (px + FONT_W <= cx + cw && py + FONT_H <= cy + ch)
            fx_draw_cursor_block(s, px, py, 0x5BC8FF);
    }
}

static void draw_files(Win *w, FXScreen *s, int cx, int cy, int cw, int ch) {
    fx_fill_rect(s, cx, cy, cw, ch, W_BODY);
    /* barre de chemin */
    fx_fill_rect(s, cx, cy, cw, 28, W_TOOLBAR);
    fx_fill_round_rect(s, cx + 8, cy + 5, cw - 16, 18, 6, 0x0C1424);
    char p[64];
    snprintf(p, sizeof p, "%.*s", (cw - 40) / FONT_W, w->cwd);
    fx_draw_text(s, cx + 14, cy + 6, p, W_FG);
    int row = 30;
    for (int i = w->fscroll; i < w->nents; i++) {
        int y = cy + row + (i - w->fscroll) * 24;
        if (y + 24 > cy + ch) break;
        if (i == w->sel) fx_fill_round_rect(s, cx + 6, y, cw - 12, 22, 5, W_SEL);
        fx_app_icon(s, cx + 12, y + 3, 16, w->ents[i].is_dir ? FX_ICON_FILES : FX_ICON_FILE);
        char nm[64];
        snprintf(nm, sizeof nm, "%.*s", (cw - 60) / FONT_W, w->ents[i].name);
        fx_draw_text(s, cx + 36, y + 4, nm, W_FG);
    }
    if (!w->nents) fx_draw_text(s, cx + 16, cy + 40, "dossier vide", W_DIM);
}

/* apercu d'un fond d'\351cran (lecture du format FXRAW, reduction au besoin) */
static void draw_thumb(FXScreen *s, const char *path, int x, int y, int w, int h) {
    FILE *f = fopen(path, "rb");
    if (!f) { fx_fill_round_rect(s, x, y, w, h, 6, 0x1B2740); return; }
    char magic[8] = {0}; int iw = 0, ih = 0;
    if (fscanf(f, "%5s %d %d", magic, &iw, &ih) != 3 || strcmp(magic, "FXRAW") != 0) { fclose(f); return; }
    fgetc(f);
    unsigned char *row = malloc((size_t)iw * 3);
    if (!row) { fclose(f); return; }
    for (int j = 0; j < h; j++) {
        int sy = j * ih / h;
        for (int i = 0; i < iw; i++) if (fread(row + i * 3, 1, 3, f) != 3) break;
        (void)sy;
        for (int i = 0; i < w; i++) {
            int sx = i * iw / w;
            uint32_t c = ((uint32_t)row[sx * 3] << 16) | ((uint32_t)row[sx * 3 + 1] << 8) | row[sx * 3 + 2];
            fx_fill_rect(s, x + i, y + j, 1, 1, c);
        }
        for (int skip = 1; skip < ih / h; skip++)
            for (int i = 0; i < iw; i++) if (fread(row + i * 3, 1, 3, f) != 3) break;
    }
    free(row);
    fclose(f);
}

static void draw_settings(Win *w, FXScreen *s, int cx, int cy, int cw, int ch) {
    (void)ch;
    fx_fill_rect(s, cx, cy, cw, 600, W_BODY);
    fx_draw_text(s, cx + 18, cy + 14, "Fond d'\351cran", W_FG);
    for (int i = 0; i < NFONDS; i++) {
        int bx = cx + 18 + i * 152, by = cy + 36;
        draw_thumb(s, fonds_mini[i], bx, by, 140, 79);
        if (i == fond_choisi) {
            fx_fill_rect(s, bx - 2, by - 2, 144, 2, W_VIOLET);
            fx_fill_rect(s, bx - 2, by + 79, 144, 2, W_VIOLET);
            fx_fill_rect(s, bx - 2, by - 2, 2, 83, W_VIOLET);
            fx_fill_rect(s, bx + 140, by - 2, 2, 83, W_VIOLET);
        }
    }
    (void)w;
    fx_draw_text(s, cx + 18, cy + 140, "Couleur d'accent", W_DIM);
    for (int i = 0; i < NACCENTS; i++) {
        int bx = cx + 18 + i * 44, by = cy + 168;
        fx_fill_round_rect(s, bx, by, 34, 34, 10, ACCENTS[i]);
        if (i == accent_sel) fx_fill_round_rect(s, bx + 11, by + 11, 12, 12, 4, 0xFFFFFF);
    }
    fx_draw_text(s, cx + 18, cy + 226, "\300 propos", W_FG);
    fx_draw_text(s, cx + 18, cy + 250, "Fritax Linux 1.0 Nova", W_DIM);
    fx_draw_text(s, cx + 18, cy + 270, "Bureau \351crit \340 la main (DRM/KMS)", W_DIM);
    fx_draw_text(s, cx + 18, cy + 290, "Noyau Linux 6.12", W_DIM);
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    char hh[64];
    snprintf(hh, sizeof hh, "Il est %02d:%02d", tm.tm_hour, tm.tm_min);
    fx_draw_text(s, cx + 18, cy + 310, hh, W_DIM);
}

static void draw_editor(Win *w, FXScreen *s, int cx, int cy, int cw, int ch) {
    fx_fill_rect(s, cx, cy, cw, ch, 0x0C1424);
    int col = 0, y = cy + 8;
    for (int i = 0; i < w->tlen && y + FONT_H < cy + ch; i++) {
        if (w->text[i] == '\n') { col = 0; y += FONT_H; continue; }
        int px = cx + 10 + col * FONT_W;
        if (px + FONT_W > cx + cw - 8) { col = 0; y += FONT_H; }
        px = cx + 10 + col * FONT_W;
        fx_draw_glyph(s, px, y, (uint32_t)(unsigned char)w->text[i], W_FG);
        col++;
    }
    fx_draw_text(s, cx + 10, cy + ch - FONT_H - 4, "Ctrl+S : rien a sauver (demo)", 0x54678A);
}

static int zorder[FX_WM_MAX];

static void sort_z(void) {
    int n = 0;
    for (int i = 0; i < FX_WM_MAX; i++) if (wins[i].used) zorder[n++] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (wins[zorder[i]].z > wins[zorder[j]].z) { int t = zorder[i]; zorder[i] = zorder[j]; zorder[j] = t; }
    zorder[n] = -1;
}

void fx_wm_render(FXScreen *s) {
    sort_z();
    for (int k = 0; zorder[k] >= 0; k++) {
        Win *w = &wins[zorder[k]];
        if (w->minimized) continue;
        int is_focus = (zorder[k] == focus);
        fx_blend_round_rect(s, w->x + 4, w->y + 8, w->w, w->h, FX_WIN_RADIUS, 0x000000, is_focus ? 150 : 90);
        fx_fill_round_rect(s, w->x, w->y, w->w, w->h, FX_WIN_RADIUS, W_BODY);
        fx_titlebar_k(s, w->x, w->y, w->w, w->title, w->kind + 1);
        int cx = w->x, cy = w->y + FX_TITLEBAR_H, cw = w->w, ch = w->h - FX_TITLEBAR_H;
        switch (w->kind) {
        case FX_APP_TERMINAL: draw_terminal(w, s, cx, cy, cw, ch); break;
        case FX_APP_FILES:    draw_files(w, s, cx, cy, cw, ch);    break;
        case FX_APP_SETTINGS: draw_settings(w, s, cx, cy, cw, ch); break;
        case FX_APP_EDITOR:   draw_editor(w, s, cx, cy, cw, ch);   break;
        default: break;
        }
        if (!w->maxed) {                                  /* poignee de redimensionnement */
            for (int k = 0; k < 3; k++)
                for (int d = 0; d < 5; d++)
                    fx_fill_rect(s, w->x + w->w - 7 - k * 4 + d, w->y + w->h - 7 - k * 4 - d, 1, 1, 0x6E86AC);
        }
    }
}

void fx_wm_tick(void) {
    for (int i = 0; i < FX_WM_MAX; i++) {
        Win *w = &wins[i];
        if (!w->used) continue;
        if (w->vt) vt_tick(w->vt);
        if (w->pty < 0) continue;
        char buf[8192];
        for (;;) {
            ssize_t n = read(w->pty, buf, sizeof buf);
            if (n > 0) { if (w->vt) vt_feed(w->vt, buf, (size_t)n); continue; }
            break;
        }
    }
}

/* ---------- entree ---------- */
static int top_at(int x, int y) {
    sort_z();
    for (int k = FX_WM_MAX - 1; k >= 0; k--) {
        int i = zorder[k];
        if (i < 0) continue;
        Win *w = &wins[i];
        if (w->minimized) continue;
        if (x >= w->x && x <= w->x + w->w && y >= w->y && y <= w->y + w->h) return i;
    }
    return -1;
}

int fx_wm_open_file(int i, const char *path) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    Win *w = &wins[i];
    if (w->kind != FX_APP_EDITOR) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    w->tlen = (int)fread(w->text, 1, sizeof w->text - 1, f);
    fclose(f);
    if (w->tlen < 0) w->tlen = 0;
    w->text[w->tlen] = 0;
    w->tcur = w->tlen;
    const char *b = strrchr(path, '/');
    snprintf(w->title, sizeof w->title, "Bloc-notes - %s", b ? b + 1 : path);
    return 0;
}

int fx_wm_snapshot(int i, char *out, int max) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    Win *w = &wins[i];
    if (w->kind == FX_APP_EDITOR) {                      /* le texte du bloc-notes */
        int n = w->tlen < max - 1 ? w->tlen : max - 1;
        memcpy(out, w->text, (size_t)n);
        out[n] = 0;
        return n;
    }
    if (w->kind == FX_APP_FILES) {                       /* la liste du dossier */
        int p = 0;
        for (int k = 0; k < w->nents; k++) {
            int n = snprintf(out + p, (size_t)(max - p), "%s\n", w->ents[k].name);
            if (n < 0 || p + n >= max - 1) break;
            p += n;
        }
        if (p < max) out[p] = 0;
        return p;
    }
    if (!w->vt) return -1;
    int p = 0, rows = vt_rows(w->vt), cols = vt_cols(w->vt);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            VTCell cell = vt_cell(w->vt, r, c);
            if (p >= max - 2) { out[p] = 0; return p; }
            out[p++] = (cell.ch >= 32 && cell.ch < 127) ? (char)cell.ch : ' ';
        }
        while (p > 0 && out[p - 1] == ' ') p--;                 /* on coupe les espaces de fin */
        if (p >= max - 2) break;
        out[p++] = '\n';
    }
    out[p] = 0;
    return p;
}

int fx_wm_size(int i, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    if (x) *x = wins[i].x;
    if (y) *y = wins[i].y;
    if (w) *w = wins[i].w;
    if (h) *h = wins[i].h;
    return 0;
}

int fx_wm_raise(int i) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    wins[i].z = ++ztop;
    wins[i].minimized = 0;
    focus = i;
    return 0;
}

int fx_wm_minimize(int i) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    wins[i].minimized = 1;
    if (focus == i) focus = -1;
    return 0;
}

int fx_wm_take_wallpaper_change(void) { int c = fond_change; fond_change = 0; return c; }
const char *fx_wm_wallpaper_path(void) { init_fonds(); return fonds[fond_choisi]; }
void fx_wm_set_wallpaper_index(int i) { if (i >= 0 && i < NFONDS) fond_choisi = i; }

int fx_wm_dragging(void) { return drag_win >= 0 || res_win >= 0; }
int fx_wm_count(void) {
    int n = 0;
    for (int i = 0; i < FX_WM_MAX; i++) if (wins[i].used) n++;
    return n;
}
const char *fx_wm_title(int i) { return (i >= 0 && i < FX_WM_MAX && wins[i].used) ? wins[i].title : ""; }
int fx_wm_kind(int i) { return (i >= 0 && i < FX_WM_MAX && wins[i].used) ? wins[i].kind : -1; }
int fx_wm_minimized(int i) { return (i >= 0 && i < FX_WM_MAX && wins[i].used) ? wins[i].minimized : 1; }
int fx_wm_focus(void) { return (focus >= 0 && focus < FX_WM_MAX && wins[focus].used) ? focus : -1; }
/* rang d'affichage d'une fenetre (0 = la plus basse) */
int fx_wm_zrank(int i) {
    if (i < 0 || i >= FX_WM_MAX || !wins[i].used) return -1;
    int r = 0;
    for (int k = 0; k < FX_WM_MAX; k++) if (wins[k].used && wins[k].z < wins[i].z) r++;
    return r;
}

int fx_wm_click(int x, int y) {
    int i = top_at(x, y);
    logf_("clic %d,%d -> fenetre %d", x, y, i);
    if (i < 0) return 0;
    Win *w = &wins[i];
    focus = i; w->z = ++ztop;
    /* coin bas droite = redimensionner (on le teste avant tout le reste) */
    if (!w->maxed && x >= w->x + w->w - 18 && y >= w->y + w->h - 18) {
        res_win = i; res_w0 = w->w; res_h0 = w->h; res_x0 = x; res_y0 = y;
        logf_("redimensionnement : fenetre %d (kind %d) %dx%d", i, w->kind, w->w, w->h);
        return 1;
    }
    if (y <= w->y + FX_TITLEBAR_H) {
        int bx, by, bsz, gap;
        fx_window_buttons_rect(w->x, w->y, w->w, &bx, &by, &bsz, &gap);
        if (x >= bx + 2 * (bsz + gap) - 4) {                  /* fermer */
            if (w->pty >= 0) { close(w->pty); w->pty = -1; }
            if (w->vt) { vt_free(w->vt); w->vt = NULL; }
            w->used = 0; focus = -1;
            logf_("fenetre fermee");
            return 1;
        }
        if (x >= bx + bsz + gap - 4 && x < bx + 2 * (bsz + gap) - 4) {   /* agrandir */
            if (!w->maxed) { w->mx = w->x; w->my = w->y; w->mw = w->w; w->mh = w->h;
                             w->x = 0; w->y = 0; w->w = SW; w->h = SH - 8; w->maxed = 1; }
            else { w->x = w->mx; w->y = w->my; w->w = w->mw; w->h = w->mh; w->maxed = 0; }
            if (w->vt) {
                int cols = (w->w - 16) / FONT_W, rows = (w->h - FX_TITLEBAR_H - 12) / FONT_H;
                vt_resize(w->vt, cols, rows);
            }
            return 1;
        }
        if (x >= bx - 4 && x < bx + bsz + gap - 4) {                      /* reduire */
            w->minimized = 1; focus = -1;
            return 1;
        }
        drag_win = i; drag_dx = x - w->x; drag_dy = y - w->y;  /* deplacement */
        return 1;
    }
    int cx = w->x, cy = w->y + FX_TITLEBAR_H, cw = w->w, ch = w->h - FX_TITLEBAR_H;
    if (w->kind == FX_APP_FILES) {
        int idx = w->fscroll + (y - (cy + 30)) / 24;
        if (y > cy + 28 && idx >= 0 && idx < w->nents) {
            if (idx == w->sel && w->ents[idx].is_dir) {
                load_cwd(w, w->cwd);                                        /* entrer dans le dossier */
            } else if (idx == w->sel && !w->ents[idx].is_dir) {
                char plein[1024];
                snprintf(plein, sizeof plein, "%s/%s", w->cwd, w->ents[idx].name);
                int e = fx_wm_open(FX_APP_EDITOR, "Bloc-notes");            /* ouvrir le fichier */
                if (e >= 0) fx_wm_open_file(e, plein);
            }
            w->sel = idx;
        }
    } else if (w->kind == FX_APP_SETTINGS) {
        for (int k = 0; k < NFONDS; k++) {                      /* choix du fond d'\351cran */
            int bx = cx + 18 + k * 152, by = cy + 36;
            if (x >= bx && x <= bx + 140 && y >= by && y <= by + 79) {
                fond_choisi = k; fond_change = 1;
                logf_("fond d'\351cran choisi : %s", fonds[k]);
                return 1;
            }
        }
        for (int k = 0; k < NACCENTS; k++) {
            int bx = cx + 18 + k * 44, by = cy + 168;
            if (x >= bx && x <= bx + 34 && y >= by && y <= by + 34) { accent_sel = k; break; }
        }
    }
    (void)cw; (void)ch;
    return 1;
}

void fx_wm_drag(int x, int y) {
    if (res_win >= 0) {                                  /* on redimensionne */
        Win *w = &wins[res_win];
        logf_("glissement : res_win %d vers %d,%d", res_win, x, y);
        int nw = res_w0 + (x - res_x0), nh = res_h0 + (y - res_y0);
        if (nw < 320) nw = 320;
        if (nh < 200) nh = 200;
        if (w->x + nw > SW) nw = SW - w->x;
        if (w->y + nh > SH - 6) nh = SH - 6 - w->y;
        w->w = nw; w->h = nh;
        if (w->vt) {                                     /* le terminal se remet a la bonne taille */
            int cols = (w->w - 16) / FONT_W, rows = (w->h - FX_TITLEBAR_H - 12) / FONT_H;
            if (cols > 2 && rows > 2) vt_resize(w->vt, cols, rows);
        }
        return;
    }
    if (drag_win < 0) return;
    Win *w = &wins[drag_win];
    w->x = x - drag_dx; w->y = y - drag_dy;
    if (w->x > SW - 80) w->x = SW - 80;
    if (w->x < -w->w + 80) w->x = -w->w + 80;
    if (w->y < 0) w->y = 0;
    if (w->y > SH - 60) w->y = SH - 60;
}

void fx_wm_release(void) { drag_win = -1; res_win = -1; (void)resizing; }

int fx_wm_wheel(int x, int y, int up) {
    int i = top_at(x, y);
    if (i < 0) return 0;
    Win *w = &wins[i];
    if (w->kind != FX_APP_TERMINAL || !w->vt) return 0;
    int off = vt_view_offset(w->vt);                 /* 0 = on est en bas */
    int n = up ? off + 3 : (off > 3 ? off - 3 : 0);  /* molette haut = remonter */
    vt_scroll_view(w->vt, n);
    return 1;
}

int fx_wm_hover(int x, int y) { return top_at(x, y) >= 0; }

void fx_wm_key(int key, int ascii) {
    if (focus < 0) return;
    Win *w = &wins[focus];
    if (!w->used) { focus = -1; return; }
    if (key == UI_KEY_SAVE && w->kind == FX_APP_EDITOR) {
        FILE *f2 = fopen(w->fpath, "w");
        if (f2) {
            fwrite(w->text, 1, (size_t)w->tlen, f2);
            fclose(f2);
            w->modifie = 0;
            char *b = strrchr(w->fpath, '/');
            snprintf(w->title, sizeof w->title, "Bloc-notes - %s", b ? b + 1 : w->fpath);
            logf_("enregistre : %s (%d octets)", w->fpath, w->tlen);
        } else {
            snprintf(w->title, sizeof w->title, "Bloc-notes - ECHEC enregistrement");
        }
        return;
    }
    if (key == UI_KEY_CLOSE) {                    /* fermer la fenetre au clavier */
        if (w->pty >= 0) { close(w->pty); w->pty = -1; }
        if (w->vt) { vt_free(w->vt); w->vt = NULL; }
        w->used = 0; focus = -1;
        return;
    }
    if (w->kind == FX_APP_TERMINAL) {
        /* touches speciales -> sequences */
        const char *seq = NULL;
        switch (key) {
        case UI_KEY_UP: seq = "\033[A"; break;
        case UI_KEY_DOWN: seq = "\033[B"; break;
        case UI_KEY_RIGHT: seq = "\033[C"; break;
        case UI_KEY_LEFT: seq = "\033[D"; break;
        case UI_KEY_ENTER: seq = "\r"; break;
        case UI_KEY_BACKSPACE: seq = "\177"; break;
        case UI_KEY_TAB: seq = "\t"; break;
        default: break;
        }
        if (w->pty < 0) return;
        if (seq) { ssize_t r = write(w->pty, seq, strlen(seq)); (void)r; return; }
        if (ascii == 3) { ssize_t r = write(w->pty, "\003", 1); (void)r; return; }
        if (ascii >= 32 && ascii < 127) { char c = (char)ascii; ssize_t r = write(w->pty, &c, 1); (void)r; }
    } else if (w->kind == FX_APP_EDITOR) {
        if (key == UI_KEY_BACKSPACE) { if (w->tcur > 0) { w->tcur--; memmove(w->text + w->tcur, w->text + w->tcur + 1, (size_t)(w->tlen - w->tcur)); w->tlen--; } return; }
        if (key == UI_KEY_ENTER) ascii = '\n';
        if (ascii >= 32 || ascii == '\n') {
            if (w->tlen < (int)sizeof w->text - 1) {
                memmove(w->text + w->tcur + 1, w->text + w->tcur, (size_t)(w->tlen - w->tcur));
                w->text[w->tcur] = (char)ascii; w->tcur++; w->tlen++;
                if (!w->modifie) {
                    w->modifie = 1;
                    char *b = strrchr(w->fpath, '/');
                    snprintf(w->title, sizeof w->title, "Bloc-notes - %s *", b ? b + 1 : w->fpath);
                }
            }
        }
    } else if (w->kind == FX_APP_FILES) {
        if (key == UI_KEY_UP && w->sel > 0) w->sel--;
        else if (key == UI_KEY_DOWN && w->sel < w->nents - 1) w->sel++;
    }
}
