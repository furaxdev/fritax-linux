#!/bin/sh
# ============================================================
#  FRITAX - essai AVANT envoi (a lancer avant chaque push)
#
#  Reproduit en local les controles que fait la chaine d'integration,
#  pour ne plus jamais decouvrir une erreur dans le nuage apres 10 minutes
#  d'attente. Chaque controle correspond a un echec deja paye cash.
#
#    sh distro/tester-avant-envoi.sh
#
#  Sortie : une ligne par controle, et un bilan final clair.
# ============================================================
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
DEPOT=$(dirname "$HERE")
ERREURS=0
ZIG=${ZIG:-/home/furax/tools/zig-x86_64-linux-0.16.0/zig}
ECRAN=desktop/fritax-terminal/src

ok()   { printf '  \033[32mOK\033[0m     %s\n' "$1"; }
ko()   { printf '  \033[31mECHEC\033[0m  %s\n' "$1"; ERREURS=$((ERREURS + 1)); }
titre(){ printf '\n\033[1m%s\033[0m\n' "$1"; }

cd "$DEPOT" || exit 1

# ------------------------------------------------------------------
titre "[1/6] Le controle de configuration de build.sh est coherent"
# Echec deja paye cash : les noms etaient listes en minuscules, donc le
# controle cherchait BR2_PACKAGE_FRITAX_antivirus qui n'existe pas.
LISTE=$(sed -n 's/^for p in \([^;]*\); do/\1/p' distro/build.sh | head -1)
[ -n "$LISTE" ] || ko "boucle de controle introuvable dans build.sh"
for p in $LISTE; do
    grep -q "^BR2_PACKAGE_FRITAX_$p=y" distro/configs/fritax_defconfig \
        || ko "build.sh exige FRITAX_$p mais la defconfig ne l'active pas"
    [ -f "distro/package/fritax-$(echo "$p" | tr 'A-Z' 'a-z')/Config.in" ] \
        || ko "build.sh exige FRITAX_$p mais aucun paquet de ce nom n'existe"
done
[ "$ERREURS" -eq 0 ] && ok "les $(echo "$LISTE" | wc -w) paquets du controle existent et sont actives"

# ------------------------------------------------------------------
titre "[2/6] Chaque Config.in est un Kconfig valide"
# Un fichier Kconfig casse fait echouer 'make <cible>_defconfig' et, comme
# l'erreur etait avalee, on ne voyait qu'un 'grep: .config: No such file'.
for f in distro/package/*/Config.in; do
    nom=$(basename "$(dirname "$f")")
    # le symbole doit commencer par BR2_PACKAGE_ et etre en majuscules
    if ! grep -qE '^config BR2_PACKAGE_[A-Z0-9_]+$' "$f"; then
        ko "$nom : ligne 'config BR2_PACKAGE_...' absente ou mal ecrite"
    fi
    # la ligne 'bool' doit etre indentee par une TABULATION
    grep -qP '^\tbool ' "$f" || ko "$nom : la ligne bool n'est pas indentee par une tabulation"
    # l'aide doit etre plus indentee que 'help'
    if grep -q '^\thelp' "$f"; then
        grep -q '^\t  ' "$f" || ko "$nom : l'aide n'est pas indentee (Kconfig exige une tabulation + 2 espaces)"
    fi
done
[ "$ERREURS" -eq 0 ] && ok "tous les Config.in sont valides"

# ------------------------------------------------------------------
titre "[3/6] Chaque paquet a son .mk avec de VRAIES tabulations"
# Un Makefile avec des '\t' litteraux ne fonctionne pas.
for f in distro/package/*/*.mk; do
    nom=$(basename "$f")
    dir=$(basename "$(dirname "$f")")
    [ "$nom" = "$dir.mk" ] || ko "$nom : le fichier doit s'appeler $dir.mk"
    grep -q '\\t' "$f" && ko "$nom : contient des '\\t' litteraux au lieu de tabulations"
    grep -q '^\$(eval \$(generic-package))' "$f" || ko "$nom : ligne \$(eval \$(generic-package)) absente"
    # le prefixe des variables doit venir du NOM DU DOSSIER (FRITAX_ANTIVIRUS_*)
    var=$(echo "$dir" | tr 'a-z-' 'A-Z_')
    grep -q "^${var}_VERSION" "$f" || ko "$nom : attendu ${var}_VERSION (Buildroot tire le nom du dossier)"
done
[ "$ERREURS" -eq 0 ] && ok "tous les .mk sont conformes"

# ------------------------------------------------------------------
titre "[4/6] Chaque paquet est declare, active ET dans la boucle de controle"
for f in distro/package/*/Config.in; do
    nom=$(basename "$(dirname "$f")")
    maj=$(echo "$nom" | tr 'a-z-' 'A-Z_')
    grep -q "package/$nom/Config.in" distro/Config.in || ko "$nom : non declare dans distro/Config.in"
    grep -q "^BR2_PACKAGE_${maj}=y" distro/configs/fritax_defconfig || ko "$nom : non active dans la defconfig"
done
[ "$ERREURS" -eq 0 ] && ok "tous les paquets sont declares et actives"

# ------------------------------------------------------------------
titre "[5/6] Le piege du cache : on teste le MAKEFILE, pas le dossier"
# Le cache du nuage ne restaure que output/ : le dossier de l'arbre peut
# exister sans que Buildroot soit la. Le test doit porter sur le Makefile.
grep -q '\[ ! -f "\$BR_DIR/Makefile" \]' distro/build.sh \
    || ko "build.sh teste encore le dossier : '-d \$BR_DIR' au lieu de '-f \$BR_DIR/Makefile'"
grep -q 'buildroot-2026.02.3/Makefile \]' .github/workflows/build-iso.yml \
    || ko "le workflow teste encore le dossier au lieu du Makefile de l'arbre"
grep -q 'cd buildroot-2026.02.3$' .github/workflows/build-iso.yml && \
    ok "le workflow telecharge l'arbre si son Makefile manque"
grep -q "make BR2_EXTERNAL=.. fritax_defconfig >/dev/null 2>&1 || true" .github/workflows/build-iso.yml \
    && ko "le workflow avale encore l'echec de la configuration ('|| true')"
[ "$ERREURS" -eq 0 ] && ok "les deux fichiers testent le bon objet et n'avalent plus d'erreur"

# ------------------------------------------------------------------
titre "[6/6] Tout compile, tous les tests passent"
if [ -x distro/valider-tout.sh ] || [ -f distro/valider-tout.sh ]; then
    BILAN=$(sh distro/valider-tout.sh 2>&1 | tail -6)
    case "$BILAN" in
        *"TOUT EST VERT"*) ok "valider-tout.sh : tout est vert" ;;
        *) ko "valider-tout.sh n'est pas vert :"; printf '%s\n' "$BILAN" | sed 's/^/         /' ;;
    esac
else
    ko "distro/valider-tout.sh absent"
fi

# ------------------------------------------------------------------
echo
if [ "$ERREURS" -eq 0 ]; then
    printf '\033[42;30m  TOUT EST BON, TU PEUX ENVOYER  \033[0m\n'
    exit 0
fi
printf '\033[41;97m  %d PROBLEME(S) : NE PAS ENVOYER  \033[0m\n' "$ERREURS"
exit 1
