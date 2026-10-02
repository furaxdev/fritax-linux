/* Fritax Terminal - framebuffer logiciel (independant de X11 et de DRM) */
#ifndef FRITAX_SCREEN_H
#define FRITAX_SCREEN_H

#include <stdint.h>
#include "vt.h"
#include "font8x14.h"

/* dimensions d'une cellule de texte */
#define FX_CELL_W FONT_W
#define FX_CELL_H FONT_H

typedef struct {
    int w, h;
    uint32_t *px;          /* 0x00RRGGBB */
} FXScreen;

FXScreen *fx_screen_new(int w, int h);
void      fx_screen_free(FXScreen *s);
void      fx_screen_resize(FXScreen *s, int w, int h);
void      fx_clear(FXScreen *s, uint32_t color);
void      fx_fill_rect(FXScreen *s, int x, int y, int w, int h, uint32_t color);
void      fx_draw_glyph(FXScreen *s, int x, int y, uint32_t ch, uint32_t color);
void      fx_draw_cursor_block(FXScreen *s, int x, int y, uint32_t color);
void      fx_draw_cursor_bar(FXScreen *s, int x, int y, uint32_t color);
void      fx_blend_px(FXScreen *s, int x, int y, uint32_t color, int alpha);
void      fx_fill_round_rect(FXScreen *s, int x, int y, int w, int h, int r, uint32_t color);
void      fx_fill_circle(FXScreen *s, int cx, int cy, int r, uint32_t color);
void      fx_blend_round_rect(FXScreen *s, int x, int y, int w, int h, int r, uint32_t color, int alpha);
void      fx_gradient_v(FXScreen *s, int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void      fx_draw_text(FXScreen *s, int x, int y, const char *txt, uint32_t color);
void      fx_draw_text_scale(FXScreen *s, int x, int y, const char *txt, uint32_t color, int scale);
int       fx_load_raw_rgb(FXScreen *s, const char *path);

/* dessine les lignes [first..last] du VT dans le framebuffer (avec le curseur) */
void        fx_render_vt(VT *vt, FXScreen *s, int first, int last, int pad,
                         uint32_t cursor_color, uint32_t default_bg);

/* theme Fritax : palette 16 couleurs + couleurs par defaut */
void        fx_default_palette(uint8_t pal[16][3]);
uint32_t    fx_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t    fx_vt_color(const VT *vt, int idx, int is_bg, uint8_t def_fg, uint8_t def_bg);

#endif
