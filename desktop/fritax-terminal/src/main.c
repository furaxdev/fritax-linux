/* ============================================================
 *  Fritax Terminal - le terminal maison de Fritax Linux
 *  Moteur VT maison (vt.c) + rendu logiciel (screen.c) + X11
 *  Aucune dependance hors libX11 (et forkpty de la libc).
 * ============================================================ */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <locale.h>
#include <stdarg.h>
#include <execinfo.h>

#include "vt.h"
#include "screen.h"

/* ---- journal de diagnostic : ~/.fritax-terminal.log ---- */
static FILE *LOG = NULL;
static void logf_(const char *fmt, ...) {
    if (!LOG) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(LOG, fmt, ap);
    va_end(ap);
    fputc('\n', LOG);
    fflush(LOG);
}
static void crash_handler(int sig) {
    if (LOG) {
        void *bt[32];
        int n = backtrace(bt, 32);
        fprintf(LOG, "\n*** PLANTAGE : signal %d ***\n", sig);
        fflush(LOG);
        backtrace_symbols_fd(bt, n, fileno(LOG));
        fflush(LOG);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

#define PAD 6                 /* marge autour du texte */
#define DEF_COLS 100
#define DEF_ROWS 30
#define BLINK_MS 550

static Display *dpy;
static Window   win;
static GC       gc;
static XImage  *img = NULL;
static Visual  *vis;
static int      depth, screen_num, bpp = 32;
static Atom     wm_delete;
static int      pty_fd = -1;
static pid_t    child = 0;
static VT      *vt;
static FXScreen *fb;
static int      cols = DEF_COLS, rows = DEF_ROWS;
static uint32_t col_fg, col_bg, col_cursor;
static int      dirty_all = 1;
static unsigned char dirty_rows[1024];
static int      want_redraw = 0;

/* ---------------- envoi vers le shell ---------------- */
static void pty_write(const char *s, size_t n) {
    if (pty_fd < 0) return;
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(pty_fd, s + off, n - off);
        if (w <= 0) { if (errno == EINTR) continue; return; }
        off += (size_t)w;
    }
}

/* ---------------- hooks du moteur ---------------- */
static void cb_dirty(void *u, int row) {
    (void)u;
    if (row < 0) dirty_all = 1;
    else if (row < (int)sizeof dirty_rows) dirty_rows[row] = 1;
    want_redraw = 1;
}
static void cb_reply(void *u, const char *s, size_t n) { (void)u; pty_write(s, n); }
static void cb_bell(void *u) { (void)u; XBell(dpy, 30); }
static void cb_title(void *u, const char *s, size_t n) {
    (void)u;
    char b[260];
    if (n > 255) n = 255;
    memcpy(b, s, n); b[n] = 0;
    if (win) XStoreName(dpy, win, b);
}

/* ---------------- rendu ---------------- */
static void render_rows(int first, int last) {
    fx_render_vt(vt, fb, first, last, PAD, col_cursor, col_bg);

    if (img) {
        for (int r = first; r <= last && r < rows; r++) {
            int y = PAD + r * FONT_H;
            XPutImage(dpy, win, gc, img, PAD ? 0 : 0, y, 0, y, (unsigned)(cols * FONT_W), FONT_H);
        }
    }
}

static void render_all(void) {
    fx_clear(fb, col_bg);
    render_rows(0, rows - 1);
    dirty_all = 0;
    memset(dirty_rows, 0, sizeof dirty_rows);
}

/* ---------------- taille de la fenetre / du pty ---------------- */
static void apply_geometry(int win_w, int win_h, int resize_pty) {
    int new_cols = (win_w - PAD * 2) / FONT_W;
    int new_rows = (win_h - PAD * 2) / FONT_H;
    if (new_cols < 2) new_cols = 2;
    if (new_rows < 2) new_rows = 2;
    cols = new_cols; rows = new_rows;

    /* IMPORTANT : XDestroyImage libere img->data ; on le detache pour ne PAS
       liberer notre framebuffer (c'etait le plantage au demarrage). */
    if (img) { img->data = NULL; XDestroyImage(img); img = NULL; }
    fx_screen_resize(fb, cols * FONT_W + PAD * 2, rows * FONT_H + PAD * 2);
    vt_resize(vt, cols, rows);
    img = XCreateImage(dpy, vis, (unsigned)depth, ZPixmap, 0, (char *)fb->px,
                       (unsigned)fb->w, (unsigned)fb->h, 32, 0);
    if (img) XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)fb->w, (unsigned)fb->h);

    if (resize_pty && pty_fd >= 0) {
        struct winsize ws;
        memset(&ws, 0, sizeof ws);
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;
        ioctl(pty_fd, TIOCSWINSZ, &ws);
        kill(child, SIGWINCH);
    }
    dirty_all = 1;
}

/* ---------------- clavier ---------------- */
static void send_key(XKeyEvent *ev) {
    KeySym ks = XLookupKeysym(ev, 0);
    char buf[64];
    int n = XLookupString(ev, buf, sizeof buf, &ks, NULL);
    int alt = (ev->state & Mod1Mask) != 0;
    int shift = (ev->state & ShiftMask) != 0;
    const char *seq = NULL;

    switch (ks) {
    case XK_Up:        seq = "\033[A"; break;
    case XK_Down:      seq = "\033[B"; break;
    case XK_Right:     seq = "\033[C"; break;
    case XK_Left:      seq = "\033[D"; break;
    case XK_Home:      seq = "\033[H"; break;
    case XK_End:       seq = "\033[F"; break;
    case XK_Insert:    seq = "\033[2~"; break;
    case XK_Delete:    seq = "\033[3~"; break;
    case XK_Page_Up:   seq = "\033[5~"; break;
    case XK_Page_Down: seq = "\033[6~"; break;
    case XK_F1:  seq = "\033OP"; break;
    case XK_F2:  seq = "\033OQ"; break;
    case XK_F3:  seq = "\033OR"; break;
    case XK_F4:  seq = "\033OS"; break;
    case XK_F5:  seq = "\033[15~"; break;
    case XK_F6:  seq = "\033[17~"; break;
    case XK_F7:  seq = "\033[18~"; break;
    case XK_F8:  seq = "\033[19~"; break;
    case XK_F9:  seq = "\033[20~"; break;
    case XK_F10: seq = "\033[21~"; break;
    case XK_F11: seq = "\033[23~"; break;
    case XK_F12: seq = "\033[24~"; break;
    case XK_BackSpace: seq = "\177"; break;
    case XK_Return: case XK_KP_Enter: seq = "\r"; break;
    case XK_Tab: seq = "\t"; break;
    case XK_Escape: seq = "\033"; break;
    case XK_space: seq = " "; break;
    default: break;
    }

    /* molette clavier : PageUp/PageDown avec Shift font defiler l'historique */
    if (shift && ks == XK_Page_Up)   { vt_scroll_view(vt, rows - 2); return; }
    if (shift && ks == XK_Page_Down) { vt_scroll_view(vt, -(rows - 2)); return; }

    if (seq) { if (alt) pty_write("\033", 1); pty_write(seq, strlen(seq)); return; }
    if (ks == XK_Shift_L || ks == XK_Shift_R || ks == XK_Control_L || ks == XK_Control_R ||
        ks == XK_Alt_L || ks == XK_Alt_R || ks == XK_Super_L || ks == XK_Super_R ||
        ks == XK_Caps_Lock || ks == XK_Menu) return;
    if (alt) pty_write("\033", 1);
    if (n > 0) pty_write(buf, (size_t)n);
}

/* ---------------- boucle principale ---------------- */
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    {
        char path[512];
        const char *home = getenv("HOME");
        snprintf(path, sizeof path, "%s/.fritax-terminal.log", home ? home : "/tmp");
        LOG = fopen(path, "w");
        if (LOG) { logf_("Fritax Terminal - demarrage (%s)", path); }
        signal(SIGSEGV, crash_handler);
        signal(SIGABRT, crash_handler);
        signal(SIGBUS, crash_handler);
    }
    char *shell = getenv("SHELL");
    if (!shell || !*shell || access(shell, X_OK) != 0) shell = "/bin/bash";
    if (access(shell, X_OK) != 0) shell = "/bin/sh";
    char *exec_argv[4] = { NULL, NULL, NULL, NULL };
    const char *cmd = NULL;
    int win_w_arg = 0, win_h_arg = 0;

    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-e") || !strcmp(argv[i], "--exec")) && i + 1 < argc) cmd = argv[++i];
        else if (!strcmp(argv[i], "--cols") && i + 1 < argc) cols = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rows") && i + 1 < argc) rows = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--width") && i + 1 < argc) win_w_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i + 1 < argc) win_h_arg = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--version")) { printf("Fritax Terminal 1.0 (Nova)\n"); return 0; }
        else if (!strcmp(argv[i], "--help")) {
            printf("Fritax Terminal 1.0 - le terminal de Fritax Linux\n\n"
                   "Usage : fritax-terminal [options] [-- commande]\n"
                   "  -e, --exec CMD   executer CMD au lieu du shell\n"
                   "      --cols N     nombre de colonnes (defaut %d)\n"
                   "      --rows N     nombre de lignes (defaut %d)\n"
                   "      --width PX   largeur de fenetre\n"
                   "      --height PX  hauteur de fenetre\n", DEF_COLS, DEF_ROWS);
            return 0;
        }
    }

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "fritax-terminal: impossible d'ouvrir l'affichage X\n"); return 1; }
    logf_("affichage X ouvert (depth=%d)", DefaultDepth(dpy, DefaultScreen(dpy)));
    screen_num = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen_num);
    depth = DefaultDepth(dpy, screen_num);
    bpp = (int)DefaultDepth(dpy, screen_num);
    if (depth == 24 || depth == 32) bpp = 32;

    int wpx = win_w_arg ? win_w_arg : cols * FONT_W + PAD * 2;
    int hpx = win_h_arg ? win_h_arg : rows * FONT_H + PAD * 2;

    fb = fx_screen_new(wpx, hpx);
    if (!fb) return 1;
    col_bg = 0x0D1422;
    col_fg = 0xEAF2FF;
    col_cursor = 0xD633FF;                 /* violet Fritax */

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen_num), 0, 0,
                              (unsigned)wpx, (unsigned)hpx, 0,
                              BlackPixel(dpy, screen_num), col_bg);
    XStoreName(dpy, win, "Fritax Terminal");
    XClassHint ch; ch.res_name = (char *)"fritax-terminal"; ch.res_class = (char *)"Fritax-Terminal";
    XSetClassHint(dpy, win, &ch);
    XSelectInput(dpy, win, KeyPressMask | ExposureMask | StructureNotifyMask |
                          ButtonPressMask | FocusChangeMask | EnterWindowMask);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, col_bg);
    XMapWindow(dpy, win);
    XFlush(dpy);

    /* ---- moteur + shell ---- */
    VTHooks hooks = { cb_dirty, cb_reply, cb_bell, cb_title };
    vt = vt_new(cols, rows, VT_SCROLLBACK, hooks, NULL);
    uint8_t pal[16][3];
    fx_default_palette(pal);
    vt_set_palette(vt, pal);

    struct winsize ws = { 0, 0, 0, 0 };
    ws.ws_col = (unsigned short)cols; ws.ws_row = (unsigned short)rows;
    child = forkpty(&pty_fd, NULL, NULL, &ws);
    if (child < 0) { perror("forkpty"); return 1; }
    if (child < 0) { logf_("forkpty a echoue"); }
    else logf_("shell lance : %s (pid %d)", shell, (int)child);
    if (child == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("TERM_PROGRAM", "fritax-terminal", 1);
        if (cmd) { exec_argv[0] = (char *)shell; exec_argv[1] = (char *)"-c"; exec_argv[2] = (char *)cmd; exec_argv[3] = NULL; execvp(shell, exec_argv); }
        else { execl(shell, shell, (char *)NULL); }
        _exit(127);
    }

    int xfd = ConnectionNumber(dpy);
    int running = 1;
    char buf[8192];

    apply_geometry(wpx, hpx, 1);
    render_all();
    XFlush(dpy);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        if (pty_fd >= 0) FD_SET(pty_fd, &rfds);
        int maxfd = pty_fd > xfd ? pty_fd : xfd;
        struct timeval tv = { 0, BLINK_MS * 1000 };

        int r = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0) { if (errno == EINTR) continue; break; }

        if (r == 0) {                      /* clignotement du curseur */
            vt_tick(vt);
            if (want_redraw || dirty_all) {
                if (dirty_all) render_all();
                else { for (int i = 0; i < rows; i++) if (dirty_rows[i]) { render_rows(i, i); dirty_rows[i] = 0; } }
                XFlush(dpy); want_redraw = 0;
            }
            continue;
        }

        if (pty_fd >= 0 && FD_ISSET(pty_fd, &rfds)) {
            ssize_t n = read(pty_fd, buf, sizeof buf);
            if (n > 0) { vt_feed(vt, buf, (size_t)n); }
            else if (n <= 0) {
                /* le shell a termine */
                if (dirty_all) render_all();
                else { for (int i = 0; i < rows; i++) if (dirty_rows[i]) { render_rows(i, i); dirty_rows[i] = 0; } }
                XFlush(dpy);
                int st = 0;
                if (waitpid(child, &st, WNOHANG) > 0) {
                    logf_("le shell a termine (statut %d) -> fermeture de la fenetre", st);
                    running = 0; break;
                }
            }
        }

        if (FD_ISSET(xfd, &rfds)) {
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                switch (ev.type) {
                case KeyPress: send_key(&ev.xkey); break;
                case ButtonPress:
                    if (ev.xbutton.button == 4) vt_scroll_view(vt, 3);
                    else if (ev.xbutton.button == 5) vt_scroll_view(vt, -3);
                    break;
                case ConfigureNotify: {
                    XConfigureEvent *c = &ev.xconfigure;
                    if (c->width != fb->w || c->height != fb->h) apply_geometry(c->width, c->height, 1);
                    break; }
                case Expose:
                    if (ev.xexpose.count == 0 && img)
                        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)fb->w, (unsigned)fb->h);
                    break;
                case ClientMessage:
                    if ((Atom)ev.xclient.data.l[0] == wm_delete) running = 0;
                    break;
                default: break;
                }
            }
        }

        if (dirty_all) render_all();
        else {
            for (int i = 0; i < rows; i++)
                if (dirty_rows[i]) { render_rows(i, i); dirty_rows[i] = 0; }
        }
        XFlush(dpy);
        want_redraw = 0;
    }

    if (pty_fd >= 0) close(pty_fd);
    if (img) { img->data = NULL; XDestroyImage(img); img = NULL; }
    vt_free(vt);
    fx_screen_free(fb);
    XCloseDisplay(dpy);
    return 0;
}
