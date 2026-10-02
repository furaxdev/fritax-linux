/* Fritax - chrome de fenetre : notre style, rien de macOS */
#include "window.h"
#include "appicon.h"

#define C_TITLE 0x1A2438
#define C_DARK  0x0F1626
#define C_TEXT  0xEAF2FF
#define C_DIM   0x5C6E8C
#define C_VIOLET 0xD633FF
#define C_CYAN   0x5BC8FF
#define C_ROSE   0xFF5C7A

void fx_window_button(FXScreen *s, int x, int y, int sz, uint32_t accent, int kind) {
    fx_fill_round_rect(s, x, y, sz, sz, 7, 0x1C2740);
    /* contour 2 px dans la couleur d'accent */
    fx_fill_rect(s, x + 3, y - 1, sz - 6, 2, accent);
    fx_fill_rect(s, x + 3, y + sz - 1, sz - 6, 2, accent);
    fx_fill_rect(s, x - 1, y + 3, 2, sz - 6, accent);
    fx_fill_rect(s, x + sz - 1, y + 3, 2, sz - 6, accent);
    fx_fill_rect(s, x - 1, y + 3, 2, sz - 6, accent);
    int m = 5;
    if (kind == 0) {                                  /* reduire : un trait */
        fx_fill_rect(s, x + m, y + sz / 2 - 1, sz - 2 * m, 3, accent);
    } else if (kind == 1) {                           /* agrandir : carre vide */
        int t = 2;
        fx_fill_rect(s, x + m, y + m, sz - 2 * m, t, accent);
        fx_fill_rect(s, x + m, y + sz - m - t, sz - 2 * m, t, accent);
        fx_fill_rect(s, x + m, y + m, t, sz - 2 * m, accent);
        fx_fill_rect(s, x + sz - m - t, y + m, t, sz - 2 * m, accent);
    } else {                                          /* fermer : une croix */
        for (int i = 0; i < sz - 2 * m; i++) {
            fx_fill_rect(s, x + m + i, y + m + i, 2, 2, accent);
            fx_fill_rect(s, x + sz - m - 1 - i, y + m + i, 2, 2, accent);
        }
    }
}

void fx_window_buttons_rect(int x, int y, int w, int *bx, int *by, int *bsz, int *gap) {
    int sz = 18, g = 10;
    *bsz = sz; *gap = g;
    *bx = x + w - (3 * sz + 2 * g) - 14;
    *by = y + (FX_TITLEBAR_H - sz) / 2;
}

void fx_titlebar_k(FXScreen *s, int x, int y, int w, const char *title, int kind) {
    fx_fill_rect(s, x, y, w, FX_TITLEBAR_H, C_TITLE);
    fx_fill_rect(s, x, y + FX_TITLEBAR_H - 1, w, 1, 0x27354E);
    if (kind > 0) fx_app_icon(s, x + 8, y + (FX_TITLEBAR_H - 16) / 2, 16, kind);
    else fx_fill_round_rect(s, x + 12, y + FX_TITLEBAR_H / 2 - 4, 9, 9, 2, C_VIOLET);   /* marque */
    if (title) fx_draw_text(s, x + (kind > 0 ? 30 : 30), y + (FX_TITLEBAR_H - FONT_H) / 2, title, C_TEXT);
    int bx, by, bsz, gap;
    fx_window_buttons_rect(x, y, w, &bx, &by, &bsz, &gap);
    fx_window_button(s, bx, by, bsz, C_CYAN, 0);
    fx_window_button(s, bx + bsz + gap, by, bsz, C_VIOLET, 1);
    fx_window_button(s, bx + 2 * (bsz + gap), by, bsz, C_ROSE, 2);
}

void fx_titlebar(FXScreen *s, int x, int y, int w, const char *title) {
    fx_titlebar_k(s, x, y, w, title, 0);
}

int fx_window(FXScreen *s, int x, int y, int w, int h, const char *title, uint32_t bg) {
    /* ombre portee + corps arrondi */
    fx_blend_round_rect(s, x + 4, y + 8, w, h, FX_WIN_RADIUS, 0x000000, 130);
    fx_fill_round_rect(s, x, y, w, h, FX_WIN_RADIUS, bg ? bg : C_DARK);
    fx_titlebar(s, x, y, w, title);
    return h - FX_TITLEBAR_H;
}
