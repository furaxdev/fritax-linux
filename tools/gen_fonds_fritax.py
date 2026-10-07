#!/usr/bin/env python3
"""
Fonds d'ecran de Fritax Linux 1.2 — style Windows 11.

On ne fait PAS un flou. La structure du fond Windows 11, c'est un RUBAN qui
s'enroule en spirale autour du centre (le "Bloom") : une bande large et
courbe, avec
  - des bords nets (ce n'est pas une tache),
  - un biseau : le bord superieur prend la lumiere, l'interieur reste dans
    l'ombre, ce qui donne l'epaisseur,
  - une ombre portee la ou le ruban passe devant le fond,
  - cinq copies tournees qui se recouvrent, la plus recente par-dessus.
C'est cette STRUCTURE qu'on reproduit ici, pas une impression de flou.

    python3 tools/gen_fonds_fritax.py
"""
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SORTIE = os.path.join(RACINE, "distro", "board", "fritax", "rootfs-overlay", "usr", "share", "fritax")
LARGEUR, HAUTEUR = 1366, 768
MINI_W, MINI_H = 280, 158
SS = 2                      # suréchantillonnage : on dessine 2x plus grand puis on réduit


# ---------------------------------------------------------------- ecriture
def ecrire_fx(chemin, W, H, trame):
    """Format FXRLE : "FXRLE <largeur> <hauteur>\\n" puis des paquets
    <1 octet = nombre de pixels moins un> <3 octets = R V B>.
    Les teintes sont arrondies sur 32 niveaux : sans cet arrondi, un degrade
    Fluent n'a aucune plage repetee et le RLE ne gagne rien."""
    trame = ((trame >> 3) << 3 | 4).astype(np.uint8)
    plat = trame.reshape(-1, 3)
    paquets = bytearray()
    for r, v, b in plat:
        paquets += bytes((0, int(r), int(v), int(b)))
    # compression : on fusionne les paquets identiques consecutifs
    sortie = bytearray()
    i, total = 0, len(plat)
    while i < total:
        c = plat[i]
        n = 1
        while i + n < total and tuple(plat[i + n]) == tuple(c) and n < 256:
            n += 1
        sortie += bytes((n - 1, int(c[0]), int(c[1]), int(c[2])))
        i += n
    entete = ("FXRLE %d %d\n" % (W, H)).encode("ascii")
    with open(chemin, "wb") as f:
        f.write(entete)
        f.write(bytes(sortie))
    return len(entete) + len(sortie)


def reduire(trame, nw, nh):
    return np.array(Image.fromarray(trame).resize((nw, nh), Image.LANCZOS))


# ---------------------------------------------------------------- le ruban
def masque_petale(W, H, angle_deg, enroulement=1.35, longueur=0.78,
                  largeur0=0.030, largeur1=0.135, pivot=0.56):
    """Dessine UN petale de ruban et renvoie son masque (L).

    La courbe s'enroule autour du centre : on part du milieu, on tourne en
    s'eloignant, et la bande s'elargit puis s'affine a la pointe — c'est ce
    qui fait la feuille de Bloom plutot qu'un trait.
    """
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    cx, cy = W / 2.0, H * 0.47
    pas = 260
    gauche, droite = [], []
    for k in range(pas + 1):
        t = k / pas
        a = math.radians(angle_deg) + t * enroulement * math.tau * 0.42
        r = (t ** pivot) * longueur * W * 0.62
        # la bande est large au milieu, fine aux deux bouts
        lw = (largeur0 + (largeur1 - largeur0) * math.sin(math.pi * min(1.0, t * 1.08)) ** 0.7)
        lw *= W
        nx, ny = -math.sin(a), math.cos(a)          # normale a la courbe
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r
        gauche.append((x + nx * lw / 2, y + ny * lw / 2))
        droite.append((x - nx * lw / 2, y - ny * lw / 2))
    d.polygon(gauche + droite[::-1], fill=255)
    return m


def composer_petale(canevas, masque, couleur_haut, couleur_bas, lumiere=0.55, ombre=0.42):
    """Compose un petale sur le canevas numpy (float 0..255) :
       - un biseau (bord eclairci, coeur assombri) donne l'epaisseur,
       - une ombre portee decalee le detache du fond."""
    H, W = masque.size[1], masque.size[0]
    # ombre portee : masque decale + flou
    om = masque.filter(ImageFilter.GaussianBlur(W * 0.006))
    om = om.transform(om.size, Image.AFFINE, (1, 0, -W * 0.012, 0, 1, H * 0.018), resample=Image.BILINEAR)
    a_ombre = (np.asarray(om, dtype=np.float32) / 255.0)[..., None] * ombre
    canevas *= (1.0 - a_ombre)
    # biseau : bord clair (dilatation) et coeur sombre
    bord = np.asarray(masque.filter(ImageFilter.MaxFilter(9)), dtype=np.float32) / 255.0
    alpha = np.asarray(masque, dtype=np.float32) / 255.0
    liseré = np.clip(bord - alpha, 0, 1)[..., None]
    # degrade le long du petale : plus clair vers le haut du disque
    yy = np.linspace(0.0, 1.0, H, dtype=np.float32)[:, None, None]
    coul = (np.array(couleur_haut, np.float32) * (1 - yy) +
            np.array(couleur_bas, np.float32) * yy)
    coul = coul + liseré * lumiere * 255.0
    a = alpha[..., None]
    canevas = canevas * (1 - a) + coul * a
    return canevas


def fond_uni(W, H, haut, bas):
    yy = np.linspace(0.0, 1.0, H, dtype=np.float32)[:, None, None]
    return (np.array(haut, np.float32) * (1 - yy) + np.array(bas, np.float32) * yy) * np.ones((H, W, 1), np.float32)


def bloom(W, H, fond_haut, fond_bas, petales, base_angle=18.0, enroulement=1.35):
    """Le Bloom complet : fond + les petales tournes qui se recouvrent."""
    Ws, Hs = W * SS, H * SS
    canevas = fond_uni(Ws, Hs, fond_haut, fond_bas)
    for (angle, coul_haut, coul_bas, lum, omb, long, larg1) in petales:
        m = masque_petale(Ws, Hs, angle + base_angle, enroulement=enroulement,
                          longueur=long, largeur1=larg1)
        canevas = composer_petale(canevas, m, coul_haut, coul_bas, lum, omb)
    img = Image.fromarray(np.clip(canevas, 0, 255).astype(np.uint8))
    return np.array(img.resize((W, H), Image.LANCZOS))


# ---------------------------------------------------------------- les fonds
def bloom_clair():
    # bleus Fluent : #E8F1FB -> #2B7CD3, biseau tres lumineux
    petales = [(0,   (250, 253, 255), (120, 180, 235), 0.75, 0.30, 0.80, 0.150),
               (72,  (245, 250, 255), (150, 200, 245), 0.70, 0.30, 0.70, 0.130),
               (144, (250, 253, 255), (110, 170, 232), 0.72, 0.28, 0.62, 0.115),
               (216, (248, 252, 255), (170, 212, 248), 0.65, 0.26, 0.52, 0.100),
               (288, (252, 254, 255), (135, 190, 240), 0.68, 0.24, 0.44, 0.088)]
    return bloom(LARGEUR, HAUTEUR, (247, 250, 254), (226, 238, 250), petales)


def bloom_sombre():
    petales = [(0,   (120, 170, 225), (28, 62, 108), 0.55, 0.55, 0.80, 0.150),
               (72,  (95, 140, 200),  (22, 50, 90),  0.50, 0.55, 0.70, 0.130),
               (144, (110, 160, 220), (30, 68, 118), 0.52, 0.52, 0.62, 0.115),
               (216, (85, 130, 195),  (20, 46, 86),  0.46, 0.50, 0.52, 0.100),
               (288, (100, 150, 210), (26, 56, 100), 0.48, 0.48, 0.44, 0.088)]
    return bloom(LARGEUR, HAUTEUR, (32, 34, 40), (20, 22, 28), petales)


def lavande():
    petales = [(0,   (252, 250, 255), (168, 150, 230), 0.70, 0.26, 0.76, 0.140),
               (90,  (250, 248, 255), (190, 175, 240), 0.62, 0.24, 0.60, 0.115),
               (200, (252, 251, 255), (150, 132, 220), 0.66, 0.22, 0.48, 0.095)]
    return bloom(LARGEUR, HAUTEUR, (251, 250, 254), (238, 235, 250), petales)


def gris_doux():
    petales = [(0,   (253, 254, 255), (214, 220, 230), 0.55, 0.20, 0.78, 0.145),
               (110, (252, 253, 255), (226, 231, 239), 0.48, 0.18, 0.58, 0.110)]
    return bloom(LARGEUR, HAUTEUR, (250, 251, 253), (240, 242, 246), petales)


def aqua():
    petales = [(0,   (250, 255, 255), (105, 205, 215), 0.72, 0.26, 0.78, 0.145),
               (80,  (248, 254, 255), (140, 220, 228), 0.62, 0.24, 0.62, 0.118),
               (190, (250, 255, 255), (85, 190, 205), 0.66, 0.22, 0.48, 0.095)]
    return bloom(LARGEUR, HAUTEUR, (248, 253, 254), (228, 244, 248), petales)


def soleil_couchant():
    petales = [(0,   (255, 248, 240), (245, 150, 110), 0.75, 0.26, 0.78, 0.145),
               (85,  (255, 250, 244), (250, 180, 130), 0.66, 0.24, 0.62, 0.118),
               (195, (255, 250, 246), (240, 130, 120), 0.68, 0.22, 0.48, 0.095)]
    return bloom(LARGEUR, HAUTEUR, (255, 252, 248), (252, 240, 230), petales)


FONDS = [("fond-1", bloom_clair, "Bloom clair"),
         ("fond-2", bloom_sombre, "Bloom sombre"),
         ("fond-3", gris_doux, "Gris doux"),
         ("fond-4", lavande, "Lavande"),
         ("fond-5", aqua, "Aqua"),
         ("fond-6", soleil_couchant, "Soleil couchant")]


def main():
    os.makedirs(SORTIE, exist_ok=True)
    total = 0
    for nom, fabrique, libelle in FONDS:
        trame = fabrique()
        assert trame.shape[:2] == (HAUTEUR, LARGEUR), f"{nom} : taille incorrecte"
        total += ecrire_fx(os.path.join(SORTIE, f"{nom}.fx"), LARGEUR, HAUTEUR, trame)
        total += ecrire_fx(os.path.join(SORTIE, f"{nom}-mini.fx"), MINI_W, MINI_H, reduire(trame, MINI_W, MINI_H))
        print(f"  {nom}.fx  ({libelle})")
    print(f"total des 12 fichiers : {total/1024:.0f} Ko")


if __name__ == "__main__":
    main()
