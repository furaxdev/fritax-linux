#!/bin/bash
# ============================================================
#  Construction de l'ISO Fritax Linux
#  Usage :  ./build.sh [buildroot.tar.gz|version]
#  Tout est compile depuis les sources (noyau, compilateur,
#  systeme de base, nos paquets). Compte 1 a 3 heures.
# ============================================================
set -e
# --- outils installes en local (utile quand il n'y a pas de sudo, hors CI) ---
FTOOLS="$HOME/fritax-tools/usr"
if [ -d "$FTOOLS" ]; then
  echo "== outils locaux trouves dans $FTOOLS =="
  export TMPDIR="$HOME/build-tmp"
  mkdir -p "$TMPDIR"
  export CPATH="$FTOOLS/include${CPATH:+:$CPATH}"
  export LIBRARY_PATH="$FTOOLS/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
  export LD_LIBRARY_PATH="$FTOOLS/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export PATH="$FTOOLS/bin${PATH:+:$PATH}"
fi
# nombre de compilations en parallele : surchargeable (CI = JOBS=$(nproc))
JOBS="${JOBS:-2}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BR_VER="${1:-2026.02.3}"
BR_DIR="${HERE}/buildroot-${BR_VER}"

echo "== 1/4 : outils necessaires =="
MISSING=""
for t in make gcc g++ bison flex wget cpio unzip rsync bc python3; do
  command -v "$t" >/dev/null 2>&1 || MISSING="$MISSING $t"
done
if [ -n "$MISSING" ]; then
  echo "Il manque :$MISSING"
  echo "Installe-les, par exemple :"
  echo "  sudo apt-get install -y build-essential git bison flex wget cpio unzip rsync bc python3 libncurses-dev"
  exit 1
fi

echo "== 2/4 : Buildroot ${BR_VER} =="
if [ ! -d "$BR_DIR" ]; then
  TAR="${HERE}/buildroot-${BR_VER}.tar.gz"
  [ -f "$TAR" ] || wget -O "$TAR" "https://buildroot.org/downloads/buildroot-${BR_VER}.tar.gz"
  tar -C "$HERE" -xzf "$TAR"
fi

echo "== 3/4 : configuration de Fritax Linux =="
cd "$BR_DIR"
make BR2_EXTERNAL="$HERE" fritax_defconfig

# --- 2e passe : ne pas dependre de l'ORDRE des lignes de la defconfig ---
# Une option dont la dependance n'est pas encore activee est ignoree EN SILENCE
# (ex. « en-tetes du noyau comme le noyau » vit dans « if BR2_LINUX_KERNEL »).
# On reaffirme les options critiques sur la config deja resolue, puis on relance
# la resolution : l'option tient des lors que sa dependance est satisfaite.
cat >> .config <<'OPTIONS_CRITIQUES'
BR2_KERNEL_HEADERS_AS_KERNEL=y
BR2_TOOLCHAIN_BUILDROOT_GLIBC=y
BR2_TOOLCHAIN_BUILDROOT_CXX=y
BR2_PACKAGE_FRITAX_BRANDING=y
BR2_PACKAGE_FRITAX_SHELL=y
BR2_PACKAGE_FRITAX_TERMINAL=y
BR2_PACKAGE_FRITAX_FILES=y
BR2_PACKAGE_FRITAX_TUNNEL=y
OPTIONS_CRITIQUES
make olddefconfig >/dev/null

# --- verification : echouer TOUT DE SUITE plutot qu'apres 40 minutes ---
ERREUR=""
grep -q '^BR2_KERNEL_HEADERS_AS_KERNEL=y' .config || ERREUR="$ERREUR en-tetes-noyau"
grep -q '^BR2_TOOLCHAIN_BUILDROOT_GLIBC=y' .config || ERREUR="$ERREUR glibc"
grep -q '^BR2_LINUX_KERNEL=y' .config              || ERREUR="$ERREUR noyau"
for p in BRANDING SHELL TERMINAL FILES TUNNEL; do
  grep -q "^BR2_PACKAGE_FRITAX_$p=y" .config || ERREUR="$ERREUR fritax-$p"
done
if [ -n "$ERREUR" ]; then
  echo
  echo "*** CONFIGURATION INCORRECTE, options absentes :$ERREUR"
  echo "    (Buildroot les a ignorees : une dependance n'est pas satisfaite)"
  echo "    bibliotheque C retenue : $(grep -E '^BR2_TOOLCHAIN_BUILDROOT_(GLIBC|UCLIBC|MUSL)=y' .config | head -1)"
  exit 1
fi
echo "   configuration verifiee : glibc + noyau 6.12 + nos 5 paquets"

echo "== 4/4 : compilation (long...) sur $JOBS taches"
make -j"$JOBS"

ISO=$(ls -1 "${BR_DIR}"/output/images/*.iso 2>/dev/null | head -1)
if [ -n "$ISO" ]; then
  cp -f "$ISO" "${HERE}/fritax-linux-1.0-nova.iso"
  echo
  echo "ISO PRETE : ${HERE}/fritax-linux-1.0-nova.iso"
  ls -lh "${HERE}/fritax-linux-1.0-nova.iso"
else
  echo "Pas d'ISO produite - regarde les erreurs ci-dessus."
  exit 1
fi
