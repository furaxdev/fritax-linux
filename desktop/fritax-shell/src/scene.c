/* ============================================================
 *  FRITAX - apercu du bureau complet
 *  Compose une vraie scene : fond d'ecran, icones, fenetre du
 *  terminal en train de tourner, barre flottante avec applis.
 *  Tout est dessine par notre propre code (aucune image "fake").
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "screen.h"
#include "ui.h"
#include "vt.h"

static const char *SESSION =
    "\033[1;38;5;213mfuraxb@fritax\033[0m:\033[1;38;5;111m~/projets/fritax-linux\033[0m$ ls\r\n"
    "\033[1;38;5;111mdesktop\033[0m     \033[38;5;245mbureau + terminal, ecrits par nous\033[0m\r\n"
    "\033[1;38;5;111mdistro\033[0m      \033[38;5;245mconfiguration de Fritax Linux\033[0m\r\n"
    "\033[1;38;5;111mbranding\033[0m    \033[38;5;245mlogo, fond d'ecran, personnage\033[0m\r\n"
    "README.md    \033[38;5;245mle projet complet\033[0m\r\n"
    "\r\n"
    "\033[1;38;5;213mfuraxb@fritax\033[0m:\033[1;38;5;111m~/projets/fritax-linux\033[0m$ \033[38;5;114mcat\033[0m version.txt\r\n"
    "\033[1;38;5;255mFritax Linux 1.0 \253 Nova \273\033[0m\r\n"
    "\033[38;5;245m  noyau    : Linux 6.12 (officiel, pris tel quel)\033[0m\r\n"
    "\033[38;5;245m  systeme  : compile depuis les sources\033[0m\r\n"
    "\033[38;5;245m  bureau   : Fritax Shell (DRM/KMS, fait maison)\033[0m\r\n"
    "\033[38;5;245m  terminal : Fritax Terminal (moteur maison)\033[0m\r\n"
    "\r\n"
    "\033[1;38;5;213mfuraxb@fritax\033[0m:\033[1;38;5;111m~/projets/fritax-linux\033[0m$ \033[1;38;5;114m./distro/build.sh\033[0m\r\n"
    "\033[38;5;245m  compilation du systeme...\033[0m\r\n"
    "\033[38;5;114m  [  412/1204 ] \033[0mconstruction du compilateur\r\n"
    "\033[38;5;114m  [  980/1204 ] \033[0mnoyau Linux + nos paquets\r\n"
    "\033[38;5;245m  ISO PRETE : fritax-linux-1.0-nova.iso\033[0m\r\n"
    "\033[1;38;5;213mfuraxb@fritax\033[0m:\033[1;38;5;111m~/projets/fritax-linux\033[0m$ \033[0;38;5;255m_\033[0m";

/* Boutons de fenetre Fritax : trois carres arrondis SEPARES, fond sombre et
   symbole epais dessine pixel par pixel dans la couleur d'accent.
   Volontairement rien a voir avec les pastilles alignees de macOS. */
static void window_button(FXScreen *s, int x, int y, int sz, uint32_t accent, int kind) {
    int r = 7;
    fx_fill_round_rect(s, x, y, sz, sz, r, 0x1C2740);
    /* contour : on dessine un carre arrondi legerement plus grand puis on evide l'interieur */
    for (int i = 0; i < 2; i++)
        fx_fill_round_rect(s, x - 1 - 0, y - 1, sz + 2, 2, 1, accent), (void)i;
    fx_fill_round_rect(s, x, y - 1, sz, 2, 1, accent);
    fx_fill_round_rect(s, x, y + sz - 1, sz, 2, 1, accent);
    fx_fill_rect(s, x - 1, y, 2, sz, accent);
    fx_fill_rect(s, x + sz - 1, y, 2, sz, accent);
    int m = 5;                             /* marge interieure */
    if (kind == 0) {                       /* reduire : un trait epais */
        fx_fill_rect(s, x + m + 1, y + sz / 2 - 1, sz - 2 * m - 2, 3, accent);
    } else if (kind == 1) {                /* agrandir : carre vide epais */
        int t = 2;
        fx_fill_rect(s, x + m, y + m, sz - 2 * m, t, accent);
        fx_fill_rect(s, x + m, y + sz - m - t, sz - 2 * m, t, accent);
        fx_fill_rect(s, x + m, y + m, t, sz - 2 * m, accent);
        fx_fill_rect(s, x + sz - m - t, y + m, t, sz - 2 * m, accent);
    } else {                               /* fermer : croix epaisse */
        for (int i = 0; i < sz - 2 * m; i++) {
            fx_fill_rect(s, x + m + i, y + m + i, 2, 2, accent);
            fx_fill_rect(s, x + sz - m - 1 - i, y + m + i, 2, 2, accent);
        }
    }
}

static uint32_t shadowed(uint32_t c, int f) {
    return (uint32_t)((((c >> 16) & 255) * f / 100) << 16) | (uint32_t)((((c >> 8) & 255) * f / 100) << 8) | (uint32_t)(((c & 255) * f / 100));
}

/* copie une source dans une zone arrondie (fenetre) */
static void blit_round(FXScreen *dst, FXScreen *src, int dx, int dy, int r) {
    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++) {
            int mx = x < r ? r - x : (x >= src->w - r ? x - (src->w - r - 1) : 0);
            int my = y < r ? r - y : (y >= src->h - r ? y - (src->h - r - 1) : 0);
            if (mx && my && mx * mx + my * my > r * r) continue;
            int px = dx + x, py = dy + y;
            if (px < 0 || py < 0 || px >= dst->w || py >= dst->h) continue;
            dst->px[(size_t)py * dst->w + px] = src->px[(size_t)y * src->w + x];
        }
}

/* icone du bureau : pavé arrondi + lettre + libelle centre */
static void desktop_icon(FXScreen *s, int cx, int y, uint32_t color, const char *ini, const char *label) {
    int sz = 76, r = 18;
    int x = cx - sz / 2;
    fx_blend_round_rect(s, x + 3, y + 6, sz, sz, r, 0x000000, 110);
    fx_fill_round_rect(s, x, y, sz, sz, r, color);
    fx_blend_round_rect(s, x + 8, y + 6, sz - 16, 26, 10, 0xFFFFFF, 46);
    fx_draw_text_scale(s, x + (sz - FONT_W * 3) / 2, y + (sz - FONT_H * 3) / 2, ini, 0xFFFFFF, 3);
    int tw = (int)strlen(label) * FONT_W;
    /* etiquette lisible (ombre + texte) */
    fx_blend_round_rect(s, cx - tw / 2 - 8, y + sz + 8, tw + 16, FONT_H + 10, 8, 0x000000, 150);
    fx_draw_text(s, cx - tw / 2, y + sz + 13, label, 0xFFFFFF);
}

static int render_scene(const char *out, int W, int H, const char *wall) {
    FXScreen *s = fx_screen_new(W, H);
    if (!s) return 1;

    /* 1. fond d'ecran */
    if (!wall || fx_load_raw_rgb(s, wall) != 0) fx_gradient_v(s, 0, 0, W, H, 0x0D1422, 0x10233F);

    /* 2. icones du bureau (colonne de gauche) */
    int col = 84, y = 56;
    desktop_icon(s, col, y,        0x5BC8FF, "T", "Terminal");  y += 130;
    desktop_icon(s, col, y,        0xF6AD55, "F", "Fichiers");  y += 130;
    desktop_icon(s, col, y,        0x9AA6BC, "C", "Corbeille");

    /* 3. la fenetre du terminal, avec le moteur en marche */
    int ww = 980, wh = 560, wx = (W - ww) / 2 + 40, wy = 150, tb = 40, PAD = 8;
    int cw = ww - PAD * 2, ch = wh - tb - PAD * 2;
    int cols = cw / FONT_W, rows = ch / FONT_H;
    FXScreen *win = fx_screen_new(ww, wh);
    FXScreen *content = fx_screen_new(cw, ch);   /* le terminal occupe la zone SOUS le titre */
    VTHooks hooks = { NULL, NULL, NULL, NULL };
    VT *vt = vt_new(cols, rows, 200, hooks, NULL);
    uint8_t pal[16][3];
    fx_default_palette(pal);
    vt_set_palette(vt, pal);
    vt_feed(vt, SESSION, strlen(SESSION));

    fx_fill_rect(win, 0, 0, ww, wh, 0x0F1626);
    /* barre de titre : le titre a gauche, nos boutons a droite */
    fx_fill_rect(win, 0, 0, ww, tb, 0x1A2438);
    fx_fill_rect(win, 0, tb - 1, ww, 1, 0x27354E);
    /* petit losange violet = marque Fritax */
    fx_fill_round_rect(win, 16, tb / 2 - 4, 9, 9, 2, C_VIOLET);
    fx_draw_text(win, 34, (tb - FONT_H) / 2, "Terminal", 0xEAF2FF);
    char t2[64];
    snprintf(t2, sizeof t2, "%dx%d", cols, rows);
    fx_draw_text(win, 34 + 8 * FONT_W + 10, (tb - FONT_H) / 2, t2, 0x5C6E8C);
    /* trois boutons separes, alignes a droite (style Fritax) */
    int bsz = 22, gap = 12, tot = 3 * bsz + 2 * gap;
    int bx = ww - tot - 18, by = (tb - bsz) / 2;
    window_button(win, bx, by, bsz, C_CYAN, 0);                       /* reduire */
    window_button(win, bx + bsz + gap, by, bsz, C_VIOLET, 1);         /* agrandir */
    window_button(win, bx + 2 * (bsz + gap), by, bsz, 0xFF5C7A, 2);   /* fermer */
    /* contenu du terminal, dans sa propre zone puis colle sous la barre de titre */
    fx_render_vt(vt, content, 0, rows - 1, 0, 0xD633FF, 0x0F1626);
    for (int y = 0; y < ch; y++)
        memcpy(win->px + (size_t)(tb + PAD + y) * ww + PAD, content->px + (size_t)y * cw, (size_t)cw * sizeof(uint32_t));

    /* ombre portee puis la fenetre */
    fx_blend_round_rect(s, wx + 6, wy + 12, ww, wh, 16, 0x000000, 130);
    blit_round(s, win, wx, wy, 16);

    /* 4. la barre flottante avec les applis en cours */
    fx_ui_init(W, H, NULL, 0);
    fx_ui_apercu_appli("Terminal", 0x5BC8FF);
    fx_ui_apercu_appli("Fichiers", 0xF6AD55);
    fx_ui_render(s);

    /* 5. le curseur de la souris */
    fx_ui_draw_cursor(s, W / 2 + 345, H / 2 + 175);

    /* 6. ecriture du fichier */
    FILE *f = fopen(out, "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint32_t p = s->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255), (unsigned char)((p >> 8) & 255), (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("scene rendue : %s (%dx%d)\n", out, W, H);
    (void)shadowed;
    return 0;
}

int main(int argc, char **argv) {
    const char *out = "/tmp/bureau.ppm", *wall = NULL;
    int W = 1920, H = 1080;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--fond") && i + 1 < argc) wall = argv[++i];
        else if (!strcmp(argv[i], "--taille") && i + 2 < argc) { W = atoi(argv[i + 1]); H = atoi(argv[i + 2]); }
    }
    return render_scene(out, W, H, wall);
}
