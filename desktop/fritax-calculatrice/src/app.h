/* Fritax Calculatrice - la calculatrice de la distribution */
#ifndef FRITAX_CALC_APP_H
#define FRITAX_CALC_APP_H

#include <stddef.h>
#include "screen.h"
#include "window.h"

/* codes de touches normalisees (en plus des codes ASCII '0'..'9','+',...) */
enum { FXC_NONE = 0, FXC_CLEAR = 1, FXC_ENTER = 2, FXC_BACK = 3, FXC_NEG = 4 };

/* --- etat du moteur de calcul (testable sans aucun ecran) --- */
typedef struct {
    char   saisie[64];   /* l'operande en cours de frappe, ou le resultat */
    double acc;          /* l'accumulateur (operande de gauche) */
    char   op;           /* '+' '-' 'x' '/' ou 0 si aucun */
    int    neuf;         /* 1 : le prochain chiffre remplace la saisie */
    int    erreur;       /* 1 : on est en etat d'erreur (division par zero...) */
    char   histo[3][96]; /* les 3 derniers calculs, le plus recent en [0] */
    int    nhisto;       /* combien d'entrees dans l'historique (0..3) */
} FXCalc;

/* remet le moteur a zero */
void fx_calc_reset(FXCalc *c);
/* le moteur pur : a (op) b -> *out. 0 = ok, -1 = erreur (division par zero) */
int  fx_calc_opere(double a, char op, double b, double *out);
/* formate un nombre en texte lisible */
void fx_calc_formate(double v, char *out, size_t n);
/* appuie sur une touche logique : "0".."9","+","-","x","/","=","C","+/-","%","." */
void fx_calc_touche(FXCalc *c, const char *touche);
/* efface le dernier caractere saisi */
void fx_calc_retour(FXCalc *c);

/* --- interface de l'appli (le motif commun) --- */
void fx_calc_init(int screen_w, int screen_h, const char *log_path);
void fx_calc_draw(FXScreen *s);
int  fx_calc_click(int x, int y);   /* 1 = demande de fermeture */
int  fx_calc_move(int x, int y);
int  fx_calc_key(int key);          /* ASCII '0'..'9','+','-','x','/','=', ou FXC_* */
int  fx_calc_scroll(int up);
void fx_calc_demo(void);            /* contenu factice pour les apercus */

#endif
