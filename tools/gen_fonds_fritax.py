#!/usr/bin/env python3
"""
Fonds d'ecran de Fritax Linux 1.2, style Fluent / Windows 11.

On quitte le neon au profit de ce qui fait le look Windows 11 :
  - fonds clairs et doux (gris tres pale, bleu lavande),
  - une grande forme abstraite floue au centre (le fameux "Bloom"),
  - des courbes lisses, aucune saturation, aucun contour dur,
  - une variante sombre digne du theme sombre de Windows 11.

Ecrits au format FXRLE (compresse, lu par le moteur) : ~3 Mo en brut
deviennent une vingtaine de kilo-octets, et le systeme etant charge
entierement en RAM, c'est de la memoire vive gagnee aussi.

    python3 tools/gen_fonds_fritax.py
"""
import math, os

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SORTIE = os.path.join(RACINE, "distro", "board", "fritax", "rootfs-overlay", "usr", "share", "fritax")
LARGEUR, HAUTEUR = 1366, 768
MINI_W, MINI_H = 280, 158


def ecrire_fx(chemin, W, H, pixels):
    """Format FXRLE : "FXRLE <largeur> <hauteur>\\n" puis des paquets
    <1 octet = nombre de pixels moins un> <3 octets = R V B>."""
    # On arrondit chaque composante sur 32 niveaux (pas de 8) avant de
    # compresser. Un degrade Fluent est par nature fait de milliers de
    # teintes presque identiques : sans arrondi, chaque pixel differe de son
    # voisin et le RLE ne trouve aucune plage a compresser (4 Mo de fichiers).
    # Avec l'arrondi, les plages se forment et la difference est invisible :
    # l'oeil ne distingue pas 254 de 248 sur un fond tres clair.
    pixels = [((p[0] >> 3) << 3 | 4, (p[1] >> 3) << 3 | 4, (p[2] >> 3) << 3 | 4)
              for p in pixels]
    paquets, i, total = [], 0, W * H
    while i < total:
        couleur = pixels[i]
        n = 1
        while i + n < total and pixels[i + n] == couleur and n < 256:
            n += 1
        paquets.append(bytes((n - 1, couleur[0], couleur[1], couleur[2])))
        i += n
    entete = ("FXRLE %d %d\n" % (W, H)).encode("ascii")
    with open(chemin, "wb") as f:
        f.write(entete)
        f.write(b"".join(paquets))
    return len(entete) + sum(len(p) for p in paquets)


def melange(a, b, t):
    t = 0.0 if t < 0 else (1.0 if t > 1 else t)
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def lisser(t):
    """Courbe douce (cosinus) : aucune arête, comme dans Fluent."""
    return 0.5 - 0.5 * math.cos(math.pi * max(0.0, min(1.0, t)))


def bloom(W, H, fond_haut, fond_bas, petales, intensite=1.0, halo=None):
    """Le "Bloom" : plusieurs nappes douces qui se recouvrent au centre,
    chacune un degrade radial tres diffus. C'est ce qui donne l'effet
    Fluent : des formes floues, jamais de bord net."""
    cx, cy = 0.5, 0.44
    pixels = []
    for y in range(H):
        ty = y / (H - 1)
        base = melange(fond_haut, fond_bas, lisser(ty * 1.15))
        for x in range(W):
            c = base
            rx = (x / (W - 1) - cx) * (W / H)     # on garde le cercle rond
            ry = y / (H - 1) - cy
            for (pcx, pcy, rayon, couleur, force) in petales:
                dx, dy = rx - pcx, ry - pcy
                d = math.sqrt(dx * dx + dy * dy) / rayon
                if d < 1.0:
                    c = melange(c, couleur, lisser((1.0 - d) * 1.6) * force * intensite)
            if halo:
                dx, dy = rx - halo[0], ry - halo[1]
                d = math.sqrt(dx * dx + dy * dy) / halo[2]
                if d < 1.0:
                    c = melange(c, halo[3], lisser(1.0 - d) * 0.55 * intensite)
            pixels.append(tuple(int(max(0, min(255, v))) for v in c))
    return pixels


def reduire(pixels, W, H, nw, nh):
    out = []
    for y in range(nh):
        y0, y1 = y * H // nh, max(y * H // nh + 1, (y + 1) * H // nh)
        for x in range(nw):
            x0, x1 = x * W // nw, max(x * W // nw + 1, (x + 1) * W // nw)
            s = [0, 0, 0]; n = 0
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    p = pixels[yy * W + xx]
                    s[0] += p[0]; s[1] += p[1]; s[2] += p[2]; n += 1
            out.append(tuple(v // max(1, n) for v in s))
    return out


# ---------------------------------------------------------------- les fonds
def fond_bloom_clair():
    """Le Bloom de Windows 11 en clair : bleu lavande sur un blanc bleute."""
    petales = [(-0.06, -0.04, 0.62, (150, 200, 250), 0.85),
               (0.10, 0.06, 0.55, (120, 175, 245), 0.75),
               (-0.16, 0.12, 0.48, (185, 220, 252), 0.70),
               (0.02, -0.18, 0.42, (205, 232, 255), 0.65),
               (0.20, -0.06, 0.34, (145, 195, 248), 0.55)]
    return bloom(LARGEUR, HAUTEUR, (250, 251, 253), (235, 241, 250), petales,
                 halo=(0.42, 0.30, 0.50, (28, 105, 190)))


def fond_bloom_sombre():
    """La version sombre, comme le theme sombre de Windows 11."""
    petales = [(-0.06, -0.04, 0.62, (38, 76, 130), 0.80),
               (0.10, 0.06, 0.55, (30, 60, 112), 0.72),
               (-0.16, 0.12, 0.48, (26, 48, 88), 0.65),
               (0.02, -0.18, 0.42, (34, 66, 116), 0.60),
               (0.20, -0.06, 0.34, (24, 52, 96), 0.52)]
    return bloom(LARGEUR, HAUTEUR, (32, 32, 36), (22, 22, 26), petales,
                 halo=(0.40, 0.28, 0.48, (0, 103, 192)))


def fond_lavande():
    petales = [(0.02, 0.02, 0.70, (226, 220, 250), 0.80),
               (-0.14, -0.08, 0.52, (236, 232, 252), 0.70),
               (0.16, 0.10, 0.44, (214, 208, 246), 0.65)]
    return bloom(LARGEUR, HAUTEUR, (252, 252, 255), (240, 238, 252), petales)


def fond_gris_doux():
    """Un gris neutre tres pale : le fond par defaut des bureaux Windows 11."""
    petales = [(-0.04, -0.02, 0.58, (244, 245, 247), 0.70),
               (0.12, 0.08, 0.46, (238, 240, 243), 0.60)]
    return bloom(LARGEUR, HAUTEUR, (248, 249, 251), (238, 240, 244), petales)


def fond_aqua():
    petales = [(-0.08, 0.02, 0.60, (150, 225, 232), 0.80),
               (0.12, -0.06, 0.50, (110, 200, 220), 0.70),
               (-0.10, 0.16, 0.42, (190, 238, 244), 0.62)]
    return bloom(LARGEUR, HAUTEUR, (250, 253, 254), (232, 246, 250), petales,
                 halo=(0.44, 0.30, 0.46, (0, 105, 140)))


def fond_soleil_couchant():
    """Un degrade doux orange/rose : le cote chaleureux de Fluent."""
    petales = [(0.06, 0.10, 0.70, (255, 205, 170), 0.85),
               (-0.12, 0.04, 0.52, (255, 225, 200), 0.70),
               (0.14, -0.08, 0.44, (250, 185, 175), 0.60)]
    return bloom(LARGEUR, HAUTEUR, (254, 250, 246), (250, 236, 226), petales,
                 halo=(0.46, 0.26, 0.44, (240, 140, 90)))


FONDS = [("fond-1", fond_bloom_clair),   # le Bloom, celui de Windows 11
         ("fond-2", fond_bloom_sombre),  # sa version sombre
         ("fond-3", fond_gris_doux),     # gris neutre tres pale
         ("fond-4", fond_lavande),       # lavande
         ("fond-5", fond_aqua),          # aqua
         ("fond-6", fond_soleil_couchant)]

NOMS = {"fond-1": "Bloom clair", "fond-2": "Bloom sombre", "fond-3": "Gris doux",
        "fond-4": "Lavande", "fond-5": "Aqua", "fond-6": "Soleil couchant"}


def main():
    os.makedirs(SORTIE, exist_ok=True)
    total = 0
    for nom, fabrique in FONDS:
        pixels = fabrique()
        assert len(pixels) == LARGEUR * HAUTEUR, f"{nom} : nombre de pixels incorrect"
        total += ecrire_fx(os.path.join(SORTIE, f"{nom}.fx"), LARGEUR, HAUTEUR, pixels)
        total += ecrire_fx(os.path.join(SORTIE, f"{nom}-mini.fx"), MINI_W, MINI_H,
                           reduire(pixels, LARGEUR, HAUTEUR, MINI_W, MINI_H))
        print(f"  {nom}.fx  ({NOMS[nom]})")
    print(f"total des 12 fichiers : {total/1024:.0f} Ko")


if __name__ == "__main__":
    main()
