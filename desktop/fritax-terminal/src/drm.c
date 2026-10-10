/* Fritax - implementation DRM/KMS (ioctls du noyau uniquement) */
#define _GNU_SOURCE
#include "drm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <linux/types.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_fourcc.h>

struct FXDrm {
    int fd;
    uint32_t conn_id, crtc_id;
    struct drm_mode_modeinfo mode;
    int w, h;
};

/* ------------------------------------------------------------------------
   La console texte du noyau reste attachee a la carte graphique, et tant
   qu'elle l'est, tout changement d'affichage est refuse : du point de vue du
   noyau, c'est elle qui tient l'ecran.

   La methode standard consiste a detacher la console le temps que le bureau
   dessine, puis a la rattacher en partant pour que le texte redevienne
   utilisable. Sans ce detachement, on obtient un refus permanent, quel que
   soit l'ordre dans lequel on ouvre la carte.
   ------------------------------------------------------------------------ */
static char CHEMIN_CONSOLE[160];

/* declaree plus bas : ces fonctions en ont besoin avant sa definition */
static void logf_(const char *fmt, ...);

static void detacher_console(void) {
    for (int i = 0; i < 8; i++) {
        char nom[192], contenu[160] = {0};
        snprintf(nom, sizeof nom, "/sys/class/vtconsole/vtcon%d/name", i);
        FILE *f = fopen(nom, "r");
        if (!f) continue;
        char *lu = fgets(contenu, sizeof contenu, f);
        fclose(f);
        if (!lu || !strstr(contenu, "frame buffer")) continue;

        snprintf(CHEMIN_CONSOLE, sizeof CHEMIN_CONSOLE,
                 "/sys/class/vtconsole/vtcon%d/bind", i);
        FILE *b = fopen(CHEMIN_CONSOLE, "w");
        if (b) {
            fputs("0", b);
            fclose(b);
            logf_("console texte detachee : la carte est libre pour le bureau");
        }
        return;
    }
    logf_("console texte non trouvee dans /sys/class/vtconsole");
}

static void rattacher_console(void) {
    if (!CHEMIN_CONSOLE[0]) return;
    FILE *b = fopen(CHEMIN_CONSOLE, "w");
    if (b) {
        fputs("1", b);
        fclose(b);
        logf_("console texte rattachee");
    }
    CHEMIN_CONSOLE[0] = 0;
}

/* Qui a la carte ouverte, en ce moment ? C'est ce qui permet de nommer le
   programme fautif dans le journal au lieu de le chercher a l'aveugle. */
static void qui_tient_la_carte(const char *carte) {
    DIR *proc = opendir("/proc");
    if (!proc) return;
    struct dirent *e;
    while ((e = readdir(proc))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char dossier[80];
        snprintf(dossier, sizeof dossier, "/proc/%s/fd", e->d_name);
        DIR *fds = opendir(dossier);
        if (!fds) continue;
        struct dirent *d;
        int trouve = 0;
        while (!trouve && (d = readdir(fds))) {
            char lien[600], cible[600];
            snprintf(lien, sizeof lien, "%s/%s", dossier, d->d_name);
            ssize_t n = readlink(lien, cible, sizeof cible - 1);
            if (n <= 0) continue;
            cible[n] = 0;
            if (strcmp(cible, carte)) continue;
            trouve = 1;
            char cmd[300] = {0}, chemin[80];
            snprintf(chemin, sizeof chemin, "/proc/%s/cmdline", e->d_name);
            FILE *c = fopen(chemin, "r");
            if (c) { size_t l = fread(cmd, 1, sizeof cmd - 1, c); cmd[l] = 0; fclose(c); }
            for (size_t i = 0; i < strlen(cmd); i++) if (cmd[i] == 0) cmd[i] = ' ';
            logf_("  la carte est ouverte par pid %s : %s", e->d_name, cmd);
        }
        closedir(fds);
    }
    closedir(proc);
}

static FILE *GLOG;
void fx_drm_log_path(const char *p) { if (p) GLOG = fopen(p, "w"); }
static void logf_(const char *fmt, ...) {
    if (!GLOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(GLOG, fmt, ap); va_end(ap); fputc('\n', GLOG); fflush(GLOG);
}

/* La carte graphique n'accepte qu'UN seul maitre a la fois : le programme qui
   la detient peut changer l'image, les autres recoivent "Permission denied" a
   chaque tentative. Sans cette demande explicite, on dependait du hasard de
   l'ordre de demarrage — et un ancien bureau reste en vie suffisait a tout
   bloquer, avec un flot de "SETCRTC: Permission denied". */
static int devenir_maitre(int fd) {
    return ioctl(fd, DRM_IOCTL_SET_MASTER, 0);
}

FXDrm *fx_drm_open(const char *card) {
    FXDrm *d = calloc(1, sizeof(FXDrm));
    if (!d) return NULL;
    d->fd = open(card, O_RDWR | O_CLOEXEC);
    if (d->fd >= 0) {
        if (devenir_maitre(d->fd) == 0) {
            logf_("ecran : maitrise obtenue");
            detacher_console();
        } else {
            logf_("ecran : un autre programme tient deja l'affichage (%s) — "
                  "lance 'killall fritax-shell fritax-login' puis reessaie", strerror(errno));
        }
    }
    if (d->fd < 0) { logf_("ouverture %s : %s", card, strerror(errno)); free(d); return NULL; }

    struct drm_mode_card_res res;
    memset(&res, 0, sizeof res);
    if (ioctl(d->fd, DRM_IOCTL_MODE_GETRESOURCES, &res)) { logf_("GETRESOURCES: %s", strerror(errno)); goto fail; }
    uint32_t *conns = calloc(res.count_connectors ? res.count_connectors : 1, sizeof(uint32_t));
    uint32_t *crtcs = calloc(res.count_crtcs ? res.count_crtcs : 1, sizeof(uint32_t));
    uint32_t *encs  = calloc(res.count_encoders ? res.count_encoders : 1, sizeof(uint32_t));
    uint32_t *fbs   = calloc(res.count_fbs ? res.count_fbs : 1, sizeof(uint32_t));
    res.connector_id_ptr = (uint64_t)(uintptr_t)conns;
    res.crtc_id_ptr      = (uint64_t)(uintptr_t)crtcs;
    res.encoder_id_ptr   = (uint64_t)(uintptr_t)encs;
    res.fb_id_ptr        = (uint64_t)(uintptr_t)fbs;
    if (ioctl(d->fd, DRM_IOCTL_MODE_GETRESOURCES, &res)) { logf_("GETRESOURCES(2): %s", strerror(errno)); goto fail2; }

    for (uint32_t i = 0; i < res.count_connectors; i++) {
        struct drm_mode_get_connector conn;
        memset(&conn, 0, sizeof conn);
        conn.connector_id = conns[i];
        if (ioctl(d->fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn)) continue;
        struct drm_mode_modeinfo *modes = calloc(conn.count_modes ? conn.count_modes : 1, sizeof *modes);
        uint32_t *cencs = calloc(conn.count_encoders ? conn.count_encoders : 1, sizeof(uint32_t));
        uint32_t *props = calloc(conn.count_props ? conn.count_props : 1, sizeof(uint32_t));
        uint64_t *pvals = calloc(conn.count_props ? conn.count_props : 1, sizeof(uint64_t));
        conn.modes_ptr = (uint64_t)(uintptr_t)modes;
        conn.encoders_ptr = (uint64_t)(uintptr_t)cencs;
        conn.props_ptr = (uint64_t)(uintptr_t)props;
        conn.prop_values_ptr = (uint64_t)(uintptr_t)pvals;
        int r = ioctl(d->fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);
        if (r == 0 && conn.connection == 1 && conn.count_modes > 0) {
            d->conn_id = conn.connector_id;
            memcpy(&d->mode, &modes[0], sizeof d->mode);
            uint32_t eid = conn.encoder_id ? conn.encoder_id : (conn.count_encoders ? cencs[0] : 0);
            struct drm_mode_get_encoder enc; memset(&enc, 0, sizeof enc); enc.encoder_id = eid;
            ioctl(d->fd, DRM_IOCTL_MODE_GETENCODER, &enc);
            d->crtc_id = enc.crtc_id ? enc.crtc_id : (res.count_crtcs ? crtcs[0] : 0);
            free(modes); free(cencs); free(props); free(pvals);
            break;
        }
        free(modes); free(cencs); free(props); free(pvals);
    }
    free(conns); free(crtcs); free(encs); free(fbs);
    if (!d->crtc_id) { logf_("aucun ecran connecte"); goto fail; }
    d->w = (int)d->mode.hdisplay; d->h = (int)d->mode.vdisplay;
    logf_("ecran %dx%d @ %uHz (crtc %u)", d->w, d->h, d->mode.vrefresh, d->crtc_id);
    return d;
fail2:
    free(conns); free(crtcs); free(encs); free(fbs);
fail:
    close(d->fd); free(d); return NULL;
}

void fx_drm_close(FXDrm *d) {
    if (!d) return;
    if (d->fd >= 0) {
        ioctl(d->fd, DRM_IOCTL_DROP_MASTER, 0);   /* on rend l'ecran au suivant */
        rattacher_console();                      /* et on redonne la console */
        close(d->fd);
    }
    free(d);
}
int fx_drm_width(FXDrm *d) { return d->w; }
int fx_drm_height(FXDrm *d) { return d->h; }
int fx_drm_fd(FXDrm *d) { return d->fd; }

FXDrmBuf *fx_drm_buf_new(FXDrm *d) {
    FXDrmBuf *b = calloc(1, sizeof(FXDrmBuf));
    if (!b) return NULL;
    struct drm_mode_create_dumb cd; memset(&cd, 0, sizeof cd);
    cd.width = (uint32_t)d->w; cd.height = (uint32_t)d->h; cd.bpp = 32;
    if (ioctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) { logf_("CREATE_DUMB: %s", strerror(errno)); free(b); return NULL; }
    b->handle = cd.handle; b->pitch = cd.pitch; b->size = cd.size;
    struct drm_mode_fb_cmd fc; memset(&fc, 0, sizeof fc);
    fc.width = (uint32_t)d->w; fc.height = (uint32_t)d->h; fc.pitch = b->pitch; fc.bpp = 32; fc.depth = 24;
    fc.handle = b->handle;
    if (ioctl(d->fd, DRM_IOCTL_MODE_ADDFB, &fc)) { logf_("ADDFB: %s", strerror(errno)); free(b); return NULL; }
    b->fb_id = fc.fb_id;
    struct drm_mode_map_dumb md; memset(&md, 0, sizeof md); md.handle = b->handle;
    if (ioctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &md)) { logf_("MAP_DUMB: %s", strerror(errno)); free(b); return NULL; }
    b->map = mmap(NULL, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, (off_t)md.offset);
    if (b->map == MAP_FAILED) { logf_("mmap: %s", strerror(errno)); free(b); return NULL; }
    return b;
}

int fx_drm_buf_flip(FXDrm *d, FXDrmBuf *b) {
    struct drm_mode_crtc c; memset(&c, 0, sizeof c);
    c.crtc_id = d->crtc_id; c.fb_id = b->fb_id;
    c.set_connectors_ptr = (uint64_t)(uintptr_t)&d->conn_id; c.count_connectors = 1;
    c.mode = d->mode; c.mode_valid = 1;
    if (ioctl(d->fd, DRM_IOCTL_MODE_SETCRTC, &c)) {
        /* Une seule fois : sinon le journal reçoit une ligne par image, soit des
           milliers par minute, et on ne lit plus rien d'autre. */
        static int deja_signale = 0;
        if (!deja_signale++) {
            logf_("SETCRTC refuse (%s) : un autre programme tient l'affichage.", strerror(errno));
            qui_tient_la_carte("/dev/dri/card0");
        }
        return -1;
    }
    return 0;
}

void fx_drm_wait_vblank(FXDrm *d) {
    union drm_wait_vblank wv; memset(&wv, 0, sizeof wv);
    wv.request.type = _DRM_VBLANK_RELATIVE; wv.request.sequence = 1;
    ioctl(d->fd, DRM_IOCTL_WAIT_VBLANK, &wv);
}
