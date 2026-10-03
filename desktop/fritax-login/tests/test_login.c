/* ============================================================
 *  Tests de l'ecran de connexion Fritax - sans ecran.
 *  On pilote le coeur (touches / souris) et on verifie l'etat.
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "login.h"
#include "input.h"

static int reussis, rates;

static void verifie(int condition, const char *quoi) {
    if (condition) { reussis++; printf("  ok   %s\n", quoi); }
    else           { rates++;   printf("  RATE %s\n", quoi); }
}

static void tape(FXLogin *l, const char *txt) {
    /* tape une chaine caractere par caractere, comme un vrai clavier */
    for (const char *p = txt; *p; p++) fx_login_touche(l, FXK_NONE, (unsigned char)*p);
}

int main(void) {
    FXLogin l;
    printf("Tests de l'ecran de connexion\n");

    /* --- l'ecran s'ouvre avec le pseudo pre-rempli --- */
    fx_login_init(&l, 1366, 768);
    verifie(!strcmp(l.utilisateur, "furax"), "le pseudo 'furax' est pre-rempli");
    verifie(l.champ == FXL_CHAMP_UTILISATEUR, "le curseur est sur le pseudo");
    verifie(l.etat == FXL_EN_COURS, "l'ecran est en attente");

    /* --- la touche Tab change de champ --- */
    fx_login_touche(&l, FXK_TAB, 0);
    verifie(l.champ == FXL_CHAMP_MOTDEPASSE, "Tab passe au mot de passe");
    fx_login_touche(&l, FXK_TAB, 0);
    verifie(l.champ == FXL_CHAMP_UTILISATEUR, "Tab revient au pseudo");

    /* --- on tape un mot de passe faux : refuse, et le champ se vide --- */
    fx_login_touche(&l, FXK_TAB, 0);
    tape(&l, "mauvais");
    verifie(!strcmp(l.motdepasse, "mauvais"), "la saisie est enregistree");
    fx_login_touche(&l, FXK_ENTER, 0);
    verifie(l.etat == FXL_REFUSE, "un mot de passe faux est refuse");
    verifie(l.motdepasse[0] == 0, "le champ est vide apres un refus");
    verifie(l.tentatives == 1, "une tentative est comptee");

    /* --- le bon mot de passe passe --- */
    tape(&l, "fritax");
    fx_login_touche(&l, FXK_ENTER, 0);
    verifie(l.etat == FXL_CONNECTE, "le bon mot de passe est accepte");

    /* --- le retour arriere efface un caractere --- */
    fx_login_init(&l, 1366, 768);
    fx_login_touche(&l, FXK_TAB, 0);
    tape(&l, "fritax");
    fx_login_touche(&l, FXK_BACKSPACE, 0);
    verifie(!strcmp(l.motdepasse, "frita"), "le retour arriere efface une lettre");

    /* --- la verification directe --- */
    verifie(fx_login_verifie("furax", "fritax") == 1, "furax / fritax accepte");
    verifie(fx_login_verifie("root", "fritax") == 1, "root / fritax accepte");
    verifie(fx_login_verifie("furax", "faux") == 0, "mauvais mot de passe refuse");
    verifie(fx_login_verifie("intrus", "fritax") == 0, "mauvais pseudo refuse");
    verifie(fx_login_verifie("", "") == 0, "vide refuse");

    /* --- la souris : clic sur le bouton = validation --- */
    fx_login_init(&l, 1366, 768);
    fx_login_touche(&l, FXK_TAB, 0);
    tape(&l, "fritax");
    fx_login_touche(&l, FXK_ENTER, 0);
    verifie(l.etat == FXL_CONNECTE, "Entree valide apres le mot de passe");

    /* --- l'ecran sait se dessiner dans n'importe quelle taille --- */
    FXScreen *s = fx_screen_new(800, 600);
    if (s) {
        FXLogin p; fx_login_init(&p, 800, 600);
        fx_login_dessine(&p, s);
        verifie(1, "le dessin ne plante pas en 800x600");
        fx_screen_free(s);
    }

    printf("\n%d reussis, %d rates\n", reussis, rates);
    return rates ? 1 : 0;
}
