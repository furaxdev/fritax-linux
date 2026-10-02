/* ============================================================
 *  fritax-open - demander au bureau Fritax d'ouvrir une fenetre
 *  usage : fritax-open terminal|fichiers|reglages|bloc [fichier]
 *  Si aucun bureau n'ecoute, on lance l'application autonome.
 * ============================================================ */
#define _GNU_SOURCE
#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage : fritax-open terminal|fichiers|reglages|bloc [fichier]\n");
        return 2;
    }
    const char *quoi = argv[1];
    char req[600];
    snprintf(req, sizeof req, "open %s%s%s", quoi, argc > 2 ? " " : "", argc > 2 ? argv[2] : "");
    if (fx_ipc_ask(req) == 0) return 0;

    /* pas de bureau : on lance le programme autonome s'il existe */
    const char *app = NULL;
    if (!strcmp(quoi, "terminal") || !strcmp(quoi, "console")) app = "/usr/bin/fritax-terminal";
    else if (!strcmp(quoi, "fichiers") || !strcmp(quoi, "files")) app = "/usr/bin/fritax-files";
    if (app && access(app, X_OK) == 0) { execl(app, app, (char *)NULL); }
    fprintf(stderr, "fritax-open : le bureau Fritax ne repond pas\n");
    return 1;
}
