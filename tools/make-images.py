# Renders assets/og.png and assets/apple-touch-icon.png from assets/logo.svg; needs Pillow and the Ubuntu Sans font

import os, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageFont

SVG = os.path.join(ROOT, 'assets', 'logo.svg')
OUT = os.path.join(ROOT, 'assets') + os.sep
FONT = '/usr/share/fonts/truetype/ubuntu/UbuntuSans[wdth,wght].ttf'

def subpaths(d):
    # absolute M, C and L only, with implicit repeats, which is all the logo uses
    tokens = re.findall(r'[MCLZ]|-?\d*\.?\d+', d)
    paths, cur, cmd, i = [], [], None, 0
    pos = (0.0, 0.0)
    while i < len(tokens):
        t = tokens[i]
        if t in 'MCLZ':
            cmd = t
            i += 1
            if t == 'M' and cur:
                paths.append(cur); cur = []
            continue
        n = [float(x) for x in tokens[i:i + (6 if cmd == 'C' else 2)]]
        if cmd == 'M':
            pos = (n[0], n[1]); cur = [pos]; cmd = 'L'; i += 2
        elif cmd == 'L':
            pos = (n[0], n[1]); cur.append(pos); i += 2
        elif cmd == 'C':
            p0 = pos; p1 = (n[0], n[1]); p2 = (n[2], n[3]); p3 = (n[4], n[5])
            for k in range(1, 17):
                u = k / 16
                a = (1 - u) ** 3; b = 3 * (1 - u) ** 2 * u; c = 3 * (1 - u) * u * u; e = u ** 3
                cur.append((a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1]))
            pos = p3; i += 6
    if cur:
        paths.append(cur)
    return paths

svg = open(SVG).read()
layers = re.findall(r'<path d="([^"]+)" fill="(#[0-9a-f]{6})"', svg)

def draw_logo(img, x, y, size):
    d = ImageDraw.Draw(img)
    s = size / 100
    for path, color in layers:
        parts = subpaths(path)
        for index, part in enumerate(parts):
            # the black path's second part is its hole
            fill = '#ffffff' if (color == '#0b0d10' and index > 0) else color
            d.polygon([(x + px * s, y + py * s) for px, py in part], fill=fill)

def font(size, weight):
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_axes([100, weight])
    return f

def card():
    k = 2
    w, h = 1200 * k, 630 * k
    img = Image.new('RGB', (w, h), '#ffffff')
    d = ImageDraw.Draw(img)
    # a soft blue disc behind the logo
    glow = Image.new('L', (w, h), 0)
    gd = ImageDraw.Draw(glow)
    cx, cy, r = 880 * k, 315 * k, 300 * k
    for i in range(60, 0, -1):
        rr = r * i / 60
        gd.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=int(70 * (1 - i / 60)))
    img = Image.composite(Image.new('RGB', (w, h), '#cfe4fa'), img, glow)
    draw_logo(img, cx - 220 * k, cy - 220 * k, 440 * k)
    d = ImageDraw.Draw(img)
    eyebrow = 'Counter-Strike 1.6 · Steam 8684'
    eyebrow_font = font(22 * k, 700)
    d.rounded_rectangle([80 * k, 150 * k, 80 * k + d.textlength(eyebrow, font=eyebrow_font) + 40 * k, 196 * k], radius=23 * k, fill='#eaf3fc')
    d.text((100 * k, 160 * k), eyebrow, font=eyebrow_font, fill='#1468be')
    d.text((76 * k, 225 * k), 'NextClient', font=font(104 * k, 850), fill='#0b0d10')
    d.text((80 * k, 365 * k), 'Современный клиент CS 1.6', font=font(38 * k, 600), fill='#0b0d10')
    d.text((80 * k, 420 * k), 'Новый интерфейс, защита и API для серверов', font=font(28 * k, 450), fill='#5d6675')
    img.resize((1200, 630), Image.LANCZOS).save(OUT + 'og.png', optimize=True)

def icon(size, name):
    k = 4
    img = Image.new('RGB', (size * k, size * k), '#ffffff')
    pad = size * k * 0.08
    draw_logo(img, pad, pad, size * k - 2 * pad)
    img.resize((size, size), Image.LANCZOS).save(OUT + name, optimize=True)

card()
icon(180, 'apple-touch-icon.png')
print('ok')
