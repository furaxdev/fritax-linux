# osh — OpenConnect Shell (réparé)

`osh` est l'outil de furaxdev : piloter un PC à distance **sans ouvrir de port,
sans ngrok, sans SSH**, en passant par un relais HTTPS.

## Ce qui avait cassé

Le relais d'origine était un projet **Supabase** (`vohddkxqdeivqcoogtzd.supabase.co`).
Ce projet a été supprimé : son domaine ne résout plus (HTTP 000). Tout le reste du
code était bon — seule la « boîte aux lettres » avait disparu.

## La réparation

Le transport passe désormais par **ntfy.sh** (service public, sans compte). Le
principe est identique :

```
opérateur ──POST commande──▶ ntfy.sh/<canal>-c ◀──écoute── PC cible (osh-server)
opérateur ◀──écoute résultat── ntfy.sh/<canal>-r ◀──POST résultat── PC cible
```

- Les deux côtés ne font que de l'**HTTPS sortant** → aucun port ouvert, ça marche
  derrière n'importe quelle box ;
- **aucune limite de temps** (contrairement aux tunnels gratuits type pinggy : 1 h) ;
- le **secret n'est jamais transmis** : le nom du canal = `SHA-256(secret)[:20]` ;
- le secret est le même qu'avant : `fritax-osh-yqm-GYEwgreebX4Jl3VlE8HQ`.
  → Il vit aussi dans `~/.fritax-relay-secret` sur la machine de l'agent.

## Fichiers

| Fichier | Rôle |
|---|---|
| `osh` | le client (interface d'origine : `--ip=…@… --password=… [commande]`, mode interactif si pas de commande) |
| `client.py` | l'envoi de la commande + l'attente du résultat (connexion continue, pas de sondage) |
| `osh-server.py` | le serveur, à lancer sur le PC cible : `python3 -u osh-server.py --secret …` |
| `server.py` | identique à `osh-server.py` (ancien nom conservé) |

## Installation sur le PC cible

```sh
cd ~/windows12/tools/openconnect
cp -n osh osh.orig            # on garde l'ancien, au cas où
curl -sL https://dpaste.com/7V7SCCJ7E.txt -o osh
curl -sL https://dpaste.com/8HRPK9GVN.txt -o osh-server.py
curl -sL https://dpaste.com/E9FYT3SGV.txt -o client.py
chmod +x osh
setsid nohup bash -c 'while true; do python3 -u osh-server.py --secret "fritax-osh-yqm-GYEwgreebX4Jl3VlE8HQ"; sleep 3; done' > ~/osh-server.log 2>&1 < /dev/null &
```

## Utilisation

```sh
./osh --ip=monpc@furax "uname -srm"        # une commande
./osh --ip=monpc@furax --password=…        # shell interactif
```

⚠️ Pièges appris à la dure : `pkill -f 'server.py'` tue **aussi la commande qui
contient ce texte** (utiliser `pkill -f 'server[.]py'`) ; et lancer le serveur avec
`python3 -u` (sinon le journal reste vide et on croit qu'il n'a pas démarré).
