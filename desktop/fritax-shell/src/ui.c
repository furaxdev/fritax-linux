/* ============================================================
 *  Fritax UI - implementation (partagee X11 / DRM)
 * ============================================================ */
#define _GNU_SOURCE
#include "ui.h"
#include "wm.h"
#include "appicon.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>

#define BAR_H 46
#define BAR_R 22
#define BAR_BOTTOM 26
#define BTN 30
#define TILE_W 160
#define TILE_H 100
#define GRID_GAP 14

typedef struct { char name[80]; char exec[256]; uint32_t color; } App;
typedef struct { pid_t pid; char name[80]; uint32_t color; } Running;

static App apps[128]; static int napps;
static Running run_apps[32]; static int nrunning;
static int launcher_open, hover = -1, hover_btn, filter_len, wants_close;
static char filter[32];
static int SW, SH;
static int visible_apps[128], nvisible;
static FILE *LOG;
static int dirty = 1;

static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(LOG, fmt, ap); va_end(ap); fputc('\n', LOG); fflush(LOG);
}

/* ---------------- applications ---------------- */
static void add_app(const char *name, const char *exec) {
    if (napps >= 128 || !name || !*name || !exec || !*exec) return;
    for (int i = 0; i < napps; i++) if (!strcmp(apps[i].name, name)) return;
    static const uint32_t cols[] = { 0x5BC8FF, 0xF6AD55, 0x39DE8A, 0xD633FF, 0xFF7A7A, 0xA0AABE, 0x7ED6DF, 0xE1B12C };
    App *a = &apps[napps];
    snprintf(a->name, sizeof a->name, "%s", name);
    snprintf(a->exec, sizeof a->exec, "%s", exec);
    a->color = cols[napps % 8];
    napps++;
}

static void scan_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 9 || strcmp(e->d_name + n - 8, ".desktop")) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        char line[512], name[80] = {0}, exec[256] = {0}, type[64] = {0};
        int in_main = 0, nodisplay = 0;
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '[') { in_main = !strncmp(line, "[Desktop Entry]", 15); continue; }
            if (!in_main) continue;
            if (!strncmp(line, "Name=", 5) && !name[0]) snprintf(name, sizeof name, "%s", line + 5);
            else if (!strncmp(line, "Exec=", 5) && !exec[0]) snprintf(exec, sizeof exec, "%s", line + 5);
            else if (!strncmp(line, "Type=", 5)) snprintf(type, sizeof type, "%s", line + 5);
            else if (!strncmp(line, "NoDisplay=true", 14)) nodisplay = 1;
        }
        fclose(f);
        char *nl;
        if ((nl = strchr(name, '\n'))) *nl = 0;
        if ((nl = strchr(exec, '\n'))) *nl = 0;
        if ((nl = strstr(exec, " %"))) *nl = 0;
        if ((type[0] && strncmp(type, "Application", 11)) || nodisplay) continue;
        if (!strcmp(name, "Terminal Fritax")) snprintf(name, sizeof name, "Terminal");
        add_app(name, exec);
    }
    closedir(d);
}

static int app_cmp(const void *a, const void *b) { return strcmp(((const App *)a)->name, ((const App *)b)->name); }

static void scan_apps(void) {
    const char *home = getenv("HOME");
    char buf[512];
    if (home) { snprintf(buf, sizeof buf, "%s/.local/share/applications", home); scan_dir(buf); }
    scan_dir("/usr/local/share/applications");
    scan_dir("/usr/share/applications");
    qsort(apps, (size_t)napps, sizeof(App), app_cmp);
    logf_("applications : %d", napps);
}

void fx_ui_init(int w, int h, const char *log_path, int scan) {
    SW = w; SH = h;
    if (log_path) LOG = fopen(log_path, "w");
    logf_("Fritax UI - ecran %dx%d", w, h);
    fx_wm_init(w, h, log_path);
    fx_ipc_init(NULL);              /* service : ouvrir une fenetre a la demande */
    if (scan) scan_apps();
    else {
        /* jeu de demonstration (pour les tests de rendu) */
        add_app("Terminal", "fritax-terminal"); add_app("Navigateur", "firefox");
        add_app("Fichiers", "dolphin");         add_app("Jeux", "steam");
        add_app("Editeur", "kate");             add_app("R\351glages", "systemsettings");
    }
}

/* ---------------- lancement ---------------- */
static void launch(int idx) {
    if (idx < 0 || idx >= napps || nrunning >= 32) return;
    pid_t p = fork();
    if (p == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", apps[idx].exec, (char *)NULL);
        _exit(127);
    } else if (p > 0) {
        run_apps[nrunning].pid = p;
        snprintf(run_apps[nrunning].name, sizeof run_apps[nrunning].name, "%s", apps[idx].name);
        run_apps[nrunning].color = apps[idx].color;
        nrunning++;
        logf_("lance : %s (pid %d)", apps[idx].name, (int)p);
    }
    launcher_open = 0; filter_len = 0; filter[0] = 0; dirty = 1;
}

void fx_ui_sync_children(void) {
    for (int i = 0; i < nrunning; i++) {
        int st;
        if (run_apps[i].pid > 0 && waitpid(run_apps[i].pid, &st, WNOHANG) == run_apps[i].pid) {
            for (int j = i; j < nrunning - 1; j++) run_apps[j] = run_apps[j + 1];
            nrunning--; i--; dirty = 1;
        }
    }
}

/* ---------------- geometrie ---------------- */
static int bar_slots(void) {                 /* les fenetres ouvertes du bureau */
    int n = fx_wm_count();
    return n > 6 ? 6 : n;
}

static void bar_geo(int *bx, int *by, int *bw, int *bh) {
    int nb = bar_slots();
    int w = BTN + 16 + nb * (BTN + 12) + 120;
    if (w < 320) w = 320;
    *bw = w; *bh = BAR_H;
    *bx = (SW - w) / 2;
    *by = SH - BAR_H - BAR_BOTTOM;
}

static void launcher_geo(int *lx, int *ly, int *lw, int *lh) {
    *lw = 3 * TILE_W + 2 * GRID_GAP + 44;
    *lh = 56 + 2 * TILE_H + GRID_GAP + 44;
    *lx = (SW - *lw) / 2;
    *ly = (SH - *lh) / 2;
}

static void compute_visible(void) {
    nvisible = 0;
    for (int i = 0; i < napps && nvisible < 6; i++) {
        if (filter_len) {
            char low[80]; int k;
            for (k = 0; apps[i].name[k] && k < 79; k++) low[k] = (char)tolower((unsigned char)apps[i].name[k]);
            low[k] = 0;
            if (!strstr(low, filter)) continue;
        }
        visible_apps[nvisible++] = i;
    }
}

/* ---------------- dessin ---------------- */
static void draw_bar(FXScreen *s) {
    int bx, by, bw, bh;
    bar_geo(&bx, &by, &bw, &bh);
    fx_blend_round_rect(s, bx + 2, by + 6, bw, bh, BAR_R, 0x000000, 120);
    fx_blend_round_rect(s, bx, by, bw, bh, BAR_R, C_PANEL, 232);

    int px = bx + 8, py = by + (bh - BTN) / 2;
    uint32_t bc = launcher_open ? C_CYAN : (hover_btn ? 0xE060FF : C_VIOLET);
    fx_fill_round_rect(s, px, py, BTN, BTN, 10, bc);
    fx_draw_text_scale(s, px + (BTN - FONT_W * 2) / 2 + 1, py + (BTN - FONT_H * 2) / 2 + 2, "F", 0xFFFFFF, 2);

    /* une pastille par fenetre ouverte : la vraie barre des taches */
    int actif = fx_wm_focus();
    for (int j = FX_WM_MAX - 1, n = 0; j >= 0 && n < 6; j--) {
        if (fx_wm_kind(j) < 0) continue;
        int tx = px + BTN + 12 + n * (BTN + 12), ty = py;
        int hot = (hover == 1000 + j);
        int est_actif = (j == actif) && !fx_wm_minimized(j);
        fx_fill_round_rect(s, tx, ty, BTN, BTN, 9,
                           est_actif ? 0x35496B : (hot ? 0x27374F : 0x1E2D46));
        if (est_actif) fx_fill_round_rect(s, tx + 3, ty + BTN - 5, BTN - 6, 3, 2, C_CYAN);
        fx_app_icon(s, tx + 5, ty + 5, BTN - 10, fx_wm_kind(j) + 1);
        n++;
    }

    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    char hh[16]; snprintf(hh, sizeof hh, "%02d:%02d", tm.tm_hour, tm.tm_min);
    int tw = (int)strlen(hh) * FONT_W;
    fx_draw_text(s, bx + bw - tw - 14, by + (bh - FONT_H) / 2, hh, C_FG);
    for (int i = 0; i < 3; i++) fx_fill_circle(s, bx + bw - tw - 34 - i * 12, by + bh / 2, 2, 0x7889A6);
}

static void draw_launcher(FXScreen *s) {
    int lx, ly, lw, lh;
    launcher_geo(&lx, &ly, &lw, &lh);
    for (int i = 0; i < SW * SH; i++) s->px[i] = ((s->px[i] & 0xFEFEFE) >> 1) & 0x7F7F7F;   /* voile */
    fx_blend_round_rect(s, lx + 4, ly + 8, lw, lh, 28, 0x000000, 140);
    fx_fill_round_rect(s, lx, ly, lw, lh, 26, C_PANEL);
    fx_draw_text_scale(s, lx + 22, ly + 18, "APPLICATIONS", C_FG, 2);

    compute_visible();
    for (int i = 0; i < nvisible; i++) {
        int col = i % 3, row = i / 3;
        int tx = lx + 22 + col * (TILE_W + GRID_GAP), ty = ly + 56 + row * (TILE_H + GRID_GAP);
        fx_fill_round_rect(s, tx, ty, TILE_W, TILE_H, 16, hover == i ? 0x25344C : C_TILE);
        int kind = fx_app_kind(apps[visible_apps[i]].name);
        if (kind != FX_ICON_GENERIC) {
            fx_app_icon(s, tx + 18, ty + 14, 44, kind);
        } else {
            fx_fill_round_rect(s, tx + 16, ty + 14, 40, 40, 11, apps[visible_apps[i]].color);
            char ini[2] = { apps[visible_apps[i]].name[0], 0 };
            fx_draw_text_scale(s, tx + 26, ty + 24, ini, 0x101828, 2);
        }
        char nm[22];
        snprintf(nm, sizeof nm, "%.18s", apps[visible_apps[i]].name);
        fx_draw_text(s, tx + 16, ty + 66, nm, C_FG);
    }
    if (!nvisible) fx_draw_text(s, lx + 24, ly + 70, "aucune application trouvee", C_DIM);

    char hint[96];
    if (filter_len) snprintf(hint, sizeof hint, "recherche : %s_   |   Echap : fermer", filter);
    else snprintf(hint, sizeof hint, "tape pour chercher   |   Echap : fermer");
    fx_draw_text(s, lx + 22, ly + lh - 28, hint, C_DIM);
}

/* ---------------- icones du bureau ---------------- */
#define ICON_SZ 74
static const char *ICON_NAMES[4] = { "Terminal", "Fichiers", "R\351glages", "Bloc-notes" };
static const int ICON_APPS[4] = { FX_APP_TERMINAL, FX_APP_FILES, FX_APP_SETTINGS, FX_APP_EDITOR };
static const uint32_t ICON_COLS[4] = { 0x5BC8FF, 0xF6AD55, 0xD633FF, 0x39DE8A };

static void icon_geo(int i, int *ix, int *iy) {
    *ix = 40;
    *iy = 52 + i * (ICON_SZ + 30);
}

static void draw_icons(FXScreen *s) {
    for (int i = 0; i < 4; i++) {
        int ix, iy;
        icon_geo(i, &ix, &iy);
        int hot = (hover == 2000 + i);
        if (hot) fx_blend_round_rect(s, ix - 2, iy - 2, ICON_SZ + 4, ICON_SZ + 4, 16, 0xFFFFFF, 30);
        else fx_blend_round_rect(s, ix + 2, iy + 4, ICON_SZ - 4, ICON_SZ - 4, 16, 0x000000, 70);
        int isz = 52;
        fx_app_icon(s, ix + (ICON_SZ - isz) / 2, iy + (ICON_SZ - isz) / 2, isz, ICON_APPS[i] + 1);
        int tw = (int)strlen(ICON_NAMES[i]) * FONT_W;
        int tx = ix + (ICON_SZ - tw) / 2, ty = iy + ICON_SZ + 6;
        fx_draw_text(s, tx + 1, ty + 1, ICON_NAMES[i], 0x05070C);            /* ombre : lisible sur le fond */
        fx_draw_text(s, tx, ty, ICON_NAMES[i], hot ? 0xFFFFFF : 0xE8F0FA);
    }
}

static int icon_at(int x, int y) {
    for (int i = 0; i < 4; i++) {
        int ix, iy;
        icon_geo(i, &ix, &iy);
        if (x >= ix && x <= ix + ICON_SZ && y >= iy && y <= iy + ICON_SZ + 22) return i;
    }
    return -1;
}

void fx_ui_render(FXScreen *s) {
    fx_wm_render(s);                 /* d'abord les fenetres... */
    draw_icons(s);                   /* ... puis les icones du bureau */
    if (launcher_open) draw_launcher(s);
    draw_bar(s);                     /* la barre reste toujours au-dessus */
}

int fx_ui_release(void) { fx_wm_release(); return 0; }
int fx_ui_wheel(int x, int y, int up) { return fx_wm_wheel(x, y, up); }
int fx_ui_take_wallpaper_change(void) { return fx_wm_take_wallpaper_change(); }
const char *fx_ui_wallpaper_path(void) { return fx_wm_wallpaper_path(); }
void fx_ui_tick(void) { fx_wm_tick(); fx_ipc_poll(); }

void fx_ui_apercu_appli(const char *name, uint32_t color) {
    if (nrunning >= 32) return;
    run_apps[nrunning].pid = 0;
    snprintf(run_apps[nrunning].name, sizeof run_apps[nrunning].name, "%s", name);
    run_apps[nrunning].color = color;
    nrunning++;
}

void fx_ui_set_launcher(int open) { launcher_open = open; dirty = 1; }
int  fx_ui_launcher_open(void) { return launcher_open; }
int  fx_ui_wants_close(void) { return wants_close; }

/* ---------------- entree ---------------- */
int fx_ui_click(int x, int y) {
    int bx, by, bw, bh;
    bar_geo(&bx, &by, &bw, &bh);
    if (y >= by && y <= by + bh && x >= bx && x <= bx + bw) {
        int px = bx + 8, py = by + (bh - BTN) / 2;
        if (x >= px && x <= px + BTN && y >= py && y <= py + BTN) {
            launcher_open = !launcher_open; filter_len = 0; filter[0] = 0; dirty = 1; return 0;
        }
        for (int j = FX_WM_MAX - 1, n = 0; j >= 0 && n < 6; j--) {
            if (fx_wm_kind(j) < 0) continue;
            int tx = px + BTN + 12 + n * (BTN + 12);
            if (x >= tx && x <= tx + BTN) {          /* cliquer une pastille : montrer ou reduire */
                if (j == fx_wm_focus() && !fx_wm_minimized(j)) fx_wm_minimize(j);
                else fx_wm_raise(j);
                logf_("barre : fenetre %d", j);
                return 0;
            }
            n++;
        }
        return 0;
    }
    if (launcher_open) {
        int lx, ly, lw, lh;
        launcher_geo(&lx, &ly, &lw, &lh);
        compute_visible();
        for (int i = 0; i < nvisible; i++) {
            int col = i % 3, row = i / 3;
            int tx = lx + 22 + col * (TILE_W + GRID_GAP), ty = ly + 56 + row * (TILE_H + GRID_GAP);
            if (x >= tx && x <= tx + TILE_W && y >= ty && y <= ty + TILE_H) { launch(visible_apps[i]); return 0; }
        }
        if (x < lx || x > lx + lw || y < ly || y > ly + lh) {
            launcher_open = 0; filter_len = 0; filter[0] = 0; dirty = 1;
        }
        return 0;
    }
    if (fx_wm_click(x, y)) return 0;           /* fenetres */
    int im = icon_at(x, y);                    /* icones du bureau */
    if (im >= 0) {
        char ti[32];
        snprintf(ti, sizeof ti, "%s", ICON_NAMES[im]);
        fx_wm_open(ICON_APPS[im], ti);
        return 0;
    }
    return 0;
}

int fx_ui_move(int x, int y) {
    int nh = -1, nbtn = 0;
    int bx, by, bw, bh;
    bar_geo(&bx, &by, &bw, &bh);
    if (y >= by && y <= by + bh && x >= bx && x <= bx + bw) {
        int px = bx + 8, py = by + (bh - BTN) / 2;
        if (x >= px && x <= px + BTN && y >= py && y <= py + BTN) nbtn = 1;
        else for (int j = FX_WM_MAX - 1, n = 0; j >= 0 && n < 6; j--) {
     if (fx_wm_kind(j) < 0) continue;
     int tx = px + BTN + 12 + n * (BTN + 12);
     if (x >= tx && x <= tx + BTN) nh = 1000 + j;
     n++;
 }
    } else if (launcher_open) {
        int lx, ly, lw, lh;
        launcher_geo(&lx, &ly, &lw, &lh);
        compute_visible();
        for (int i = 0; i < nvisible; i++) {
            int col = i % 3, row = i / 3;
            int tx = lx + 22 + col * (TILE_W + GRID_GAP), ty = ly + 56 + row * (TILE_H + GRID_GAP);
            if (x >= tx && x <= tx + TILE_W && y >= ty && y <= ty + TILE_H) { nh = i; break; }
        }
    }
    if (fx_wm_dragging()) { fx_wm_drag(x, y); return 0; }
    if (!nh && !nbtn) {
        int ia = icon_at(x, y);
        if (ia >= 0) nh = 2000 + ia;
    }
    fx_wm_hover(x, y);
    if (nh != hover || nbtn != hover_btn) { hover = nh; hover_btn = nbtn; dirty = 1; }
    return 0;
}

int fx_ui_key(int key, int ascii) {
    if (key == UI_KEY_ESC) {
        if (launcher_open) { launcher_open = 0; filter_len = 0; filter[0] = 0; dirty = 1; }
        else wants_close = 1;
    } else if (key == UI_KEY_SUPER) {
        launcher_open = !launcher_open; filter_len = 0; filter[0] = 0; dirty = 1;
    } else if (launcher_open) {
        if (key == UI_KEY_BACKSPACE) { if (filter_len) filter[--filter_len] = 0; dirty = 1; }
        else if (ascii >= 32 && ascii < 127 && filter_len < 24) { filter[filter_len++] = (char)ascii; filter[filter_len] = 0; dirty = 1; }
    } else {
        fx_wm_key(key, ascii);                  /* la fenetre active traite la touche */
    }
    return 0;
}

void fx_ui_toggle_launcher(void) { launcher_open = !launcher_open; filter_len = 0; filter[0] = 0; dirty = 1; }

/* ---------------- curseur de souris ---------------- */
/* ---------- Curseur de souris FRITAX ----------
   Silhouette reprise du curseur KDE (eprouvee) et entierement re-stylee :
   corps blanc a peine degrade, contour marine, liseré bleu doux, ombre portee.
   Rendu en niveaux + alpha -> bords parfaitement lisses (aucun effet pixel).
   1 = ombre, 2 = contour, 3 = liseré, 4 = corps.
   La pointe (point actif) se trouve en (5, 5) du sprite. */
#define CURSOR_W 32
#define CURSOR_H 32
#define CURSOR_TIP_X 5
#define CURSOR_TIP_Y 5
static const unsigned char CURSOR_LVL[CURSOR_H][CURSOR_W] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  1,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  3,  3,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  3,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  3,  3,  3,  3,  3,  3,  4,  4,  4,  2,  2,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  2,  2,  2,  2,  2,  2,  2,  2,  3,  4,  4,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  4,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  2,  2,  2,  2,  1,  1,  2,  2,  2,  2,  3,  2,  2,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  1,  1,  1,  0,  0,  1,  2,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  2,  2,  2,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  2,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static const unsigned char CURSOR_ALPHA[CURSOR_H][CURSOR_W] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 15,139, 90, 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 90,255,255, 83, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 13, 88,241,238,255, 79, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 97,226, 53,241,255, 78, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,240,104, 60,244,252, 76, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 68, 72, 60,243,253, 75, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 64,251, 82, 62,245,251, 72, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 68,255,250, 81, 63,246,251, 72, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,251, 81, 65,246,249, 70, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,250, 82, 67,248,248, 68, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,250, 83, 68,248,248, 67, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,251, 84, 70,251,246, 64, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,255,250, 85, 71,250,246, 64, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,255,255,250, 85, 74,253,246, 61, 17,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,255,255,255,250, 79, 77,244,244, 62, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,255,255,255,255, 44,112,105,255,233, 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 67,255,255,255,255,255,255,255,253, 82, 42,157,250,255,199, 14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 68,255,254,254,254,255,255,252,254, 77,250,255,180, 64, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,236, 64,252,255,255,255,253,255,251,255, 99,243, 54, 26, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 14, 95,237, 79, 64, 80, 71, 72, 82,255,251,255,100,232, 30, 14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0, 13, 95,231, 32,104,223,255,250,124, 97,248,255,102,233, 26,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 95,255,254,255,229,122,211,255, 79, 67,251, 91,241, 27,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 38,209,185, 63, 27, 27, 27,237,226, 42,106,101,255, 74,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0, 14, 16, 14,  0,  0, 20, 98,255,228,105,248,232, 21,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 19,104,245,255,233, 74, 13,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 16, 28, 75, 22, 14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

void fx_ui_draw_cursor(FXScreen *s, int x, int y) {
    static const uint32_t col[5] = { 0x000000, 0x060A12, 0x171E30, 0x8CCDFF, 0xFAFCFF };
    int ox = x - CURSOR_TIP_X, oy = y - CURSOR_TIP_Y;
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++) {
            unsigned char lv = CURSOR_LVL[j][i];
            if (lv < 1 || lv > 4) continue;
            fx_blend_px(s, ox + i, oy + j, col[lv], CURSOR_ALPHA[j][i]);
        }
}
