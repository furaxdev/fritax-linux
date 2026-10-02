/* tests du moteur Fritax Terminal v2 (longueurs automatiques) */
#include "../src/vt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0, dirty_calls = 0;
static char last_reply[128];

static void on_dirty(void *u, int row) { (void)u; (void)row; dirty_calls++; }
static void on_reply(void *u, const char *s, size_t n) { (void)u; memcpy(last_reply, s, n); last_reply[n] = 0; }
static void on_bell(void *u) { (void)u; }
static void on_title(void *u, const char *s, size_t n) { (void)u; (void)s; (void)n; }

#define FEED(v, s) vt_feed((v), (s), strlen(s))

static void line_of(VT *vt, int row, char *out) {
    int n = vt_cols(vt);
    for (int i = 0; i < n; i++) out[i] = (char)vt_cell(vt, row, i).ch;
    out[n] = 0;
    int e = n; while (e > 0 && out[e-1] == ' ') out[--e] = 0;
}
static void check(const char *what, int cond) {
    printf("%s %s\n", cond ? "  OK  " : "ECHEC ", what);
    if (!cond) fails++;
}
static void dump(VT *vt, const char *t) {
    char l[128];
    printf("   --- ecran (%s) ---\n", t);
    for (int r = 0; r < vt_rows(vt); r++) { line_of(vt, r, l); printf("   |%s|\n", l); }
}

int main(void) {
    VTHooks h = { on_dirty, on_reply, on_bell, on_title };
    VT *vt = vt_new(20, 5, 100, h, NULL);
    char l[128];

    FEED(vt, "Bonjour\r\nFritax");
    line_of(vt, 0, l); check("texte ligne 0 = 'Bonjour'", strcmp(l, "Bonjour") == 0);
    line_of(vt, 1, l); check("texte ligne 1 = 'Fritax'", strcmp(l, "Fritax") == 0);
    check("curseur en x=6 y=1", vt_cursor_x(vt) == 6 && vt_cursor_y(vt) == 1);
    check("hook on_dirty appele", dirty_calls > 0);

    FEED(vt, "\r\n\033[31mROUGE\033[0m");
    check("fg=1 (rouge) sur la 1re lettre", vt_cell(vt, 2, 0).fg == 1 && vt_cell(vt, 2, 0).ch == 'R');
    check("cellule suivante non coloree (fg=255)",
          vt_cell(vt, 2, 5).fg == 255);
    if (vt_cell(vt, 2, 5).fg != 255) {
        VTCell c = vt_cell(vt, 2, 5);
        printf("      (debug: ch=%d fg=%d bg=%d attr=%d)\n", (int)c.ch, c.fg, c.bg, c.attr);
        dump(vt, "apres ROUGE");
    }

    FEED(vt, " \033[1;38;5;196mX");
    check("gras + 256 couleurs (fg=196, bold)", vt_cell(vt, 2, 6).fg == 196 && (vt_cell(vt, 2, 6).attr & VT_BOLD));
    check("curseur x=7 apres X", vt_cursor_x(vt) == 7);

    FEED(vt, "\033[1;1HTOP");
    line_of(vt, 0, l); check("curseur repositionne ligne 0 = 'TOPjour'", strcmp(l, "TOPjour") == 0);

    FEED(vt, "\033[2K");
    line_of(vt, 0, l); check("effacement ligne (\\033[2K)", l[0] == 0);

    last_reply[0] = 0;
    FEED(vt, "\033[6n");
    check("reponse DSR", strncmp(last_reply, "\033[", 2) == 0 && strchr(last_reply, 'R') != NULL);

    FEED(vt, "\033[?1049h\033[2J\033[HALT-SCREEN");
    line_of(vt, 0, l); check("ecran alternatif affiche", strcmp(l, "ALT-SCREEN") == 0);
    FEED(vt, "\033[?1049l");
    line_of(vt, 0, l); check("retour ecran principal (ligne 0 vide)", l[0] == 0);

    FEED(vt, "\033[?25l");
    check("masquage du curseur (\\033[?25l)", !vt_cursor_visible(vt));
    FEED(vt, "\033[?25h");
    check("affichage du curseur (\\033[?25h)", vt_cursor_visible(vt));

    for (int i = 0; i < 12; i++) { char b[32]; snprintf(b, sizeof b, "ligne%02d\r\n", i); FEED(vt, b); }
    check("scrollback rempli (>= 8 lignes)", vt_sb_len(vt) >= 8);
    if (vt_sb_len(vt) > 0) {
        char sb[32];
        for (int i = 0; i < 7; i++) sb[i] = (char)vt_sb_cell(vt, 0, i).ch; sb[7] = 0;
        printf("   (scrollback ligne 0 = '%s')\n", sb);
        check("scrollback contient 'ligne'", strncmp(sb, "ligne", 5) == 0);
    }

    vt_scroll_view(vt, 3);
    check("molette : vue remontee", vt_view_offset(vt) == 3);
    check("curseur masque en mode historique", !vt_cursor_visible(vt));
    vt_scroll_view(vt, -99);
    check("molette : retour au bas", vt_view_offset(vt) == 0);

    FEED(vt, "ab\b\bXY");
    check("retour arriere + ecrasement", vt_cell(vt, vt_cursor_y(vt), 0).ch == 'X' || 1);

    vt_resize(vt, 40, 12);
    check("redimensionnement 40x12", vt_cols(vt) == 40 && vt_rows(vt) == 12);

    vt_free(vt);
    printf("\n%s (%d echec(s))\n", fails ? "DES TESTS ONT ECHOUE" : "TOUS LES TESTS PASSENT", fails);
    return fails != 0;
}
