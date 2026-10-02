/* Fritax Terminal - moteur d'emulation VT100 / xterm. Aucune dependance externe. */
#include "vt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- etats du parseur ---------- */
enum { ST_GROUND, ST_ESC, ST_CSI, ST_OSC, ST_OSC_ESC, ST_CHARSET, ST_ESC_INT, ST_CSI_INT };

#define MAXPARAM 24
#define MAXOSC   512

struct VT {
    int cols, rows;
    int cx, cy;
    VTCell *grid;            /* ecran courant (rows * cols) */
    VTCell *alt;             /* ecran alternatif */
    VTCell *saved_grid;      /* ecran principal sauvegarde */
    int top, bot;            /* zone de scroll, inclusif */
    uint8_t fg, bg;          /* couleurs courantes ; 255 = defaut */
    uint16_t attr;
    int wrap_pending;
    int origin, autowrap, cursor_visible;
    int alt_active;
    int scx, scy; uint8_t sfg, sbg; uint16_t sattr;
    int sb_lines, sb_head, sb_count;
    VTCell *sb;
    int view_offset;
    int blink_on, blink_ticks;
    int state;
    unsigned prm[MAXPARAM]; int nprm, curprm, hasprm;
    int priv, has_int, intermediate;
    uint32_t utf8_cp; int utf8_need;
    char osc[MAXOSC]; int osclen;
    char title[256];
    VTHooks hooks; void *user;
    uint8_t pal[16][3];
    int named[16];
    int mouse_mode, bpaste;
};

/* ---------- palette xterm par defaut ---------- */
static const uint8_t DEF16[16][3] = {
    {0,0,0},{205,49,49},{13,188,121},{229,229,16},
    {36,114,200},{188,63,188},{17,168,205},{229,229,229},
    {102,102,102},{241,76,76},{35,209,139},{245,245,67},
    {59,142,234},{214,112,214},{41,184,219},{255,255,255}
};

static void dirty(VT *v, int row) { if (v->hooks.on_dirty) v->hooks.on_dirty(v->user, row); }
static void reply(VT *v, const char *s, size_t n) { if (v->hooks.on_reply) v->hooks.on_reply(v->user, s, n); }
static void replyf(VT *v, const char *fmt, ...) {
    char b[128]; va_list ap;
    va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    reply(v, b, strlen(b));
}

static int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* ---------- ecriture / scroll ---------- */
static void cell_clear(VT *v, VTCell *c) { c->ch = ' '; c->fg = 255; c->bg = 255; c->attr = 0; }

static void scroll_up(VT *v, int n) {
    if (n <= 0) return;
    int h = v->bot - v->top + 1;
    if (n > h) n = h;
    /* memoire de scroll : on garde la ligne qui sort si on scrolle tout l'ecran */
    if (v->top == 0 && v->bot == v->rows - 1 && !v->alt_active && v->sb) {
        for (int k = 0; k < n; k++) {
            VTCell *dst = v->sb + v->sb_head * v->cols;
            memcpy(dst, v->grid + (v->top + k) * v->cols, (size_t)v->cols * sizeof(VTCell));
            v->sb_head = (v->sb_head + 1) % v->sb_lines;
            if (v->sb_count < v->sb_lines) v->sb_count++;
        }
    }
    memmove(v->grid + v->top * v->cols, v->grid + (v->top + n) * v->cols,
            (size_t)(h - n) * v->cols * sizeof(VTCell));
    for (int r = v->bot - n + 1; r <= v->bot; r++) {
        VTCell *row = v->grid + r * v->cols;
        for (int c = 0; c < v->cols; c++) cell_clear(v, &row[c]);
    }
    dirty(v, -1);
}

static void scroll_down(VT *v, int n) {
    if (n <= 0) return;
    int h = v->bot - v->top + 1;
    if (n > h) n = h;
    memmove(v->grid + (v->top + n) * v->cols, v->grid + v->top * v->cols,
            (size_t)(h - n) * v->cols * sizeof(VTCell));
    for (int r = v->top; r < v->top + n; r++) {
        VTCell *row = v->grid + r * v->cols;
        for (int c = 0; c < v->cols; c++) cell_clear(v, &row[c]);
    }
    dirty(v, -1);
}

static void insert_lines(VT *v, int n) {
    if (v->cy < v->top || v->cy > v->bot) return;
    int h = v->bot - v->cy + 1;
    if (n > h) n = h;
    memmove(v->grid + (size_t)(v->cy + n) * v->cols, v->grid + (size_t)v->cy * v->cols,
            (size_t)(h - n) * v->cols * sizeof(VTCell));
    for (int r = v->cy; r < v->cy + n; r++)
        for (int c = 0; c < v->cols; c++) cell_clear(v, &v->grid[(size_t)r * v->cols + c]);
    dirty(v, -1);
}

static void delete_lines(VT *v, int n) {
    if (v->cy < v->top || v->cy > v->bot) return;
    int h = v->bot - v->cy + 1;
    if (n > h) n = h;
    memmove(v->grid + (size_t)v->cy * v->cols, v->grid + (size_t)(v->cy + n) * v->cols,
            (size_t)(h - n) * v->cols * sizeof(VTCell));
    for (int r = v->bot - n + 1; r <= v->bot; r++)
        for (int c = 0; c < v->cols; c++) cell_clear(v, &v->grid[(size_t)r * v->cols + c]);
    dirty(v, -1);
}

static void linefeed(VT *v) {
    v->wrap_pending = 0;
    if (v->cy == v->bot) scroll_up(v, 1);
    else if (v->cy < v->rows - 1) v->cy++;
}

static void put_char(VT *v, uint32_t ch) {
    if (v->view_offset) { v->view_offset = 0; dirty(v, -1); }
    if (v->wrap_pending) {
        v->wrap_pending = 0;
        v->cx = 0;
        linefeed(v);
        if (v->origin) v->cx = 0;
    }
    if (v->cx < 0) v->cx = 0;
    if (v->cx >= v->cols) v->cx = v->cols - 1;
    VTCell *c = &v->grid[v->cy * v->cols + v->cx];
    c->ch = ch; c->fg = v->fg; c->bg = v->bg; c->attr = v->attr;
    dirty(v, v->cy);
    if (v->cx + 1 >= v->cols) {
        if (v->autowrap) v->wrap_pending = 1;
    } else v->cx++;
}

static void erase_cells(VT *v, int row, int from, int to) {
    if (row < 0 || row >= v->rows) return;
    if (from < 0) from = 0;
    if (to >= v->cols) to = v->cols - 1;
    for (int c = from; c <= to; c++) cell_clear(v, &v->grid[row * v->cols + c]);
    dirty(v, row);
}

/* ---------- SGR ---------- */
static void sgr(VT *v) {
    if (v->nprm == 0) { v->attr = 0; v->fg = v->bg = 255; return; }
    for (int i = 0; i < v->nprm; i++) {
        unsigned p = v->prm[i];
        switch (p) {
        case 0:  v->attr = 0; v->fg = v->bg = 255; break;
        case 1:  v->attr |= VT_BOLD; break;
        case 2:  v->attr |= VT_DIM; break;
        case 3:  v->attr |= VT_ITALIC; break;
        case 4:  v->attr |= VT_UNDERLINE; break;
        case 5:  v->attr |= VT_BLINK; break;
        case 7:  v->attr |= VT_REVERSE; break;
        case 22: v->attr &= ~(VT_BOLD | VT_DIM); break;
        case 23: v->attr &= ~VT_ITALIC; break;
        case 24: v->attr &= ~VT_UNDERLINE; break;
        case 25: v->attr &= ~VT_BLINK; break;
        case 27: v->attr &= ~VT_REVERSE; break;
        case 38:
        case 48: {
            uint8_t col = 255;
            if (i + 2 < v->nprm && v->prm[i + 1] == 5) { col = (uint8_t)v->prm[i + 2]; i += 2; }
            else if (i + 4 < v->nprm && v->prm[i + 1] == 2) {
                /* truecolor : on ramene aux 256 couleurs les plus proches */
                int r = (int)v->prm[i + 2], g = (int)v->prm[i + 3], b = (int)v->prm[i + 4];
                int best = 0; long bd = 1L << 30;
                for (int k = 16; k < 256; k++) {
                    uint8_t rr, gg, bb; vt_palette(v, k, &rr, &gg, &bb);
                    long d = (long)(r - rr) * (r - rr) + (long)(g - gg) * (g - gg) + (long)(b - bb) * (b - bb);
                    if (d < bd) { bd = d; best = k; }
                }
                col = (uint8_t)best; i += 4;
            }
            if (p == 38) v->fg = col; else v->bg = col;
            break; }
        default:
            if (p >= 30 && p <= 37) v->fg = (uint8_t)(p - 30);
            else if (p == 39) v->fg = 255;
            else if (p >= 40 && p <= 47) v->bg = (uint8_t)(p - 40);
            else if (p == 49) v->bg = 255;
            else if (p >= 90 && p <= 97) v->fg = (uint8_t)(p - 90 + 8);
            else if (p >= 100 && p <= 107) v->bg = (uint8_t)(p - 100 + 8);
            break;
        }
    }
}

/* ---------- modes DEC ---------- */
static void decset(VT *v, int on) {
    for (int i = 0; i < v->nprm; i++) {
        switch (v->prm[i]) {
        case 1:   v->origin = on; v->cx = v->cy = 0; break;
        case 6:   v->origin = on; break;
        case 7:   v->autowrap = on; break;
        case 25:  v->cursor_visible = on; dirty(v, -1); break;
        case 1000: case 1002: case 1003: v->mouse_mode = on ? (int)v->prm[i] : 0; break;
        case 1006: break;                       /* souris SGR : accepte, mode gere par l'UI */
        case 2004: v->bpaste = on; break;
        case 1049:
            if (on) {
                if (!v->alt_active) {
                    v->scx = v->cx; v->scy = v->cy; v->sfg = v->fg; v->sbg = v->bg; v->sattr = v->attr;
                    memcpy(v->alt, v->grid, (size_t)v->rows * v->cols * sizeof(VTCell));
                    VTCell *t = v->alt; v->alt = v->grid; v->grid = t;
                    v->alt_active = 1;
                }
                for (int r = 0; r < v->rows; r++) erase_cells(v, r, 0, v->cols - 1);
                v->cx = v->cy = 0;
            } else if (v->alt_active) {
                VTCell *t = v->alt; v->alt = v->grid; v->grid = t;
                v->alt_active = 0;
                v->cx = v->scx; v->cy = v->scy; v->fg = v->sfg; v->bg = v->sbg; v->attr = v->sattr;
            }
            dirty(v, -1);
            break;
        case 47: case 1047:
            if (on && !v->alt_active) {
                memcpy(v->alt, v->grid, (size_t)v->rows * v->cols * sizeof(VTCell));
                VTCell *t = v->alt; v->alt = v->grid; v->grid = t;
                v->alt_active = 1;
                for (int r = 0; r < v->rows; r++) erase_cells(v, r, 0, v->cols - 1);
            } else if (!on && v->alt_active) {
                VTCell *t = v->alt; v->alt = v->grid; v->grid = t;
                v->alt_active = 0;
            }
            dirty(v, -1);
            break;
        default: break;
        }
    }
}

/* ---------- CSI ---------- */
static unsigned prm_or(VT *v, int i, unsigned def) {
    return (i < v->nprm && v->prm[i] != 0) ? v->prm[i] : def;
}

static void csi_dispatch(VT *v, char final) {
    int n = v->nprm;
    switch (final) {
    case 'A': v->cy = clampi(v->cy - (int)prm_or(v, 0, 1), v->origin ? v->top : 0, v->bot); break;
    case 'B': v->cy = clampi(v->cy + (int)prm_or(v, 0, 1), v->origin ? v->top : 0, v->bot); break;
    case 'C': v->cx = clampi(v->cx + (int)prm_or(v, 0, 1), 0, v->cols - 1); break;
    case 'D': v->cx = clampi(v->cx - (int)prm_or(v, 0, 1), 0, v->cols - 1); break;
    case 'E': v->cx = 0; v->cy = clampi(v->cy + (int)prm_or(v, 0, 1), 0, v->rows - 1); break;
    case 'F': v->cx = 0; v->cy = clampi(v->cy - (int)prm_or(v, 0, 1), 0, v->rows - 1); break;
    case 'G': v->cx = clampi((int)prm_or(v, 0, 1) - 1, 0, v->cols - 1); break;
    case 'H': case 'f': {
        int r = (int)prm_or(v, 0, 1) - 1, c = (n > 1 && v->prm[1]) ? (int)v->prm[1] - 1 : 0;
        if (v->origin) { r += v->top; if (r > v->bot) r = v->bot; }
        v->cy = clampi(r, 0, v->rows - 1);
        v->cx = clampi(c, 0, v->cols - 1);
        break; }
    case 'J': {
        unsigned m = n ? v->prm[0] : 0;
        if (m == 0) { erase_cells(v, v->cy, v->cx, v->cols - 1);
                      for (int r = v->cy + 1; r < v->rows; r++) erase_cells(v, r, 0, v->cols - 1); }
        else if (m == 1) { for (int r = 0; r < v->cy; r++) erase_cells(v, r, 0, v->cols - 1);
                           erase_cells(v, v->cy, 0, v->cx); }
        else for (int r = 0; r < v->rows; r++) erase_cells(v, r, 0, v->cols - 1);
        break; }
    case 'K': {
        unsigned m = n ? v->prm[0] : 0;
        if (m == 0) erase_cells(v, v->cy, v->cx, v->cols - 1);
        else if (m == 1) erase_cells(v, v->cy, 0, v->cx);
        else erase_cells(v, v->cy, 0, v->cols - 1);
        break; }
    case 'L': insert_lines(v, (int)prm_or(v, 0, 1)); break;
    case 'M': delete_lines(v, (int)prm_or(v, 0, 1)); break;
    case 'P': { int k = (int)prm_or(v, 0, 1); if (k > v->cols - v->cx) k = v->cols - v->cx;
                VTCell *row = v->grid + v->cy * v->cols;
                memmove(row + v->cx, row + v->cx + k, (size_t)(v->cols - v->cx - k) * sizeof(VTCell));
                for (int c = v->cols - k; c < v->cols; c++) cell_clear(v, &row[c]);
                dirty(v, v->cy); break; }
    case 'S': scroll_up(v, (int)prm_or(v, 0, 1)); break;
    case 'T': scroll_down(v, (int)prm_or(v, 0, 1)); break;
    case 'X': { int k = (int)prm_or(v, 0, 1); erase_cells(v, v->cy, v->cx, v->cx + k - 1); break; }
    case 'd': v->cy = clampi((int)prm_or(v, 0, 1) - 1, 0, v->rows - 1); break;
    case 'm': sgr(v); break;
    case 'n':
        if (n && v->prm[0] == 5) replyf(v, "\033[0n");
        else if (n && v->prm[0] == 6) replyf(v, "\033[%d;%dR", v->cy + 1, v->cx + 1);
        break;
    case 'c': if (v->priv) replyf(v, "\033[?1;2c"); else replyf(v, "\033[?62;1;6;9;15;22c"); break;
    case 'r': {
        int t = n && v->prm[0] ? (int)v->prm[0] - 1 : 0;
        int b = (n > 1 && v->prm[1]) ? (int)v->prm[1] - 1 : v->rows - 1;
        if (t < b) { v->top = clampi(t, 0, v->rows - 1); v->bot = clampi(b, 0, v->rows - 1); }
        else { v->top = 0; v->bot = v->rows - 1; }
        v->cx = v->cy = v->origin ? v->top : 0;
        break; }
    case 's': v->scx = v->cx; v->scy = v->cy; break;
    case 'u': v->cx = v->scx; v->cy = v->scy; break;
    case 'h': if (v->priv) decset(v, 1); break;
    case 'l': if (v->priv) decset(v, 0); break;
    case 'g': break;
    case 'q': break;
    case '@': { int k = (int)prm_or(v, 0, 1); if (k > v->cols - v->cx) k = v->cols - v->cx;
                VTCell *row = v->grid + v->cy * v->cols;
                memmove(row + v->cx + k, row + v->cx, (size_t)(v->cols - v->cx - k) * sizeof(VTCell));
                for (int c = 0; c < k; c++) cell_clear(v, &row[v->cx + c]);
                dirty(v, v->cy); break; }
    default: break;
    }
}

/* ---------- parseur ---------- */
static void utf8_byte(VT *v, unsigned char b) {
    if (v->utf8_need == 0) {
        if (b < 0x80) { put_char(v, b); return; }
        if ((b & 0xE0) == 0xC0) { v->utf8_cp = b & 0x1F; v->utf8_need = 1; }
        else if ((b & 0xF0) == 0xE0) { v->utf8_cp = b & 0x0F; v->utf8_need = 2; }
        else if ((b & 0xF8) == 0xF0) { v->utf8_cp = b & 0x07; v->utf8_need = 3; }
        else put_char(v, '?');
        return;
    }
    if ((b & 0xC0) != 0x80) { v->utf8_need = 0; put_char(v, '?'); return; }
    v->utf8_cp = (v->utf8_cp << 6) | (b & 0x3F);
    if (--v->utf8_need == 0) put_char(v, v->utf8_cp);
}

static void csi_finish(VT *v, char final) {
    csi_dispatch(v, final);
    v->state = ST_GROUND; v->nprm = 0; v->curprm = 0; v->hasprm = 0; v->priv = 0; v->has_int = 0;
}

void vt_feed(VT *v, const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        switch (v->state) {
        case ST_GROUND:
            if (c == 0x1B) { v->state = ST_ESC; }
            else if (c == 0x0D) { v->cx = 0; v->wrap_pending = 0; }
            else if (c == 0x0A || c == 0x0B || c == 0x0C) linefeed(v);
            else if (c == 0x08) { if (v->cx > 0) v->cx--; v->wrap_pending = 0; dirty(v, v->cy); }
            else if (c == 0x09) { int nx = ((v->cx / 8) + 1) * 8; v->cx = clampi(nx, 0, v->cols - 1); }
            else if (c == 0x07) { if (v->hooks.on_bell) v->hooks.on_bell(v->user); }
            else if (c == 0x0E || c == 0x0F) { }
            else if (c < 0x20) { }
            else utf8_byte(v, c);
            break;

        case ST_ESC:
            if (c == '[') { v->state = ST_CSI; v->nprm = 0; v->curprm = 0; v->hasprm = 0; v->priv = 0; }
            else if (c == ']') { v->state = ST_OSC; v->osclen = 0; }
            else if (c == 'P' || c == '^' || c == '_') { v->state = ST_OSC; v->osclen = 0; }
            else if (c == '(' || c == ')' || c == '*' || c == '+') { v->state = ST_CHARSET; }
            else if (c == '7') { v->scx = v->cx; v->scy = v->cy; v->state = ST_GROUND; }
            else if (c == '8') { v->cx = v->scx; v->cy = v->scy; v->state = ST_GROUND; }
            else if (c == 'D') { v->wrap_pending = 0; if (v->cy == v->bot) scroll_up(v, 1); else if (v->cy < v->rows - 1) v->cy++; v->state = ST_GROUND; }
            else if (c == 'M') { v->wrap_pending = 0; if (v->cy == v->top) scroll_down(v, 1); else if (v->cy > 0) v->cy--; v->state = ST_GROUND; }
            else if (c == 'E') { v->cx = 0; v->wrap_pending = 0; if (v->cy == v->bot) scroll_up(v, 1); else if (v->cy < v->rows - 1) v->cy++; v->state = ST_GROUND; }
            else if (c == 'c') { /* RIS : remise a zero complete */
                vt_resize(v, v->cols, v->rows);
                v->cx = v->cy = 0; v->attr = 0; v->fg = v->bg = 255;
                v->top = 0; v->bot = v->rows - 1; v->autowrap = 1; v->cursor_visible = 1;
                v->state = ST_GROUND; dirty(v, -1); }
            else if (c == '#') { v->state = ST_ESC_INT; }
            else { v->state = ST_GROUND; }
            break;

        case ST_ESC_INT: v->state = ST_GROUND; break;
        case ST_CHARSET:  v->state = ST_GROUND; break;

        case ST_CSI:
            if (c >= '0' && c <= '9') { v->curprm = v->curprm * 10 + (c - '0'); v->hasprm = 1; }
            else if (c == ';') {
                if (v->nprm < MAXPARAM) v->prm[v->nprm++] = (unsigned)v->curprm;
                v->curprm = 0; v->hasprm = 0;
            }
            else if (c == '?' || c == '>' || c == '=' || c == '<') { v->priv = c; v->has_int = 1; }
            else if (c >= 0x20 && c <= 0x2F) { v->has_int = 1; }
            else if (c >= 0x40 && c <= 0x7E) {
                if (v->nprm < MAXPARAM) v->prm[v->nprm++] = (unsigned)v->curprm;
                csi_finish(v, (char)c);
            }
            else v->state = ST_GROUND;
            break;

        case ST_OSC:
            if (c == 0x07) {
                v->osc[v->osclen] = 0;
                const char *semi = strchr(v->osc, ';');
                if (semi && (v->osc[0] == '0' || v->osc[0] == '2')) {
                    snprintf(v->title, sizeof v->title, "%s", semi + 1);
                    if (v->hooks.on_title) v->hooks.on_title(v->user, v->title, strlen(v->title));
                }
                v->state = ST_GROUND;
            } else if (c == 0x1B) v->state = ST_OSC_ESC;
            else if (v->osclen < MAXOSC - 1) v->osc[v->osclen++] = (char)c;
            break;

        case ST_OSC_ESC:
            if (c == '\\') { v->osc[v->osclen] = 0;
                const char *semi = strchr(v->osc, ';');
                if (semi && (v->osc[0] == '0' || v->osc[0] == '2')) {
                    snprintf(v->title, sizeof v->title, "%s", semi + 1);
                    if (v->hooks.on_title) v->hooks.on_title(v->user, v->title, strlen(v->title));
                }
            }
            v->state = ST_GROUND;
            break;

        default: v->state = ST_GROUND; break;
        }
    }
}

/* ---------- cycle de vie ---------- */
VT *vt_new(int cols, int rows, int sb_lines, VTHooks hooks, void *user) {
    VT *v = calloc(1, sizeof(VT));
    if (!v) return NULL;
    v->hooks = hooks; v->user = user;
    /* on part de tableaux vides : vt_resize ci-dessous alloue ET nettoie tout
       (sinon on copierait de la memoire non initialisee dans l'ecran) */
    v->cols = 0; v->rows = 0; v->grid = NULL; v->alt = NULL;
    v->sb_lines = sb_lines > 0 ? sb_lines : 1;
    v->sb = malloc((size_t)v->sb_lines * cols * sizeof(VTCell));
    v->fg = v->bg = 255;
    v->cursor_visible = 1; v->autowrap = 1; v->blink_on = 1;
    memcpy(v->pal, DEF16, sizeof DEF16);
    for (int i = 0; i < 16; i++) v->named[i] = 1;
    vt_resize(v, cols, rows);
    return v;
}

void vt_resize(VT *vt, int cols, int rows) {
    if (!vt) return;
    cols = clampi(cols, 1, 10000); rows = clampi(rows, 1, 10000);
    VTCell *g = malloc((size_t)cols * rows * sizeof(VTCell));
    VTCell *a = malloc((size_t)cols * rows * sizeof(VTCell));
    if (!g || !a) { free(g); free(a); return; }
    for (int i = 0; i < cols * rows; i++) cell_clear(vt, &g[i]);
    for (int i = 0; i < cols * rows; i++) cell_clear(vt, &a[i]);
    int rc = vt->cols < cols ? vt->cols : cols;
    int rr = vt->rows < rows ? vt->rows : rows;
    for (int r = 0; r < rr; r++)
        memcpy(g + (size_t)r * cols, vt->grid + (size_t)r * vt->cols, (size_t)rc * sizeof(VTCell));
    free(vt->grid); free(vt->alt);
    vt->grid = g; vt->alt = a; vt->cols = cols; vt->rows = rows;
    vt->top = 0; vt->bot = rows - 1;
    if (vt->cx >= cols) vt->cx = cols - 1;
    if (vt->cy >= rows) vt->cy = rows - 1;
    vt->wrap_pending = 0;
    dirty(vt, -1);
}

void vt_free(VT *vt) {
    if (!vt) return;
    free(vt->grid); free(vt->alt); free(vt->sb); free(vt);
}

int vt_cols(const VT *v) { return v->cols; }
int vt_rows(const VT *v) { return v->rows; }
VTCell vt_cell(const VT *v, int row, int col) {
    if (row < 0 || row >= v->rows || col < 0 || col >= v->cols) { VTCell z = {' ', 255, 255, 0}; return z; }
    return v->grid[(size_t)row * v->cols + col];
}
int vt_sb_len(const VT *v) { return v->sb_count; }
VTCell vt_sb_cell(const VT *v, int row, int col) {
    if (row < 0 || row >= v->sb_count || col < 0 || col >= v->cols) { VTCell z = {' ', 255, 255, 0}; return z; }
    int idx = (v->sb_head - 1 - row + v->sb_lines * 2) % v->sb_lines;   /* 0 = la plus recente */
    return v->sb[(size_t)idx * v->cols + col];
}
int vt_cursor_x(const VT *v) { return v->cx; }
int vt_cursor_y(const VT *v) { return v->cy; }
int vt_cursor_visible(const VT *v) { return v->cursor_visible && v->view_offset == 0; }
int vt_cursor_blink_phase(const VT *v) { return v->blink_on; }
int vt_view_offset(const VT *v) { return v->view_offset; }

void vt_scroll_view(VT *vt, int lines) {
    vt->view_offset = clampi(vt->view_offset + lines, 0, vt->sb_count);
    dirty(vt, -1);
}

void vt_tick(VT *vt) {
    if (++vt->blink_ticks >= 5) { vt->blink_ticks = 0; vt->blink_on = !vt->blink_on;
        if (vt->cursor_visible && vt->view_offset == 0) dirty(vt, vt->cy); }
}

void vt_set_palette(VT *vt, const uint8_t rgb[16][3]) {
    memcpy(vt->pal, rgb, sizeof(uint8_t) * 16 * 3);
    for (int i = 0; i < 16; i++) vt->named[i] = 0;
    dirty(vt, -1);
}

int vt_default_named(const VT *vt, int idx) { return (idx >= 0 && idx < 16) ? vt->named[idx] : 0; }

void vt_palette(const VT *v, int idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (idx < 0) idx = 0;
    if (idx < 16) { *r = v->pal[idx][0]; *g = v->pal[idx][1]; *b = v->pal[idx][2]; return; }
    if (idx < 232) {
        int i = idx - 16;
        int ri = i / 36, gi = (i / 6) % 6, bi = i % 6;
        static const uint8_t lv[6] = {0, 95, 135, 175, 215, 255};
        *r = lv[ri]; *g = lv[gi]; *b = lv[bi]; return;
    }
    if (idx < 256) { uint8_t l = (uint8_t)(8 + (idx - 232) * 10); *r = *g = *b = l; return; }
    *r = *g = *b = 255;
}
