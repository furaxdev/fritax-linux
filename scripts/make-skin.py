#!/usr/bin/env python3
"""Prepare le skin de Fritax : bandeau + swoosh (style Nike) sur le front.
Entree : assets-src/skin-brut.png   ->   Sortie : assets-src/skin-fritax.png
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets-src")

NOIR = (16, 16, 18, 255)      # bandeau noir
BLANC = (245, 245, 245, 255)  # swoosh blanc

skin = Image.open(os.path.join(SRC, "skin-brut.png")).convert("RGBA")
px = skin.load()

# le bandeau fait le tour de la tete : avant 8-15, droite 0-7, gauche 16-23, arriere 24-31
BAND_ROWS = (9, 10)
for y in BAND_ROWS:
    for x in range(0, 32):
        px[x, y] = NOIR

# --- swoosh sur la FACE AVANT (x 8..15) : depart fin a gauche, queue qui monte ---
for x in (9, 10, 11):        # corps du swoosh (ligne basse)
    px[x, 10] = BLANC
for x in (11, 12, 13):       # queue qui remonte, reliee au corps en x=11
    px[x, 9] = BLANC

# --- second swoosh sur le cote droit (x 0..7), plus discret ---
px[3, 10] = BLANC
px[5, 9] = BLANC

# --- on reporte sur le calque "chapeau" la ou il est opaque ---
for y in BAND_ROWS:
    for x in range(0, 32):
        if px[x + 32, y][3] > 0:
            px[x + 32, y] = px[x, y]
for y in BAND_ROWS:
    for x in range(0, 32):
        if px[x + 32, y][3] > 0 and px[x, y] == BLANC:
            px[x + 32, y] = BLANC

out = os.path.join(SRC, "skin-fritax.png")
skin.save(out)
skin.crop((8, 8, 16, 16)).resize((400, 400), Image.NEAREST).save("/tmp/skin-face-nike.png")
print("skin pret :", out)
