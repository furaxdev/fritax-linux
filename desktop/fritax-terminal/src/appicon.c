/* ============================================================
 *  FRITAX - icones d'applications en PIXEL ART
 *
 *  Chaque icone est une petite grille de 16x16 pixels, ecrite a la main
 *  (un caractere par pixel), puis agrandie en blocs nets : aucun lissage,
 *  c'est ce qui donne le cote pixel art.
 *
 *  Le style suit la palette Fluent de la 1.2 : fonds unis, un seul accent
 *  bleu, de l'ambre pour les dossiers, plus de violet ni de cyan neon.
 * ============================================================ */
#include "appicon.h"
#include <string.h>

#define TAILLE_GRILLE 16

/* Lettre -> couleur. '.' laisse voir le fond. */
static uint32_t lettre_couleur(char c) {
    switch (c) {
        case 'w': return 0xFFFFFF;   /* blanc          */
        case 'k': return 0x1A1A1A;   /* presque noir   */
        case 'b': return 0x0078D4;   /* bleu accent    */
        case 'l': return 0x5FA8E8;   /* bleu clair     */
        case 'a': return 0xE8A33D;   /* ambre dossier  */
        case 'd': return 0xB07A22;   /* ambre fonce    */
        case 'g': return 0x8A8A8A;   /* gris           */
        case 'e': return 0xE5E5E5;   /* gris clair     */
        case 'r': return 0xC42B1C;   /* rouge          */
        case 'n': return 0x2D7D46;   /* vert           */
        default:  return 0;          /* transparent    */
    }
}

typedef struct {
    uint32_t fond;                     /* 0 = pas de carre de fond */
    const char *px[TAILLE_GRILLE];
} FxIcon;

/* ---------------------------------------------------------------- les icones */

/* Terminal : carre sombre, chevron ">" et souligne, comme une console. */
static const FxIcon ICONE_TERMINAL = { 0x1A1A1A, {
    "................",
    "..kkkkkkkkkkkk..",
    ".kkkkkkkkkkkkkk.",
    ".kkkkkkkkkkkkkk.",
    ".kkwwkkkkkkkkkk.",
    ".kkkwwkkkkkkkkk.",
    ".kkkkwwkkkkkkkk.",
    ".kkkkwwkkkkkkkk.",
    ".kkkwwkkkkkkkkk.",
    ".kkwwkkkkkkkkkk.",
    ".kkkkkkkkkkkkkk.",
    ".kkwwwwwwwwkkkk.",
    ".kkkkkkkkkkkkkk.",
    ".kkkkkkkkkkkkkk.",
    "..kkkkkkkkkkkk..",
    "................",
} };

/* Dossier : la forme classique, ambre, avec un onglet plus clair. */
static const FxIcon ICONE_FICHIERS = { 0, {
    "................",
    "................",
    "..aaaaaaa.......",
    ".aaaaaaaaaaaa...",
    ".aaaaaaddaaaaa..",
    ".aaaaaddddaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    ".aaaaaaaaaaaaaa.",
    "..aaaaaaaaaaaa..",
    "................",
    "................",
} };

/* Reglages : engrenage blanc sur carre bleu. */
static const FxIcon ICONE_REGLAGES = { 0x0078D4, {
    "................",
    "....ww....ww....",
    "...wwwwwwwwww...",
    "..wwwwwwwwwwww..",
    ".wwwwwwwwwwwwww.",
    ".wwww......wwww.",
    "wwwww......wwwww",
    "wwww........wwww",
    "wwww........wwww",
    "wwwww......wwwww",
    ".wwww......wwww.",
    ".wwwwwwwwwwwwww.",
    "..wwwwwwwwwwww..",
    "...wwwwwwwwww...",
    "....ww....ww....",
    "................",
} };

/* Bloc-notes : page claire, lignes de texte, en-tete colore. */
static const FxIcon ICONE_EDITEUR = { 0xFFFFFF, {
    "................",
    ".eeeeeeeeeeeeee.",
    ".e............e.",
    ".ebbbbbbbbbbbbe.",
    ".e............e.",
    ".e.gggggggggg.e.",
    ".e.gggggggggg.e.",
    ".e............e.",
    ".e.gggggggggg.e.",
    ".e.gggggggggg.e.",
    ".e............e.",
    ".e.gggggggggg.e.",
    ".e.gggggggggg.e.",
    ".e............e.",
    ".eeeeeeeeeeeeee.",
    "................",
} };

/* Fichier : page grise avec coin plie. */
static const FxIcon ICONE_FICHIER = { 0xFFFFFF, {
    "................",
    "..ggggggggggg...",
    "..gggggggggggg..",
    "..ggggggggggge..",
    "..gggggggggge...",
    "..gggggggggee...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "..ggggggggggg...",
    "................",
} };

/* Eteindre : symbole power blanc sur carre rouge. */
static const FxIcon ICONE_POWER = { 0xC42B1C, {
    "................",
    "......wwww......",
    "......wwww......",
    "......wwww......",
    "...www....www...",
    "..ww........ww..",
    "..ww........ww..",
    ".ww..........ww.",
    ".ww..........ww.",
    ".ww..........ww.",
    "..ww........ww..",
    "..ww........ww..",
    "...www....www...",
    ".....wwwwww.....",
    "................",
    "................",
} };

/* Application sans icone dediee : une fenetre neutre. */
static const FxIcon ICONE_GENERIQUE = { 0xE5E5E5, {
    "................",
    ".eeeeeeeeeeeeee.",
    ".egggggggggggge.",
    ".e............e.",
    ".e............e.",
    ".e....gggg....e.",
    ".e....gggg....e.",
    ".e....gggg....e.",
    ".e....gggg....e.",
    ".e............e.",
    ".e............e.",
    ".e............e.",
    ".e............e.",
    ".eeeeeeeeeeeeee.",
    "................",
    "................",
} };

static const FxIcon *choisir(int kind) {
    switch (kind) {
        case FX_ICON_TERMINAL: return &ICONE_TERMINAL;
        case FX_ICON_FILES:    return &ICONE_FICHIERS;
        case FX_ICON_SETTINGS: return &ICONE_REGLAGES;
        case FX_ICON_EDITOR:   return &ICONE_EDITEUR;
        case FX_ICON_FILE:     return &ICONE_FICHIER;
        case FX_ICON_POWER:    return &ICONE_POWER;
        default:               return &ICONE_GENERIQUE;
    }
}

/* ---------------------------------------------------------------- dessin */

void fx_app_icon(FXScreen *s, int x, int y, int sz, int kind) {
    if (sz < 8) return;
    const FxIcon *ic = choisir(kind);

    /* on reste sur des blocs entiers : c'est le pixel art qui veut ca */
    int cellule = sz / TAILLE_GRILLE;
    if (cellule < 1) cellule = 1;
    int cote = cellule * TAILLE_GRILLE;
    int ox = x + (sz - cote) / 2;
    int oy = y + (sz - cote) / 2;

    /* carre de fond arrondi (facultatif) */
    if (ic->fond) {
        int rayon = cote / 5;
        fx_fill_round_rect(s, ox, oy, cote, cote, rayon, ic->fond);
    }

    /* les pixels */
    for (int ly = 0; ly < TAILLE_GRILLE; ly++) {
        const char *ligne = ic->px[ly];
        if (!ligne) continue;
        for (int lx = 0; lx < TAILLE_GRILLE; lx++) {
            char c = ligne[lx];
            if (c == '\0' || c == '.') continue;
            uint32_t coul = lettre_couleur(c);
            if (!coul) continue;
            fx_fill_rect(s, ox + lx * cellule, oy + ly * cellule, cellule, cellule, coul);
        }
    }
}

/* ---------------------------------------------------------------- nom -> icone */

static char sans_accent(unsigned char c) {
    switch (c) {
        case 0xC3: return 0;   /* premier octet des accents UTF-8 : ignore */
        default:   return (c >= 0x80) ? 0 : (char)c;
    }
}

int fx_app_kind(const char *name) {
    if (!name) return FX_ICON_GENERIC;
    char bas[64];
    int n = 0;
    for (const char *p = name; *p && n < 63; p++) {
        char c = sans_accent((unsigned char)*p);
        if (!c) continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        bas[n++] = c;
    }
    bas[n] = 0;

    /* Attention aux accents : ils sont en UTF-8, donc "Réglages" arrive ici
       sous la forme "rglages" (l'octet d'accent est retire). On cherche donc
       des bouts de mot qui survivent a l'accentuation. */
    if (strstr(bas, "termin") || strstr(bas, "console") || strstr(bas, "shell")) return FX_ICON_TERMINAL;
    if (strstr(bas, "fichier") || strstr(bas, "dossier") || strstr(bas, "files")) return FX_ICON_FILES;
    if (strstr(bas, "glages") || strstr(bas, "reglage") || strstr(bas, "parametre") ||
        strstr(bas, "settings") || strstr(bas, "configuration")) return FX_ICON_SETTINGS;
    if (strstr(bas, "diteur") || strstr(bas, "editeur") || strstr(bas, "note") ||
        strstr(bas, "texte") || strstr(bas, "editor")) return FX_ICON_EDITOR;
    if (strstr(bas, "teindre") || strstr(bas, "eteindre") || strstr(bas, "power") ||
        strstr(bas, "arret") || strstr(bas, "reboot")) return FX_ICON_POWER;
    if (strstr(bas, "document") || strstr(bas, "rapport")) return FX_ICON_FILE;
    return FX_ICON_GENERIC;
}
