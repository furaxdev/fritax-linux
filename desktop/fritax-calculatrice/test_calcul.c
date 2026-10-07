/* ============================================================
 *  Test du moteur de la calculatrice Fritax - sans aucun ecran.
 *  On appelle fx_calc_opere() sur une dizaine de cas, et on pilote
 *  l'etat complet avec fx_calc_touche(). Affiche OK / ECHEC partout.
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "app.h"

static int reussis, rates;

static void chk(int condition, const char *quoi) {
    if (condition) { reussis++; printf("  OK    %s\n", quoi); }
    else           { rates++;   printf("  ECHEC %s\n", quoi); }
}

/* comparaison de flottants avec tolerance relative (grands nombres) */
static int proche(double a, double b) {
    double d = a - b; if (d < 0) d = -d;
    double aa = a < 0 ? -a : a, bb = b < 0 ? -b : b;
    double m = bb > aa ? bb : aa;
    if (m < 1.0) m = 1.0;
    return d <= 1e-6 * m;
}

/* un cas du moteur pur : a (op) b doit donner attendu */
static void cas(double a, char op, double b, double attendu, const char *nom) {
    double r = 0;
    int rc = fx_calc_opere(a, op, b, &r);
    if (rc != 0) { printf("  ECHEC %s (%g %c %g -> code %d)\n", nom, a, op, b, rc); rates++; return; }
    if (proche(r, attendu)) { printf("  OK    %s (%g %c %g = %g)\n", nom, a, op, b, r); reussis++; }
    else { printf("  ECHEC %s (%g %c %g = %g, attendu %g)\n", nom, a, op, b, r, attendu); rates++; }
}

/* tape une suite de touches sur un FXCalc, comme un vrai clavier */
static void tape(FXCalc *c, const char **touches, int n) {
    for (int i = 0; i < n; i++) fx_calc_touche(c, touches[i]);
}

int main(void) {
    printf("Tests de la calculatrice Fritax\n\n");

    /* --- le moteur pur sur une dizaine de cas --- */
    printf("Moteur de calcul :\n");
    cas(2, '+', 3, 5, "addition simple");
    cas(10, '-', 4, 6, "soustraction simple");
    cas(6, 'x', 7, 42, "multiplication simple");
    cas(20, '/', 4, 5, "division exacte");
    cas(3.5, '+', 1.25, 4.75, "nombres a virgule");
    cas(-8, '+', 3, -5, "nombre negatif plus positif");
    cas(-6, 'x', -7, 42, "produit de deux negatifs");
    cas(0, '-', 9, -9, "resultat negatif");
    cas(-10, '/', 4, -2.5, "division negative fractionnaire");
    cas(1000000000000.0, 'x', 100000000.0, 1e20, "tres grands nombres");
    cas(999999999.0, '+', 1.0, 1000000000.0, "passage du milliard");

    /* --- division par zero : doit etre refusee, pas de plantage --- */
    printf("\nDivision par zero :\n");
    {
        double r = 0;
        chk(fx_calc_opere(5, '/', 0, &r) != 0, "5 / 0 renvoie un code d'erreur");
        chk(fx_calc_opere(0, '/', 0, &r) != 0, "0 / 0 renvoie un code d'erreur");
        FXCalc c; fx_calc_reset(&c);
        const char *seq[] = { "5", "/", "0", "=" };
        tape(&c, seq, 4);
        chk(!strcmp(c.saisie, "Erreur"), "la touche = affiche 'Erreur' au lieu de planter");
        chk(c.erreur == 1, "l'etat passe en erreur");
        chk(c.nhisto >= 1, "le calcul fautif est note dans l'historique");
        /* une touche quelconque efface l'erreur et repart de zero */
        fx_calc_touche(&c, "7");
        chk(!strcmp(c.saisie, "7") && c.erreur == 0, "on repart proprement apres l'erreur");
    }

    /* --- une saisie complete au clavier --- */
    printf("\nSaisie complete :\n");
    {
        FXCalc c; fx_calc_reset(&c);
        const char *seq[] = { "1", "2", "+", "3", "=" };
        tape(&c, seq, 5);
        chk(!strcmp(c.saisie, "15"), "12 + 3 = 15");
        chk(c.nhisto == 1 && !strcmp(c.histo[0], "12 + 3 = 15"), "l'historique enregistre le calcul");
    }

    /* --- les operateurs en chaine --- */
    printf("\nOperateurs en chaine :\n");
    {
        FXCalc c; fx_calc_reset(&c);
        const char *seq[] = { "2", "+", "3", "x", "4", "=" };
        tape(&c, seq, 6);
        chk(!strcmp(c.saisie, "20"), "2 + 3 x 4 = 20 (calcul de gauche a droite)");
    }

    /* --- le signe, le point et le pourcent --- */
    printf("\nFonctions annexes :\n");
    {
        FXCalc c; fx_calc_reset(&c);
        const char *seq[] = { "8", "+/-" };
        tape(&c, seq, 2);
        chk(!strcmp(c.saisie, "-8"), "la touche +/- rend le nombre negatif");
        fx_calc_touche(&c, "+/-");
        chk(!strcmp(c.saisie, "8"), "un second +/- redevient positif");

        fx_calc_reset(&c);
        const char *seq2[] = { "3", ".", "5", "x", "2", "=" };
        tape(&c, seq2, 6);
        chk(!strcmp(c.saisie, "7"), "3.5 x 2 = 7");

        fx_calc_reset(&c);
        const char *seq3[] = { "5", "0", "%" };
        tape(&c, seq3, 3);
        chk(!strcmp(c.saisie, "0.5"), "50 % = 0.5");
    }

    /* --- le retour arriere --- */
    printf("\nRetour arriere :\n");
    {
        FXCalc c; fx_calc_reset(&c);
        const char *seq[] = { "1", "2", "3" };
        tape(&c, seq, 3);
        fx_calc_retour(&c);
        chk(!strcmp(c.saisie, "12"), "la touche retour efface un chiffre");
        fx_calc_retour(&c); fx_calc_retour(&c);
        chk(!strcmp(c.saisie, "0"), "tout efface revient a 0");
    }

    /* --- l'historique garde les 3 derniers calculs --- */
    printf("\nHistorique :\n");
    {
        FXCalc c; fx_calc_reset(&c);
        const char *a[] = { "1", "+", "1", "=" }; tape(&c, a, 4);
        const char *b[] = { "2", "+", "2", "=" }; tape(&c, b, 4);
        const char *d[] = { "3", "+", "3", "=" }; tape(&c, d, 4);
        const char *e[] = { "4", "+", "4", "=" }; tape(&c, e, 4);
        chk(c.nhisto == 3, "on ne garde que 3 calculs");
        chk(!strcmp(c.histo[0], "4 + 4 = 8"), "le plus recent est en tete");
        chk(!strcmp(c.histo[2], "2 + 2 = 4"), "le plus ancien est bien le 3e");
    }

    /* --- le dessin ne doit pas planter dans n'importe quelle taille --- */
    printf("\nDessin et souris :\n");
    {
        FXScreen *s = fx_screen_new(800, 600);
        if (s) {
            fx_calc_init(800, 600, NULL);
            fx_calc_demo();
            fx_calc_draw(s);
            chk(1, "le dessin ne plante pas en 800x600");

            /* la fenetre est centree : 420 de large, 520 de haut en 800x600 */
            int wx = (800 - 420) / 2, wy = (600 - 520) / 2;
            int bx, by, bsz, bgap;
            fx_window_buttons_rect(wx, wy, 420, &bx, &by, &bsz, &bgap);
            int cx = bx + 2 * (bsz + bgap) + bsz / 2;   /* la croix de fermeture */
            int cy = by + bsz / 2;
            chk(fx_calc_click(cx, cy) == 1, "un clic sur la croix demande la fermeture");

            /* un clic sur une touche chiffre ne ferme pas et alimente la saisie */
            chk(fx_calc_click(wx + 300, wy + 500) == 0, "un clic sur une touche ne ferme pas");
            fx_screen_free(s);
        }
    }

    printf("\n%d reussis, %d rates\n", reussis, rates);
    return rates ? 1 : 0;
}
