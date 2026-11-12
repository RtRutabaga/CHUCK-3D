"""The captain's rug (Claude, 2026-11-11; user: "make the rug in the captain's quarters look more like a nice ornate rug").

Writes SourceAssets/Setting/Ship/ShipRug.png: a Persian-style knotted rug drawn procedurally (standard library only:
math, zlib, struct), 3.1 x 2.6 m at 3.3 px/cm. Madder-red field with a lattice of small motifs, a lobed navy medallion
with pendants, navy corner spandrels, a navy main border of rosettes on a vine between gold and ivory guards, a dark
overcast edge; abrash (the dye-lot bands across the rows), knot speckle and a little wear in the middle.
U runs along the rug's length (the ship's X), V across it. The fringe is geometry (ShipSetting.cpp).

python -I Tools/make_ship_rug.py   (deterministic; about a minute)
"""
import math
import os
import struct
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'SourceAssets', 'Setting', 'Ship', 'ShipRug.png')

LX, LY = 310.0, 260.0          # cm
W, H = 1024, 860               # px
RED = (128, 26, 24); RED2 = (160, 44, 34); NAVY = (24, 32, 66); IVORY = (220, 202, 164); GOLD = (192, 138, 52)
BROWN = (46, 24, 16); TEAL = (42, 92, 88); ROSE = (176, 74, 62)

TAU = 2 * math.pi


def hash01(i, j):
    h = (i * 374761393 + j * 668265263) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0


def rosette(lx, ly, rr):
    r = math.hypot(lx, ly)
    if r > rr:
        return None
    th = math.atan2(ly, lx)
    edge = rr * (0.58 + 0.42 * abs(math.cos(4 * th)))
    if r > edge:
        return None
    if r > edge - 0.55:
        return BROWN
    if r < rr * 0.22:
        return RED
    if r < rr * 0.38:
        return IVORY
    if r < rr * 0.44:
        return BROWN
    return GOLD if abs(math.cos(4 * th)) > 0.35 else ROSE


def border(a, b):
    """The main border's ground (navy) with rosettes every 26 cm along a and a vine between; b across it (-10.5..10.5)."""
    k = round(a / 26.0)
    c = rosette(a - k * 26.0, b, 8.5)
    if c:
        return c
    vine = 5.2 * math.sin(TAU * (a - 13.0) / 52.0)
    if abs(b - vine) < 0.75:
        return IVORY
    # A leaf at each crest of the vine, turned along it.
    m = a - 13.0
    kk = round(m / 26.0)
    la, lb = m - kk * 26.0, b - 5.2 * (1 if kk % 2 == 0 else -1) * 1.0
    if (la / 5.0) ** 2 + (lb / 2.2) ** 2 < 1:
        return TEAL if (la / 5.0) ** 2 + (lb / 2.2) ** 2 < 0.75 else BROWN
    return NAVY


def medallion(x, y):
    """Lobed medallion with pendants; None outside."""
    px, py = (x - LX / 2) / 66.0, (y - LY / 2) / 50.0
    r = math.hypot(px, py)
    th = math.atan2(py, px)
    R = 1 + 0.10 * math.cos(8 * th) + 0.05 * math.cos(16 * th)
    s = r / R
    if s <= 1.0:
        if s > 0.965:
            return BROWN
        if s > 0.90:
            return GOLD
        if s > 0.885:
            return BROWN
        if s < 0.34:
            star = r / (0.30 * (1 + 0.38 * math.cos(8 * th)))
            if star < 0.45:
                return GOLD
            if star < 1:
                return RED2 if star < 0.93 else BROWN
            return IVORY
        if s < 0.36:
            return BROWN
        if s < 0.42:
            return GOLD
        if s < 0.435:
            return BROWN
        # The navy ring: ivory buds on its spokes, small red dots between.
        k = round(th / (TAU / 16))
        dth = th - k * TAU / 16
        bx, by = (s - 0.66) / 0.10, dth / 0.12
        if bx * bx + by * by < 1:
            return IVORY if k % 2 == 0 else ROSE
        if abs(dth) < 0.012 * 3 and 0.45 < s < 0.86 and k % 2 == 0:
            return GOLD
        return NAVY
    # Pendants at the ends along the length, on a short stem.
    for sgn in (-1, 1):
        cx = LX / 2 + sgn * 84.0
        dx, dy = (x - cx) / 13.0, (y - LY / 2) / 10.0
        q = math.hypot(dx, dy)
        if q < 1:
            if q > 0.86:
                return BROWN
            if q > 0.7:
                return GOLD
            return NAVY if q > 0.32 else IVORY
        if abs(y - LY / 2) < 1.1 and min(cx, LX / 2 + sgn * 66) < x < max(cx, LX / 2 + sgn * 66):
            return GOLD
    return None


def spandrel(x, y, f0):
    """Navy quarter-lobes in the field's corners."""
    for cx in (f0, LX - f0):
        for cy in (f0, LY - f0):
            dx, dy = abs(x - cx) / 48.0, abs(y - cy) / 36.0
            r = math.hypot(dx, dy)
            th = math.atan2(dy, dx)
            R = 1 + 0.12 * math.cos(6 * th)
            s = r / R
            if s < 1:
                if s > 0.95:
                    return BROWN
                if s > 0.88:
                    return GOLD
                if s > 0.865:
                    return BROWN
                k = round(th / (math.pi / 8))
                bx, by = (s - 0.55) / 0.09, (th - k * math.pi / 8) / 0.1
                if bx * bx + by * by < 1:
                    return IVORY if k % 2 else ROSE
                return NAVY
    return None


def field(x, y):
    # An all-over lattice of small motifs, offset every other row.
    sx, sy = 19.0, 16.0
    j = math.floor(y / sy)
    ox = (sx / 2) if j % 2 else 0
    i = math.floor((x - ox) / sx)
    lx, ly = x - ox - (i + 0.5) * sx, y - (j + 0.5) * sy
    d = abs(lx) / 3.4 + abs(ly) / 3.0
    if d < 1:
        if d > 0.8:
            return BROWN
        return IVORY if d > 0.38 else NAVY
    # Four dots round it.
    for ex, ey in ((5.2, 0), (-5.2, 0), (0, 4.6), (0, -4.6)):
        if (lx - ex) ** 2 + (ly - ey) ** 2 < 0.9:
            return GOLD
    return RED


def colour(x, y):
    d = min(x, LX - x, y, LY - y)
    if d < 1.6:
        return BROWN
    near_y = min(y, LY - y) < min(x, LX - x)
    a = x if near_y else y
    if d < 6.2:
        # The outer guard: ivory with a run of small red and navy diamonds.
        b = d - 3.9
        k = round(a / 3.6)
        q = abs(a - k * 3.6) / 1.5 + abs(b) / 1.5
        if q < 1:
            return RED if k % 2 else NAVY
        return IVORY
    if d < 7.2:
        return BROWN
    if d < 28.0:
        # The corners: a rosette square in each.
        cx, cy = min(x, LX - x), min(y, LY - y)
        if cx < 28 and cy < 28:
            c = rosette(cx - 17.6, cy - 17.6, 9.0)
            return c or NAVY
        return border(a - 17.6, d - 17.6)
    if d < 29.0:
        return BROWN
    if d < 33.2:
        b = d - 31.1
        k = round(a / 4.2)
        if (a - k * 4.2) ** 2 + b * b < 0.85:
            return RED
        return GOLD
    if d < 34.2:
        return BROWN
    return medallion(x, y) or spandrel(x, y, 34.2) or field(x, y)


def shade(c, x, y, px, py):
    # Abrash: dye-lot bands across the rows (rows run across the width, stacked along the length).
    ab = 1 + 0.06 * math.sin(x * 0.045 + 1.7 * math.sin(x * 0.011)) + 0.025 * math.sin(x * 0.31)
    # Knots: a speckle per knot (about 0.6 cm) and a faint row ridge.
    kn = 0.93 + 0.12 * hash01(int(x / 0.6), int(y / 0.6)) + 0.03 * math.sin(px * 2.2)
    # Wear: the middle a little lighter and greyer where he walked.
    wx, wy = (x - LX * 0.52) / 120.0, (y - LY * 0.5) / 90.0
    wear = max(0.0, 1 - (wx * wx + wy * wy)) * (0.6 + 0.4 * hash01(int(x / 9), int(y / 9)))
    out = []
    lum = (c[0] + c[1] + c[2]) / 3
    for ch in c:
        v = ch * ab * kn
        v = v + (lum - v) * 0.12                      # a little dust in the dyes
        v = v + (lum * 1.08 - v) * 0.22 * wear + 10 * wear
        out.append(max(0, min(255, int(v + 0.5))))
    return out


def main():
    rows = []
    for py in range(H):
        y = (py + 0.5) / H * LY
        row = bytearray([0])
        for px in range(W):
            x = (px + 0.5) / W * LX
            row.extend(shade(colour(x, y), x, y, px, py))
        rows.append(bytes(row))
    raw = b''.join(rows)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'wb') as f:
        f.write(png)
    print('CHUCK_SHIP_RUG_PNG', OUT, W, H, len(png))


if __name__ == '__main__':
    main()
