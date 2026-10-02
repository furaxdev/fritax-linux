/* Fritax - icones d'applications, dessinees a la main (aucun fichier externe) */
#ifndef FRITAX_APPICON_H
#define FRITAX_APPICON_H

#include "screen.h"

enum { FX_ICON_GENERIC = 0, FX_ICON_TERMINAL = 1, FX_ICON_FILES = 2, FX_ICON_SETTINGS = 3, FX_ICON_EDITOR = 4, FX_ICON_FILE = 5, FX_ICON_POWER = 6 };

/* Dessine une icone dans un carre de cote sz (>= 16 px conseille) */
void fx_app_icon(FXScreen *s, int x, int y, int sz, int kind);

/* Devine l'icone d'apres le nom de l'application */
int  fx_app_kind(const char *name);

#endif
