/* Rendu de demonstration : simule une session shell et ecrit un PPM. */
#include "../src/vt.h"
#include "../src/screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PAD 6

static void on_dirty(void *u, int r) { (void)u; (void)r; }
static void on_reply(void *u, const char *s, size_t n) { (void)u; (void)s; (void)n; }
static void on_bell(void *u) { (void)u; }
static void on_title(void *u, const char *s, size_t n) { (void)u; (void)s; (void)n; }
#define FEED(v, s) vt_feed((v), (s), strlen(s))

int main(int argc, char **argv) {
    int cols = 96, rows = 26;
    VTHooks h = { on_dirty, on_reply, on_bell, on_title };
    VT *vt = vt_new(cols, rows, 2000, h, NULL);
    uint8_t pal[16][3];
    fx_default_palette(pal);
    vt_set_palette(vt, pal);

    vt_feed(vt, "\033[2J\033[H", 7);
    FEED(vt, "\r\n");
    FEED(vt, "  \033[1;38;5;213mF R I T A X   T E R M I N A L\033[0m   \033[38;5;245mVERSION 1.0 \253 NOVA \273\033[0m\r\n");
    FEED(vt, "  \033[38;5;245mLe terminal maison de Fritax Linux - ecrit de zero, sans dependance\033[0m\r\n");
    FEED(vt, "\r\n");
    FEED(vt, "  \033[1;38;5;213m[Couleurs]\033[0m  ");
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\033[4%d m  \033[0m", i); vt_feed(vt, b, strlen(b)); }
    FEED(vt, "\r\n            ");
    for (int i = 0; i < 8; i++) { char b[32]; snprintf(b, sizeof b, "\033[10%d m  \033[0m", i); vt_feed(vt, b, strlen(b)); }
    FEED(vt, "\r\n\r\n");
    FEED(vt, "  \033[1;38;5;114mfuraxb@fritax\033[0m:\033[1;38;5;75m~/projets/fritax-linux\033[0m$ ls -la\r\n");
    FEED(vt, "  drwxr-xr-x  \033[1;38;5;75mdesktop\033[0m      \033[38;5;245m4,0K\033[0m   le bureau et le terminal maison\r\n");
    FEED(vt, "  drwxr-xr-x  \033[1;38;5;75mbranding\033[0m     \033[38;5;245m4,0K\033[0m   logo, fond d'ecran, perso de Fritax\r\n");
    FEED(vt, "  -rw-r--r--  \033[1;38;5;75mREADME.md\033[0m    \033[38;5;245m2,1K\033[0m   presentation du projet\r\n");
    FEED(vt, "  -rwxr-xr-x  \033[1;38;5;114mfritax-terminal\033[0m  \033[38;5;245m48K\033[0m   le terminal (binaire compile)\r\n");
    FEED(vt, "\r\n");
    FEED(vt, "  \033[1;38;5;114mfuraxb@fritax\033[0m:\033[1;38;5;75m~/projets/fritax-linux\033[0m$ \033[1;38;5;213m./fritax-terminal --version\033[0m\r\n");
    FEED(vt, "  \033[38;5;250mFritax Terminal 1.0 (Nova)\033[0m\r\n");
    FEED(vt, "\r\n");
    FEED(vt, "  \033[1;38;5;114mfuraxb@fritax\033[0m:\033[1;38;5;75m~/projets/fritax-linux\033[0m$ \033[1;38;5;213mecho \"\303\247a marche, les accents aussi : \303\251\303\250\303\240\303\271\303\247\304\231\303\252\"\033[0m\r\n");
    FEED(vt, "  \303\247a marche, les accents aussi : \303\251\303\250\303\240\303\271\303\247\303\252\303\252\r\n");
    FEED(vt, "\r\n");
    FEED(vt, "  \033[1;38;5;114mfuraxb@fritax\033[0m:\033[1;38;5;75m~/projets/fritax-linux\033[0m$ ");
    FEED(vt, "touch \033[1;38;5;75mFritax-Linux.iso\033[0m\r\n\r\n");
    FEED(vt, "  \033[38;5;245m-- moteur VT100/xterm : couleurs, curseur, historique, vim, htop --\033[0m\r\n");
    FEED(vt, "  \033[1;38;5;114mOK\033[0m  \033[38;5;245m22/22 tests du moteur passent\033[0m\r\n");

    FXScreen *s = fx_screen_new(cols * FONT_W + PAD * 2, rows * FONT_H + PAD * 2);
    fx_clear(s, 0x0D1422);
    fx_render_vt(vt, s, 0, rows - 1, PAD, 0xD633FF, 0x0D1422);

    const char *out = argc > 1 ? argv[1] : "/tmp/fx-demo.ppm";
    FILE *f = fopen(out, "wb");
    if (!f) { perror("fopen"); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
    for (int i = 0; i < s->w * s->h; i++) {
        uint32_t p = s->px[i];
        unsigned char rgb[3] = { (unsigned char)((p >> 16) & 255), (unsigned char)((p >> 8) & 255), (unsigned char)(p & 255) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("ecrit %s (%dx%d)\n", out, s->w, s->h);
    fx_screen_free(s);
    vt_free(vt);
    return 0;
}
