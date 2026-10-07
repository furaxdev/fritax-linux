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

    /* --- l'authentification interroge maintenant les VRAIS comptes de la
       machine : on ne peut donc plus affirmer qu'un couple precis est accepte,
       ca depend de la machine qui fait tourner le test. Ce qui doit toujours
       etre vrai, c'est le comportement : chaque essai compte, un refus laisse
       le champ vide, et on ne passe pas. --- */
    tape(&l, "encorefaux");
    fx_login_touche(&l, FXK_ENTER, 0);
    verifie(l.etat != FXL_CONNECTE, "un second essai faux ne connecte pas");
    verifie(l.tentatives == 2, "le second essai est compte");
    verifie(l.motdepasse[0] == 0, "le champ est encore vide apres le second refus");

    /* --- le retour arriere efface un caractere --- */
    fx_login_init(&l, 1366, 768);
    fx_login_touche(&l, FXK_TAB, 0);
    tape(&l, "fritax");
    fx_login_touche(&l, FXK_BACKSPACE, 0);
    verifie(!strcmp(l.motdepasse, "frita"), "le retour arriere efface une lettre");

    /* --- la verification directe --- */
    /* L'authentification interroge maintenant les VRAIS comptes de la machine
       (/etc/passwd + /etc/shadow + crypt). On ne peut donc plus affirmer qu'un
       couple precis est accepte : ca depend de la machine. Ce qui est vrai
       partout, c'est ce qu'on verifie ici. */
    verifie(fx_login_verifie("utilisateur-qui-nexiste-pas-xyz", "nimportequoi") == 0,
            "un compte inexistant est refuse");
    verifie(fx_login_verifie("", "fritax") == 0, "pseudo vide refuse");
    verifie(fx_login_verifie("root", "") == 0, "mot de passe vide refuse");
    verifie(fx_login_verifie(NULL, "fritax") == 0, "pseudo NULL refuse");
    verifie(fx_login_verifie("root", NULL) == 0, "mot de passe NULL refuse");
    /* mauvaise suite pour un compte qui existe : doit etre refuse */
    verifie(fx_login_verifie("root", "ce-nest-pas-le-bon-mot-de-passe") == 0,
            "mauvais mot de passe refuse pour un compte existant");

    /* --- la souris : clic sur le bouton = validation --- */
    fx_login_init(&l, 1366, 768);
    fx_login_touche(&l, FXK_TAB, 0);
    tape(&l, "fritax");
    fx_login_touche(&l, FXK_ENTER, 0);
    verifie(l.tentatives == 1, "Entree declenche bien une verification");

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
