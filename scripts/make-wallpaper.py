#!/usr/bin/env python3
"""Genere les fonds d'ecran Fritax Linux (branding/fritax-*.png)."""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "assets-src")
OUT = os.path.join(ROOT, "branding")
os.makedirs(OUT, exist_ok=True)

NAVY_TOP = (7, 12, 22)
NAVY_BOT = (16, 35, 63)
BLUE = (18, 103, 181)      # bleu FURAX
CYAN = (91, 200, 255)      # cyan Fritax
TXT = (234, 242, 255)
MUTED = (124, 147, 181)
VIOLET = (214, 51, 255)   # violet de l'avatar FRITAX


def font(name, size, weight="Bold"):
    f = ImageFont.truetype(os.path.join(SRC, name), size)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def gradient(w, h):
    g = Image.new("RGB", (w, h), NAVY_TOP)
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(h - 1, 1)
        d.line([(0, y), (w, y)], fill=tuple(int(NAVY_TOP[i] + (NAVY_BOT[i] - NAVY_TOP[i]) * t) for i in range(3)))
    return g


def glow(w, h):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx, cy = int(w * 0.5), int(h * 0.42)
    # halo violet/magenta pour s'accorder avec l'avatar de Fritax
    for r in range(min(w, h) // 2, 0, -6):
        t = r / (min(w, h) / 2)
        a = int(26 * (1 - t))
        col = tuple(int(VIOLET[i] * (1 - t) + CYAN[i] * t) for i in range(3))
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col + (a,))
    return layer.filter(ImageFilter.GaussianBlur(60))


def stripes(w, h):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(-h, w, 90):
        d.line([(i, h), (i + h, 0)], fill=CYAN + (10,), width=1)
    return layer


def build(w, h):
    img = gradient(w, h).convert("RGBA")
    img.alpha_composite(glow(w, h))
    img.alpha_composite(stripes(w, h))

    # --- logo circulaire ---
    logo = Image.open(os.path.join(SRC, "logo-fritax.png")).convert("RGBA").resize((360, 360), Image.LANCZOS)
    mask = Image.new("L", (360, 360), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, 359, 359], fill=255)
    halo = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    lx, ly = w // 2 - 180, int(h * 0.20)
    hd.ellipse([lx - 26, ly - 26, lx + 386, ly + 386], outline=CYAN + (90,), width=6)
    halo = halo.filter(ImageFilter.GaussianBlur(2))
    img.alpha_composite(halo)
    img.paste(logo, (lx, ly), mask)

    d = ImageDraw.Draw(img)

    # --- wordmark ---
    fm = font("montserrat.ttf", int(h * 0.145), "ExtraBold")
    txt = "FRITAX"
    spaced = "  ".join(txt)
    tw = d.textlength(spaced, font=fm)
    ty = int(h * 0.20) + 360 + int(h * 0.045)
    d.text(((w - tw) / 2, ty), spaced, font=fm, fill=TXT)

    fs = font("spacegrotesk.ttf", int(h * 0.055), "Medium")
    sub = "L I N U X"
    sw = d.textlength(sub, font=fs)
    d.text(((w - sw) / 2, ty + int(h * 0.155)), sub, font=fs, fill=CYAN)

    # --- signature ---
    fmono = font("jetbrainsmono.ttf", int(h * 0.022), "Regular")
    sig = "1.0 " + chr(171) + " Nova " + chr(187) + "  -  fritax linux"
    d.text((int(w * 0.06), int(h * 0.93)), sig, font=fmono, fill=MUTED)

    # --- le personnage de Fritax (pixel art, rendu depuis son skin) ---
    perso_path = os.path.join(ROOT, "branding", "fritax-perso-16x.png")
    if os.path.exists(perso_path):
        scale = max(4, int(h * 0.50) // 32)          # echelle entiere -> pixel art net
        perso = Image.open(perso_path).convert("RGBA")
        base_w, base_h = 16, 32
        pw, ph = base_w * scale, base_h * scale
        perso = perso.resize((pw, ph), Image.NEAREST)
        px_, py_ = w - pw - int(w * 0.05), h - ph - int(h * 0.03)

        # ombre au sol + lueur violette
        deco = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        dd = ImageDraw.Draw(deco)
        dd.ellipse([px_ - pw // 4, py_ + ph - 14, px_ + pw + pw // 4, py_ + ph + 14], fill=(0, 0, 0, 150))
        # lueur douce et large derriere le personnage
        for grow, alpha in ((20, 10), (60, 9), (110, 7), (170, 5)):
            dd.ellipse([px_ - grow, py_ - grow, px_ + pw + grow, py_ + ph + grow // 2],
                       fill=VIOLET + (alpha,))
        img.alpha_composite(deco.filter(ImageFilter.GaussianBlur(22)))
        img.alpha_composite(perso, (px_, py_))

    out = os.path.join(OUT, f"fritax-{w}x{h}.png")
    img.convert("RGB").save(out, quality=95)
    print("ecrit:", out)
    return out


for size in [(1366, 768), (1920, 1080), (2560, 1440)]:
    build(*size)
