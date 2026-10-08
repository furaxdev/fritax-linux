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
    """Envoie du texte touche par touche.

    Attention : la console de Fritax est en AZERTY (le script S10clavier charge
    la disposition francaise). Or QEMU envoie des *positions* de touches d'un
    clavier americain. Il faut donc traduire chaque caractere voulu vers la
    touche qui le produit reellement sur un clavier francais — sinon "cat"
    devient "cqt" et le mot de passe "fritax" devient "fritqx".
    """
    # lettres deplacees par l'AZERTY
    deplace = {"a": "q", "q": "a", "z": "w", "w": "z", "m": "semicolon"}
    # caracteres obtenus par une autre touche (avec ou sans majuscule).
    # Sur un clavier francais, ">" et "<" sont sur la touche supplementaire a
    # cote du shift gauche (que QEMU appelle "less"), et "&" est la touche "1"
    # sans majuscule.
    autres = {
        " ": ("spc", False), "/": ("dot", True), ".": ("comma", True),
        "-": ("6", False), "=": ("equal", False), "_": ("8", False),
        "'": ("4", False), ":": ("dot", False), "&": ("1", False),
        ">": ("less", True), "<": ("less", False), ";": ("comma", False),
    }
    for c in texte:
        if c.isdigit():
            # sur un clavier francais, les chiffres sont en Majuscule
            commande("send-key", keys=[{"type": "qcode", "data": "shift"},
                                       {"type": "qcode", "data": c}])
            time.sleep(pause)
            continue
        if c in deplace:
            commande("send-key", keys=[{"type": "qcode", "data": deplace[c]}])
        elif c in autres:
            touche, maj = autres[c]
            suite = ([{"type": "qcode", "data": "shift"}] if maj else []) + \
                    [{"type": "qcode", "data": touche}]
            commande("send-key", keys=suite)
        else:
            commande("send-key", keys=[{"type": "qcode", "data": c}])
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

    # 6) lancement du bureau. On ne redirige PAS la sortie et on ne met pas en
    #    arriere-plan : taper ">" et "&" sur un clavier francais demandait des
    #    combinaisons de touches que le test ratait (c'est pourquoi il n'a jamais
    #    lance le bureau). Ici il tourne au premier plan : on regarde l'ecran,
    #    puis QEMU est arrete de toute facon a la fin du test.
    journalise("lancement du bureau (au premier plan)")
    taper("fritax-shell"); entree()
    attente(12)
    capture("06-bureau-12-secondes")
    attente(20)
    capture("07-bureau-32-secondes")

    # 7) on l'arrete et on lit son journal interne
    journalise("arret du bureau")
    commande("send-key", keys=[{"type": "qcode", "data": "ctrl"},
                               {"type": "qcode", "data": "c"}])
    attente(4)
    capture("08-apres-arret")
    taper("cat /root/.fritax-shell.log"); entree()
    attente(6)
    capture("09-journal-interne-du-bureau")
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
