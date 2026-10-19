# Renders assets/og.jpg from tools/og-game.jpg and assets/logo.svg, and assets/apple-touch-icon.png from the logo; needs Pillow and the Ubuntu Sans font

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
    # a frame from a real match behind, dark on the left where the text sits
    k = 2
    w, h = 1200 * k, 630 * k
    img = Image.open(os.path.join(ROOT, 'tools', 'og-game.jpg')).convert('RGB').resize((w, h), Image.LANCZOS)
    shade = Image.new('L', (w, h), 0)
    sd = ImageDraw.Draw(shade)
    for x in range(w):
        t = max(0.0, 1 - x / (w * 0.72))
        sd.line([(x, 0), (x, h)], fill=int(255 * (0.25 + 0.7 * t ** 0.8)))
    img = Image.composite(Image.new('RGB', (w, h), '#0b0d10'), img, shade)
    d = ImageDraw.Draw(img)
    tile = 132 * k
    d.rounded_rectangle([80 * k, 92 * k, 80 * k + tile, 92 * k + tile], radius=22 * k, fill='#ffffff')
    draw_logo(img, 80 * k + 14 * k, 92 * k + 14 * k, tile - 28 * k)
    d = ImageDraw.Draw(img)
    d.text((236 * k, 100 * k), 'NextClient', font=font(92 * k, 850), fill='#ffffff')
    d.text((240 * k, 200 * k), 'Counter-Strike 1.6 · Windows & Linux', font=font(26 * k, 600), fill='#c1e0ff')
    d.text((80 * k, 300 * k), 'Та самая 1.6.', font=font(64 * k, 800), fill='#ffffff')
    d.text((80 * k, 372 * k), 'Только лучше.', font=font(64 * k, 800), fill='#5aa8f0')
    d.text((80 * k, 478 * k), 'Радар, Discord, защита от вредных серверов', font=font(28 * k, 500), fill='#d5dce6')
    d.text((80 * k, 518 * k), 'и API для серверов. Бесплатно, код на GitHub.', font=font(28 * k, 500), fill='#d5dce6')
    img.resize((1200, 630), Image.LANCZOS).save(OUT + 'og.jpg', quality=86, optimize=True, progressive=True)

def icon(size, name):
    k = 4
    img = Image.new('RGB', (size * k, size * k), '#ffffff')
    pad = size * k * 0.08
    draw_logo(img, pad, pad, size * k - 2 * pad)
    img.resize((size, size), Image.LANCZOS).save(OUT + name, optimize=True)

card()
icon(180, 'apple-touch-icon.png')
print('ok')
