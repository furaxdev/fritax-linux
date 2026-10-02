#!/bin/bash
# ============================================================
#  Verifier une ISO Fritax Linux : est-ce qu'elle demarrera ?
#  Usage : ./verifier-iso.sh fritax-linux-1.0-nova.iso
# ============================================================
set -uo pipefail
ISO="${1:-fritax-linux-1.0-nova.iso}"
[ -f "$ISO" ] || { echo "ISO introuvable : $ISO"; exit 1; }

echo "== 1. Fichier =="
ls -lh "$ISO"
echo -n "   type : "; file "$ISO" 2>/dev/null | cut -d: -f2-

echo
echo "== 2. Contenu de l'image =="
MNT=$(mktemp -d)
if mount -o loop,ro "$ISO" "$MNT" 2>/dev/null; then
  echo "   racine de l'ISO :"
  ls -la "$MNT" | sed 's/^/     /' | head -12
  echo "   chargeur :"
  ls -la "$MNT/isolinux" 2>/dev/null | sed 's/^/     /' | head -8
  K=$(ls "$MNT"/isolinux/* 2>/dev/null | grep -iE 'vmlinuz|bzImage|kernel' | head -1)
  I=$(ls "$MNT"/isolinux/* 2>/dev/null | grep -iE 'initrd|initramfs' | head -1)
  echo
  echo "== 3. Amorcage =="
  if [ -n "$K" ]; then echo "   noyau : $(basename "$K") ($(du -h "$K" | cut -f1))"; else echo "   ✗ AUCUN NOYAU trouve"; fi
  if [ -n "$I" ]; then echo "   initrd : $(basename "$I") ($(du -h "$I" | cut -f1))"; else echo "   ✗ AUCUN INITRD trouve"; fi
  echo "   configuration du menu :"
  cat "$MNT"/isolinux/isolinux.cfg 2>/dev/null | sed 's/^/     /' | head -12
  echo
  echo "== 4. Notre systeme est-il dedans ? =="
  SFS=$(ls "$MNT"/*.squashfs "$MNT"/squashfs.img 2>/dev/null | head -1)
  [ -n "$SFS" ] && echo "   systeme compresse : $(basename "$SFS") ($(du -h "$SFS" | cut -f1))"
  if [ -n "$I" ]; then
    echo "   (on regarde dans l'initrd s'il contient notre bureau)"
    T=$(mktemp -d); cp "$I" "$T/initrd.img" 2>/dev/null
    case "$(file -b "$T/initrd.img" 2>/dev/null)" in
      *gzip*)  (cd "$T" && gzip -dc initrd.img > initrd.cpio 2>/dev/null) ;;
      *XZ*)    (cd "$T" && xz -dc initrd.img > initrd.cpio 2>/dev/null) ;;
      *Zstandard*|*Zstd*) (cd "$T" && zstd -dc initrd.img > initrd.cpio 2>/dev/null) ;;
      *)       cp "$T/initrd.img" "$T/initrd.cpio" 2>/dev/null ;;
    esac
    if [ -s "$T/initrd.cpio" ]; then
      (cd "$T" && cpio -it < initrd.cpio 2>/dev/null) > "$T/liste.txt"
      for f in usr/bin/fritax-shell usr/bin/fritax-terminal usr/bin/fritax-files \
               usr/bin/fritax-open usr/share/fritax/wallpaper.raw etc/init.d/S99fritax-shell; do
        if grep -q "$f" "$T/liste.txt" 2>/dev/null; then echo "   ✅ $f"; else echo "   ✗ $f MANQUANT"; fi
      done
      echo "   (initrd : $(wc -l < "$T/liste.txt") fichiers)"
    else
      echo "   ⚠️  impossible de lire l'initrd automatiquement"
    fi
    rm -rf "$T"
  fi
  umount "$MNT"
else
  echo "   ⚠️  montage impossible (droits ? essayer avec sudo) - on lit la structure brute :"
  if command -v isoinfo >/dev/null 2>&1; then isoinfo -J -l -i "$ISO" 2>/dev/null | head -20 | sed 's/^/     /'; fi
fi
rmdir "$MNT" 2>/dev/null

echo
echo "== 5. Amorcage BIOS present ? =="
if command -v isoinfo >/dev/null 2>&1; then
  isoinfo -d -i "$ISO" 2>/dev/null | grep -iE 'volume id|bootable|el torito' | sed 's/^/   /'
fi
dd if="$ISO" bs=1 skip=510 count=2 2>/dev/null | od -An -tx1 | grep -q '55  aa' && echo "   ✅ signature de demarrage BIOS (0x55AA)" || echo "   ⚠️  signature de demarrage non trouvee"
echo
echo "Verification terminee."
