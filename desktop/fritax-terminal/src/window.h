/* Fritax - chrome de fenetre (barre de titre + nos boutons) */
#ifndef FRITAX_WINDOW_H
#define FRITAX_WINDOW_H

#include "screen.h"

#define FX_TITLEBAR_H 30
#define FX_WIN_RADIUS 12

/* Dessine une fenetre complete : ombre, fond, barre de titre, marque,
   titre et les trois boutons Fritax (reduire / agrandir / fermer).
   Renvoie la hauteur restante pour le contenu (hauteur - barre de titre). */
int fx_window(FXScreen *s, int x, int y, int w, int h, const char *title, uint32_t bg);

/* Juste la barre de titre (si le contenu est dessine entre-temps). */
void fx_titlebar(FXScreen *s, int x, int y, int w, const char *title);
/* variante avec une vraie icone d'application (kind de appicon.h) */
void fx_titlebar_k(FXScreen *s, int x, int y, int w, const char *title, int kind);

/* Bouton isole (kind : 0 reduire, 1 agrandir, 2 fermer). */
void fx_window_button(FXScreen *s, int x, int y, int size, uint32_t accent, int kind);

/* Zone des trois boutons, pour le test de clic. */
void fx_window_buttons_rect(int x, int y, int w, int *bx, int *by, int *bsz, int *gap);

#endif
