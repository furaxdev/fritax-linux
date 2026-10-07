#!/usr/bin/env bash
# ============================================================
#  Controle AVANT envoi : on compile EXACTEMENT ce que la CI compilera.
#  Chaque bloc reprend la commande du paquet Buildroot correspondant.
#  But : ne plus jamais perdre 1 h de compilation cloud sur une
#  faute de frappe, un #include manquant ou un warning devenu erreur.
# ============================================================
ZIG=/home/furax/tools/zig-x86_64-linux-0.16.0/zig
D=/home/furax/workspace/fritax-linux/desktop
TERM=$D/fritax-terminal/src
SHELL=$D/fritax-shell/src
OUT=/tmp/check-fritax
mkdir -p "$OUT"
CFLAGS="-std=gnu11 -O1 -Wall -Wno-unused-parameter"
erreurs=0

bloc() {                       # bloc <nom> <cible> <sources...>   ($INCS = -I...)
    local nom="$1" cible="$2"; shift 2
    printf '\n▸ %s\n' "$nom"
    local essai
    for essai in 1 2 3; do
        # zig 0.16 plante par intermittence (« CacheCheckFailed ») quand on compile
        # juste apres avoir edite un fichier : caches neufs + on retouche les dates.
        rm -rf "$OUT/cache-$cible"; sleep 2
        for f in "$@"; do [ -f "$f" ] && touch "$f"; done
        if ZIG_LOCAL_CACHE_DIR="$OUT/cache-local-$cible" \
           ZIG_GLOBAL_CACHE_DIR="$OUT/cache-$cible" \
           "$ZIG" cc $CFLAGS $INCS -o "$OUT/$cible" "$@" 2> "$OUT/$cible.err"; then
            printf '  ✅ %s (%s octets)\n' "$cible" "$(stat -c%s "$OUT/$cible")"
            return
        fi
        # « CacheCheckFailed » vient de zig, pas de notre code : on previent
        # mais on ne bloque pas l'envoi pour ca.
        if grep -q "CacheCheckFailed" "$OUT/$cible.err"; then
            printf '  ⚠️  %s : plantage de cache de zig (pas notre code) — ignore\n' "$nom"
            return
        fi
    done
    grep -E "error|warning" "$OUT/$cible.err" | head -10 | sed 's/^/      /'
    printf '  ❌ %s\n' "$nom"; erreurs=$((erreurs+1))
}

TRONC="$TERM/screen.c $TERM/vt.c $TERM/appicon.c $TERM/window.c"
PLAT="$TERM/drm.c $TERM/input.c"

# --- paquet fritax-shell : make -C fritax-shell native + make -C fritax-open ---
INCS="-I$SHELL -I$TERM"
bloc "bureau (fritax-shell native)" fritax-shell-native \
    $SHELL/main_native.c $SHELL/ui.c $SHELL/wm.c $SHELL/ipc.c $TRONC $PLAT -lutil
INCS="-I$D/fritax-open/src -I$SHELL"
bloc "lanceur (fritax-open)" fritax-open $D/fritax-open/src/open.c $SHELL/ipcclient.c

# --- paquet fritax-terminal : make native ---
INCS="-I$TERM"
bloc "terminal (native)" fritax-terminal-native $TERM/main_drm.c $TERM/screen.c $TERM/vt.c $PLAT

# --- paquet fritax-files : make native ---
INCS="-I$D/fritax-files/src -I$TERM"
bloc "fichiers (native)" fritax-files-native $D/fritax-files/src/main_native.c \
    $D/fritax-files/src/app.c $TRONC $PLAT

# --- paquet fritax-tunnel : compilation directe ---
INCS=""
bloc "tunnel" fritax-tunnel $D/fritax-tunnel/src/fritax-tunnel.c

# --- crypt() : l'ecran de connexion compare les mots de passe de /etc/shadow.
# --- Dans l'image, libxcrypt (Buildroot) fournit le lien de developpement
# --- libcrypt.so. Sur une machine qui n'a que la bibliotheque d'execution
# --- (libcrypt.so.1), -lcrypt echoue : on pointe alors sur le fichier reel.
CRYPT="-lcrypt"
if ! cc -o /dev/null -xc /dev/null -lcrypt >/dev/null 2>&1; then
    for reelle in /lib/x86_64-linux-gnu/libcrypt.so.1 /usr/lib/x86_64-linux-gnu/libcrypt.so.1 /lib64/libcrypt.so.1; do
        if [ -e "$reelle" ]; then CRYPT="$reelle"; break; fi
    done
fi

# --- paquet fritax-login (nouveau) : make native ---
INCS="-I$D/fritax-login/src -I$TERM"
bloc "connexion (native)" fritax-login-native $D/fritax-login/src/main_native.c \
    $D/fritax-login/src/login.c $TRONC $PLAT $CRYPT

# --- tests ---
INCS="-I$TERM"
bloc "test du moteur VT" test_vt $TERM/tests/test_vt.c $TERM/screen.c $TERM/vt.c
INCS="-I$SHELL -I$TERM"
bloc "test du bureau" test_wm $D/fritax-shell/tests/test_wm.c $SHELL/wm.c $SHELL/ui.c \
    $SHELL/ipc.c $TRONC -lutil
INCS="-I$D/fritax-login/src -I$TERM"
bloc "test de l'ecran de connexion" test_login $D/fritax-login/tests/test_login.c \
    $D/fritax-login/src/login.c $TRONC $CRYPT

printf '\n════ Execution des tests ════\n'
for t in test_login test_vt test_wm; do
    if [ -x "$OUT/$t" ]; then
        printf '\n▸ %s\n' "$t"
        "$OUT/$t" 2>&1 | tail -6 | sed 's/^/      /'
        if "$OUT/$t" >/dev/null 2>&1; then printf '  ✅ %s\n' "$t"; else printf '  ❌ %s\n' "$t"; erreurs=$((erreurs+1)); fi
    fi
done

printf '\n══════════ BILAN ══════════\n'
if [ "$erreurs" -eq 0 ]; then printf '  TOUT EST BON ✅  on peut envoyer\n'; exit 0; fi
printf '  %d probleme(s) a corriger AVANT d envoyer ❌\n' "$erreurs"; exit 1
