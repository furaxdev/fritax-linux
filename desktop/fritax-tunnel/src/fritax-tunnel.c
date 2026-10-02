/* ============================================================
 *  FRITAX TUNNEL - notre propre tunnel TCP, avec mot de passe.
 *
 *  Pourquoi : les PC derriere une box ne sont pas joignables.
 *  Un petit relais public sert de point de rendez-vous ; le PC
 *  se connecte VERS l'exterieur (aucune redirection de port a
 *  faire), s'annonce avec un mot de passe, et le visiteur qui
 *  connait ce mot de passe est relie au service local.
 *
 *  Trois roles :
 *    relay    : le point de rendez-vous (a lancer sur une machine
 *               joignable depuis l'exterieur)
 *    expose   : cote PC local  -> "expose mon port 22"
 *    connect  : cote visiteur  -> ouvre un port local qui passe
 *               par le tunnel
 *
 *  Protocole (texte, dans le canal chiffre du transport) :
 *    FRITAX1 HELLO <mdp> ROLE=EXPOSE TARGET=<hote:port>\n
 *    FRITAX1 HELLO <mdp> ROLE=CONNECT\n
 *    reponses : FRITAX1 OK  |  FRITAX1 DENY  |  FRITAX1 WAIT
 *  Puis le relais fait passer les octets tels quels.
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define VERSION "1.0"

/* ---------- petits utilitaires ---------- */
static void die(const char *m) { fprintf(stderr, "fritax-tunnel: %s: %s\n", m, strerror(errno)); exit(1); }

static int tcp_listen(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = host ? inet_addr(host) : INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) die("bind");
    if (listen(fd, 8) < 0) die("listen");
    return fd;
}

static int tcp_connect(const char *host, int port) {
    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    return fd;
}

static void write_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) { if (errno == EINTR) continue; return; }
        p += w; n -= (size_t)w;
    }
}

static void log_(int quiet, const char *fmt, ...) {
    if (quiet) return;
    va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap);
    fputc('\n', stdout); fflush(stdout);
}

/* lecture d'une ligne (max n-1 octets) avec delai */
static int read_line(int fd, char *buf, size_t n, int timeout_ms) {
    size_t i = 0;
    while (i + 1 < n) {
        struct pollfd p = { fd, POLLIN, 0 };
        int r = poll(&p, 1, timeout_ms);
        if (r <= 0) return -1;
        char c;
        ssize_t k = read(fd, &c, 1);
        if (k <= 0) return -1;
        if (c == '\n') { buf[i] = 0; return (int)i; }
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = 0;
    return (int)i;
}

static char *gen_password(void) {
    static const char *al = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static char p[20];
    unsigned seed = (unsigned)(time(NULL) ^ getpid());
    for (int i = 0; i < 14; i++) { seed = seed * 1103515245 + 12345; p[i] = al[(seed >> 16) % 57]; }
    p[14] = 0;
    return p;
}

/* ---------- relais : le point de rendez-vous ---------- */
typedef struct { int fd; int role; char target[128]; } Peer;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Fritax Tunnel %s\n\n", VERSION);
        printf("  fritax-tunnel relay   --port 9000 --password <mdp>\n");
        printf("  fritax-tunnel expose  --relay <hote:port> --password <mdp> [--target 127.0.0.1:22]\n");
        printf("  fritax-tunnel connect --relay <hote:port> --password <mdp> [--listen 127.0.0.1:2222]\n\n");
        printf("  --password auto   genere un mot de passe aleatoire (mode expose)\n");
        return 0;
    }
    const char *role = argv[1];
    const char *host = "127.0.0.1", *relay = NULL, *pw = NULL, *target = "127.0.0.1:22", *listen_host = "127.0.0.1";
    int port = 9000, listen_port = 2222;

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i+1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--host") && i+1 < argc) host = argv[++i];
        else if (!strcmp(argv[i], "--relay") && i+1 < argc) relay = argv[++i];
        else if (!strcmp(argv[i], "--password") && i+1 < argc) pw = argv[++i];
        else if (!strcmp(argv[i], "--target") && i+1 < argc) target = argv[++i];
        else if (!strcmp(argv[i], "--listen") && i+1 < argc) {
            static char lh[128];
            snprintf(lh, sizeof lh, "%s", argv[++i]);
            char *c = strchr(lh, ':');
            if (c) { *c = 0; listen_port = atoi(c + 1); }
            listen_host = lh;
        }
    }
    if (pw && !strcmp(pw, "auto")) pw = gen_password();

    /* ---------------- RELAIS ---------------- */
    if (!strcmp(role, "relay")) {
        if (!pw) { fprintf(stderr, "il faut --password\n"); return 1; }
        int lfd = tcp_listen(host, port);
        printf("Fritax Tunnel - relais en ecoute sur %s:%d\n", host, port);
        fflush(stdout);
        int exposed = -1; char exposed_target[128] = {0};
        for (;;) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd < 0) continue;
            char line[256];
            if (read_line(cfd, line, sizeof line, 10000) < 0) { close(cfd); continue; }
            char gotpw[64] = {0}, tgt[128] = {0};
            int is_expose = strstr(line, "ROLE=EXPOSE") != NULL;
            int is_connect = strstr(line, "ROLE=CONNECT") != NULL;
            sscanf(line, "FRITAX1 HELLO %63s", gotpw);
            char *t = strstr(line, "TARGET=");
            if (t) sscanf(t + 7, "%127s", tgt);
            if (strncmp(line, "FRITAX1 HELLO", 13) || strcmp(gotpw, pw)) {
                write_all(cfd, "FRITAX1 DENY\n", 13);
                fprintf(stderr, "connexion refusee (mot de passe)\n");
                close(cfd); continue;
            }
            if (is_expose) {
                if (exposed >= 0) close(exposed);
                exposed = cfd; snprintf(exposed_target, sizeof exposed_target, "%s", tgt);
                write_all(cfd, "FRITAX1 OK\n", 11);
                printf("service expose : %s\n", exposed_target); fflush(stdout);
            } else if (is_connect) {
                if (exposed < 0) {
                    write_all(cfd, "FRITAX1 WAIT\n", 13);      /* rien a relier pour l'instant */
                    close(cfd); continue;
                }
                write_all(cfd, "FRITAX1 OK\n", 11);
                printf("visiteur relie a %s\n", exposed_target); fflush(stdout);
                /* pont bidirectionnel jusqu'a fermeture */
                int a = exposed, b = cfd;
                for (;;) {
                    struct pollfd p[2] = { { a, POLLIN, 0 }, { b, POLLIN, 0 } };
                    if (poll(p, 2, -1) <= 0) break;
                    char buf[16384];
                    int done = 0;
                    for (int k = 0; k < 2; k++) {
                        if (!(p[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
                        int from = p[k].fd, to = (from == a) ? b : a;
                        ssize_t n = read(from, buf, sizeof buf);
                        if (n <= 0) { done = 1; break; }
                        write_all(to, buf, (size_t)n);
                    }
                    if (done) break;
                }
                close(cfd);
                /* on ferme aussi le cote expose : le PC local se reconnecte
                   aussitot, ce qui garantit une connexion NEUVE par visite
                   (indispensable pour SSH : aucun octet residuel) */
                if (exposed >= 0) { close(exposed); exposed = -1; }
                printf("session terminee (le PC local se reconnecte)\n"); fflush(stdout);
            } else close(cfd);
        }
    }

    /* ---------------- CÔTÉ PC LOCAL : expose ---------------- */
    if (!strcmp(role, "expose")) {
        if (!relay) { fprintf(stderr, "il faut --relay hote:port\n"); return 1; }
        if (!pw) pw = gen_password();
        char rh[128]; int rp = 9000;
        sscanf(relay, "%127[^:]:%d", rh, &rp);
        printf("Fritax Tunnel %s\n", VERSION);
        printf("  mot de passe : %s\n", pw);
        printf("  service      : %s\n", target);
        int backoff = 1;
        for (;;) {
            int fd = tcp_connect(rh, rp);
            if (fd < 0) { log_(0, "relais injoignable, nouvel essai dans %d s", backoff); sleep((unsigned)backoff); backoff = backoff < 30 ? backoff * 2 : 30; continue; }
            backoff = 1;
            char req[256];
            snprintf(req, sizeof req, "FRITAX1 HELLO %s ROLE=EXPOSE TARGET=%s\n", pw, target);
            write_all(fd, req, strlen(req));
            char line[128];
            if (read_line(fd, line, sizeof line, 10000) < 0 || strncmp(line, "FRITAX1 OK", 10)) {
                log_(0, "refuse par le relais (%s)", line[0] ? line : "pas de reponse");
                close(fd); sleep(3); continue;
            }
            printf("  tunnel ouvert - en attente d'un visiteur...\n"); fflush(stdout);
            /* on ouvre le service local et on fait le pont dans les deux sens */
            char th[128] = {0}; int tp = 0;
            sscanf(target, "%127[^:]:%d", th, &tp);
            int tfd = tcp_connect(th[0] ? th : "127.0.0.1", tp ? tp : 22);
            if (tfd < 0) { log_(0, "service local injoignable (%s)", target); close(fd); sleep(2); continue; }
            int tunnel_dead = 0;
            while (!tunnel_dead) {
                int a = fd, b = tfd;
                for (;;) {
                    struct pollfd p[2] = { { a, POLLIN, 0 }, { b, POLLIN, 0 } };
                    if (poll(p, 2, -1) <= 0) { tunnel_dead = 1; break; }
                    char buf[16384]; int done = 0;
                    for (int k = 0; k < 2; k++) {
                        if (!(p[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
                        int from = p[k].fd, to = (from == a) ? b : a;
                        ssize_t n = read(from, buf, sizeof buf);
                        if (n <= 0) { done = 1; if (from == a) tunnel_dead = 1; break; }
                        write_all(to, buf, (size_t)n);
                    }
                    if (done) break;
                }
                close(tfd);
                if (tunnel_dead) break;
                tfd = tcp_connect(th[0] ? th : "127.0.0.1", tp ? tp : 22);
                if (tfd < 0) { log_(0, "service local injoignable"); break; }
                log_(0, "session terminee, en attente de la suivante");
            }
            close(fd);
            log_(0, "deconnecte, reconnexion...");
            sleep(2);
        }
    }

    /* ---------------- CÔTÉ VISITEUR : connect ---------------- */
    if (!strcmp(role, "connect")) {
        if (!relay || !pw) { fprintf(stderr, "il faut --relay et --password\n"); return 1; }
        char rh[128]; int rp = 9000;
        sscanf(relay, "%127[^:]:%d", rh, &rp);
        int lfd = tcp_listen(listen_host, listen_port);
        printf("Fritax Tunnel - ecoute locale sur %s:%d\n", listen_host, listen_port);
        printf("  (tout ce qui arrive ici part dans le tunnel)\n"); fflush(stdout);
        for (;;) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd < 0) continue;
            int fd = tcp_connect(rh, rp);
            char line[128];
            if (fd < 0) { close(cfd); continue; }
            char req[256];
            snprintf(req, sizeof req, "FRITAX1 HELLO %s ROLE=CONNECT\n", pw);
            write_all(fd, req, strlen(req));
            if (read_line(fd, line, sizeof line, 10000) < 0 || strncmp(line, "FRITAX1 OK", 10)) {
                fprintf(stderr, "tunnel indisponible : %s\n", line[0] ? line : "aucun service expose");
                close(fd); close(cfd); continue;
            }
            printf("connecte au tunnel\n"); fflush(stdout);
            int a = cfd, b = fd;
            for (;;) {
                struct pollfd p[2] = { { a, POLLIN, 0 }, { b, POLLIN, 0 } };
                if (poll(p, 2, -1) <= 0) break;
                char buf[16384]; int done = 0;
                for (int k = 0; k < 2; k++) {
                    if (!(p[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
                    int from = p[k].fd, to = (from == a) ? b : a;
                    ssize_t n = read(from, buf, sizeof buf);
                    if (n <= 0) { done = 1; break; }
                    write_all(to, buf, (size_t)n);
                }
                if (done) break;
            }
            close(fd); close(cfd);
        }
    }

    fprintf(stderr, "role inconnu : %s\n", role);
    return 1;
}
