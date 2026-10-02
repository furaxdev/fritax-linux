/* ============================================================
 *  FRITAX - icones d'applications (dessinees pixel par pixel)
 *  Tout est relatif a la taille demandee : une seule source,
 *  utilisable pour le bureau, le lanceur, la barre et les titres.
 * ============================================================ */
#include "appicon.h"
#include <string.h>
#include <ctype.h>

#define W_FG   0xEAF2FF
#define W_DIM  0x7C93B5
#define W_CYAN 0x5BC8FF
#define W_VIOLET 0xD633FF
#define W_AMBER 0xF6AD55
#define W_GREEN 0x39DE8A
#define W_DARK 0x101828
#define W_PAPER 0xF2F6FF

/* cosinus / sinus en millièmes, sans bibliotheque mathematique */
static int my_cos(int deg) {
    static const int t[13] = { 1000, 866, 500, 0, -500, -866, -1000, -866, -500, 0, 500, 866, 1000 };
    int d = ((deg % 360) + 360) % 360;
    int i = d / 30, f = d % 30;                    /* interpolation lineaire -> cercle lisse */
    return t[i] + (t[i + 1] - t[i]) * f / 30;
}
static int my_sin(int deg) { return my_cos(deg - 90); }

/* petit utilitaire : trace une ligne epaisse (pour le crayon, le chevron) */
static void line(FXScreen *s, int x0, int y0, int x1, int y1, int th, uint32_t c) {
    int dx = x1 - x0, dy = y1 - y0;
    int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (n < 1) n = 1;
    for (int i = 0; i <= n; i++) {
        int x = x0 + dx * i / n, y = y0 + dy * i / n;
        fx_fill_rect(s, x - th / 2, y - th / 2, th, th, c);
    }
}

/* ramene les lettres accentuees (latin-1) a leur equivalent ascii */
static char sans_accent(unsigned char c) {
    switch (c) {
    case 0xE0: case 0xE2: case 0xE4: return 'a';
    case 0xE7: return 'c';
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
    case 0xEE: case 0xEF: return 'i';
    case 0xF4: case 0xF6: return 'o';
    case 0xF9: case 0xFB: case 0xFC: return 'u';
    default: return (char)c;
    }
}

int fx_app_kind(const char *name) {
    if (!name) return FX_ICON_GENERIC;
    char low[64] = {0};
    for (int i = 0; i < 63 && name[i]; i++)
        low[i] = (char)tolower((unsigned char)(unsigned char)sans_accent((unsigned char)name[i]));
    if (strstr(low, "terminal") || strstr(low, "console") || strstr(low, "shell")) return FX_ICON_TERMINAL;
    if (strstr(low, "fichier") || strstr(low, "file") || strstr(low, "dossier")) return FX_ICON_FILES;
    if (strstr(low, "reglage") || strstr(low, "parametre") || strstr(low, "setting") || strstr(low, "config")) return FX_ICON_SETTINGS;
    if (strstr(low, "bloc") || strstr(low, "editeur") || strstr(low, "editor") || strstr(low, "note") || strstr(low, "texte")) return FX_ICON_EDITOR;
    if (strstr(low, "eteindre") || strstr(low, "eteint") || strstr(low, "arret") || strstr(low, "arret")
        || strstr(low, "power") || strstr(low, "quitter") || strstr(low, "extinction")) return FX_ICON_POWER;
    if (strstr(low, "redemarr") || strstr(low, "reboot") || strstr(low, "restart")) return FX_ICON_POWER;
    return FX_ICON_GENERIC;
}

void fx_app_icon(FXScreen *s, int x, int y, int sz, int kind) {
    if (sz < 16) sz = 16;
    int u = sz / 16;                              /* unite d'echelle */
    if (u < 1) u = 1;

    if (kind == FX_ICON_TERMINAL) {
        /* une petite fenetre de terminal : fond sombre + chevron cyan + tiret */
        fx_fill_round_rect(s, x, y, sz, sz, 4 * u, 0x16203A);
        fx_fill_round_rect(s, x + u, y + u, sz - 2 * u, (sz - 2 * u) / 3, 2 * u, 0x25344C);
        line(s, x + 4 * u, y + 7 * u, x + 7 * u, y + 10 * u, 2 * u, W_CYAN);
        line(s, x + 7 * u, y + 10 * u, x + 4 * u, y + 13 * u, 2 * u, W_CYAN);
        fx_fill_rect(s, x + 9 * u, y + 12 * u, 4 * u, 2 * u, W_FG);
        return;
    }

    if (kind == FX_ICON_FILES) {
        /* un dossier : onglet + corps + pli interieur */
        fx_fill_round_rect(s, x + u, y + 2 * u, 6 * u, 3 * u, 2 * u, 0xD08A2E);
        fx_fill_round_rect(s, x, y + 4 * u, sz, sz - 5 * u, 3 * u, W_AMBER);
        fx_fill_round_rect(s, x + u, y + 6 * u, sz - 2 * u, 2 * u, u, 0xFFD9A0);
        return;
    }

    if (kind == FX_ICON_SETTINGS) {
        /* un engrenage net : 8 dents droites + anneau epais + moyeu */
        int cx = x + sz / 2, cy = y + sz / 2, r = sz / 2;
        static const int dxs[8] = { 0, 5, 7, 5, 0, -5, -7, -5 };
        static const int dys[8] = { -7, -5, 0, 5, 7, 5, 0, -5 };
        for (int i = 0; i < 8; i++) {
            int tx = cx + dxs[i] * r / 8, ty = cy + dys[i] * r / 8;
            fx_fill_rect(s, tx - 2 * u, ty - 2 * u, 4 * u, 4 * u, W_VIOLET);   /* dents carrees */
        }
        fx_fill_circle(s, cx, cy, r - 2 * u, W_VIOLET);        /* corps */
        fx_fill_circle(s, cx, cy, r - 6 * u, 0x16203A);        /* creux */
        return;
    }

    if (kind == FX_ICON_POWER) {
        /* symbole d'extinction : anneau ouvert en haut + tige verticale */
        int cx = x + sz / 2, cy = y + sz / 2 + u, r = sz / 2 - 3 * u;
        for (int d = 0; d < 360; d++) {                    /* anneau PLEIN, ouvert entre 55 et 125 degres */
            if (d > 55 && d < 125) continue;
            int px = cx + r * my_cos(d) / 1000, py = cy + r * my_sin(d) / 1000;
            fx_fill_round_rect(s, px - 2 * u, py - 2 * u, 3 * u, 3 * u, u, 0xFF7A7A);      /* se recouvrent -> trait continu */
        }
        fx_fill_round_rect(s, cx - u, y + 2 * u, 2 * u + (u == 1), 7 * u, u, 0xFF7A7A);   /* tige */
        return;
    }

    if (kind == FX_ICON_FILE) {
        /* une feuille avec un coin plie */
        fx_fill_round_rect(s, x + 2 * u, y + u, 11 * u, 14 * u, 2 * u, 0xE8EEFA);
        fx_fill_rect(s, x + 4 * u, y + (4) * u, 7 * u, u, 0xA9B8D0);
        fx_fill_rect(s, x + 4 * u, y + (7) * u, 7 * u, u, 0xA9B8D0);
        fx_fill_rect(s, x + 4 * u, y + (10) * u, 4 * u, u, 0xA9B8D0);
        fx_fill_round_rect(s, x + 11 * u, y + u, 3 * u, 3 * u, u, 0xC6D2E6);
        return;
    }

    if (kind == FX_ICON_EDITOR) {
        /* une feuille + un crayon bien fin, pointe en bas a gauche */
        fx_fill_round_rect(s, x + 2 * u, y + u, 11 * u, 14 * u, 2 * u, W_PAPER);
        for (int i = 0; i < 4; i++)
            fx_fill_rect(s, x + 4 * u, y + (4 + i * 3) * u, 6 * u, u, 0xA9B8D0);
        line(s, x + 11 * u, y + 5 * u, x + 15 * u, y + u, 2 * u, 0x2FA36B);      /* corps du crayon */
        line(s, x + 10 * u, y + 6 * u, x + 11 * u, y + 5 * u, 2 * u, 0x1C6E45);  /* pointe */
        return;
    }

    /* generique : carre arrondi + pastille */
    fx_fill_round_rect(s, x, y, sz, sz, 4 * u, 0x1B2740);
    fx_fill_round_rect(s, x + 4 * u, y + 4 * u, sz - 8 * u, sz - 8 * u, 3 * u, W_DIM);
}
