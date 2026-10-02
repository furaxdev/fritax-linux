/* ============================================================
 *  Tests du bureau Fritax : gestionnaire de fenetres + terminal PTY
 *  On verifie que le terminal heberge VRAIMENT un shell qui travaille.
 * ============================================================ */
#include "wm.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include "ipc.h"

static int ok = 0, ko = 0;

static void check(const char *nom, int cond) {
    printf("  %s %s\n", cond ? "[OK]" : "[ECHEC]", nom);
    if (cond) ok++; else ko++;
}

static void taper(int win, const char *txt) {
    (void)win;
    for (const char *p = txt; *p; p++) {
        fx_wm_key(UI_KEY_NONE, (unsigned char)*p);
        usleep(3000);
    }
    fx_wm_key(UI_KEY_ENTER, 0);
}

/* attend qu'une chaine apparaisse dans le terminal (au plus ~4 s) */
static int attendre(int win, const char *quoi, char *dernier, int taille) {
    for (int i = 0; i < 200; i++) {
        fx_wm_tick();
        usleep(20000);
        fx_wm_snapshot(win, dernier, taille);
        if (strstr(dernier, quoi)) return 1;
    }
    return 0;
}

int main(void) {
    char snap[8192] = {0};
    printf("=== Tests du gestionnaire de fenetres Fritax ===\n");
    fx_wm_init(1024, 768, "/tmp/fritax-wm-test.log");

    check("aucune fenetre au depart", fx_wm_count() == 0);

    int t = fx_wm_open(FX_APP_TERMINAL, "Terminal");
    check("ouverture d'un terminal", t >= 0);
    check("la fenetre est comptee", fx_wm_count() == 1);

    int pret = attendre(t, "$", snap, sizeof snap);
    check("le shell affiche son invite ($)", pret);

    /* commande dont le RESULTAT ne figure pas dans le texte tape */
    taper(t, "echo A$((2+3))B");
    check("le shell renvoie le resultat d'un calcul (A5B)", attendre(t, "A5B", snap, sizeof snap));

    /* une deuxieme preuve, differente */
    taper(t, "echo fritax-$(uname -m)");
    check("le shell repond avec le nom de la machine", attendre(t, "fritax-x86_64", snap, sizeof snap));

    /* le clavier : la touche Retour arriere doit effacer */
    taper(t, "echo EFFACEX");
    fx_wm_key(UI_KEY_BACKSPACE, 0);
    fx_wm_key(UI_KEY_ENTER, 0);
    check("la touche Retour arriere est transmise au shell", attendre(t, "EFFAC", snap, sizeof snap));

    int f = fx_wm_open(FX_APP_FILES, "Fichiers");
    check("ouverture du gestionnaire de fichiers", f >= 0);
    check("deux fenetres ouvertes", fx_wm_count() == 2);
    int s = fx_wm_open(FX_APP_SETTINGS, "Reglages");
    check("ouverture des reglages", s >= 0);
    check("trois fenetres ouvertes", fx_wm_count() == 3);

    /* --- le service de fenetres : un programme demande, le bureau ouvre --- */
    {
        fx_ipc_init(NULL);
        int avant = fx_wm_count();
        /* demande brute, comme le ferait fritax-open */
        int s = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un a;
        memset(&a, 0, sizeof a);
        a.sun_family = AF_UNIX;
        snprintf(a.sun_path, sizeof a.sun_path, "%s", fx_ipc_path());
        int connecte = (s >= 0 && connect(s, (struct sockaddr *)&a, sizeof a) == 0);
        if (connecte) { ssize_t w = write(s, "open bloc", 9); (void)w; }
        fx_ipc_poll();                       /* le bureau traite la demande */
        if (connecte) close(s);
        check("le service de fenetres repond (fritax-open)", connecte && fx_wm_count() == avant + 1);
    }

    /* --- le bloc-notes : taper puis enregistrer avec Ctrl+S --- */
    {
        const char *h = getenv("HOME");
        char chemin[600];
        snprintf(chemin, sizeof chemin, "%s/document-fritax.txt", h ? h : "/tmp");
        unlink(chemin);
        int e = fx_wm_open(FX_APP_EDITOR, "Bloc-notes");
        check("ouverture du bloc-notes", e >= 0);
        const char *txt = "BONJOUR-FRITAX-SAUVE";
        for (const char *p = txt; *p; p++) fx_wm_key(UI_KEY_NONE, (unsigned char)*p);
        fx_wm_key(UI_KEY_SAVE, 0);
        char lu[512] = {0};
        FILE *f = fopen(chemin, "r");
        if (f) { size_t n = fread(lu, 1, sizeof lu - 1, f); lu[n] = 0; fclose(f); }
        check("Ctrl+S a bien enregistre le texte sur le disque", strstr(lu, txt) != NULL);
        /* on ouvre un fichier existant dans une autre fenetre */
        int e2 = fx_wm_open(FX_APP_EDITOR, "Bloc-notes");
        int charge = fx_wm_open_file(e2, chemin);
        char snap2[4096] = {0};
        fx_wm_snapshot(e2, snap2, sizeof snap2);
        check("on peut ouvrir un fichier existant dans le bloc-notes",
              charge == 0 && strstr(snap2, txt) != NULL);
        unlink(chemin);
    }

    /* --- redimensionner une fenetre en attrapant le coin --- */
    {
        int x = 0, y = 0, w = 0, h = 0, w2 = 0, h2 = 0;
        fx_wm_size(t, &x, &y, &w, &h);
        fx_wm_raise(t);                             /* on met le terminal devant */
        fx_wm_size(t, &x, &y, &w, &h);
        fx_wm_click(x + w - 5, y + h - 5);          /* on attrape le coin bas droite */
        fx_wm_drag(x + w - 5 + 140, y + h - 5 + 90);
        fx_wm_release();
        fx_wm_size(t, NULL, NULL, &w2, &h2);
        printf("      (taille avant %dx%d en %d,%d -> apres %dx%d)\n", w, h, x, y, w2, h2);
        check("on peut redimensionner une fenetre (coin bas droite)", w2 > w + 50 && h2 > h + 40);
        /* la fenetre ne doit pas sortir de l'ecran */
        fx_wm_click(x + w2 - 5, y + h2 - 5);
        fx_wm_drag(5000, 5000);
        fx_wm_release();
        fx_wm_size(t, &x, &y, &w2, &h2);
        check("une fenetre ne peut pas depasser l'ecran", x + w2 <= 1024 && y + h2 <= 768);
    }

    /* --- choisir un fond d'ecran depuis Reglages --- */
    {
        (void)fx_wm_take_wallpaper_change();
        fx_wm_raise(s);
        int x = 0, y = 0, w = 0, h = 0;
        fx_wm_size(s, &x, &y, &w, &h);
        int bx = x + 18 + 152, by = y + 30 + 36;     /* deuxieme miniature */
        fx_wm_click(bx + 20, by + 20);
        int change = fx_wm_take_wallpaper_change();
        const char *chemin = fx_wm_wallpaper_path();
        check("on peut changer le fond d'ecran depuis Reglages",
              change == 1 && strstr(chemin, "fond-2") != NULL);
    }

    printf("\n--- dernier contenu du terminal :\n%s\n", snap);
    printf("\n=== %d reussis, %d echoues ===\n", ok, ko);
    fflush(stdout);
    return ko == 0 ? 0 : 1;
}
