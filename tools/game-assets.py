# Builds assets/game/ (the pictures on the tour's boards) and tools/og-game.jpg out of real
# screenshots and the client's own killfeed sprite; needs Pillow.
#   python3 tools/game-assets.py [screenshots dir] [game dir]
# UI pieces are cut at their native size and stored twice as large with nearest-neighbour scaling,
# so the browser only ever scales them down and the game's text stays sharp. Nothing is blurred:
# rows and columns with a site's name in them are left out whole.

import os
import struct
import sys
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'assets', 'game')
SHOTS = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/Pictures/Screenshots')
GAME = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/nextclient-run')
# the game's dialogs are one flat colour with a darker 1px edge
kPanel = (76, 88, 68)
kPanelEdge = (40, 46, 34)

SERVERS = 'Screenshot From 2026-10-19 15-18-41.png'   # the server browser, a window capture
FULL_MAP = 'Screenshot From 2026-10-19 15-39-06.png'    # TAB: the whole map, with the scoreboard
WORLD = 'Screenshot From 2026-10-19 15-39-36.png'       # de_dust2 with the round radar
OPTIONS = 'Screenshot From 2026-10-19 15-42-16.png'     # the Game tab and the Share settings dialog
RADAR = 'Screenshot From 2026-10-19 15-42-35.png'       # HUD Radar - Settings
PROFILE = 'Screenshot From 2026-10-19 18-06-48.png'     # a Discord profile playing on Next21
CHAT = 'Screenshot From 2026-10-19 18-07-12.png'        # Ask to Join and Game Invitation in a chat
KILLFEED = os.path.expanduser('~/Downloads/feature6.jpg')  # the killfeed with every kind of kill on one line


def load(name):
    return Image.open(os.path.join(SHOTS, name)).convert('RGB')


def save(img, name, double=True, photo=False):
    # the game's UI keeps every pixel; what is a picture of the world can lose what no one sees
    if double:
        img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    path = os.path.join(OUT, name)
    if photo:
        img.save(path, quality=90, method=6)
    else:
        img.save(path, lossless=True, quality=100, method=6)
    print(f'{name:22} {img.size[0]}x{img.size[1]} {os.path.getsize(path) // 1024} KB')


def fill(img, hole):
    # onion peel: each hole pixel takes the mean of its known neighbours, edge first
    px = img.load()
    w, h = img.size
    hole = set(hole)
    while hole:
        ring = []
        for (x, y) in hole:
            n = [px[x + dx, y + dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                 if (dx or dy) and 0 <= x + dx < w and 0 <= y + dy < h and (x + dx, y + dy) not in hole]
            if n:
                ring.append(((x, y), tuple(sum(c[i] for c in n) // len(n) for i in range(3))))
        if not ring:
            break
        for p, c in ring:
            px[p] = c
            hole.discard(p)


def box(x0, y0, x1, y1):
    return [(x, y) for y in range(y0, y1) for x in range(x0, x1)]


def rounded(img, radius):
    mask = Image.new('L', (img.width * 4, img.height * 4), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, mask.width - 1, mask.height - 1), radius=radius * 4, fill=255)
    rgba = img.convert('RGBA')
    rgba.putalpha(mask.resize(img.size, Image.LANCZOS))
    return rgba


def servers():
    # a compact browser out of the real dialog: Servers (cut after the longest kept name), Mode,
    # Players, Map and Latency, and only the rows without a site in them
    im = load(SERVERS)
    row_top, row_height = 164, 16
    rows = [7, 8, 9, 12, 15, 22, 27, 28]
    list_columns = [(28, 470), (811, 896), (1141, 1368)]
    width = sum(b - a for a, b in list_columns)
    # the filter panel is empty in the middle: its left part plus the dialog's right edge
    panel_columns = [(28, 28 + width - 9), (1359, 1368)]

    def band(y0, y1, columns):
        piece = Image.new('RGB', (width, y1 - y0))
        x = 0
        for a, b in columns:
            piece.paste(im.crop((a, y0, b, y1)), (x, 0))
            x += b - a
        return piece

    bands = [band(62, row_top, list_columns)]
    bands += [band(row_top + r * row_height, row_top + (r + 1) * row_height, list_columns) for r in rows]
    bands += [band(628, 633, list_columns), band(633, 768, panel_columns), band(799, 803, panel_columns)]
    dialog = Image.new('RGB', (width, sum(b.height for b in bands)))
    y = 0
    for b in bands:
        dialog.paste(b, (0, y))
        y += b.height
    save(dialog, 'servers.webp')


def radar():
    # the round radar, cut along its ring, and the whole map TAB opens
    ring = load(WORLD).crop((86, 88, 321, 323)).convert('RGBA')
    mask = Image.new('L', (235 * 4, 235 * 4), 0)
    c, r = 117.5 * 4, 116.6 * 4
    ImageDraw.Draw(mask).ellipse((c - r, c - r, c + r, c + r), fill=255)
    ring.putalpha(mask.resize((235, 235), Image.LANCZOS))
    save(ring, 'radar.webp', photo=True)
    save(load(FULL_MAP).crop((87, 89, 366, 368)), 'radar-full.webp', photo=True)


def options():
    shot = load(OPTIONS)
    dialog = shot.crop((860, 328, 1459, 794))
    # the mouse cursor, caught in the screenshot over the panel's flat face and its bottom edge
    draw = ImageDraw.Draw(dialog)
    draw.rectangle((328, 380, 348, 406), fill=kPanel)
    draw.line([(328, 389), (348, 389)], fill=kPanelEdge)
    save(dialog, 'options.webp')
    save(shot.crop((433, 326, 853, 656)), 'share.webp')


def radar_settings():
    dialog = load(RADAR).crop((464, 321, 844, 817))
    ImageDraw.Draw(dialog).rectangle((157, 15, 173, 35), fill=kPanel)  # the mouse cursor, on the title
    save(dialog, 'radar-settings.webp')


def killfeed():
    # three lines over a dark wall; the last one has every rarity icon there is
    save(Image.open(KILLFEED).convert('RGB').crop((0, 50, 488, 145)), 'killfeed-full.webp', double=False, photo=True)


def rarity_icons():
    # the killfeed's own icons, out of the sprite the client draws them from, in the order of its
    # RarityFrame enum; the sprite is additive, so its brightness is the icon's shape
    data = open(os.path.join(GAME, 'cstrike', 'sprites', 'kill_rarity.spr'), 'rb').read()
    ident, version, _, tex_format = struct.unpack_from('<4siii', data, 0)
    frames = struct.unpack_from('<i', data, 28)[0]
    if ident != b'IDSP' or version != 2 or tex_format != 1:
        raise SystemExit('kill_rarity.spr is not the additive sprite this expects')
    colors = struct.unpack_from('<h', data, 40)[0]
    palette = data[42:42 + colors * 3]
    offset = 42 + colors * 3
    names = ['headshot', 'blind', 'noscope', 'wall', 'smoke', 'flash', 'domination', 'revenge', 'air']
    if frames != len(names):
        raise SystemExit(f'kill_rarity.spr has {frames} frames, not {len(names)}')
    for name in names:
        _, _, w, h = struct.unpack_from('<iiii', data, offset + 4)
        pixels = data[offset + 20:offset + 20 + w * h]
        offset += 20 + w * h
        alpha = Image.new('L', (w, h))
        alpha.putdata([max(palette[i * 3:i * 3 + 3]) for i in pixels])
        icon = Image.new('RGBA', (w, h), (11, 13, 16, 0))
        icon.putalpha(alpha)
        save(icon, f'rarity-{name}.webp', double=False)


def discord():
    # whole cards from Next21's own server, so nobody's ads are in them
    profile = load(PROFILE)
    save(rounded(profile.crop((476, 62, 928, 298)), 10), 'discord-status.webp')
    chat = load(CHAT)
    save(rounded(chat.crop((67, 37, 410, 348)), 8), 'discord-ask.webp')
    save(rounded(chat.crop((67, 395, 410, 720)), 8), 'discord-invite.webp')


def social_card():
    # the frame behind the social card's text: de_dust2 with nothing of the HUD left, at 1480x777
    shot = load(WORLD)
    px = shot.load()
    stats = [(x, y) for (x, y) in box(326, 243, 566, 400) if px[x, y][1] > px[x, y][0] + 25 and px[x, y][1] > px[x, y][2] + 15]
    fill(shot, {(x + dx, y + dy) for (x, y) in stats for dx in range(-2, 3) for dy in range(-2, 3)})
    for region in [(978, 626, 1009, 650),                       # damage number
                   (70, 1030, 170, 1074), (430, 1036, 530, 1076), (920, 1036, 1050, 1078),
                   (1776, 996, 1914, 1038), (1766, 1038, 1916, 1078),  # health, armour, timer, money, ammo
                   (84, 536, 112, 568)]:                         # the defuse kit icon
        fill(shot, box(*region))
    fill(shot, [(x, y) for (x, y) in box(82, 84, 326, 328) if (x + 0.5 - 203.5) ** 2 + (y + 0.5 - 205.5) ** 2 <= 120 ** 2])
    # the killfeed sat on plain blocks: the row of blocks above it is laid over it, mortar lines and all
    shot.paste(shot.crop((1584, 91, 1920, 131)), (1584, 131))
    card = shot.crop((69, 89, 1920, 1061)).resize((1480, 777), Image.LANCZOS)
    card.save(os.path.join(ROOT, 'tools', 'og-game.jpg'), quality=85)


os.makedirs(OUT, exist_ok=True)
servers()
radar()
options()
radar_settings()
killfeed()
rarity_icons()
discord()
social_card()
