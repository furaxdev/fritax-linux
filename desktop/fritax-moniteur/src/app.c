/* ============================================================
 *  FRITAX MONITEUR - le moniteur systeme de la distribution
 *
 *  Dessine avec notre boite a outils (screen.c + window.c) et
 *  affiche les VRAIES donnees lues dans /proc par le module proc.c.
 *  Aucune dependance : juste la libc.
 * ============================================================ */
#define _GNU_SOURCE
#include "app.h"
#include "appicon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define PAD 20
#define BAR_H 18

static FXMonInfo info;
static int W, H, win_x, win_y, win_w, win_h;
static FILE *LOG;
static int mode_demo = 0;

static const uint32_t C_BG = 0x0F1626, C_FG = 0xEAF2FF, C_DIM = 0x7C93B5;
static const uint32_t C_ACCENT = 0xD633FF, C_PISTE = 0x18233C;
static const uint32_t C_VERT = 0x4ADE80, C_JAUNE = 0xFBBF24, C_ROUGE = 0xF87171;
static const uint32_t C_SWAP = 0x60A5FA;

static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(LOG, fmt, ap); va_end(ap); fputc('\n', LOG); fflush(LOG);
}

/* couleur de la barre selon le taux (vert / orange / rouge) */
static uint32_t couleur_pct(double p) {
    if (p < 60) return C_VERT;
    if (p < 85) return C_JAUNE;
    return C_ROUGE;
}

/* barre de progression arrondie, remplie a `pct` % */
static void barre(FXScreen *s, int x, int y, int w, int h, double pct, uint32_t col) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    fx_fill_round_rect(s, x, y, w, h, h / 2, C_PISTE);
    int fw = (int)((double)w * pct / 100.0);
    if (fw > 0) {
        if (fw < h) fw = h;                 /* garde la forme arrondie lisible */
        if (fw > w) fw = w;
        fx_fill_round_rect(s, x, y, fw, h, h / 2, col);
    }
}

/* une mesure : etiquette a gauche, valeur a droite, puis la barre */
static void mesure(FXScreen *s, int x, int *y, int w, const char *label,
                   const char *valeur, double pct, uint32_t col) {
    fx_draw_text(s, x, *y, label, C_DIM);
    fx_draw_text(s, x + w - (int)strlen(valeur) * FONT_W, *y, valeur, C_FG);
    *y += FONT_H + 4;
    barre(s, x, *y, w, BAR_H, pct, col);
    *y += BAR_H + 22;
}

void fx_mon_init(int sw, int sh, const char *log_path) {
    W = sw; H = sh;
    if (log_path) LOG = fopen(log_path, "w");
    win_w = sw > 1000 ? 760 : sw - 60;
    win_h = sh > 700 ? 540 : sh - 100;
    win_x = (sw - win_w) / 2;
    win_y = (sh - win_h) / 2;
    mode_demo = 0;
    if (fx_mon_lire(&info) != 0) logf_("avertissement : lecture de /proc incomplete");
    logf_("fenetre %dx%d, %s", win_w, win_h, info.cpu_modele);
}

void fx_mon_rafraichir(void) {
    if (mode_demo) return;
    static time_t dernier = -1;             /* -1 : la premiere passe lit toujours */
    time_t maintenant = time(NULL);
    if (maintenant == dernier) return;      /* au plus un releve par seconde */
    dernier = maintenant;
    FXMonInfo nouveau;
    if (fx_mon_lire(&nouveau) == 0) info = nouveau;
    else logf_("releve /proc rate, on garde le precedent");
}

void fx_mon_draw(FXScreen *s) {
    fx_clear(s, 0x0A0F1A);
    fx_gradient_v(s, 0, 0, W, H, 0x0D1422, 0x16233C);

    fx_window(s, win_x, win_y, win_w, win_h, "Moniteur systeme", C_BG);

    int x = win_x + PAD;
    int w = win_w - 2 * PAD;
    int y = win_y + FX_TITLEBAR_H + PAD;

    /* --- processeur : modele + nombre de coeurs --- */
    fx_draw_text(s, x, y, "PROCESSEUR", C_ACCENT);
    y += FONT_H + 6;
    char modele[128];
    snprintf(modele, sizeof modele, "%.*s", w / FONT_W, info.cpu_modele);
    fx_draw_text(s, x, y, modele, C_FG);
    y += FONT_H + 2;
    char coeurs[64];
    snprintf(coeurs, sizeof coeurs, "%d processeur%s logique%s",
             info.nb_coeurs, info.nb_coeurs > 1 ? "s" : "", info.nb_coeurs > 1 ? "s" : "");
    fx_draw_text(s, x, y, coeurs, C_DIM);
    y += FONT_H + 14;

    /* --- occupation CPU : barre + pourcentage --- */
    char tv[32];
    snprintf(tv, sizeof tv, "%.1f %%", info.cpu_pct);
    mesure(s, x, &y, w, "Occupation du processeur", tv, info.cpu_pct, couleur_pct(info.cpu_pct));

    /* --- memoire vive : barre + Mo --- */
    double mem_pct = info.mem_total_mo > 0 ? 100.0 * info.mem_utilisee_mo / info.mem_total_mo : 0;
    char tmem[64];
    snprintf(tmem, sizeof tmem, "%.0f / %.0f Mo", info.mem_utilisee_mo, info.mem_total_mo);
    mesure(s, x, &y, w, "Memoire vive", tmem, mem_pct, couleur_pct(mem_pct));

    /* --- memoire d'echange (swap) --- */
    double swap_util = info.swap_total_mo - info.swap_libre_mo;
    if (swap_util < 0) swap_util = 0;
    double swap_pct = info.swap_total_mo > 0 ? 100.0 * swap_util / info.swap_total_mo : 0;
    char tsw[64];
    snprintf(tsw, sizeof tsw, "%.0f / %.0f Mo", swap_util, info.swap_total_mo);
    mesure(s, x, &y, w, "Memoire d'echange (swap)", tsw, swap_pct, C_SWAP);

    /* --- charge moyenne 1 / 5 / 15 minutes --- */
    fx_draw_text(s, x, y, "Charge moyenne", C_DIM);
    char tch[64];
    snprintf(tch, sizeof tch, "%.2f   %.2f   %.2f", info.charge1, info.charge5, info.charge15);
    fx_draw_text(s, x + w - (int)strlen(tch) * FONT_W, y, tch, C_FG);
    y += FONT_H + 2;
    fx_draw_text(s, x, y, "1 min                                     15 min", 0x54678A);
    y += FONT_H + 14;

    /* --- temps de fonctionnement en jour / heure / minute --- */
    long up = (long)info.uptime_s;
    long j = up / 86400, h = (up % 86400) / 3600, m = (up % 3600) / 60;
    fx_draw_text(s, x, y, "Temps de fonctionnement", C_DIM);
    char tup[64];
    snprintf(tup, sizeof tup, "%ld j  %02ld h  %02ld min", j, h, m);
    fx_draw_text(s, x + w - (int)strlen(tup) * FONT_W, y, tup, C_FG);
    y += FONT_H + 18;

    /* --- pied d'aide --- */
    const char *aide = "r : rafraichir    Echap : quitter";
    if (win_w > 2 * PAD + 360)
        fx_draw_text(s, x, win_y + win_h - FONT_H - 12, aide, 0x54678A);
    (void)mode_demo;
}

int fx_mon_click(int x, int y) {
    int bx, by, bsz, gap;
    fx_window_buttons_rect(win_x, win_y, win_w, &bx, &by, &bsz, &gap);
    if (y >= by - 4 && y <= by + bsz + 4) {
        if (x >= bx + 2 * (bsz + gap) - 4) return 1;                 /* fermer */
        if (x >= bx + bsz + gap - 4 && x < bx + 2 * (bsz + gap) - 4) return 0;
        if (x >= bx - 4 && x < bx + bsz + gap - 4) return 0;
    }
    return 0;
}

int fx_mon_move(int x, int y) { (void)x; (void)y; return 0; }

int fx_mon_key(int key) {
    if (key == FXM_RAFRAICHIR) {
        FXMonInfo nouveau;
        if (fx_mon_lire(&nouveau) == 0) info = nouveau;
    }
    return 0;
}

int fx_mon_valide(void) { return info.mem_total_mo > 0; }

void fx_mon_demo(void) {
    mode_demo = 1;
    snprintf(info.cpu_modele, sizeof info.cpu_modele, "Fritax Virtual CPU @ 3.60 GHz");
    info.nb_coeurs    = 8;
    info.cpu_pct      = 37.5;
    info.mem_total_mo = 15984;
    info.mem_dispo_mo = 6789;
    info.mem_utilisee_mo = info.mem_total_mo - info.mem_dispo_mo;
    info.swap_total_mo = 4096;
    info.swap_libre_mo = 4096;
    info.charge1 = 0.49; info.charge5 = 0.43; info.charge15 = 0.39;
    info.uptime_s = 123456;
    logf_("apercu : valeurs factices");
}
