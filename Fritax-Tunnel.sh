#!/bin/bash
# ============================================================
#  FRITAX TUNNEL - ouvrir l'acces a ce PC (double-clic)
#  Aucun mot de passe a taper : c'est le tunnel qui demande
#  un mot de passe aux visiteurs.
# ============================================================
BIN="$HOME/fritax-tunnel-static"
[ -x "$BIN" ] || BIN="$HOME/fritax-tunnel/fritax-tunnel-static"
[ -x "$BIN" ] || BIN="$HOME/.local/bin/fritax-tunnel"

if [ ! -x "$BIN" ]; then
  kdialog --error "Fritax Tunnel introuvable.\nCherche 'fritax-tunnel-static' dans ~/" 2>/dev/null || echo "binaire introuvable"
  exit 1
fi

DEFAUT="$(cat "$HOME/fritax-relais.txt" 2>/dev/null)"
ADRESSE=$(kdialog --inputbox "Adresse du relais Fritax (fournie par l'assistant) :" "$DEFAUT" \
          --title "Fritax Tunnel") || exit 0
[ -z "$ADRESSE" ] && exit 0
echo "$ADRESSE" > "$HOME/fritax-relais.txt"

MDP="fritax-$(head -c 6 /dev/urandom | base64 | tr -dc 'a-z0-9' | head -c 12)"
LOG="$HOME/fritax-tunnel.log"

pkill -f fritax-tunnel 2>/dev/null
sleep 0.5
setsid nohup "$BIN" expose --relay "$ADRESSE" --password "$MDP" --target 127.0.0.1:22 \
      > "$LOG" 2>&1 < /dev/null &
sleep 3

if grep -q "tunnel ouvert" "$LOG" 2>/dev/null || grep -q "mot de passe" "$LOG" 2>/dev/null; then
  kdialog --title "Fritax Tunnel" --msgbox "TUNNEL OUVERT ✅\n\nMot de passe a envoyer a l'assistant :\n\n    $MDP\n\nLe tunnel reste actif tant que ce PC est allume.\n(Ouvre la console : journal dans ~/fritax-tunnel.log)"
else
  kdialog --error "Le tunnel n'a pas pu s'ouvrir :\n\n$(tail -4 "$LOG" 2>/dev/null)" --title "Fritax Tunnel"
fi
