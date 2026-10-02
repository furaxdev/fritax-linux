/* ============================================================
 *  FRITAX IPC - implementation (prise Unix locale)
 * ============================================================ */
#define _GNU_SOURCE
#include "ipc.h"
#include "wm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

static int srv_fd = -1;
static char chemin[256] = FX_IPC_PATH;

const char *fx_ipc_path(void) { return chemin; }

void fx_ipc_init(const char *path) {
    if (path && *path) snprintf(chemin, sizeof chemin, "%s", path);
    unlink(chemin);                                   /* une ancienne prise peut trainer */
    srv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv_fd < 0) return;
    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", chemin);
    if (bind(srv_fd, (struct sockaddr *)&a, sizeof a) != 0) { close(srv_fd); srv_fd = -1; return; }
    listen(srv_fd, 8);
    chmod(chemin, 0666);                              /* tout le monde peut demander une fenetre */
    int fl = fcntl(srv_fd, F_GETFL, 0);
    fcntl(srv_fd, F_SETFL, fl | O_NONBLOCK);
    fprintf(stderr, "fritax: service de fenetres pret (%s)\n", chemin);
}

/* "open <app>" ou "open <app> <fichier>" -> ouvre la bonne fenetre du bureau */
static void traiter(const char *req, int fd) {
    char quoi[64] = {0}, arg[256] = {0};
    if (sscanf(req, "open %63s %255[^\n]", quoi, arg) < 1) {
        ssize_t w = write(fd, "non\n", 4); (void)w;
        return;
    }
    int kind = -1;
    if (!strcmp(quoi, "terminal") || !strcmp(quoi, "console")) kind = FX_APP_TERMINAL;
    else if (!strcmp(quoi, "fichiers") || !strcmp(quoi, "files")) kind = FX_APP_FILES;
    else if (!strcmp(quoi, "r\351glages") || !strcmp(quoi, "settings")) kind = FX_APP_SETTINGS;
    else if (!strcmp(quoi, "bloc") || !strcmp(quoi, "editeur") || !strcmp(quoi, "editor")) kind = FX_APP_EDITOR;
    if (kind < 0) { ssize_t w = write(fd, "non\n", 4); (void)w; return; }
    const char *titre = kind == FX_APP_TERMINAL ? "Terminal" : kind == FX_APP_FILES ? "Fichiers"
                        : kind == FX_APP_SETTINGS ? "R\351glages" : "Bloc-notes";
    int w = fx_wm_open(kind, titre);
    if (w >= 0 && arg[0]) fx_wm_open_file(w, arg);
    ssize_t n = write(fd, w >= 0 ? "ok\n" : "plein\n", w >= 0 ? 3 : 6);
    (void)n;
}

void fx_ipc_poll(void) {
    if (srv_fd < 0) return;
    for (;;) {
        int c = accept(srv_fd, NULL, NULL);
        if (c < 0) return;
        char buf[512];
        ssize_t n = read(c, buf, sizeof buf - 1);
        if (n > 0) { buf[n] = 0; traiter(buf, c); }
        close(c);
    }
}

