#!/usr/bin/env python3
"""Rend le personnage Minecraft de Fritax en vue de face (pixel art) depuis son skin."""
import os, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets-src")
OUT = os.path.join(ROOT, "branding")
os.makedirs(OUT, exist_ok=True)

skin = Image.open(os.path.join(SRC, "skin-fritax.png")).convert("RGBA")

# planches du skin 64x64 : (nom, x, y, w, h)
# planches du skin 64x64 (format 1.8) :
#   tete (8,8) · torse (20,20) · bras droit (44,20) · jambe droite (4,20)
#   bras gauche (36,52) · jambe gauche (20,52)   <- 2e rangee du skin
PARTS = {
    "tete":        (8, 8, 8, 8),
    "torse":       (20, 20, 8, 12),
    "bras_d":      (44, 20, 4, 12),
    "bras_g":      (36, 52, 4, 12),
    "jambe_d":     (4, 20, 4, 12),
    "jambe_g":     (20, 52, 4, 12),
}
COUCHES = {k: Image.open(os.path.join(SRC, "skin-fritax.png")).convert("RGBA") for k in ("tete",)}

def part(name):
    x, y, w, h = PARTS[name]
    return skin.crop((x, y, x + w, y + h))

def overlays(name):
    """calque chapeau/veste (x+32 pour la tete, x+16 pour le corps)."""
    # calque exterieur : tete = +32 en x ; corps/bras/jambes = +16 en y
    off = {"tete": (32, 0), "torse": (0, 16), "bras_d": (0, 16), "jambe_d": (0, 16),
           "bras_g": (16, 0), "jambe_g": (-16, 0)}[name]
    x, y, w, h = PARTS[name]
    ox, oy = off
    out = skin.crop((x + ox, y + oy, x + ox + w, y + oy + h))
    # on ne garde que les pixels opaques
    base = part(name).copy()
    base.alpha_composite(out)
    return base

# --- assemblage vue de face : tete / torse / bras / jambes ---
W, H = 16, 32
canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))

head = overlays("tete")
canvas.paste(head, (4, 0), head)

torso = overlays("torse")
canvas.paste(torso, (4, 8), torso)

arm_r = overlays("bras_d")
canvas.paste(arm_r, (0, 8), arm_r)

arm_l = overlays("bras_g")
canvas.paste(arm_l, (12, 8), arm_l)

leg_r = overlays("jambe_d")
canvas.paste(leg_r, (4, 20), leg_r)

leg_l = overlays("jambe_g")
canvas.paste(leg_l, (8, 20), leg_l)

# rognage au contenu puis export
bbox = canvas.getbbox()
canvas = canvas.crop(bbox)

for scale in (8, 16, 32):
    canvas.resize((canvas.width * scale, canvas.height * scale), Image.NEAREST).save(
        os.path.join(OUT, f"fritax-perso-{scale}x.png"))
print("personnage rendu :", canvas.size, "-> branding/fritax-perso-*.png")
