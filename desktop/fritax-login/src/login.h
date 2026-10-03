/* ============================================================
 *  FRITAX LOGIN - ecran de connexion de Fritax Linux
 *  Coeur independant de l'affichage (testable sans ecran).
 * ============================================================ */
#ifndef FRITAX_LOGIN_H
#define FRITAX_LOGIN_H

#include "screen.h"

/* champ actif */
enum { FXL_CHAMP_UTILISATEUR = 0, FXL_CHAMP_MOTDEPASSE = 1, FXL_CHAMP_BOUTON = 2 };

/* etat de la connexion */
enum { FXL_EN_COURS = 0, FXL_CONNECTE = 1, FXL_REFUSE = 2 };

typedef struct {
    char utilisateur[64];
    char motdepasse[64];
    int  champ;                 /* champ actif (FXL_CHAMP_*) */
    int  etat;                  /* FXL_EN_COURS / CONNECTE / REFUSE */
    int  tentatives;
    char message[128];          /* message affiche sous les champs */
    int  curseur;               /* clignotement : 0/1, pilote par l'appelant */
    int  survol;                /* souris au-dessus du bouton */
    int  w, h;
} FXLogin;

void fx_login_init(FXLogin *l, int w, int h);

/* touche : une des FXK_* (voir input.h) ; ascii : le caractere si touche == FXK_NONE */
void fx_login_touche(FXLogin *l, int touche, int ascii);

/* souris : position, bouton enfonce (1) ou relache (0) */
void fx_login_souris(FXLogin *l, int x, int y, int enfonce);

void fx_login_dessine(FXLogin *l, FXScreen *s);

/* verification du couple utilisateur / mot de passe (1 = accepte) */
int fx_login_verifie(const char *utilisateur, const char *motdepasse);

/* nom de la session lancee apres connexion reussie */
const char *fx_login_session(void);

#endif
