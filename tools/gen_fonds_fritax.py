#!/usr/bin/env python3
"""
Genere les fonds d'ecran de Fritax Linux au format FXRAW.

Format (lu par fx_load_raw_rgb dans fritax-terminal/src/screen.c) :
    "FXRAW <largeur> <hauteur>\\n\\n" suivi des octets RGB, ligne par ligne.

Chaque fond est ecrit en deux tailles : la grande pour le bureau, la
miniature (280x158) pour les vignettes des Reglages.

Sortie : distro/board/fritax/rootfs-overlay/usr/share/fritax/
    fond-1.fx / fond-1-mini.fx   Nuit
    fond-2.fx / fond-2-mini.fx   Aurore
    fond-3.fx / fond-3-mini.fx   Graphite
    fond-4.fx / fond-4-mini.fx   Ocean
    fond-5.fx / fond-5-mini.fx   Foret
    fond-6.fx / fond-6-mini.fx   Neon
"""
import math, os, random

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SORTIE = os.path.join(RACINE, "distro", "board", "fritax", "rootfs-overlay", "usr", "share", "fritax")
LARGEUR, HAUTEUR = 1366, 768
MINI_W, MINI_H = 280, 158


def ecrire_fx(chemin, W, H, pixels):
    """pixels : liste de (r, g, b) de longueur W*H, ligne par ligne."""
    with open(chemin, "wb") as f:
        f.write(f"FXRAW {W} {H}\n\n".encode("ascii"))
        f.write(bytes(octet for p in pixels for octet in p))


def degrade(W, H, haut, bas, radial=None, radial_couleur=None, radial_force=0.0):
    """Degrade vertical, avec un halo radial optionnel."""
    pixels = []
    for y in range(H):
        t = y / (H - 1)
        base = [haut[i] * (1 - t) + bas[i] * t for i in range(3)]
        for x in range(W):
            c = base[:]
            if radial and radial_force > 0:
                dx = (x / (W - 1) - radial[0]) ** 2 + (y / (H - 1) - radial[1]) ** 2
                halo = max(0.0, 1.0 - dx * 4.2)
                for i in range(3):
                    c[i] += (radial_couleur[i] - c[i]) * halo * radial_force
            pixels.append(tuple(int(max(0, min(255, v))) for v in c))
    return pixels


def etoiles(pixels, W, H, n, graine, couleur=(255, 255, 255), haut=0.72, taille_max=2):
    r = random.Random(graine)
    for _ in range(n):
        x, y = r.randint(0, W - 1), r.randint(0, int(H * haut))
        intensite = r.uniform(0.35, 1.0)
        c = tuple(int(v * intensite + pixels[y * W + x][i] * (1 - intensite)) for i, v in enumerate(couleur))
        taille = r.randint(1, taille_max)
        for dy in range(-taille, taille + 1):
            for dx in range(-taille, taille + 1):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and dx * dx + dy * dy <= taille * taille:
                    pixels[ny * W + nx] = c
    return pixels


def bandes(pixels, W, H, y0, hauteur, pas, couleur, alpha=0.16):
    """Bandes horizontales douces (effet graphique)."""
    for y in range(y0, min(H, y0 + hauteur)):
        if (y - y0) % pas < max(1, pas // 3):
            for x in range(W):
                i = y * W + x
                pixels[i] = tuple(int(pixels[i][k] * (1 - alpha) + couleur[k] * alpha) for k in range(3))
    return pixels


def grille(pixels, W, H, pas, couleur, alpha=0.14, y_debut=0.45):
    """Grille en perspective (style neon)."""
    for y in range(int(H * y_debut), H):
        if y % pas < 2:
            for x in range(W):
                i = y * W + x
                pixels[i] = tuple(int(pixels[i][k] * (1 - alpha) + couleur[k] * alpha) for k in range(3))
    for x in range(0, W, pas * 2):
        for y in range(int(H * y_debut), H):
            i = y * W + x
            pixels[i] = tuple(int(pixels[i][k] * (1 - alpha) + couleur[k] * alpha) for k in range(3))
    return pixels


def reduire(pixels, W, H, nw, nh):
    """Reduction par moyenne de blocs (pour les vignettes)."""
    out = []
    for y in range(nh):
        y0, y1 = y * H // nh, max(y * H // nh + 1, (y + 1) * H // nh)
        for x in range(nw):
            x0, x1 = x * W // nw, max(x * W // nw + 1, (x + 1) * W // nw)
            somme = [0, 0, 0]
            n = 0
            for yy in range(y0, y1):
                for xx in range(x0, x1):
                    p = pixels[yy * W + xx]
                    somme[0] += p[0]; somme[1] += p[1]; somme[2] += p[2]
                    n += 1
            out.append(tuple(s // max(1, n) for s in somme))
    return out


def fond_nuit():
    p = degrade(LARGEUR, HAUTEUR, (14, 20, 44), (6, 8, 18), (0.72, 0.18), (70, 96, 190), 0.42)
    return etoiles(p, LARGEUR, HAUTEUR, 420, graine=1)


def fond_aurore():
    p = degrade(LARGEUR, HAUTEUR, (74, 40, 120), (240, 130, 70), (0.30, 0.78), (255, 190, 120), 0.38)
    return etoiles(p, LARGEUR, HAUTEUR, 90, graine=2, couleur=(255, 235, 200), haut=0.4, taille_max=1)


def fond_graphite():
    p = degrade(LARGEUR, HAUTEUR, (58, 62, 70), (24, 26, 32), (0.78, 0.22), (140, 148, 162), 0.30)
    return bandes(p, LARGEUR, HAUTEUR, 0, HAUTEUR, 120, (200, 206, 216), 0.05)


def fond_ocean():
    p = degrade(LARGEUR, HAUTEUR, (16, 92, 120), (6, 30, 54), (0.62, 0.14), (140, 230, 240), 0.40)
    # rayons de lumiere qui traversent l'eau
    for k in range(9):
        x0 = int(LARGEUR * (0.06 + k * 0.11))
        for y in range(HAUTEUR):
            largeur = 8 + (y * y) // 26000
            for dx in range(-largeur, largeur):
                x = x0 + dx + k * 6
                if 0 <= x < LARGEUR:
                    i = y * LARGEUR + x
                    a = 0.05 * (1 - y / HAUTEUR)
                    p[i] = tuple(int(p[i][c] * (1 - a) + (200, 250, 255)[c] * a) for c in range(3))
    return p


def fond_foret():
    p = degrade(LARGEUR, HAUTEUR, (24, 62, 40), (10, 24, 18), (0.40, 0.10), (170, 220, 130), 0.36)
    r = random.Random(5)
    # feuillage : taches vertes plus ou moins denses en haut
    for _ in range(2600):
        x, y = r.randint(0, LARGEUR - 1), r.randint(0, int(HAUTEUR * 0.42))
        rayon = r.randint(6, 26)
        teinte = r.randint(30, 90)
        a = r.uniform(0.05, 0.20)
        for dy in range(-rayon, rayon + 1, 2):
            for dx in range(-rayon, rayon + 1, 2):
                nx, ny = x + dx, y + dy
                if 0 <= nx < LARGEUR and 0 <= ny < HAUTEUR and dx * dx + dy * dy <= rayon * rayon:
                    i = ny * LARGEUR + nx
                    p[i] = tuple(int(p[i][c] * (1 - a) + (30 + teinte, 90 + teinte, 46)[c] * a) for c in range(3))
    return p


def fond_neon():
    p = degrade(LARGEUR, HAUTEUR, (34, 18, 64), (12, 6, 26), (0.60, 0.26), (214, 51, 255), 0.55)
    p = grille(p, LARGEUR, HAUTEUR, 26, (90, 220, 255), 0.16)
    return etoiles(p, LARGEUR, HAUTEUR, 140, graine=6, couleur=(255, 160, 255), haut=0.45, taille_max=2)


FONDS = [("fond-1", fond_nuit), ("fond-2", fond_aurore), ("fond-3", fond_graphite),
         ("fond-4", fond_ocean), ("fond-5", fond_foret), ("fond-6", fond_neon)]


def main():
    os.makedirs(SORTIE, exist_ok=True)
    for nom, fabrique in FONDS:
        pixels = fabrique()
        assert len(pixels) == LARGEUR * HAUTEUR, f"{nom} : {len(pixels)} pixels au lieu de {LARGEUR*HAUTEUR}"
        ecrire_fx(os.path.join(SORTIE, f"{nom}.fx"), LARGEUR, HAUTEUR, pixels)
        ecrire_fx(os.path.join(SORTIE, f"{nom}-mini.fx"), MINI_W, MINI_H,
                  reduire(pixels, LARGEUR, HAUTEUR, MINI_W, MINI_H))
        print(f"  {nom}.fx + {nom}-mini.fx")
    print("termine")


if __name__ == "__main__":
    main()
