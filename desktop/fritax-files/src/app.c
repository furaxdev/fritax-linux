/* ============================================================
 *  FRITAX FICHIERS - navigateur de fichiers
 *  Dessine avec notre boite a outils (screen.c + window.c).
 *  Aucune dependance : juste la libc.
 * ============================================================ */
#define _GNU_SOURCE
#include "app.h"
#include "appicon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdarg.h>

#define MAX_ENTRIES 512
#define ROW_H 26
#define TOOLBAR_H 32
#define FOOTER_H 24
#define LIST_PAD 10

typedef struct { char name[256]; int is_dir; long long size; } Entry;

static Entry ents[MAX_ENTRIES];
static int nents, sel, scroll;
static char path_[512];
static int W, H, win_x, win_y, win_w, win_h;
static FILE *LOG;
static int hover_row = -1;

static const uint32_t C_BG = 0x0F1626, C_FG = 0xEAF2FF, C_DIM = 0x7C93B5;
static const uint32_t C_FOLDER = 0xF6AD55, C_FILE = 0x8FA6C4, C_SEL = 0x25344C, C_TOOLBAR = 0x16203A, C_VIOLET = 0xD633FF;

static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(LOG, fmt, ap); va_end(ap); fputc('\n', LOG); fflush(LOG);
}

static int ent_cmp(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;   /* dossiers d'abord */
    return strcasecmp(x->name, y->name);
}

static void human(long long n, char *out, size_t n_out) {
    if (n < 1024) snprintf(out, n_out, "%lld o", n);
    else if (n < 1024 * 1024) snprintf(out, n_out, "%.0f Ko", n / 1024.0);
    else if (n < 1024LL * 1024 * 1024) snprintf(out, n_out, "%.1f Mo", n / 1048576.0);
    else snprintf(out, n_out, "%.1f Go", n / 1073741824.0);
}

static void load_dir(const char *p) {
    nents = 0; sel = 0; scroll = 0;
    DIR *d = opendir(p);
    if (!d) { logf_("impossible d'ouvrir %s", p); return; }
    snprintf(path_, sizeof path_, "%s", p);
    struct dirent *e;
    while ((e = readdir(d)) && nents < MAX_ENTRIES) {
        if (!strcmp(e->d_name, ".")) continue;
        Entry *t = &ents[nents];
        snprintf(t->name, sizeof t->name, "%s", e->d_name);
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", p, e->d_name);
        struct stat st;
        if (lstat(full, &st) == 0) {
            t->is_dir = S_ISDIR(st.st_mode);
            t->size = (long long)st.st_size;
        } else { t->is_dir = 0; t->size = 0; }
        nents++;
    }
    closedir(d);
    qsort(ents, (size_t)nents, sizeof(Entry), ent_cmp);
    logf_("dossier %s : %d elements", p, nents);
}

void fx_files_init(int sw, int sh, const char *start, const char *log_path) {
    W = sw; H = sh;
    if (log_path) LOG = fopen(log_path, "w");
    win_w = sw > 1000 ? 900 : sw - 80;
    win_h = sh > 700 ? 560 : sh - 120;
    win_x = (sw - win_w) / 2;
    win_y = (sh - win_h) / 2;
    const char *home = getenv("HOME");
    load_dir(start ? start : (home ? home : "/"));
}

static int row_y(int idx) {
    return win_y + FX_TITLEBAR_H + TOOLBAR_H + LIST_PAD + (idx - scroll) * ROW_H;
}
static int visible_rows(void) {
    int zone = win_h - FX_TITLEBAR_H - TOOLBAR_H - FOOTER_H - LIST_PAD * 2;
    return zone / ROW_H;
}
static void clamp_scroll(void) {
    int vis = visible_rows();
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + vis) scroll = sel - vis + 1;
    if (scroll < 0) scroll = 0;
}
static void enter_dir(void) {
    if (sel < 0 || sel >= nents) return;
    Entry *e = &ents[sel];
    if (!e->is_dir) return;
    char np[1024];
    if (!strcmp(e->name, "..")) {
        char *slash = strrchr(path_, '/');
        if (!slash) return;
        if (slash == path_) snprintf(np, sizeof np, "/");
        else { size_t n = (size_t)(slash - path_); snprintf(np, sizeof np, "%.*s", (int)n, path_); }
    } else {
        if (!strcmp(path_, "/")) snprintf(np, sizeof np, "/%s", e->name);
        else snprintf(np, sizeof np, "%s/%s", path_, e->name);
    }
    load_dir(np);
}

void fx_files_draw(FXScreen *s) {
    fx_clear(s, 0x0A0F1A);
    /* fond degrade discret */
    fx_gradient_v(s, 0, 0, W, H, 0x0D1422, 0x16233C);

    /* la fenetre (notre chrome) */
    fx_window(s, win_x, win_y, win_w, win_h, "Fichiers", C_BG);

    /* barre d'outils : le chemin + le compteur */
    int ty = win_y + FX_TITLEBAR_H;
    fx_fill_rect(s, win_x, ty, win_w, TOOLBAR_H, C_TOOLBAR);
    fx_fill_round_rect(s, win_x + 10, ty + 5, win_w - 190, TOOLBAR_H - 10, 8, 0x0C1424);
    fx_fill_round_rect(s, win_x + 18, ty + 12, 10, 10, 3, C_VIOLET);
    char sh[512];
    snprintf(sh, sizeof sh, "%.*s", (win_w - 240) / FONT_W, path_);
    fx_draw_text(s, win_x + 36, ty + (TOOLBAR_H - FONT_H) / 2, sh, C_FG);
    char cnt[64];
    snprintf(cnt, sizeof cnt, "%d element%s", nents, nents > 1 ? "s" : "");
    fx_draw_text(s, win_x + win_w - (int)strlen(cnt) * FONT_W - 14, ty + (TOOLBAR_H - FONT_H) / 2, cnt, C_DIM);

    /* liste */
    int vis = visible_rows();
    for (int i = scroll; i < nents && i < scroll + vis; i++) {
        int y = row_y(i);
        int selected = (i == sel);
        if (selected || i == hover_row)
            fx_fill_round_rect(s, win_x + 6, y, win_w - 12, ROW_H - 2, 6, C_SEL);
        fx_app_icon(s, win_x + 12, y + 4, 18, ents[i].is_dir ? FX_ICON_FILES : FX_ICON_FILE);
        char nm[80];
        int maxc = (win_w - 220) / FONT_W;
        snprintf(nm, sizeof nm, "%.*s", maxc, ents[i].name);
        fx_draw_text(s, win_x + 40, y + (ROW_H - FONT_H) / 2, nm, selected ? C_FG : 0xD7E2F2);
        if (!ents[i].is_dir) {
            char hs[32]; human(ents[i].size, hs, sizeof hs);
            fx_draw_text(s, win_x + win_w - (int)strlen(hs) * FONT_W - 20, y + (ROW_H - FONT_H) / 2, hs, C_DIM);
        }
    }
    if (!nents) fx_draw_text(s, win_x + 20, win_y + FX_TITLEBAR_H + TOOLBAR_H + 20, "dossier vide", C_DIM);

    /* barre de defilement */
    if (nents > vis) {
        int zone = (win_h - FX_TITLEBAR_H - TOOLBAR_H - FOOTER_H - LIST_PAD * 2);
        int bar_h = zone * vis / nents; if (bar_h < 20) bar_h = 20;
        int bar_y = win_y + FX_TITLEBAR_H + TOOLBAR_H + LIST_PAD + (zone - bar_h) * scroll / (nents - vis ? nents - vis : 1);
        fx_fill_round_rect(s, win_x + win_w - 7, bar_y, 4, bar_h, 2, 0x3A4C6B);
    }

    /* pied : place libre + aide */
    int fy = win_y + win_h - FOOTER_H;
    fx_fill_rect(s, win_x, fy, win_w, FOOTER_H, C_TOOLBAR);
    struct statvfs vfs;
    if (statvfs(path_, &vfs) == 0) {
        unsigned long long libre = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
        char h[32]; human((long long)libre, h, sizeof h);
        char txt[64]; snprintf(txt, sizeof txt, "libre : %s", h);
        fx_draw_text(s, win_x + 12, fy + (FOOTER_H - FONT_H) / 2, txt, C_DIM);
    }
    const char *aide = "Entree : ouvrir    Retour : remonter    fleches : naviguer";
    int aw = (int)strlen(aide) * FONT_W;
    if (win_w > aw + 200) fx_draw_text(s, win_x + win_w - aw - 12, fy + (FOOTER_H - FONT_H) / 2, aide, 0x54678A);
}

int fx_files_click(int x, int y) {
    int bx, by, bsz, gap;
    fx_window_buttons_rect(win_x, win_y, win_w, &bx, &by, &bsz, &gap);
    if (y >= by - 4 && y <= by + bsz + 4) {
        if (x >= bx + 2 * (bsz + gap) - 4) return 1;                 /* fermer */
        if (x >= bx + bsz + gap - 4 && x < bx + 2 * (bsz + gap) - 4) return 0;  /* agrandir */
        if (x >= bx - 4 && x < bx + bsz + gap - 4) return 0;         /* reduire */
    }
    int vis = visible_rows();
    for (int i = scroll; i < nents && i < scroll + vis; i++) {
        int ry = row_y(i);
        if (y >= ry && y < ry + ROW_H && x >= win_x + 6 && x <= win_x + win_w - 6) {
            if (i == sel) enter_dir(); else sel = i;
            clamp_scroll();
            return 0;
        }
    }
    return 0;
}

int fx_files_move(int x, int y) {
    hover_row = -1;
    int vis = visible_rows();
    for (int i = scroll; i < nents && i < scroll + vis; i++) {
        int ry = row_y(i);
        if (y >= ry && y < ry + ROW_H && x >= win_x + 6 && x <= win_x + win_w - 6) { hover_row = i; break; }
    }
    return 0;
}

int fx_files_key(int key) {
    switch (key) {
    case FXF_UP: if (sel > 0) sel--; clamp_scroll(); break;
    case FXF_DOWN: if (sel < nents - 1) sel++; clamp_scroll(); break;
    case FXF_ENTER: enter_dir(); break;
    case FXF_BACK: {
        char *slash = strrchr(path_, '/');
        if (slash && slash != path_) {
            char np[512]; snprintf(np, sizeof np, "%.*s", (int)(slash - path_), path_);
            load_dir(np);
        } else if (strcmp(path_, "/")) load_dir("/");
        break; }
    case FXF_HOME: { const char *h = getenv("HOME"); if (h) load_dir(h); break; }
    default: break;
    }
    return 0;
}

void fx_files_demo(void) {
    static const char *dirs[] = { "Bureau", "Documents", "Images", "Musique", "Projets", "T\351l\351chargements", "Videos" };
    nents = 0;
    for (unsigned i = 0; i < sizeof dirs / sizeof dirs[0] && nents < MAX_ENTRIES; i++) {
        snprintf(ents[nents].name, sizeof ents[nents].name, "%s", dirs[i]);
        ents[nents].is_dir = 1; ents[nents].size = 0; nents++;
    }
    static const char *files[] = { "fritax-linux-1.0-nova.iso", "notes.txt", "photo-ecran.png", "musique-preferee.mp3", "rapport.pdf" };
    static const long long sizes[] = { 742391808LL, 2048, 1548288, 4718592, 350208 };
    for (unsigned i = 0; i < sizeof files / sizeof files[0] && nents < MAX_ENTRIES; i++) {
        snprintf(ents[nents].name, sizeof ents[nents].name, "%s", files[i]);
        ents[nents].is_dir = 0; ents[nents].size = sizes[i]; nents++;
    }
    snprintf(path_, sizeof path_, "%s", "/home/furaxb");
    sel = 0; scroll = 0;
    logf_("apercu : %d elements factices", nents);
}

int fx_files_scroll(int up) { if (sel > 0 && up) sel--; if (sel < nents - 1 && !up) sel++; clamp_scroll(); return 0; }
