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
