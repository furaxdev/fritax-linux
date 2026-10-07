/* Fritax Moniteur - la fenetre et l'affichage du moniteur systeme */
#ifndef FRITAX_MON_APP_H
#define FRITAX_MON_APP_H

#include "screen.h"
#include "window.h"
#include "proc.h"

/* touches reconnues par l'application */
enum { FXM_NONE = 0, FXM_RAFRAICHIR = 1, FXM_QUITTER = 2 };

void fx_mon_init(int screen_w, int screen_h, const char *log_path);
void fx_mon_rafraichir(void);   /* relit /proc (au plus une fois par seconde) */
void fx_mon_draw(FXScreen *s);
int  fx_mon_click(int x, int y);   /* 1 = demande de fermeture */
int  fx_mon_move(int x, int y);
int  fx_mon_key(int key);
void fx_mon_demo(void);            /* valeurs factices pour les apercus sans /proc */

#endif
