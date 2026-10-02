/* Fritax Fichiers - le gestionnaire de fichiers de la distribution */
#ifndef FRITAX_FILES_APP_H
#define FRITAX_FILES_APP_H

#include "screen.h"
#include "window.h"

enum { FXF_NONE = 0, FXF_UP = 1, FXF_DOWN = 2, FXF_ENTER = 3, FXF_BACK = 4, FXF_HOME = 5 };

void fx_files_init(int screen_w, int screen_h, const char *start_path, const char *log_path);
void fx_files_draw(FXScreen *s);
int  fx_files_click(int x, int y);      /* 1 = demande de fermeture */
int  fx_files_move(int x, int y);
int  fx_files_key(int key);
int  fx_files_scroll(int up);
void fx_files_demo(void);   /* contenu factice pour les apercus */

#endif
