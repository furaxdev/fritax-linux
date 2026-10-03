/* ============================================================
 *  FRITAX LOGIN - coeur (etat + dessin)
 *  Aucune dependance hors de screen.h : testable et affichable
 *  sans ecran (voir tests/ et le mode apercu du backend X11).
 *  Charte Fritax : violet #6b5bf5, cyan #38e1ff, magenta #ff3a9d,
 *  coins lisses, formes pleines, police bitmap 8x16.
 * ============================================================ */
#include "login.h"
#include "input.h"      /* FXK_* */
#include <stdio.h>
#include <string.h>
#include <time.h>

/* --- couleurs de la charte --- */
#define C_FOND_HAUT   fx_rgb(0x1b, 0x10, 0x30)
#define C_FOND_BAS    fx_rgb(0x0a, 0x07, 0x14)
#define C_CARTE       fx_rgb(0x14, 0x11, 0x22)
#define C_BORDURE     fx_rgb(0x3a, 0x2f, 0x5c)
#define C_VIOLET      fx_rgb(0x6b, 0x5b, 0xf5)
#define C_CYAN        fx_rgb(0x38, 0xe1, 0xff)
#define C_MAGENTA     fx_rgb(0xff, 0x3a, 0x9d)
#define C_TEXTE       fx_rgb(0xf4, 0xf2, 0xff)
#define C_GRIS        fx_rgb(0x8d, 0x86, 0xa8)
#define C_CHAMP       fx_rgb(0x0e, 0x0c, 0x1a)
#define C_ROUGE       fx_rgb(0xff, 0x5a, 0x7a)

/* --- geometrie (calculee depuis la taille de l'ecran) --- */
typedef struct {
    int cx, cy;              /* centre */
    int carte_w, carte_h;
    int carte_x, carte_y;
    int champ_w, champ_h;
    int user_x, user_y;
    int pass_x, pass_y;
    int btn_x, btn_y, btn_w, btn_h;
} Geo;

static void geo(FXLogin *l, Geo *g) {
    g->carte_w = 520; g->carte_h = 420;
    g->cx = l->w / 2; g->cy = l->h / 2;
    g->carte_x = g->cx - g->carte_w / 2;
    g->carte_y = g->cy - g->carte_h / 2;
    g->champ_w = 340; g->champ_h = 38;
    g->user_x = g->cx - g->champ_w / 2;
    g->user_y = g->carte_y + 210;
    g->pass_x = g->user_x;
    g->pass_y = g->user_y + 76;
    g->btn_w = 200; g->btn_h = 44;
    g->btn_x = g->cx - g->btn_w / 2;
    g->btn_y = g->pass_y + 74;
}

/* ---------------------------------------------------------------
 *  Initialisation
 * --------------------------------------------------------------- */
void fx_login_init(FXLogin *l, int w, int h) {
    memset(l, 0, sizeof(*l));
    l->w = w; l->h = h;
    l->champ = FXL_CHAMP_UTILISATEUR;
    l->etat = FXL_EN_COURS;
    l->curseur = 0;
    strcpy(l->utilisateur, "furax");
    strcpy(l->message, "Entree = se connecter    Tab = changer de champ");
}

/* ---------------------------------------------------------------
 *  Verification du couple utilisateur / mot de passe
 *  (ecran d'accueil de la distribution : le compte est documente
 *   dans LIVRAISON.md ; le mot de passe est celui du root, defini
 *   par BR2_TARGET_GENERIC_ROOT_PASSWD.)
 * --------------------------------------------------------------- */
int fx_login_verifie(const char *utilisateur, const char *motdepasse) {
    if (!utilisateur || !motdepasse) return 0;
    int bon_nom = (!strcmp(utilisateur, "furax") || !strcmp(utilisateur, "root")
                   || !strcmp(utilisateur, "fritaxdev") || !strcmp(utilisateur, "fritax"));
    int bon_mdp = !strcmp(motdepasse, "fritax");
    return bon_nom && bon_mdp;
}

const char *fx_login_session(void) { return "fritax-shell"; }

/* ---------------------------------------------------------------
 *  Clavier
 * --------------------------------------------------------------- */
static void ajoute(char *champ, int taille, int ascii) {
    size_t n = strlen(champ);
    if (ascii >= 32 && ascii < 127 && n + 1 < (size_t)taille) {
        champ[n] = (char)ascii;
        champ[n + 1] = 0;
    }
}
static void retire(char *champ) {
    size_t n = strlen(champ);
    if (n) champ[n - 1] = 0;
}

static void fx_login_valide(FXLogin *l) {
    l->tentatives++;
    if (fx_login_verifie(l->utilisateur, l->motdepasse)) {
        l->etat = FXL_CONNECTE;
        strcpy(l->message, "Connexion... ouverture du bureau");
    } else {
        l->etat = FXL_REFUSE;
        strcpy(l->message, "Mot de passe incorrect. Ecris encore.");
        memset(l->motdepasse, 0, sizeof(l->motdepasse));
        l->champ = FXL_CHAMP_MOTDEPASSE;
    }
}

/* Validation, partagee par le clavier et la souris (une seule logique). */
static void fx_login_valide(FXLogin *l);

void fx_login_touche(FXLogin *l, int touche, int ascii) {
    if (l->etat == FXL_CONNECTE) return;          /* deja connecte : on ne touche plus a rien */

    if (touche == FXK_TAB) {
        l->champ = (l->champ == FXL_CHAMP_UTILISATEUR) ? FXL_CHAMP_MOTDEPASSE
                                                       : FXL_CHAMP_UTILISATEUR;
        l->etat = FXL_EN_COURS;
        return;
    }
    if (touche == FXK_ESC) {
        memset(l->utilisateur, 0, sizeof(l->utilisateur));
        l->champ = FXL_CHAMP_UTILISATEUR;
        l->etat = FXL_EN_COURS;
        strcpy(l->message, "Pseudo efface : ecris ton nom puis Entree");
        return;
    }
    if (touche == FXK_BACKSPACE) {
        if (l->champ == FXL_CHAMP_UTILISATEUR)      retire(l->utilisateur);
        else if (l->champ == FXL_CHAMP_MOTDEPASSE)  retire(l->motdepasse);
        l->etat = FXL_EN_COURS;
        return;
    }
    if (touche == FXK_ENTER) {
        /* tant que le mot de passe est vide, Entree sert a changer de champ */
        if (l->champ == FXL_CHAMP_UTILISATEUR && l->motdepasse[0] == 0) {
            l->champ = FXL_CHAMP_MOTDEPASSE;
            strcpy(l->message, "Mot de passe puis Entree");
            return;
        }
        fx_login_valide(l);
        return;
    }
    if (touche == FXK_NONE && ascii >= 32 && ascii < 127) {
        if (l->champ == FXL_CHAMP_BOUTON) l->champ = FXL_CHAMP_MOTDEPASSE;
        if (l->champ == FXL_CHAMP_UTILISATEUR)
            ajoute(l->utilisateur, (int)sizeof(l->utilisateur), ascii);
        else
            ajoute(l->motdepasse, (int)sizeof(l->motdepasse), ascii);
        l->etat = FXL_EN_COURS;
    }
}

/* ---------------------------------------------------------------
 *  Souris
 * --------------------------------------------------------------- */
void fx_login_souris(FXLogin *l, int x, int y, int enfonce) {
    Geo g; geo(l, &g);
    int dans_btn = (x >= g.btn_x && x <= g.btn_x + g.btn_w &&
                    y >= g.btn_y && y <= g.btn_y + g.btn_h);
    l->survol = dans_btn ? 1 : 0;

    if (!enfonce) return;                    /* on agit au clic */

    if (dans_btn) {
        l->champ = FXL_CHAMP_BOUTON;
        fx_login_valide(l);
        return;
    }
    if (x >= g.user_x && x <= g.user_x + g.champ_w &&
        y >= g.user_y && y <= g.user_y + g.champ_h) {
        l->champ = FXL_CHAMP_UTILISATEUR;
    } else if (x >= g.pass_x && x <= g.pass_x + g.champ_w &&
               y >= g.pass_y && y <= g.pass_y + g.champ_h) {
        l->champ = FXL_CHAMP_MOTDEPASSE;
    }
}

/* ---------------------------------------------------------------
 *  Dessin
 * --------------------------------------------------------------- */
static void centre_texte(FXScreen *s, int cx, int y, const char *txt, uint32_t c, int echelle) {
    int larg = (int)strlen(txt) * FX_CELL_W * echelle;
    fx_draw_text_scale(s, cx - larg / 2, y, txt, c, echelle);
}

static void champ(FXScreen *s, Geo *g, int x, int y, const char *label,
                  const char *valeur, int actif, int masque, int curseur) {
    fx_draw_text(s, x, y - 22, label, actif ? C_CYAN : C_GRIS);

    /* anneau de focus */
    if (actif) fx_fill_round_rect(s, x - 3, y - 3, g->champ_w + 6, g->champ_h + 6, 11, C_CYAN);
    fx_fill_round_rect(s, x, y, g->champ_w, g->champ_h, 9, C_CHAMP);

    /* valeur (masquee pour le mot de passe) */
    char aff[128];
    if (masque) {
        size_t n = strlen(valeur);
        if (n > sizeof(aff) - 1) n = sizeof(aff) - 1;
        memset(aff, '*', n);
        aff[n] = 0;
    } else {
        strncpy(aff, valeur, sizeof(aff) - 1);
        aff[sizeof(aff) - 1] = 0;
    }
    fx_draw_text(s, x + 12, y + (g->champ_h - FX_CELL_H) / 2, aff, C_TEXTE);

    /* curseur clignotant */
    if (actif && curseur) {
        int cx = x + 12 + (int)strlen(aff) * FX_CELL_W;
        fx_fill_rect(s, cx + 1, y + 8, 2, g->champ_h - 16, C_CYAN);
    }
}

void fx_login_dessine(FXLogin *l, FXScreen *s) {
    Geo g; geo(l, &g);

    /* fond : degrade violet -> noir + halo magenta en haut */
    fx_gradient_v(s, 0, 0, s->w, s->h, C_FOND_HAUT, C_FOND_BAS);
    fx_blend_round_rect(s, g.cx - 420, -260, 840, 420, 210, C_MAGENTA, 26);
    fx_blend_round_rect(s, g.cx - 260, -180, 520, 300, 150, C_VIOLET, 30);

    /* ombre puis carte */
    fx_blend_round_rect(s, g.carte_x + 8, g.carte_y + 10, g.carte_w, g.carte_h, 20, fx_rgb(0, 0, 0), 120);
    fx_fill_round_rect(s, g.carte_x - 2, g.carte_y - 2, g.carte_w + 4, g.carte_h + 4, 21, C_BORDURE);
    fx_fill_round_rect(s, g.carte_x, g.carte_y, g.carte_w, g.carte_h, 20, C_CARTE);

    /* titre */
    centre_texte(s, g.cx, g.carte_y + 34, "FRITAX", C_TEXTE, 4);
    fx_fill_round_rect(s, g.cx - 46, g.carte_y + 34 + FX_CELL_H * 4 + 8, 92, 4, 2, C_CYAN);
    centre_texte(s, g.cx, g.carte_y + 34 + FX_CELL_H * 4 + 24, "LINUX 1.0  -  NOVA", C_GRIS, 1);

    /* champs */
    champ(s, &g, g.user_x, g.user_y, "Utilisateur", l->utilisateur,
          l->champ == FXL_CHAMP_UTILISATEUR, 0, l->curseur);
    champ(s, &g, g.pass_x, g.pass_y, "Mot de passe", l->motdepasse,
          l->champ == FXL_CHAMP_MOTDEPASSE, 1, l->curseur);

    /* bouton */
    uint32_t fond = C_VIOLET;
    if (l->champ == FXL_CHAMP_BOUTON || l->survol) fond = fx_rgb(0x84, 0x76, 0xff);
    if (l->etat == FXL_REFUSE) fond = fx_rgb(0x8c, 0x24, 0x46);
    fx_blend_round_rect(s, g.btn_x + 4, g.btn_y + 6, g.btn_w, g.btn_h, 12, fx_rgb(0, 0, 0), 110);
    fx_fill_round_rect(s, g.btn_x, g.btn_y, g.btn_w, g.btn_h, 12, fond);
    /* reflet clair en haut du bouton (volume, sans degrade) */
    fx_blend_round_rect(s, g.btn_x + 3, g.btn_y + 3, g.btn_w - 6, 8, 4, fx_rgb(255, 255, 255), 40);
    centre_texte(s, g.cx, g.btn_y + (g.btn_h - FX_CELL_H) / 2, "Se connecter", C_TEXTE, 1);

    /* message */
    uint32_t cm = C_GRIS;
    if (l->etat == FXL_REFUSE) cm = C_ROUGE;
    if (l->etat == FXL_CONNECTE) cm = C_CYAN;
    centre_texte(s, g.cx, g.btn_y + g.btn_h + 26, l->message, cm, 1);
    if (l->etat == FXL_REFUSE) {
        char essai[64];
        snprintf(essai, sizeof(essai), "tentative %d", l->tentatives);
        centre_texte(s, g.cx, g.btn_y + g.btn_h + 48, essai, C_GRIS, 1);
    }

    /* pied : horloge + nom de la machine */
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    char hor[64];
    strftime(hor, sizeof(hor), "%d/%m/%Y  %H:%M", tmv);
    int larg = (int)strlen(hor) * FX_CELL_W;
    fx_draw_text(s, s->w - larg - 24, 20, hor, C_GRIS);
    fx_draw_text(s, 24, 20, "fritax-linux", C_GRIS);
    fx_draw_text(s, 24, s->h - FX_CELL_H - 20, "Fritax Linux 1.0 (Nova)", C_GRIS);
    const char *aide = "Ecrit ton mot de passe puis Entree";
    fx_draw_text(s, s->w - (int)strlen(aide) * FX_CELL_W - 24, s->h - FX_CELL_H - 20, aide, C_GRIS);
}
