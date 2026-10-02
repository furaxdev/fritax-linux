/* Fritax Terminal - rendu dans un framebuffer logiciel */
#include "screen.h"
#include "font8x14.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern uint32_t vt_palette_rgb(const VT *vt, int idx);

FXScreen *fx_screen_new(int w, int h) {
    FXScreen *s = calloc(1, sizeof(FXScreen));
    if (!s) return NULL;
    s->w = w; s->h = h;
    s->px = malloc((size_t)w * h * sizeof(uint32_t));
    if (!s->px) { free(s); return NULL; }
    return s;
}

void fx_screen_free(FXScreen *s) { if (s) { free(s->px); free(s); } }

void fx_screen_resize(FXScreen *s, int w, int h) {
    if (!s || (w == s->w && h == s->h)) return;
    uint32_t *p = realloc(s->px, (size_t)w * h * sizeof(uint32_t));
    if (!p) return;
    s->px = p; s->w = w; s->h = h;
}

static inline void put(FXScreen *s, int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    s->px[(size_t)y * s->w + x] = c;
}

void fx_clear(FXScreen *s, uint32_t color) {
    for (size_t i = 0, n = (size_t)s->w * s->h; i < n; i++) s->px[i] = color;
}

void fx_fill_rect(FXScreen *s, int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) put(s, x + i, y + j, color);
}

void fx_draw_glyph(FXScreen *s, int x, int y, uint32_t ch, uint32_t color) {
    int idx = (int)ch;
    if (idx < FONT_FIRST || idx > FONT_LAST) {
        /* hors ASCII : on dessine un petit carre plein (glyphe de remplacement) */
        fx_fill_rect(s, x + 2, y + 5, FONT_W - 4, FONT_H - 8, color);
        return;
    }
    const unsigned char *g = FONT8X14[idx - FONT_FIRST];
    for (int r = 0; r < FONT_H; r++) {
        unsigned char bits = g[r];
        if (!bits) continue;
        for (int c = 0; c < FONT_W; c++)
            if (bits & (1 << (7 - c))) put(s, x + c, y + r, color);
    }
}

void fx_draw_cursor_block(FXScreen *s, int x, int y, uint32_t color) {
    for (int j = 2; j < FONT_H - 1; j++)
        for (int i = 1; i < FONT_W - 1; i++) put(s, x + i, y + j, color);
}

void fx_draw_cursor_bar(FXScreen *s, int x, int y, uint32_t color) {
    for (int j = 2; j < FONT_H - 1; j++) { put(s, x + 1, y + j, color); put(s, x + 2, y + j, color); }
}

void fx_render_vt(VT *vt, FXScreen *s, int first, int last, int pad,
                  uint32_t cursor_color, uint32_t default_bg) {
    int rows = vt_rows(vt), cols = vt_cols(vt);
    if (first < 0) first = 0;
    if (last >= rows) last = rows - 1;
    for (int r = first; r <= last; r++) {
        int y = pad + r * FONT_H;
        for (int c = 0; c < cols; c++) {
            VTCell cell = vt_cell(vt, r, c);
            uint32_t bg = fx_vt_color(vt, cell.bg, 1, 255, 255);
            uint32_t fg = fx_vt_color(vt, cell.fg, 0, 255, 255);
            if (cell.attr & VT_REVERSE) { uint32_t t = bg; bg = fg; fg = t; }
            fx_fill_rect(s, pad + c * FONT_W, y, FONT_W, FONT_H, bg);
            if (cell.ch > 32) fx_draw_glyph(s, pad + c * FONT_W, y, cell.ch, fg);
            if (cell.attr & VT_UNDERLINE)
                fx_fill_rect(s, pad + c * FONT_W, y + FONT_H - 3, FONT_W, 1, fg);
        }
    }
    if (vt_cursor_visible(vt) && vt_cursor_y(vt) >= first && vt_cursor_y(vt) <= last
        && vt_cursor_blink_phase(vt)) {
        int cx = pad + vt_cursor_x(vt) * FONT_W, cy = pad + vt_cursor_y(vt) * FONT_H;
        fx_draw_cursor_block(s, cx, cy, cursor_color);
    }
    (void)default_bg;
}

/* --- rectangles arrondis, cercles, degrade, melange alpha --- */
static inline uint32_t blend_px(uint32_t dst, uint32_t src, int a) {
    int dr = (int)((dst >> 16) & 255), dg = (int)((dst >> 8) & 255), db = (int)(dst & 255);
    int sr = (int)((src >> 16) & 255), sg = (int)((src >> 8) & 255), sb = (int)(src & 255);
    int r = (sr * a + dr * (255 - a)) / 255, g = (sg * a + dg * (255 - a)) / 255, b = (sb * a + db * (255 - a)) / 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void fx_blend_px(FXScreen *s, int x, int y, uint32_t color, int alpha) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h || alpha <= 0) return;
    s->px[(size_t)y * s->w + x] = blend_px(s->px[(size_t)y * s->w + x], color, alpha);
}

void fx_blend_round_rect(FXScreen *s, int x, int y, int w, int h, int r, uint32_t color, int alpha) {
    if (alpha >= 255) { fx_fill_round_rect(s, x, y, w, h, r, color); return; }
    if (alpha <= 0) return;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int dx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
            int dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
            int mx = i < r ? (r - i) : (i >= w - r ? i - (w - r - 1) : 0);
            int my = j < r ? (r - j) : (j >= h - r ? j - (h - r - 1) : 0);
            (void)dx; (void)dy;
            if (mx && my) {
                int d2 = mx * mx + my * my;
                if (d2 > r * r) continue;
                if (d2 > (r - 1) * (r - 1)) {
                    int px = x + i, py = y + j;
                    if (px < 0 || py < 0 || px >= s->w || py >= s->h) continue;
                    s->px[(size_t)py * s->w + px] = blend_px(s->px[(size_t)py * s->w + px], color, alpha / 2);
                    continue;
                }
            }
            int px = x + i, py = y + j;
            if (px < 0 || py < 0 || px >= s->w || py >= s->h) continue;
            s->px[(size_t)py * s->w + px] = blend_px(s->px[(size_t)py * s->w + px], color, alpha);
        }
    }
}

void fx_fill_round_rect(FXScreen *s, int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) { fx_fill_rect(s, x, y, w, h, color); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    fx_fill_rect(s, x + r, y, w - 2 * r, h, color);
    fx_fill_rect(s, x, y + r, r, h - 2 * r, color);
    fx_fill_rect(s, x + w - r, y + r, r, h - 2 * r, color);
    for (int j = 0; j < r; j++) {
        for (int i = 0; i < r; i++) {
            int d2 = (r - 1 - i) * (r - 1 - i) + (r - 1 - j) * (r - 1 - j);
            if (d2 <= (r - 1) * (r - 1)) {
                put(s, x + i, y + j, color);
                put(s, x + w - 1 - i, y + j, color);
                put(s, x + i, y + h - 1 - j, color);
                put(s, x + w - 1 - i, y + h - 1 - j, color);
            }
        }
    }
}

void fx_fill_circle(FXScreen *s, int cx, int cy, int r, uint32_t color) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r) put(s, cx + i, cy + j, color);
}

void fx_gradient_v(FXScreen *s, int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; j++) {
        int t = h > 1 ? j * 255 / (h - 1) : 0;
        int tr = (int)((top >> 16) & 255), tg = (int)((top >> 8) & 255), tb = (int)(top & 255);
        int br = (int)((bottom >> 16) & 255), bg = (int)((bottom >> 8) & 255), bb = (int)(bottom & 255);
        uint32_t c = (uint32_t)((tr + (br - tr) * t / 255) << 16) |
                     (uint32_t)((tg + (bg - tg) * t / 255) << 8) |
                     (uint32_t)(tb + (bb - tb) * t / 255);
        fx_fill_rect(s, x, y + j, w, 1, c);
    }
}

void fx_draw_text(FXScreen *s, int x, int y, const char *txt, uint32_t color) {
    for (int i = 0; txt[i]; i++) fx_draw_glyph(s, x + i * FONT_W, y, (unsigned char)txt[i], color);
}

void fx_draw_text_scale(FXScreen *s, int x, int y, const char *txt, uint32_t color, int scale) {
    if (scale < 1) scale = 1;
    int cx = x;
    for (int i = 0; txt[i]; i++) {
        int idx = (unsigned char)txt[i];
        if (idx >= FONT_FIRST && idx <= FONT_LAST) {
            const unsigned char *g = FONT8X14[idx - FONT_FIRST];
            for (int r = 0; r < FONT_H; r++) {
                unsigned char bits = g[r];
                if (!bits) continue;
                for (int c = 0; c < FONT_W; c++)
                    if (bits & (1 << (7 - c)))
                        fx_fill_rect(s, cx + c * scale, y + r * scale, scale, scale, color);
            }
        }
        cx += FONT_W * scale;
    }
}

/* fichier brut : "FXRAW <w> <h>\n" puis w*h*3 octets RGB */
int fx_load_raw_rgb(FXScreen *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char magic[8] = {0}; int w = 0, h = 0;
    if (fscanf(f, "%5s %d %d", magic, &w, &h) != 3 || strcmp(magic, "FXRAW") != 0) { fclose(f); return -1; }
    fgetc(f);
    unsigned char *row = malloc((size_t)w * 3);
    if (!row) { fclose(f); return -1; }
    for (int y = 0; y < h && y < s->h; y++) {
        if (fread(row, 1, (size_t)w * 3, f) != (size_t)w * 3) break;
        for (int x = 0; x < w && x < s->w; x++)
            s->px[(size_t)y * s->w + x] = ((uint32_t)row[x*3] << 16) | ((uint32_t)row[x*3+1] << 8) | row[x*3+2];
    }
    free(row); fclose(f);
    return 0;
}

uint32_t fx_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void fx_default_palette(uint8_t pal[16][3]) {
    static const uint8_t P[16][3] = {
        {13,20,34},   {237,73,86},   {57,222,138}, {246,173,85},
        {18,103,181}, {214,51,255},  {91,200,255}, {234,242,255},
        {60,78,110},  {255,110,120}, {90,235,165}, {255,200,120},
        {80,150,235}, {235,130,255}, {140,220,255},{255,255,255}
    };
    memcpy(pal, P, sizeof P);
}

/* couleur d'une cellule : 255 = couleur par defaut */
uint32_t fx_vt_color(const VT *vt, int idx, int is_bg, uint8_t def_fg, uint8_t def_bg) {
    uint8_t r, g, b;
    if (idx == 255) {
        int d = is_bg ? def_bg : def_fg;
        if (d == 255) {
            if (is_bg) { r = 13; g = 20; b = 34; }
            else       { r = 234; g = 242; b = 255; }
        } else {
            vt_palette(vt, d, &r, &g, &b);
        }
    } else {
        vt_palette(vt, idx, &r, &g, &b);
    }
    return fx_rgb(r, g, b);
}
