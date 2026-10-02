/* Fritax Terminal - moteur d'emulation VT100/xterm (aucune dependance) */
#ifndef FRITAX_VT_H
#define FRITAX_VT_H

#include <stdint.h>
#include <stddef.h>

#define VT_SCROLLBACK 2000

/* attributs */
#define VT_BOLD      1
#define VT_DIM       2
#define VT_ITALIC    4
#define VT_UNDERLINE 8
#define VT_REVERSE   16
#define VT_BLINK     32

typedef struct {
    uint32_t ch;
    uint8_t  fg, bg;
    uint16_t attr;
} VTCell;

typedef struct VT VT;

/* appeles par le moteur quand il faut parler au monde exterieur */
typedef struct {
    void (*on_dirty)(void *user, int row);          /* une ligne a redessiner (-1 = tout) */
    void (*on_reply)(void *user, const char *s, size_t n); /* reponse a envoyer au pty */
    void (*on_bell)(void *user);
    void (*on_title)(void *user, const char *s, size_t n);
} VTHooks;

VT  *vt_new(int cols, int rows, int sb_lines, VTHooks hooks, void *user);
void vt_free(VT *vt);
void vt_resize(VT *vt, int cols, int rows);
void vt_feed(VT *vt, const char *buf, size_t len);

/* acces a l'ecran pour le rendu */
int      vt_cols(const VT *vt);
int      vt_rows(const VT *vt);
VTCell   vt_cell(const VT *vt, int row, int col);      /* row 0 = haut de l'ecran */
VTCell   vt_sb_cell(const VT *vt, int row, int col);   /* lignes de scrollback (0 = la plus recente) */
int      vt_sb_len(const VT *vt);
int      vt_cursor_x(const VT *vt);
int      vt_cursor_y(const VT *vt);
int      vt_cursor_visible(const VT *vt);
int      vt_cursor_blink_phase(const VT *vt);
void     vt_tick(VT *vt);                              /* pour le clignotement */
void     vt_scroll_view(VT *vt, int lines);            /* molette : 0 = bas */
int      vt_view_offset(const VT *vt);

/* palette : les 16 premieres couleurs sont personnalisables (theme Fritax) */
void vt_set_palette(VT *vt, const uint8_t rgb[16][3]);
void vt_palette(const VT *vt, int idx, uint8_t *r, uint8_t *g, uint8_t *b);
int  vt_default_named(const VT *vt, int idx);          /* 1 si la couleur n'a pas ete personnalisee */

#endif
