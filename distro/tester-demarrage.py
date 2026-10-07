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


def taper(texte, pause=0.06):
    """Envoie du texte touche par touche, en traduisant les caracteres qui ne
    sont pas de simples lettres (QEMU utilise des noms de touches)."""
    nom = {" ": "spc", "/": "slash", ".": "dot", "-": "minus",
           "=": "equal", "_": "shift-minus", "|": "shift-backslash"}
    for c in texte:
        touche = nom.get(c, c)
        if touche.startswith("shift-"):
            commande("send-key", keys=[{"type": "qcode", "data": "shift"},
                                       {"type": "qcode", "data": touche[6:]}])
        else:
            commande("send-key", keys=[{"type": "qcode", "data": touche}])
        time.sleep(pause)


def entree():
    commande("send-key", keys=[{"type": "qcode", "data": "ret"}])


def attente(secondes, quoi=""):
    journalise("attente %ds %s" % (secondes, quoi))
    time.sleep(secondes)


try:
    # 1) demarrage : menu d'amorcage puis noyau
    attente(45, "(demarrage)")
    capture("01-demarrage")

    # 2) chargement du systeme en memoire, arrivee sur l'invite de connexion
    attente(75, "(connexion)")
    capture("02-invite-de-connexion")

    # 3) on se connecte en root au prompt texte, pour pouvoir diagnostiquer
    journalise("connexion root")
    taper("root"); entree()
    attente(8)
    taper("fritax"); entree()
    attente(10)
    capture("03-shell-obtenu")

    # 4) pourquoi l'ecran de connexion graphique ne prend-il pas la main ?
    journalise("lecture du journal de l'ecran de connexion")
    taper("cat /var/log/fritax-login.log"); entree()
    attente(6)
    capture("04-journal-ecran-de-connexion")

    # 5) les peripheriques dont nos programmes ont besoin sont-ils la ?
    journalise("verification des peripheriques")
    taper("ls -l /dev/dri /dev/input"); entree()
    attente(6)
    capture("05-peripheriques")

    # 6) on lance l'ecran de connexion a la main : l'erreur s'affichera
    journalise("lancement direct de l'ecran de connexion")
    taper("fritax-login"); entree()
    attente(25)
    capture("06-lancement-direct-ecran-connexion")

    # 7) et le bureau tout seul
    journalise("lancement direct du bureau")
    taper("fritax-shell"); entree()
    attente(30)
    capture("07-lancement-direct-bureau")
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
