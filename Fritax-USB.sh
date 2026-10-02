#!/bin/bash
# ============================================================
#  FRITAX LINUX - mettre l'ISO sur une cle USB (double-clic)
#  ATTENTION : la cle selectionnee sera EFFACEE.
# ============================================================
ISO=$(ls "$HOME"/fritax-linux/distro/*.iso "$HOME"/fritax-linux/distro/buildroot-*/output/images/*.iso 2>/dev/null | head -1)
if [ -z "$ISO" ]; then
  ISO=$(kdialog --getopenfilename "$HOME" "*.iso|Image ISO (*.iso)" --title "Choisir l'ISO Fritax") || exit 0
fi
[ -f "$ISO" ] || exit 0

LISTE=$(lsblk -dno NAME,SIZE,TYPE,TRAN,MODEL 2>/dev/null | grep -E "disk" | grep -v "loop")
CHOIX=$(kdialog --menu "ATTENTION : la cle choisie sera EFFACEE.\n\nTransport contenant :\n\n$LISTE" \
        $(lsblk -dno NAME 2>/dev/null | grep -E "^sd[b-z]$|^nvme[0-9]n[0-9]$" | while read d; do echo "$d"; echo "$(lsblk -dno SIZE,MODEL /dev/$d 2>/dev/null)"; done) \
        --title "Fritax Linux - ecriture USB") || exit 0
[ -z "$CHOIX" ] && exit 0

kdialog --yesno "Confirmation finale : ECRIRE l'ISO sur /dev/$CHOIX ?\n\nTout le contenu de ce disque sera perdu." --title "Confirmation" || exit 0

MDP=$(kdialog --password "Mot de passe administrateur :" --title "Fritax Linux")
echo "$MDP" | sudo -S -p "" bash -c "dd if='$ISO' of=/dev/$CHOIX bs=4M status=progress oflag=sync conv=fsync" 2> "$HOME/fritax-usb.log"
if [ $? -eq 0 ]; then
  kdialog --msgbox "C'EST FAIT ✅\n\nLa cle USB contient Fritax Linux.\nTu peux demarrer le PC dessus." --title "Fritax Linux"
else
  kdialog --error "Echec de l'ecriture. Journal :\n$(tail -5 "$HOME/fritax-usb.log")" --title "Fritax Linux"
fi
