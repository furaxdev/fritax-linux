/* ============================================================
 *  FRITAX WM - gestionnaire de fenetres du bureau
 *  Fenetres deplacables, redimensionnables, empilables.
 *  Chaque fenetre heberge une "application" interne :
 *    TERMINAL  -> vrai shell (PTY) rendu par notre moteur VT
 *    FICHIERS  -> liste du dossier courant
 *    REGLAGES  -> theme, accent, infos
 *    EDITEUR   -> bloc-notes
 *  Aucun serveur graphique, aucune bibliotheque : que du C.
 * ============================================================ */
#ifndef FRITAX_WM_H
#define FRITAX_WM_H

#include "screen.h"
#include "vt.h"

#define FX_WM_MAX 6

enum { FX_APP_TERMINAL = 0, FX_APP_FILES = 1, FX_APP_SETTINGS = 2, FX_APP_EDITOR = 3 };

void fx_wm_init(int screen_w, int screen_h, const char *log_path);
int  fx_wm_open(int kind, const char *title);          /* ouvre une fenetre, -1 si echec */
void fx_wm_close_all(void);
void fx_wm_add_demo(void);                             /* contenu factice pour les apercus */

void fx_wm_render(FXScreen *s);
void fx_wm_tick(void);                                 /* lit les PTY (non bloquant) */
int  fx_wm_click(int x, int y);                        /* 1 = clic consomme */
void fx_wm_drag(int x, int y);
void fx_wm_release(void);
int  fx_wm_hover(int x, int y);
int  fx_wm_wheel(int x, int y, int up);   /* molette : defile le terminal survole */
/* geometrie d'une fenetre (tests et reglages) */
int  fx_wm_size(int i, int *x, int *y, int *w, int *h);
int  fx_wm_raise(int i);       /* mettre une fenetre devant, sans clic */
int  fx_wm_minimize(int i);    /* reduire une fenetre */
void fx_wm_key(int key, int ascii);
int  fx_wm_dragging(void);
int  fx_wm_count(void);
/* pour les tests : copie le texte visible d'une fenetre (terminal) */
int  fx_wm_snapshot(int i, char *out, int max);
/* charge un fichier dans une fenetre Bloc-notes */
int  fx_wm_open_file(int i, const char *path);
const char *fx_wm_title(int i);
int  fx_wm_kind(int i);        /* type d'application d'une fenetre */
int  fx_wm_minimized(int i);
int  fx_wm_focus(void);        /* fenetre active (-1 si aucune) */
int  fx_wm_zrank(int i);       /* rang d'empilement (pour les tests) */
/* le moteur d'affichage demande : le fond d'ecran a-t-il change ? (remise a zero) */
int         fx_wm_take_wallpaper_change(void);
const char *fx_wm_wallpaper_path(void);
void        fx_wm_set_wallpaper_index(int i);

#endif
