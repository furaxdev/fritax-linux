/* ============================================================
 *  Fritax - acces direct a l'affichage (DRM/KMS du noyau)
 *  Aucune bibliotheque : uniquement les ioctls du noyau.
 * ============================================================ */
#ifndef FRITAX_DRM_H
#define FRITAX_DRM_H

#include <stdint.h>

typedef struct {
    uint32_t fb_id, handle, pitch, size;
    void *map;
} FXDrmBuf;

typedef struct FXDrm FXDrm;

FXDrm *fx_drm_open(const char *card);              /* /dev/dri/card0 */
void   fx_drm_close(FXDrm *d);
int    fx_drm_width(FXDrm *d);
int    fx_drm_height(FXDrm *d);
FXDrmBuf *fx_drm_buf_new(FXDrm *d);                 /* tampon a la taille de l'ecran */
int    fx_drm_buf_flip(FXDrm *d, FXDrmBuf *b);      /* affiche ce tampon */
void   fx_drm_wait_vblank(FXDrm *d);
int    fx_drm_fd(FXDrm *d);
void   fx_drm_log_path(const char *path);           /* journal (peut etre NULL) */

#endif
