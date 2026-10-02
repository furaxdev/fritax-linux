/* Fritax - entrees directes (souris + clavier via evdev) */
#ifndef FRITAX_INPUT_H
#define FRITAX_INPUT_H

typedef struct FXInputs FXInputs;

/* touches normalisees renvoyees au programme */
enum { FXK_NONE = 0, FXK_ESC, FXK_SUPER, FXK_BACKSPACE, FXK_ENTER, FXK_TAB, FXK_UP, FXK_DOWN, FXK_LEFT, FXK_RIGHT, FXK_PAGEUP, FXK_PAGEDOWN };

FXInputs *fx_inputs_open(void);                 /* ouvre tous les /dev/input/event* */
void      fx_inputs_close(FXInputs *in);
int       fx_inputs_fd_count(FXInputs *in);
int       fx_inputs_pollfds(FXInputs *in, void *pollfd_array, int max);
void      fx_inputs_handle(FXInputs *in, int index,
                           void (*on_move)(void *u, int dx, int dy),
                           void (*on_button)(void *u, int pressed),
                           void (*on_key)(void *u, int key, int ascii),
                           void (*on_scroll)(void *u, int up),
                           void *user);
void fx_inputs_log_path(const char *path);
int  fx_input_key_ascii(unsigned code, int shift);   /* disposition francaise AZERTY */

#endif
