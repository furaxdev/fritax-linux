#!/usr/bin/env python3
"""
Demarre l'ISO Fritax Linux dans QEMU, sans ecran, et prend des captures.

Le principe : on lance QEMU en mode "aucun affichage" mais avec une prise de
commande QMP. Par cette prise on peut, a tout moment :
  - prendre une capture de ce que la machine affiche (screendump),
  - appuyer sur des touches (send-key), comme si on tapait au clavier.
C'est ce qui permet de verifier le demarrage, l'ecran de connexion puis le
bureau, sans avoir la machine sous la main.

Usage : tester-demarrage.py <iso> <dossier-de-sortie>
"""
import json, os, socket, subprocess, sys, time

ISO = sys.argv[1] if len(sys.argv) > 1 else "fritax-linux.iso"
SORTIE = sys.argv[2] if len(sys.argv) > 2 else "/tmp/captures"
SOCK = "/tmp/qemu-qmp.sock"
JOURNAL = "/tmp/qemu-console.log"

os.makedirs(SORTIE, exist_ok=True)
if os.path.exists(SOCK):
    os.remove(SOCK)


def journalise(message):
    print("  " + message, flush=True)


# ---------------------------------------------------------------- QEMU
qemu = subprocess.Popen([
    "qemu-system-x86_64",
    "-m", "2048",
    "-cdrom", ISO,
    "-boot", "d",
    "-display", "none",
    "-vga", "std",
    "-serial", "file:" + JOURNAL,
    "-qmp", "unix:%s,server,nowait" % SOCK,
    "-no-reboot",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
journalise("QEMU lance (PID %d)" % qemu.pid)


def prise():
    """Se connecte a la prise de commande QMP."""
    for _ in range(60):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(SOCK)
            f = s.makefile("rw")
            f.readline()                       # message d'accueil
            f.write(json.dumps({"execute": "qmp_capabilities"}) + "\n")
            f.flush()
            f.readline()
            return f
        except Exception:
            time.sleep(1)
    raise SystemExit("impossible de se connecter a QEMU")


f = prise()
journalise("prise de commande connectee")


def commande(nom, **arguments):
    f.write(json.dumps({"execute": nom, "arguments": arguments}) + "\n")
    f.flush()
    return f.readline()


def capture(nom):
    chemin = os.path.join(SORTIE, nom + ".ppm")
    commande("screendump", filename=chemin)
    journalise("capture : " + nom)


def taper(texte, pause=0.05):
    """Envoie du texte touche par touche (codes clavier QEMU)."""
    for c in texte:
        commande("send-key", keys=[{"type": "qcode", "data": c}])
        time.sleep(pause)


def attente(secondes, quoi=""):
    journalise("attente %ds %s" % (secondes, quoi))
    time.sleep(secondes)


try:
    # 1) demarrage : menu d'amorcage puis noyau
    attente(45, "(demarrage)")
    capture("01-demarrage")

    # 2) chargement du systeme en memoire, arrivee sur l'ecran de connexion
    attente(75, "(ecran de connexion)")
    capture("02-ecran-connexion")

    # 3) on tape le compte et le mot de passe
    journalise("saisie du compte")
    taper("furax")
    commande("send-key", keys=[{"type": "qcode", "data": "ret"}])
    attente(6)
    taper("fritax")
    commande("send-key", keys=[{"type": "qcode", "data": "ret"}])
    attente(30, "(lancement du bureau)")
    capture("03-apres-connexion")

    # 4) on laisse le bureau s'installer
    attente(40)
    capture("04-bureau")

    # 5) on ouvre le lanceur au clavier, pour verifier qu'il repond
    commande("send-key", keys=[{"type": "qcode", "data": "esc"}])
    attente(3)
    capture("05-final")
finally:
    qemu.terminate()
    try:
        qemu.wait(timeout=20)
    except Exception:
        qemu.kill()

# la console du noyau, ecrite sur le port serie : tres utile si rien ne s'affiche
if os.path.exists(JOURNAL):
    with open(JOURNAL, "r", errors="replace") as fh:
        lignes = fh.read().splitlines()[-40:]
    print("\n  --- 40 dernieres lignes de la console ---")
    for l in lignes:
        print("  " + l)
else:
    print("\n  (aucune sortie console)")

print("\n  captures dans :", SORTIE)
for n in sorted(os.listdir(SORTIE)):
    print("   ", n)
