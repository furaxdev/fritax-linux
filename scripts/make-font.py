#!/usr/bin/env python3
"""Genere une police bitmap 8x14 (ASCII 32..126) pour Fritax Terminal.
Sortie : src/font8x14.h  (aucune dependance a l'execution)"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets-src")
OUT = os.path.join(ROOT, "desktop", "fritax-terminal", "src", "font8x14.h")
TTF = os.path.join(SRC, "jetbrainsmono.ttf")

W, H = 8, 16           # cellule (taille des vrais terminaux : de la place pour les accents)
GLYPH_PX = 13          # taille de rendu finale (aucune reduction ensuite)
first, last = 32, 255   # ASCII + Latin-1 (accents francais)

font = ImageFont.truetype(TTF, GLYPH_PX)
try:
    font.set_variation_by_name("Medium")      # traits plus francs a petite taille
except Exception:
    pass
rows = []
n_glyphs = last - first + 1
pcols = 16
prows = (n_glyphs + pcols - 1) // pcols
preview = Image.new("RGB", (pcols * W * 4, prows * H * 4), (13, 20, 34))

# On dessine chaque glyphe a une origine FIXE et on decoupe toujours la MEME
# fenetre : ainsi toutes les lettres partagent la meme ligne de base (les
# descendantes de p, g, y passent bien sous la ligne, les accents au-dessus).
PEN_X, PEN_Y = 4, 5              # origine de la ligne, identique pour tous
CANV_W, CANV_H = W + 10, H + 12

for code in range(first, last + 1):
    ch = chr(code)
    tmp = Image.new("L", (CANV_W, CANV_H), 0)
    d = ImageDraw.Draw(tmp)
    d.text((PEN_X, PEN_Y), ch, font=font, fill=255)
    # fenetre fixe : (PEN_X, PEN_Y-2) de taille W x H  -> ligne de base constante
    glyph = tmp.crop((PEN_X, PEN_Y - 4, PEN_X + W, PEN_Y - 4 + H))
    bits = []
    for y in range(H):
        b = 0
        for x in range(W):
            if glyph.getpixel((x, y)) > 70:
                b |= (1 << (7 - x))
        bits.append(b)
    rows.append(bits)
    # apercu
    ox, oy = (code - first) % 16 * W * 4, (code - first) // 16 * H * 4
    for y in range(H):
        for x in range(W):
            if bits[y] & (1 << (7 - x)):
                for dy in range(4):
                    for dx in range(4):
                        preview.putpixel((ox + x * 4 + dx, oy + y * 4 + dy), (234, 242, 255))

lines = ["/* Police bitmap Fritax - generee automatiquement par scripts/make-font.py */",
         "/* ASCII + Latin-1 (32..255), 1 octet par ligne, bit 7 = pixel le plus a gauche */",
         "#ifndef FRITAX_FONT8X14_H", "#define FRITAX_FONT8X14_H", "",
         f"#define FONT_W {W}", f"#define FONT_H {H}", "#define FONT_FIRST 32", "#define FONT_LAST 255", "",
         f"static const unsigned char FONT8X14[{last-first+1}][{H}] = {{"]
for i, bits in enumerate(rows):
    c = chr(first + i)
    code = first + i
    esc = "?" if code < 33 else chr(code)
    if code > 126: esc = "U+%04X" % code
    lines.append("    { %s },   /* '%s' */" % (", ".join("0x%02x" % b for b in bits), esc))
lines += ["};", "", "#endif"]
open(OUT, "w").write("\n".join(lines) + "\n")
preview.save("/tmp/font-preview.png")
print("police ecrite:", OUT, "|", len(rows), "caracteres")
print("apercu: /tmp/font-preview.png")
