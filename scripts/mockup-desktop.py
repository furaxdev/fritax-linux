#!/usr/bin/env python3
"""Maquette du bureau Fritax (rendu de reference, avant implementation en C).
Genere : desktop/mockup-*.png
"""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets-src")
BR = os.path.join(ROOT, "branding")
OUT = os.path.join(ROOT, "desktop")
os.makedirs(OUT, exist_ok=True)

W, H = 1366, 768
NOIR = (13, 20, 34)
BLANC = (234, 242, 255)
CYAN = (91, 200, 255)
VIOLET = (214, 51, 255)


def font(name, size, weight="Bold"):
    f = ImageFont.truetype(os.path.join(SRC, name), size)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def rounded_panel(size, radius, fill):
    """Panneau arrondi semi-transparent avec ombre portee."""
    w, h = size
    pad = 24
    layer = Image.new("RGBA", (w + pad * 2, h + pad * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle([pad, pad, pad + w, pad + h], radius=radius, fill=fill)
    shadow = layer.filter(ImageFilter.GaussianBlur(14))
    return shadow, layer, pad


def make_panel(wall):
    img = wall.convert("RGBA").copy()
    pw, ph = 640, 46
    shadow, layer, pad = rounded_panel((pw, ph), 22, NOIR + (215,))
    px, py = (W - pw) // 2, H - ph - 26
    img.alpha_composite(shadow, (px - pad + 4, py - pad + 8))
    img.alpha_composite(layer, (px - pad, py - pad))

    d = ImageDraw.Draw(img)
    # bouton lanceur
    d.rounded_rectangle([px + 6, py + 6, px + 34, py + 34], radius=10, fill=VIOLET + (235,))
    d.text((px + 14, py + 9), "F", font=font("montserrat.ttf", 18, "ExtraBold"), fill=(255, 255, 255))

    # icones d'applis (taches ouvertes)
    for i, c in enumerate([CYAN, (57, 222, 138), (246, 173, 85)]):
        x = px + 48 + i * 44
        d.rounded_rectangle([x, py + 8, x + 30, py + 30], radius=9, fill=(30, 45, 70, 240))
        d.rounded_rectangle([x + 7, py + 15, x + 23, py + 23], radius=4, fill=c + (230,))

    # horloge + zone systeme
    f = font("jetbrainsmono.ttf", 15, "SemiBold")
    d.text((px + pw - 96, py + 15), "14:32", font=f, fill=BLANC)
    for i in range(3):
        d.ellipse([px + pw - 34 + i * 8, py + 19, px + pw - 30 + i * 8, py + 23], fill=(120, 140, 170, 255))

    # petit indicateur "barre flottante"
    img.convert("RGB").save(os.path.join(OUT, "mockup-barre.png"))
    print("ecrit: desktop/mockup-barre.png")
    return img


def make_launcher(wall):
    img = wall.convert("RGBA").copy()
    # voile sombre
    veil = Image.new("RGBA", (W, H), (6, 10, 18, 165))
    img.alpha_composite(veil)

    pw, ph = 620, 330
    shadow, layer, pad = rounded_panel((pw, ph), 26, (17, 26, 44, 245))
    px, py = (W - pw) // 2, (H - ph) // 2
    img.alpha_composite(shadow, (px - pad + 6, py - pad + 10))
    img.alpha_composite(layer, (px - pad, py - pad))

    d = ImageDraw.Draw(img)
    d.text((px + 24, py + 14), "Applications", font=font("montserrat.ttf", 20, "Bold"), fill=BLANC)

    apps = [("Terminal", CYAN), ("Navigateur", (246, 173, 85)), ("Fichiers", (57, 222, 138)),
            ("Jeux", VIOLET), ("Editeur", (255, 120, 120)), ("Reglages", (160, 170, 190))]
    fapp = font("spacegrotesk.ttf", 13, "Medium")
    for i, (name, col) in enumerate(apps):
        cx = px + 34 + (i % 3) * 190
        cy = py + 66 + (i // 3) * 120
        d.rounded_rectangle([cx, cy, cx + 160, cy + 100], radius=16, fill=(26, 38, 60, 240))
        d.rounded_rectangle([cx + 16, cy + 16, cx + 52, cy + 52], radius=11, fill=col + (235,))
        d.text((cx + 16, cy + 64), name, font=fapp, fill=BLANC)

    d.text((px + 24, py + ph - 34), "Tape pour rechercher...   |   Echap pour fermer",
           font=font("jetbrainsmono.ttf", 12, "Regular"), fill=(124, 147, 181))

    # curseur
    cur = Image.new("RGBA", (40, 40), (0, 0, 0, 0))
    cd = ImageDraw.Draw(cur)
    cd.polygon([(4, 2), (4, 26), (11, 19), (16, 30), (20, 28), (15, 17), (24, 17)], fill=(255, 255, 255, 255),
               outline=(20, 20, 25, 255))
    img.alpha_composite(cur, (W // 2 + 120, H // 2 + 60))
    img.convert("RGB").save(os.path.join(OUT, "mockup-lanceur.png"))
    print("ecrit: desktop/mockup-lanceur.png")
    return img


wall = Image.open(os.path.join(BR, "fritax-1366x768.png"))
make_panel(wall)
make_launcher(wall)
