/* ============================================================
 *  Fritax UI - le bureau (barre flottante + lanceur)
 *  Code partage entre les deux moteurs :
 *    - main_x11.c    : fenetre X11 (test sur un bureau existant)
 *    - main_native.c : DRM/KMS + evdev (le bureau de Fritax Linux)
 * ============================================================ */
#ifndef FRITAX_UI_H
#define FRITAX_UI_H

#include "screen.h"
#include "wm.h"

/* theme */
#define C_BG      0x0D1422
#define C_BG2     0x10233F
#define C_FG      0xEAF2FF
#define C_DIM     0x7C93B5
#define C_VIOLET  0xD633FF
#define C_CYAN    0x5BC8FF
#define C_GREEN   0x39DE8A
#define C_ORANGE  0xF6AD55
#define C_PANEL   0x111A2C
#define C_TILE    0x1A263C

/* touches normalisees (independantes du backend) */
enum { UI_KEY_NONE = 0, UI_KEY_ESC = 1, UI_KEY_SUPER = 2, UI_KEY_BACKSPACE = 3, UI_KEY_ENTER = 4, UI_KEY_LEFT = 5, UI_KEY_RIGHT = 6, UI_KEY_UP = 7, UI_KEY_DOWN = 8, UI_KEY_TAB = 9, UI_KEY_CLOSE = 10, UI_KEY_SAVE = 11 };

void fx_ui_init(int screen_w, int screen_h, const char *log_path, int scan_applications);
int  fx_ui_release(void);                       /* relachement du bouton de souris */
void fx_ui_tick(void);                          /* rafraichit les fenetres (PTY) */
void fx_ui_render(FXScreen *s);                 /* dessine le bureau (barre + lanceur) */
int  fx_ui_click(int x, int y);
int  fx_ui_move(int x, int y);
int  fx_ui_wheel(int x, int y, int up);
/* le fond d'ecran a-t-il ete change dans Reglages ? */
int  fx_ui_take_wallpaper_change(void);
const char *fx_ui_wallpaper_path(void);
int  fx_ui_key(int key, int ascii);
void fx_ui_toggle_launcher(void);
void fx_ui_sync_children(void);                 /* retire les applis fermees */
int  fx_ui_wants_close(void);
void fx_ui_set_launcher(int open);
void fx_ui_apercu_appli(const char *name, uint32_t color);   /* ajoute une appli "ouverte" (apercus) */

/* petit curseur de souris (flèche) dessine par nous */
void fx_ui_draw_cursor(FXScreen *s, int x, int y);

#endif
