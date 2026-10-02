/* Fritax - entrees evdev (aucune bibliotheque) */
#define _GNU_SOURCE
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define MAX_DEV 32

struct FXInputs {
    int fd[MAX_DEV];
    char path[MAX_DEV][64];
    int n;
};

static FILE *GLOG;
void fx_inputs_log_path(const char *p) { if (p) GLOG = fopen(p, "a"); }
static void logf_(const char *fmt, ...) {
    if (!GLOG) return;
    va_list ap; va_start(ap, fmt); vfprintf(GLOG, fmt, ap); va_end(ap); fputc('\n', GLOG); fflush(GLOG);
}

FXInputs *fx_inputs_open(void) {
    FXInputs *in = calloc(1, sizeof(FXInputs));
    if (!in) return NULL;
    DIR *d = opendir("/dev/input");
    if (!d) { logf_("pas de /dev/input"); return in; }
    struct dirent *e;
    while ((e = readdir(d)) && in->n < MAX_DEV) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char path[64];
        snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned long bits[8] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof bits), bits) < 0) { close(fd); continue; }
        int has_key = (bits[EV_KEY / (8 * sizeof(long))] >> (EV_KEY % (8 * sizeof(long)))) & 1;
        int has_rel = (bits[EV_REL / (8 * sizeof(long))] >> (EV_REL % (8 * sizeof(long)))) & 1;
        int has_abs = (bits[EV_ABS / (8 * sizeof(long))] >> (EV_ABS % (8 * sizeof(long)))) & 1;
        if (!has_key && !has_rel && !has_abs) { close(fd); continue; }
        in->fd[in->n] = fd;
        snprintf(in->path[in->n], sizeof in->path[in->n], "%s", path);
        logf_("entree %s (clavier=%d souris=%d tactile=%d)", path, has_key, has_rel, has_abs);
        in->n++;
    }
    closedir(d);
    logf_("%d peripheriques", in->n);
    return in;
}

void fx_inputs_close(FXInputs *in) {
    if (!in) return;
    for (int i = 0; i < in->n; i++) close(in->fd[i]);
    free(in);
}

int fx_inputs_fd_count(FXInputs *in) { return in ? in->n : 0; }

int fx_inputs_pollfds(FXInputs *in, void *arr, int max) {
    struct pollfd *pf = (struct pollfd *)arr;
    int n = 0;
    for (int i = 0; i < in->n && n < max; i++) { pf[n].fd = in->fd[i]; pf[n].events = POLLIN; n++; }
    return n;
}

/* disposition francaise (AZERTY) */
int fx_input_key_ascii(unsigned code, int shift) {
    static const char lo[128] = {
        [KEY_1]='&',[KEY_2]=0xE9,[KEY_3]='"',[KEY_4]=0x27,[KEY_5]='(',[KEY_6]='-',[KEY_7]=0xE8,[KEY_8]='_',[KEY_9]=0xE7,[KEY_0]=0xE0,
        [KEY_MINUS]=')',[KEY_EQUAL]='=',[KEY_SPACE]=' ',[KEY_A]='a',[KEY_Z]='z',[KEY_E]='e',[KEY_R]='r',[KEY_T]='t',[KEY_Y]='y',[KEY_U]='u',
        [KEY_I]='i',[KEY_O]='o',[KEY_P]='p',[KEY_Q]='q',[KEY_S]='s',[KEY_D]='d',[KEY_F]='f',[KEY_G]='g',[KEY_H]='h',[KEY_J]='j',
        [KEY_K]='k',[KEY_L]='l',[KEY_M]='m',[KEY_W]='w',[KEY_X]='x',[KEY_C]='c',[KEY_V]='v',[KEY_B]='b',[KEY_N]='n',[KEY_COMMA]=',',
        [KEY_SEMICOLON]=';',[KEY_APOSTROPHE]=':',[KEY_SLASH]='.',[KEY_DOT]='.',[KEY_BACKSPACE]=8,[KEY_ENTER]='\r',[KEY_TAB]='\t',
    };
    static const char sh[128] = {
        [KEY_1]='1',[KEY_2]='2',[KEY_3]='3',[KEY_4]='4',[KEY_5]='5',[KEY_6]='6',[KEY_7]='7',[KEY_8]='8',[KEY_9]='9',[KEY_0]='0',
        [KEY_A]='A',[KEY_Z]='Z',[KEY_E]='E',[KEY_R]='R',[KEY_T]='T',[KEY_Y]='Y',[KEY_U]='U',[KEY_I]='I',[KEY_O]='O',[KEY_P]='P',
        [KEY_Q]='Q',[KEY_S]='S',[KEY_D]='D',[KEY_F]='F',[KEY_G]='G',[KEY_H]='H',[KEY_J]='J',[KEY_K]='K',[KEY_L]='L',[KEY_M]='M',
        [KEY_W]='W',[KEY_X]='X',[KEY_C]='C',[KEY_V]='V',[KEY_B]='B',[KEY_N]='N',[KEY_SPACE]=' ',
    };
    if (code >= 128) return 0;
    return shift ? sh[code] : lo[code];
}

static int shift_down, super_down, ctrl_down;

void fx_inputs_handle(FXInputs *in, int index,
                      void (*on_move)(void *u, int dx, int dy),
                      void (*on_button)(void *u, int pressed),
                      void (*on_key)(void *u, int key, int ascii),
                      void (*on_scroll)(void *u, int up),
                      void *user) {
    if (!in || index < 0 || index >= in->n) return;
    struct input_event evs[64];
    ssize_t n = read(in->fd[index], evs, sizeof evs);
    if (n <= 0) return;
    for (size_t k = 0; k < (size_t)n / sizeof(struct input_event); k++) {
        struct input_event *ev = &evs[k];
        if (ev->type == EV_REL) {
            if (ev->code == REL_X && on_move) on_move(user, ev->value, 0);
            else if (ev->code == REL_Y && on_move) on_move(user, 0, ev->value);
            else if (ev->code == REL_WHEEL && on_scroll) on_scroll(user, ev->value > 0);
        } else if (ev->type == EV_KEY) {
            int pressed = ev->value != 0;
            if (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT) { shift_down = pressed; continue; }
            if (ev->code == KEY_LEFTCTRL || ev->code == KEY_RIGHTCTRL) { ctrl_down = pressed; continue; }
            if (ev->code == KEY_LEFTMETA || ev->code == KEY_RIGHTMETA) {
                if (pressed && !super_down && on_key) on_key(user, FXK_SUPER, 0);
                super_down = pressed; continue;
            }
            if (ev->code == BTN_LEFT) { if (on_button) on_button(user, pressed); continue; }
            if (!pressed) continue;
            int norm = FXK_NONE, asc = 0;
            switch (ev->code) {
            case KEY_ESC: norm = FXK_ESC; break;
            case KEY_BACKSPACE: norm = FXK_BACKSPACE; break;
            case KEY_ENTER: norm = FXK_ENTER; break;
            case KEY_TAB: norm = FXK_TAB; break;
            case KEY_UP: norm = FXK_UP; break;
            case KEY_DOWN: norm = FXK_DOWN; break;
            case KEY_LEFT: norm = FXK_LEFT; break;
            case KEY_RIGHT: norm = FXK_RIGHT; break;
            case KEY_PAGEUP: norm = FXK_PAGEUP; break;
            case KEY_PAGEDOWN: norm = FXK_PAGEDOWN; break;
            default:
                asc = fx_input_key_ascii((unsigned)ev->code, shift_down);
                /* Ctrl + lettre -> code de controle (Ctrl+S = 19), comme dans une console */
                if (ctrl_down && asc >= 'a' && asc <= 'z') asc = asc - 'a' + 1;
                else if (ctrl_down && asc >= 'A' && asc <= 'Z') asc = asc - 'A' + 1;
                break;
            }
            if (on_key) on_key(user, norm, asc);
        }
    }
}
