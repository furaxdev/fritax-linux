#!/usr/bin/env bash
# ============================================================
#  FRITAX LINUX - controle complet AVANT envoi sur GitHub.
#  Un seul point d'entree : si ce script est vert, le build
#  cloud ne peut plus echouer sur une erreur bete.
#  Usage : distro/valider-tout.sh
# ============================================================
cd "$(dirname "$0")/.." || exit 1
RACINE=$(pwd)
erreurs=0
ok()   { printf '  ✅ %s\n' "$1"; }
ko()   { printf '  ❌ %s\n' "$1"; erreurs=$((erreurs+1)); }

# --- crypt() : l'ecran de connexion compare les mots de passe de /etc/shadow,
# --- il se lie donc a -lcrypt. Dans l'image, libxcrypt (Buildroot) fournit le
# --- lien de developpement. Sur une machine de developpement qui n'a que la
# --- bibliotheque d'execution (libcrypt.so.1 sans libcrypt.so), la liaison
# --- echoue alors que le code est bon : on fabrique le lien manquant
# --- localement, sans rien modifier au projet.
if ! cc -o /dev/null -xc /dev/null -lcrypt >/dev/null 2>&1; then
    for reelle in /lib/x86_64-linux-gnu/libcrypt.so.1 /usr/lib/x86_64-linux-gnu/libcrypt.so.1 /lib64/libcrypt.so.1 /usr/lib64/libcrypt.so.1; do
        if [ -e "$reelle" ]; then
            mkdir -p /tmp/fritax-libs
            ln -sf "$reelle" /tmp/fritax-libs/libcrypt.so
            # zig cc ignore LIBRARY_PATH : on passe le chemin directement
            # dans LIBS, que le Makefile laisse surchargeable.
            export LIBS="-L/tmp/fritax-libs -lcrypt"
            printf '  (libcrypt : lien local fabrique pour la compilation)\n'
            break
        fi
    done
fi

printf '\n╔══════════════════════════════════════════════════╗\n'
printf '║   FRITAX LINUX - controle avant envoi            ║\n'
printf '╚══════════════════════════════════════════════════╝\n'

# ---------- 1. le code C compile et les tests passent ----------
printf '\n[1/6] Compilation de tout le code C + tests\n'
if ./desktop/verifier-compilation.sh > /tmp/fritax-compile.log 2>&1; then
    ok "tout compile, tous les tests passent"
    grep -cE '^  ✅' /tmp/fritax-compile.log | sed 's/^/      modules verts : /'
else
    grep -E '❌|error' /tmp/fritax-compile.log | head -10 | sed 's/^/      /'
    ko "du code ne compile pas (voir /tmp/fritax-compile.log)"
fi

# ---------- 2. la configuration Buildroot est valide ----------
printf '\n[2/6] Configuration Buildroot\n'
if [ -d buildroot-2026.02.3 ] || [ -d /tmp/buildroot-2026.02.3 ]; then
    sortie=$(cd distro 2>/dev/null || cd .; python3 verifier-config.py 2>&1 | tail -3)
    if echo "$sortie" | grep -q "existent dans cet arbre"; then ok "toutes les options existent"; else ko "option inconnue : $sortie"; fi
else
    printf '      (arbre Buildroot absent : validation ignoree)\n'
fi

# ---------- 3. chaque paquet fritax est declare ----------
printf '\n[3/6] Paquets declares dans l arbre externe\n'
for p in branding shell files terminal tunnel login; do
    if grep -q "package/fritax-$p/Config.in" distro/Config.in; then ok "fritax-$p declare"; else ko "fritax-$p NON declare (sera ignore en silence)"; fi
    [ -d "distro/package/fritax-$p" ] || ko "dossier fritax-$p absent"
done
# chaque paquet doit avoir sa ligne dans la defconfig et dans build.sh
for p in BRANDING SHELL FILES TERMINAL TUNNEL LOGIN; do
    grep -q "BR2_PACKAGE_FRITAX_$p=y" distro/configs/fritax_defconfig || ko "BR2_PACKAGE_FRITAX_$p absent de la defconfig"
    grep -q "$p" distro/build.sh || ko "BR2_PACKAGE_FRITAX_$p non reverifie par build.sh"
done

# ---------- 4. fichiers demandes par la config ----------
# Le point cle : Buildroot resout ces chemins DEPUIS SA PROPRE RACINE
# (distro/buildroot-*/), pas depuis distro/. Un chemin sans "../" est
# donc introuvable -> le build s'arrete (ou pire, l'overlay est ignore
# en silence et l'ISO sort sans nos fonds d'ecran). On teste pour de vrai.
printf '\n[4/6] Fichiers cites par la configuration\n'
# En CI, l arbre Buildroot est TOUJOURS a distro/buildroot-<version>/.
# On simule donc ce chemin exactement : le shell resout ".." tout seul,
# donc "distro/buildroot-2026.02.3/../board/..." tombe dans distro/board/.
# C est le meme raisonnement que celui de Buildroot -> verdict fiable.
# En CI l arbre vit dans distro/buildroot-<version>/, donc un chemin "../x"
# depuis cet arbre = distro/x. On reproduit ce raisonnement a l identique :
# tout chemin doit commencer par "../" et pointer sur un fichier de distro/.
verifie_chemin() {   # <chemin> <type: f|d> <libelle>
    local chemin="$1" type="$2" libelle="$3"
    case "$chemin" in
        ../*) ;;
        *) ko "$libelle : \"$chemin\" ne commence pas par ../ -> Buildroot cherchera dans son propre dossier et ne trouvera rien"; return ;;
    esac
    local reel="distro/${chemin#../}"
    if [ "$type" = d ]; then
        [ -d "$reel" ] && ok "$libelle : $reel" || ko "$libelle : $reel INTROUVABLE"
    else
        [ -f "$reel" ] && ok "$libelle : $reel" || ko "$libelle : $reel INTROUVABLE"
    fi
}
for f in $(grep -oE '"[^"]*\.(fragment|cfg)"' distro/configs/fritax_defconfig | tr -d '"'); do
    verifie_chemin "$f" f "fichier de config"
done
OVL=$(grep -oE 'BR2_ROOTFS_OVERLAY="[^"]*"' distro/configs/fritax_defconfig | cut -d'"' -f2)
verifie_chemin "$OVL" d "overlay (fonds d ecran, menus)"
for f in board/fritax/isolinux.cfg board/fritax/rootfs-overlay/etc/init.d/S99fritax-shell; do
    [ -f "distro/$f" ] && ok "$(basename $f) present" || ko "$f absent"
done

# ---------- 5. scripts shell et workflow ----------
printf '\n[5/6] Scripts et workflow\n'
for s in $(find distro -name 'S*' -path '*init.d*' -not -path '*buildroot*'); do
    sh -n "$s" 2>/dev/null && ok "$(basename $s) syntaxe shell ok" || ko "$(basename $s) syntaxe shell KO"
done
bash -n distro/build.sh 2>/dev/null && ok "build.sh ok" || ko "build.sh KO"
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/build-iso.yml'))" 2>/dev/null \
    && ok "workflow YAML ok" || ko "workflow YAML casse"
# on cherche la VRAIE cle YAML, pas une mention dans un commentaire
grep -qE '^[[:space:]]*restore-keys:' .github/workflows/build-iso.yml \
    && ko "restore-keys encore la (cache perime !)" || ok "pas de cache perime possible"

# ---------- 6. pieges deja payes ----------
printf '\n[6/6] Pieges deja payes cash\n'
grep -q "BR2_KERNEL_HEADERS_AS_KERNEL=y" distro/configs/fritax_defconfig \
    && ko "BR2_KERNEL_HEADERS_AS_KERNEL de retour (casse glibc)" || ok "en-tetes noyau declares par serie"
grep -q "BR2_TOOLCHAIN_BUILDROOT_GLIBC=y" distro/configs/fritax_defconfig \
    && ok "glibc demande" || ko "glibc non demande"
grep -qE 'FRAGMENT_FILES="\.\./board' distro/configs/fritax_defconfig \
    && ok "chemin du fragment correct" || ko "chemin du fragment suspect (doit commencer par ../board)"
grep -q "rm -rf output/build/fritax-" distro/build.sh \
    && ok "nos paquets seront bien recompiles" || ko "nos paquets risquent de ne pas etre recompiles"

printf '\n╔══════════════════════════════════════════════════╗\n'
if [ "$erreurs" -eq 0 ]; then
    printf '║   TOUT EST VERT ✅  tu peux envoyer              ║\n'
else
    printf '║   %2d PROBLEME(S) ❌  ne pas envoyer            ║\n' "$erreurs"
fi
printf '╚══════════════════════════════════════════════════╝\n'
exit $erreurs
