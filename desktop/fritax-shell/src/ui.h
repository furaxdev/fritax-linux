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
/* Palette Fluent / Windows 11 (1.2). On quitte le bleu nuit, le violet et
   le cyan neon : surfaces claires, texte presque noir, un seul accent bleu.
   Valeurs reprises de Windows 11 : bureau #F3F3F3, surface blanche, texte
   #1A1A1A, accent #0067C0, bordures #E5E5E5. */
#define C_BG      0xF3F3F3
#define C_BG2     0xE9E9E9
#define C_FG      0x1A1A1A
#define C_DIM     0x5E5E5E
#define C_VIOLET  0x0067C0
#define C_CYAN    0x0078D4
#define C_GREEN   0x0F7B0F
#define C_ORANGE  0x9D5D00
#define C_PANEL   0xFFFFFF
#define C_TILE    0xF7F7F7

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
