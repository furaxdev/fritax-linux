#!/usr/bin/env python3
"""
Verifie le demarrage de l'ISO Fritax Linux dans QEMU, sans ecran.

Methode : on donne a la machine un port serie, et on lui parle par la. On
envoie les commandes en texte, on lit les reponses en texte. Plus besoin de
simuler un clavier francais ni de dechiffrer des captures d'ecran : tout est
lisible, et en cas d'echec on sait enfin ce que la machine a dit.

On garde malgre tout des captures d'ecran, parce que la seule facon de savoir
si un bureau graphique s'affiche vraiment, c'est de le regarder.

Usage : tester-demarrage.py <iso> <dossier-de-sortie>
"""

import json
import os
import socket
import subprocess
import sys
import time

ISO = sys.argv[1] if len(sys.argv) > 1 else "fritax-linux.iso"
SORTIE = sys.argv[2] if len(sys.argv) > 2 else "/tmp/captures"
SERIE = "/tmp/qemu-serie.sock"
PRISE = "/tmp/qemu-qmp.sock"

os.makedirs(SORTIE, exist_ok=True)
for chemin in (SERIE, PRISE):
    if os.path.exists(chemin):
        os.remove(chemin)

TOUT = []            # tout ce que la machine a repondu, dans l'ordre


def journalise(message):
    print("  " + message, flush=True)


# ---------------------------------------------------------------- la machine
qemu = subprocess.Popen([
    "qemu-system-x86_64",
    "-m", "2048",
    "-cdrom", ISO,
    "-boot", "d",
    "-display", "none",
    "-vga", "std",
    "-serial", "unix:%s,server,nowait" % SERIE,
    "-qmp", "unix:%s,server,nowait" % PRISE,
    "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
journalise("machine lancee (pid %d)" % qemu.pid)


def connecter(chemin, delai=90):
    for _ in range(delai):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(chemin)
            s.settimeout(0.4)
            return s
        except Exception:
            time.sleep(1)
    raise SystemExit("connexion impossible : " + chemin)


serie = connecter(SERIE)
journalise("console serie connectee")


def lire(secondes=2):
    """Lit tout ce que la machine envoie pendant N secondes."""
    fin = time.time() + secondes
    morceaux = []
    while time.time() < fin:
        try:
            donnees = serie.recv(8192)
            if not donnees:
                break
            morceaux.append(donnees)
            fin = time.time() + 0.4        # on prolonge tant qu'il arrive du texte
        except socket.timeout:
            continue
    texte = b"".join(morceaux).decode("utf-8", "replace")
    if texte:
        TOUT.append(texte)
    return texte


def ecrire(texte):
    """Envoie une ligne a la machine, comme au clavier."""
    serie.sendall(texte.encode())
    time.sleep(0.4)


def attendre(motif, maximum=240, quoi=""):
    """Attend un motif dans ce que la machine raconte."""
    debut = time.time()
    vu = ""
    while time.time() - debut < maximum:
        vu += lire(2)
        if motif in vu:
            journalise("vu : %s%s" % (motif, (" (" + quoi + ")") if quoi else ""))
            return True
    journalise("PAS VU : %s%s" % (motif, (" (" + quoi + ")") if quoi else ""))
    return False


def commande(texte, attente=4):
    journalise("$ " + texte)
    ecrire(texte + "\n")
    return lire(attente)


# ------------------------------------------------------- captures d'ecran
def prise_qmp():
    try:
        s = connecter(PRISE, 30)
        f = s.makefile("rw")
        f.readline()
        f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n")
        f.flush()
        f.readline()
        return f
    except Exception:
        return None


qmp = prise_qmp()


def capture(nom):
    if not qmp:
        return
    chemin = os.path.join(SORTIE, nom + ".ppm")
    try:
        qmp.write(json.dumps({"execute": "screendump",
                              "arguments": {"filename": chemin}}) + "\n")
        qmp.flush()
        qmp.readline()
        journalise("capture : " + nom)
    except Exception as e:
        journalise("capture impossible : %s" % e)


try:
    # 1) demarrage : menu d'amorcage, noyau, init
    attendre("login:", 300, "invite de connexion")
    capture("01-invite-de-connexion")

    commande("root")
    attendre("Password:", 60, "demande de mot de passe")
    commande("fritax")
    attendre("#", 60, "invite du shell")
    capture("02-shell-obtenu")

    # 2) qui tourne, et sur quelle carte ?
    commande("ps")
    capture("03-processus")

    # 3) le bureau a-t-il pris l'affichage ?
    commande("ls -l /dev/dri")
    capture("04-peripheriques")

    # 4) son journal, du debut et de la fin
    commande("head -30 /var/log/fritax-shell.log")
    capture("05-journal-debut")
    commande("tail -30 /var/log/fritax-shell.log")
    capture("06-journal-fin")

    # 5) le bureau tient-il dans le temps ?
    time.sleep(15)
    capture("07-bureau-apres-15-secondes")
    commande("ps")
    capture("08-processus-apres")

finally:
    # On ecrit tout ce que la machine a dit : c'est le document le plus utile
    # du test, et il est lisible sans rien deviner.
    with open(os.path.join(SORTIE, "console.txt"), "w", errors="replace") as f:
        f.write("".join(TOUT))
    journalise("console enregistree (%d caracteres)" % len("".join(TOUT)))

    qemu.terminate()
    try:
        qemu.wait(timeout=20)
    except Exception:
        qemu.kill()
