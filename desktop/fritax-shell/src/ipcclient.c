/* ============================================================
 *  FRITAX IPC - cote client (aucune dependance : juste un socket)
 * ============================================================ */
#define _GNU_SOURCE
#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int fx_ipc_ask(const char *requete) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", FX_IPC_PATH);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) { close(fd); return -1; }
    char buf[64] = {0};
    ssize_t w = write(fd, requete, strlen(requete));
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    (void)w;
    return (n > 0 && buf[0] == 'o') ? 0 : -1;
}
