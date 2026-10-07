/* ============================================================
 *  FRITAX CALCULATRICE - touches 0-9, +, -, x, /, =, C
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
#include <stdarg.h>

#define PAD      14   /* marge interieure de la fenetre */
#define DISP_H   118  /* hauteur de l'ecran de la calculatrice */
#define GAP      10   /* espace entre les touches */

/* grille des touches : 4 colonnes x 5 lignes. Le "0" occupe 2 cases. */
static const char *BTN[5][4] = {
    { "C",   "+/-", "%", "/" },
    { "7",   "8",   "9", "x" },
    { "4",   "5",   "6", "-" },
    { "1",   "2",   "3", "+" },
    { "0",   "",    ".", "=" },
};

/* --- etat global de l'appli --- */
static FXCalc G;
static int W, H, win_x, win_y, win_w, win_h;
static int hover = -1;
static FILE *LOG;

static const uint32_t C_BG      = 0x0F1626, C_FG = 0xEAF2FF, C_DIM = 0x7C93B5;
static const uint32_t C_PANEL   = 0x0C1424, C_RED = 0xE5484D, C_VIOLET = 0xD633FF;
static const uint32_t C_ORANGE  = 0xF6AD55, C_NUM = 0x1B2740, C_FUNC = 0x2C3A55;

static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(LOG, fmt, ap); va_end(ap);
    fputc('\n', LOG); fflush(LOG);
}

/* ============================================================
 *  LE MOTEUR (aucun ecran : testable tout seul)
 * ============================================================ */

void fx_calc_reset(FXCalc *c) {
    memset(c, 0, sizeof *c);
    snprintf(c->saisie, sizeof c->saisie, "0");
    c->op = 0;
    c->neuf = 1;
}

int fx_calc_opere(double a, char op, double b, double *out) {
    switch (op) {
    case '+': *out = a + b; return 0;
    case '-': *out = a - b; return 0;
    case 'x': case '*': *out = a * b; return 0;
    case '/': case ':':
        if (b == 0.0) return -1;      /* division par zero : on refuse */
        *out = a / b; return 0;
    default: return -2;
    }
}

void fx_calc_formate(double v, char *out, size_t n) {
    if (v == 0.0) { snprintf(out, n, "0"); return; }
    snprintf(out, n, "%.10g", v);
}

static double lire(const char *s) {
    if (!s || !s[0]) return 0.0;
    return strtod(s, NULL);
}

static void pousse_histo(FXCalc *c, const char *ligne) {
    for (int i = 2; i > 0; i--) memcpy(c->histo[i], c->histo[i - 1], sizeof c->histo[i]);
    snprintf(c->histo[0], sizeof c->histo[0], "%s", ligne);
    if (c->nhisto < 3) c->nhisto++;
}

static void ajoute_chiffre(FXCalc *c, char k) {
    if (c->neuf) { c->saisie[0] = 0; c->neuf = 0; }
    if (k == '.') {
        if (!strchr(c->saisie, '.')) {
            size_t l = strlen(c->saisie);
            if (l == 0) { snprintf(c->saisie, sizeof c->saisie, "0."); }
            else if (l < sizeof c->saisie - 1) { c->saisie[l] = '.'; c->saisie[l + 1] = 0; }
        }
        return;
    }
    size_t l = strlen(c->saisie);
    if (l == 0) { c->saisie[0] = k; c->saisie[1] = 0; }
    else if (c->saisie[0] == '0' && l == 1) { c->saisie[0] = k; }   /* pas de zero de tete */
    else if (l < sizeof c->saisie - 1) { c->saisie[l] = k; c->saisie[l + 1] = 0; }
}

void fx_calc_touche(FXCalc *c, const char *t) {
    if (!t || !t[0]) return;

    /* C : on remet tout a zero */
    if (!strcmp(t, "C")) { fx_calc_reset(c); logf_("touche C"); return; }

    /* toute autre touche sort d'abord de l'etat d'erreur */
    if (c->erreur) { c->erreur = 0; c->acc = 0; c->op = 0; c->neuf = 1; snprintf(c->saisie, sizeof c->saisie, "0"); }

    /* +/- : on change le signe de la saisie */
    if (!strcmp(t, "+/-")) {
        double v = -lire(c->saisie);
        fx_calc_formate(v, c->saisie, sizeof c->saisie);
        return;
    }
    /* % : on divise la saisie par 100 */
    if (!strcmp(t, "%")) {
        double v = lire(c->saisie) / 100.0;
        fx_calc_formate(v, c->saisie, sizeof c->saisie);
        return;
    }

    char k = t[0];

    if ((k >= '0' && k <= '9') || k == '.') { ajoute_chiffre(c, k); return; }

    if (k == '+' || k == '-' || k == 'x' || k == '/') {
        double v = lire(c->saisie), r = 0;
        if (c->op != 0 && !c->neuf) {
            /* un calcul etait en attente : on le resout d'abord */
            if (fx_calc_opere(c->acc, c->op, v, &r) != 0) {
                c->erreur = 1; c->op = 0; c->neuf = 1;
                snprintf(c->saisie, sizeof c->saisie, "Erreur");
                logf_("erreur en chainant l'operateur");
                return;
            }
            c->acc = r;
        } else if (c->op == 0) {
            c->acc = v;
        }
        c->op = k;
        c->neuf = 1;
        fx_calc_formate(c->acc, c->saisie, sizeof c->saisie);
        return;
    }

    if (k == '=') {
        if (c->op != 0) {
            double v = lire(c->saisie), r = 0;
            char ga[64], gb[64], gr[64], ligne[96];
            fx_calc_formate(c->acc, ga, sizeof ga);
            fx_calc_formate(v, gb, sizeof gb);
            if (fx_calc_opere(c->acc, c->op, v, &r) != 0) {
                c->erreur = 1;
                snprintf(ligne, sizeof ligne, "%s %c %s = Erreur", ga, c->op, gb);
                c->op = 0; c->neuf = 1;
                pousse_histo(c, ligne);
                snprintf(c->saisie, sizeof c->saisie, "Erreur");
                logf_("division par zero refusee");
                return;
            }
            fx_calc_formate(r, gr, sizeof gr);
            snprintf(ligne, sizeof ligne, "%s %c %s = %s", ga, c->op, gb, gr);
            pousse_histo(c, ligne);
            c->acc = r;
            snprintf(c->saisie, sizeof c->saisie, "%s", gr);
        } else {
            c->acc = lire(c->saisie);
        }
        c->op = 0;
        c->neuf = 1;
        return;
    }
}

void fx_calc_retour(FXCalc *c) {
    if (c->erreur) { fx_calc_reset(c); return; }
    if (c->neuf) return;
    size_t l = strlen(c->saisie);
    if (l > 1) c->saisie[l - 1] = 0;
    else snprintf(c->saisie, sizeof c->saisie, "0");
}

/* ============================================================
 *  L'AFFICHAGE
 * ============================================================ */

static void layout(int *gx, int *gy, int *cw, int *ch) {
    int pw = win_w - 2 * PAD;
    int avail_h = win_h - FX_TITLEBAR_H - 3 * PAD - DISP_H;
    *cw = (pw - 3 * GAP) / 4;
    *ch = (avail_h - 4 * GAP) / 5;
    *gx = win_x + PAD;
    *gy = win_y + FX_TITLEBAR_H + PAD + DISP_H + PAD;
}

static void btn_rect(int r, int c, int *x, int *y, int *w, int *h) {
    int gx, gy, cw, ch;
    layout(&gx, &gy, &cw, &ch);
    *x = gx + c * (cw + GAP);
    *y = gy + r * (ch + GAP);
    *w = cw; *h = ch;
    if (r == 4 && c == 0) *w = 2 * cw + GAP;   /* le zero est large */
}

static void couleurs_touche(const char *lab, uint32_t *bg, uint32_t *fg) {
    if (!strcmp(lab, "C"))      { *bg = C_RED;    *fg = 0xFFFFFF; }
    else if (!strcmp(lab, "=")) { *bg = C_VIOLET; *fg = 0xFFFFFF; }
    else if (!strcmp(lab, "+") || !strcmp(lab, "-") || !strcmp(lab, "x") || !strcmp(lab, "/"))
                                { *bg = C_ORANGE; *fg = 0x1A1206; }
    else if (!strcmp(lab, "+/-") || !strcmp(lab, "%") || !strcmp(lab, "."))
                                { *bg = C_FUNC;   *fg = C_FG; }
    else                        { *bg = C_NUM;    *fg = C_FG; }
}

void fx_calc_draw(FXScreen *s) {
    fx_clear(s, 0x0A0F1A);
    fx_gradient_v(s, 0, 0, W, H, 0x0D1422, 0x16233C);
    fx_window(s, win_x, win_y, win_w, win_h, "Calculatrice", C_BG);

    /* --- l'ecran de la calculatrice --- */
    int px = win_x + PAD, py = win_y + FX_TITLEBAR_H + PAD;
    int pw = win_w - 2 * PAD, ph = DISP_H;
    fx_fill_round_rect(s, px, py, pw, ph, 10, C_PANEL);

    /* le calcul en attente, en petit, en haut a droite */
    if (G.op) {
        char acc[64], tb[80];
        fx_calc_formate(G.acc, acc, sizeof acc);
        snprintf(tb, sizeof tb, "%s %c", acc, G.op);
        int tw = (int)strlen(tb) * FONT_W;
        fx_draw_text(s, px + pw - tw - 12, py + 8, tb, C_ORANGE);
    }

    /* l'historique des 3 derniers calculs, en petit (plus ancien en haut) */
    for (int k = 0; k < G.nhisto; k++) {
        const char *h = G.histo[G.nhisto - 1 - k];
        fx_draw_text(s, px + 12, py + 8 + k * FONT_H, h, C_DIM);
    }

    /* la valeur courante, en grand, alignee a droite */
    int len = (int)strlen(G.saisie);
    int sc = 3, maxw = pw - 24;
    while (sc > 1 && len * FONT_W * sc > maxw) sc--;
    int vw = len * FONT_W * sc;
    int vy = py + ph - FONT_H * sc - 10;
    fx_draw_text_scale(s, px + pw - 12 - vw, vy, G.saisie, G.erreur ? C_RED : C_FG, sc);

    /* --- les touches --- */
    int ch0 = 0;
    { int gx, gy, cw, ch; layout(&gx, &gy, &cw, &ch); ch0 = ch; }
    int bsc = (ch0 >= 44 && (win_w - 2 * PAD) / 4 >= 56) ? 2 : 1;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 1) continue;   /* case absorbee par le zero */
            const char *lab = BTN[r][c];
            if (!lab[0]) continue;
            int x, y, w, h;
            btn_rect(r, c, &x, &y, &w, &h);
            uint32_t bg, fg;
            couleurs_touche(lab, &bg, &fg);
            fx_fill_round_rect(s, x, y, w, h, 8, bg);
            if (hover == r * 4 + c) fx_blend_round_rect(s, x, y, w, h, 8, 0xFFFFFF, 26);
            int tw = (int)strlen(lab) * FONT_W * bsc;
            int th = FONT_H * bsc;
            fx_draw_text_scale(s, x + (w - tw) / 2, y + (h - th) / 2, lab, fg, bsc);
        }
    }
}

/* cherche la touche sous le pointeur, renvoie son libelle ou NULL */
static const char *touche_sous(int x, int y) {
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 1) continue;
            const char *lab = BTN[r][c];
            if (!lab[0]) continue;
            int bx, by, bw, bh;
            btn_rect(r, c, &bx, &by, &bw, &bh);
            if (x >= bx && x < bx + bw && y >= by && y < by + bh) { hover = r * 4 + c; return lab; }
        }
    }
    return NULL;
}

int fx_calc_click(int x, int y) {
    int bx, by, bsz, bgap;
    fx_window_buttons_rect(win_x, win_y, win_w, &bx, &by, &bsz, &bgap);
    if (y >= by - 4 && y <= by + bsz + 4) {
        if (x >= bx + 2 * (bsz + bgap) - 4) return 1;   /* fermer */
        return 0;
    }
    const char *lab = touche_sous(x, y);
    if (lab) { fx_calc_touche(&G, lab); logf_("clic sur %s -> %s", lab, G.saisie); }
    return 0;
}

int fx_calc_move(int x, int y) {
    hover = -1;
    (void)touche_sous(x, y);
    return 0;
}

int fx_calc_key(int key) {
    switch (key) {
    case FXC_CLEAR: fx_calc_touche(&G, "C"); break;
    case FXC_ENTER: fx_calc_touche(&G, "="); break;
    case FXC_BACK:  fx_calc_retour(&G); break;
    case FXC_NEG:   fx_calc_touche(&G, "+/-"); break;
    case 'C': case 'c': fx_calc_touche(&G, "C"); break;
    case '=': case '\n': case '\r': fx_calc_touche(&G, "="); break;
    case '*': fx_calc_touche(&G, "x"); break;
    case ',': fx_calc_touche(&G, "."); break;
    case '+': case '-': case '/': case '.': case '%':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
        char t[2] = { (char)key, 0 };
        fx_calc_touche(&G, t);
        break; }
    default: break;
    }
    return 0;
}

int fx_calc_scroll(int up) { (void)up; return 0; }

void fx_calc_init(int sw, int sh, const char *log_path) {
    W = sw; H = sh;
    if (log_path) LOG = fopen(log_path, "w");
    win_w = sw > 520 ? 420 : (sw > 120 ? sw - 80 : sw);
    win_h = sh > 660 ? 600 : (sh > 200 ? sh - 80 : sh);
    win_x = (sw - win_w) / 2;
    win_y = (sh - win_h) / 2;
    fx_calc_reset(&G);
    logf_("calculatrice ouverte en %dx%d", sw, sh);
}

void fx_calc_demo(void) {
    fx_calc_reset(&G);
    pousse_histo(&G, "9 + 3 = 12");
    pousse_histo(&G, "144 / 12 = 12");
    pousse_histo(&G, "7 x 6 = 42");
    snprintf(G.saisie, sizeof G.saisie, "42");
    G.neuf = 1;
    logf_("apercu : %d calculs factices", G.nhisto);
}
