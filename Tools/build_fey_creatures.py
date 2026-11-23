"""Blender 4.5.14: the Feywild's creatures, stage 5 part two (Claude, 2026-11-22; docs/regions/FEYWILD.md#stage-5-part-two).

After the 2D game's PHASE-9.md sections 7.1, 7.2 and 7.5 and the goal image References/Feywild/redcap-camp.png. Everything
is modelled as signed distance fields with the kit and helpers of Tools/build_phleg_creatures.py (imported, unchanged):

* SK_Redcap: a gnome-sized, wizened, vicious fey on the temple skeleton's own rig (scaled down in the game), so the
  skeleton's motion-capture clips drive it: a long drooping cap soaked blood red, pointed ears, a wild grey beard and
  hair, a long hooked nose, a wide grin of sharp teeth, yellow eyes (glowing), ragged red tatters over a leather jerkin,
  wrapped forearms, long clawed fingers, huge iron-banded boots, and a rusted sickle in its right fist.
* SM_Displacer{Body,Head,Upper,Lower,Tail,Tentacle,Pad}: the displacer beast, a great six-legged blue-black panther with
  two long tentacles from its shoulders, each ending in a spiked pad. Static parts, each about its own joint at the
  origin (+X forward, Z up), posed in code (FeyCreatures.cpp): the legs are an upper and a lower segment hanging down
  -Z from their joints, the tentacles chains of one segment along +X ending in the pad, the tail along -X.
* SM_ThornMite{Body,Leg}: a thorn mite, smaller than Chuck: a hunched, bark-shelled body bristling with thorns, small
  ember eyes, mandibles; one leg (out along +Y from its joint, then down) used six times.

Weights: as the lemure's (every primitive belongs to a bone). Colour: vertex colour RGB the albedo with its occlusion,
A the glow (eyes). The game's material is the Phlegethos creatures' (M_PhlegCreature) with instances of its own.

blender --background --factory-startup --python Tools/build_fey_creatures.py -- [--only redcap,displacer,mite] [--review DIR]
  -> SourceAssets/Enemies/Feywild/*.fbx and creatures.json (each mesh's UV scale, cm per UV unit)
"""
from pathlib import Path
import importlib.util
import json
import math
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default
ONLY = (arg('--only') or 'redcap,displacer,mite').split(',')
REVIEW = arg('--review')

_spec = importlib.util.spec_from_file_location('phleg_creatures', ROOT / 'Tools/build_phleg_creatures.py')
P = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(P)
P.OUT = ROOT / 'SourceAssets/Enemies/Feywild'
P.REVIEW = REVIEW
import sdf_kit as K  # noqa: E402
from sdf_kit import Balls, Ellipsoids, fbm, smooth  # noqa: E402
Body, ell, V3, lin = P.Body, P.ell, P.V3, P.lin
H, T, along, dirn = P.H, P.T, P.along, P.dirn
MANIFEST = {}


def norm(v):
    v = np.asarray(v, float); return v / np.linalg.norm(v)


# ------------------------------------------------------------------ the redcap (on the temple rig, rest pose in cm)
REDCAP_EYES = [V3(15.6, 4.6, 170.4), V3(15.6, -4.6, 170.4)]
SICKLE_GRIP = V3(9.5, -43.4, 84.)


def redcap_body():
    B = Body(tag_reach=.6)
    # A compact, hunched trunk with a pot belly; the leather jerkin over it (tag 'jerkin').
    B.ells('pelvis', [ell((1, 0, 100), (15, 16.5, 13))], tag='jerkin', k=4)
    B.ells('spine_01', [ell((7, 0, 111), (16, 16.5, 13.5))], tag='jerkin', k=5)
    B.ells('spine_02', [ell((2, 0, 124), (14.5, 18, 12))], tag='jerkin', k=5)
    B.ells('spine_03', [ell((1, 0, 137), (13.5, 19, 12.5)), ell((-8, 0, 145), (11, 16, 10))], tag='jerkin', k=5)
    for s, S in ((1, 'l'), (-1, 'r')):
        B.balls(f'clavicle_{S}', [V3(0, 17 * s, 143)], [8.5], tag='rag', k=4)
    # The ragged mantle over the shoulders and down the back, its hem torn in tongues.
    B.ells('spine_03', [ell((-6, 0, 140), (13, 21.5, 12)), ell((-11, 0, 128), (8, 18, 15))], tag='rag', k=4)
    rng = np.random.default_rng(23)
    for i in range(9):
        y = -17 + i * 4.25 + rng.uniform(-1, 1)
        top = V3(-13 + abs(y) * .15, y, 125); L = rng.uniform(14, 26)
        B.tube('spine_02', [top, top + V3(-1.5, y * .05, -L * .5), top + V3(-1, y * .08, -L)], [3.2, 2.4, .9], tag='rag', k=2.)
    # Tatters hung from the belt at the sides and back (clear of the legs' swing in front).
    for i in range(10):
        a = math.radians(70 + i * 24.5); top = V3(1 + 15.5 * math.cos(a), 17 * math.sin(a), 94); L = rng.uniform(12, 20)
        out = norm([math.cos(a), math.sin(a), 0])
        B.tube('pelvis', [top, top + out * 2.5 + V3(0, 0, -L * .5), top + out * 3.5 + V3(0, 0, -L)], [3.4, 2.6, .8], tag='rag', k=2.)
    # Short thick neck; a big head: brow, cheeks, the long hooked nose, pointed ears.
    B.tube('neck_01', [V3(0, 0, 147), V3(3, 0, 155), V3(4, 0, 161)], [8.5, 7.5, 7.2], k=4)
    B.ells('head', [ell((5.5, 0, 168), (12, 10.5, 12.5)), ell((10, 0, 162), (8.5, 9.5, 7))], k=4)
    B.ells('head', [ell((13.5, 0, 173.5), (3.5, 9.5, 2.6))], k=2.5)                           # the heavy brow
    for s in (1, -1):
        B.ells('head', [ell((13, s * 6.5, 165.5), (4, 3.6, 3.4))], k=2.5)                     # cheeks
        ear = [V3(3, s * 10, 169), V3(-1, s * 16, 172.5), V3(-6, s * 22.5, 177.5)]
        B.tube('head', ear, [3.6, 2.2, .3], k=1.6)
        B.ells('head', [ell((-1, s * 15.5, 172.3), (1.2, 4.8, 3.2), V3(-.45, s * .85, .45))], k=1.2)
    B.tube('head', [V3(16.5, 0, 168.5), V3(22, 0, 166), V3(26.5, 0, 161.5), V3(26.5, 0, 157.5)], [3., 2.6, 2.1, 1.3], k=2.)   # nose
    B.balls('head', [V3(18.5, 0, 165.5)], [3.2], k=1.6)
    # Eyes deep under the brow; the grin, wide, full of sharp teeth.
    B.balls('head', REDCAP_EYES, [1.9, 1.9], tag='eye', k=.25)
    B.ells('jaw', [ell((12, 0, 157.5), (6.5, 8.5, 4.2))], k=3)
    B.ells('jaw', [ell((16.8, 0, 159.2), (1.6, 7.6, 1.5))], tag='mouth', k=.6)
    for i in range(7):
        y = -6 + i * 2.
        top = V3(18.1 - abs(y) * .22, y, 160.7); B.tube('head', [top, top + V3(.4, 0, -2.2)], [.8, .05], tag='tooth', k=.12)
        bot = V3(17.9 - abs(y) * .22, y + 1, 157.6); B.tube('jaw', [bot, bot + V3(.4, 0, 1.8)], [.7, .05], tag='tooth', k=.12)
    # The beard, wild, grey-white: a mass from the cheeks and chin with long locks hanging down to the belly.
    B.ells('jaw', [ell((13, 0, 152), (6.5, 11, 8))], tag='hair', k=3)
    for i in range(11):
        y = -9 + i * 1.8 + rng.uniform(-.6, .6)
        top = V3(14 - abs(y) * .3, y, 152); L = rng.uniform(14, 24) * (1 - abs(y) / 18)
        B.tube('jaw', [top, top + V3(1.5, y * .15, -L * .5), top + V3(.5 + rng.uniform(-1, 1), y * .25, -L)], [3., 2.4, 1.3], tag='hair', k=2.)
    # Hair escaping the cap at the sides and back.
    for i in range(12):   # (2026-11-22: only at the sides and back, blunt and clumped: thin pale locks near the face read as fangs)
        a = math.radians(105 + i * 13.6); p = V3(1 + 11 * math.cos(a), 11.4 * math.sin(a), 171)
        d = norm([math.cos(a) * .15 - .15, math.sin(a) * .2, -1]); L = rng.uniform(8, 13)
        B.tube('head', [p, p + d * L * .5, p + d * L + V3(rng.uniform(-1, 1), 0, 0)], [2.8, 2.3, 1.4], tag='hair', k=1.8)
    # The cap: a stiff band round the head, a tall cone that flops back and over to one side, soaked red.
    cap = P.catmull([V3(4, 0, 174), V3(2, 0, 186), V3(-5, -2, 198), V3(-16, -6, 203), V3(-26, -10, 197), V3(-31, -12, 186), V3(-30, -12, 177)], 48)
    B.tube('head', cap, 13.2 * (1 - np.linspace(0, 1, 48)) ** 1.15 + .8, tag='cap', k=4.)
    B.ells('head', [ell((4.5, 0, 175.5), (12.4, 11.6, 3.2))], tag='cap', k=1.5)
    # Arms: wiry; the forearms wrapped in strips; long clawed fingers on the rig's own finger bones.
    for S in 'lr':
        P.limb(B, f'upperarm_{S}', 6.2, 5., ext0=2)
        P.limb(B, f'lowerarm_{S}', 5., 3.9, tag='wrap')
        P.limb(B, f'hand_{S}', 3.6, 3.2)
        B.ells(f'hand_{S}', [ell(along(f'hand_{S}', .6), (2.6, 4., 4.5), dirn(f'hand_{S}'))], k=1.8)
        for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
            for j, r0, r1 in ((1, 1.45, 1.2), (2, 1.2, 1.), (3, 1., .7)):
                b = f'{f}_0{j}_{S}'
                B.tube(b, [H(b), T(b)], [r0, r1], k=.6)
            b = f'{f}_03_{S}'
            B.tube(b, [T(b) - dirn(b) * .4, T(b) + dirn(b) * 2.4], [.75, .05], tag='claw', k=.3)
    # Legs in baggy breeches to the knee, wrapped shins, and huge iron-shod boots.
    for s, S in ((1, 'l'), (-1, 'r')):
        P.limb(B, f'thigh_{S}', 10.5, 9.5, tag='jerkin', ext0=3)
        B.balls(f'thigh_{S}', [along(f'thigh_{S}', .85), along(f'thigh_{S}', .5) + V3(1, s * 1.5, 0)], [10, 11], tag='jerkin', k=3)
        P.limb(B, f'calf_{S}', 6.3, 5.4, tag='wrap')
        # the boot's shaft to mid-calf, wide at its cuff
        c0 = along(f'calf_{S}', .98) + V3(0, 0, 1); c1 = along(f'calf_{S}', .22)
        B.tube(f'calf_{S}', [c0, (c0 + c1) / 2, c1], [10.4, 9.6, 10.6], tag='boot', k=2)
        B.ells(f'calf_{S}', [ell(c1 + V3(0, 0, 1.5), (12.4, 12.4, 4.2))], tag='boot', k=1.5)   # the turned-down cuff
        B.ells(f'foot_{S}', [ell(V3(9, s * 13.5, 6.5), (15.5, 9.5, 8))], tag='boot', k=3)
        B.ells(f'ball_{S}', [ell(V3(21, s * 15, 5.5), (9.5, 8.2, 6.5))], tag='boot', k=3)
        B.ells(f'foot_{S}', [ell(V3(-4, s * 12.5, 4.5), (6, 8, 4.5))], tag='iron', k=1.5)              # the heel's iron
        for z in (12, 24, 36):                                                                          # the iron bands
            ring = V3(1.5, s * 12.2, z)
            B.ells(f'calf_{S}', [ell(ring, (11.2, 11.2, 1.5))], tag='iron', k=.8)
        B.ells(f'ball_{S}', [ell(V3(26.5, s * 15.5, 4), (4.8, 7.8, 4.6))], tag='iron', k=.8)            # the toe cap
        B.balls(f'calf_{S}', [V3(1.5 + 11.1 * math.cos(a), s * 12.2 + 11.1 * math.sin(a), z) for z in (12, 24, 36) for a in np.linspace(0, 2 * math.pi, 9)[:-1]],
                [.9] * 24, tag='iron', k=.3)                                                               # rivets
    # The sickle: a wrapped handle through the right fist, forward, then a crescent of rusted iron curving up and back.
    g = SICKLE_GRIP
    B.tube('hand_r', [g + V3(-7, 0, 0), g + V3(12, 0, .5)], [1.6, 1.5], tag='handle', k=.4)
    arc = [g + V3(12 + 20 * math.sin(a), 0, 20 - 20 * math.cos(a) + 1) for a in np.linspace(0, math.radians(200), 16)]
    widths = np.concatenate([np.linspace(1.6, 2.4, 5), np.linspace(2.4, .2, 11)])
    B.tube('hand_r', arc, widths, tag='blade', k=.4)
    B.disp(lambda Q: .25 * fbm(Q / 1.6, 2, 81) + .35 * fbm(Q / 5., 2, 83))
    return B


def redcap_look(body, ops, V, N):
    tags = body.tag_mask(V)
    convex = np.clip(P.laplacian(ops, V, 2.) * 3., -1, 1)
    n1 = fbm(V / 7., 2, 5) * .5 + .5; n2 = fbm(V / 2.2, 2, 9) * .5 + .5
    skin = lin(150, 132, 112)[None] * (.85 + .3 * n1[:, None])
    col = skin.copy()
    def put(tag, c):
        nonlocal col
        m = tags.get(tag, np.zeros(len(V)))[:, None]; col = col * (1 - m) + c * m
    put('jerkin', lin(74, 56, 40)[None] * (.7 + .5 * n2[:, None]))
    put('rag', lin(92, 22, 22)[None] * (.65 + .55 * n1[:, None]))
    put('wrap', lin(120, 104, 82)[None] * (.75 + .45 * (np.sin(V[:, 2] * 1.6 + V[:, 0]) * .5 + .5)[:, None]))
    put('hair', lin(168, 162, 152)[None] * (.7 + .3 * n2[:, None]))
    blood = smooth(.35, .75, fbm(V / 6., 3, 13) * .5 + .5)[:, None]
    put('cap', lin(124, 14, 16)[None] * (1 - .55 * blood) + lin(52, 6, 8)[None] * (.55 * blood))
    put('boot', lin(58, 40, 28)[None] * (.75 + .4 * n2[:, None]))
    put('iron', lin(78, 76, 76)[None] * (.7 + .4 * n2[:, None]))
    put('handle', lin(70, 48, 30)[None])
    rust = smooth(.3, .7, fbm(V / 3., 2, 17) * .5 + .5)[:, None]
    put('blade', lin(112, 108, 100)[None] * (1 - rust) + lin(120, 64, 34)[None] * rust)
    put('tooth', lin(222, 204, 160)[None])
    put('claw', lin(52, 40, 30)[None])
    put('mouth', lin(26, 8, 8)[None])
    put('eye', lin(255, 210, 60)[None])
    # Blood spattered on the sickle's edge and the cap's tip, dirt low on the boots.
    hi = smooth(.05, .6, convex)[:, None]; lo = smooth(-.05, -.6, convex)[:, None]
    col = col * (1 - .3 * hi) + col * 1.5 * (.3 * hi)
    col = col * (1 - .45 * lo) + lin(30, 20, 16)[None] * (.45 * lo)
    ao = P.sdf_ao(V, N, ops)
    col = col * (.3 + .7 * ao)[:, None]
    alpha = tags.get('eye', np.zeros(len(V))) * .95
    return col, alpha


# ------------------------------------------------------------------ the displacer beast (static parts, cm)
DISP = dict(body_len=230., upper=62., lower=66., seg=26., pad=30., tail=150.)
DISP_HIPS = {'FL': V3(78, 30, -6), 'FR': V3(78, -30, -6), 'ML': V3(6, 31, -8), 'MR': V3(6, -31, -8), 'HL': V3(-74, 29, -2), 'HR': V3(-74, -29, -2)}
DISP_HEAD_AT = V3(128, 0, 34)
DISP_TENTACLE_AT = {'L': V3(66, 20, 24), 'R': V3(66, -20, 24)}
DISP_TAIL_AT = V3(-112, 0, 8)
DISP_EYES = [V3(19.5, 8.2, 8.5), V3(19.5, -8.2, 8.5)]


def displacer_body():
    B = Body(tag_reach=.8)
    # A long, lean, deep-chested cat: shoulders high, a tucked waist, strong haunches.
    for x, z, r in ((96, 4, (34, 24, 28)), (66, 5, (36, 27, 31)), (36, 2, (36, 25, 28)), (6, -1, (34, 21, 23)), (-24, 0, (34, 21, 22)),
                    (-54, 2, (34, 24, 25)), (-84, 4, (34, 27, 27))):
        B.ells('b', [ell((x, 0, z), r)], k=12)
    for y in (1, -1):
        B.ells('b', [ell((76, y * 24, -10), (24, 14, 22))], k=10)       # shoulder blades and upper fore-legs' muscle
        B.ells('b', [ell((-80, y * 23, -4), (28, 15, 24))], k=10)       # haunches
        B.ells('b', [ell((8, y * 25, -10), (18, 12, 15))], k=8)
        B.ells('b', [ell((66, y * 16, 26), (12, 9, 8))], k=6)           # the tentacles' roots on the shoulders
    B.tube('b', [V3(96, 0, 14), V3(112, 0, 26), V3(126, 0, 34)], [22, 17, 14], k=10)   # neck
    B.disp(lambda Q: .5 * fbm(Q / 6., 2, 41) + .25 * fbm(Q / 1.6, 2, 43))
    return B


def displacer_head():
    B = Body(tag_reach=.8)
    # Pivot at the neck's end: a broad cat skull, a short muzzle, heavy jaw, tufted ears laid back; green eyes.
    B.ells('h', [ell((6, 0, 6), (17, 15, 15)), ell((21, 0, 0), (12, 10.5, 9)), ell((14, 0, -7), (13, 10, 6.5))], k=6)
    B.ells('h', [ell((30, 0, 0), (5, 7, 5))], k=4)                       # the nose pad's bulk
    for s in (1, -1):
        B.tube('h', [V3(1, s * 9, 16), V3(-6, s * 13, 25), V3(-14, s * 14, 30)], [6.5, 4, .6], k=3)   # ears
        B.tube('h', [V3(28, s * 3.5, -6), V3(29.5, s * 3.3, -13), V3(29, s * 3, -17)], [1.6, 1.2, .1], tag='fang', k=.5)
        B.ells('h', [ell((22, s * 9.5, -1), (9, 5, 7))], k=4)            # whisker pads, cheeks
    B.balls('h', DISP_EYES, [3., 3.], tag='eye', k=.4)
    B.ells('h', [ell((25.5, 0, -8.6), (1.6, 6.5, 1.1))], tag='mouth', k=.5)
    B.disp(lambda Q: .3 * fbm(Q / 4., 2, 45) + .15 * fbm(Q / 1.2, 2, 47))
    return B


def displacer_upper():
    B = Body()
    L = DISP['upper']
    B.tube('u', [V3(0, 0, 4), V3(2, 0, -L * .45), V3(0, 0, -L)], [15, 12, 8.5], k=5)
    B.disp(lambda Q: .35 * fbm(Q / 4., 2, 49))
    return B


def displacer_lower():
    B = Body()
    L = DISP['lower']
    B.tube('l', [V3(0, 0, 3), V3(-1, 0, -L * .5), V3(0, 0, -L + 6)], [8.8, 6.8, 6.4], k=3)
    B.ells('l', [ell((6, 0, -L + 4), (12, 9.5, 5.5))], k=4)              # the paw
    for y in (-4.5, -1.5, 1.5, 4.5):
        B.ells('l', [ell((15, y, -L + 2.6), (3.2, 2.3, 2.6))], k=1.2)     # toes
    B.disp(lambda Q: .3 * fbm(Q / 3.5, 2, 51))
    return B


def displacer_tail():
    B = Body()
    pts = P.catmull([V3(0, 0, 0), V3(-40, 0, -8), V3(-85, 4, -2), V3(-125, 8, 14), V3(-150, 6, 30)], 14)
    B.tube('t', pts, np.linspace(9.5, 3.4, 14), k=4)
    B.ells('t', [ell((-146, 6, 27), (12, 6.5, 6.5), V3(-.8, 0, .6))], k=4)   # the tuft
    B.disp(lambda Q: .3 * fbm(Q / 3., 2, 53))
    return B


def displacer_tentacle():
    B = Body()
    S = DISP['seg']
    B.tube('s', [V3(-1.5, 0, 0), V3(S * .5, 0, .3), V3(S + 1.5, 0, 0)], [5.2, 4.8, 5.], k=2)
    for x in (S * .25, S * .75):
        B.ells('s', [ell((x, 0, 0), (2.4, 5.6, 5.6))], tag='ring', k=1.2)    # its segment rings
    B.disp(lambda Q: .2 * fbm(Q / 2., 2, 55))
    return B


def displacer_pad():
    B = Body()
    L = DISP['pad']
    B.ells('p', [ell((L * .5, 0, 0), (L * .5, 10, 5.5))], k=3)
    B.tube('p', [V3(-1.5, 0, 0), V3(L * .2, 0, 0)], [5, 6], k=2)
    for i in range(6):
        x = 5 + i * 4.2
        for s in (1, -1):
            a = V3(x, s * 8.5, 1); B.tube('p', [a, a + V3(2, s * 6, 3.5)], [1.6, .1], tag='spike', k=.4)
        a = V3(x, 0, 4.5); B.tube('p', [a, a + V3(2.5, 0, 6)], [1.4, .1], tag='spike', k=.3)
    B.disp(lambda Q: .2 * fbm(Q / 2., 2, 57))
    return B


def fur_look(body, ops, V, N, glow_tag='eye'):
    tags = body.tag_mask(V)
    n1 = fbm(V / 9., 2, 61) * .5 + .5; streak = np.sin(V[:, 0] / 4. + 3 * fbm(V / 12., 2, 63)) * .5 + .5
    col = lin(26, 30, 44)[None] * (.75 + .3 * n1[:, None] + .15 * streak[:, None])
    # a faint blue sheen along the back, lighter belly
    col = col * (1 + .22 * smooth(.4, .95, N[:, 2])[:, None] * np.array([.6, .8, 1.3])[None])
    col = col * (1 - .3 * smooth(-.2, -.8, N[:, 2])[:, None])
    for tag, c in (('mouth', lin(10, 6, 8)), ('fang', lin(225, 215, 190)), ('spike', lin(210, 200, 180)), ('ring', lin(40, 46, 64)), ('eye', lin(120, 255, 150))):
        m = tags.get(tag, np.zeros(len(V)))[:, None]; col = col * (1 - m) + c[None] * m
    ao = P.sdf_ao(V, N, ops)
    col = col * (.35 + .65 * ao)[:, None]
    return col, tags.get(glow_tag, np.zeros(len(V))) * .95


# ------------------------------------------------------------------ the thorn mite (static parts, cm)
MITE_LEGS = [V3(5, 5, 4), V3(0, 5.6, 4), V3(-5, 5, 4)]   # the left legs' joints; the right mirror them
MITE_EYES = [V3(9.9, 1.6, 6.), V3(9.9, -1.6, 6.)]


def mite_body():
    B = Body(tag_reach=.3)
    B.ells('m', [ell((-1, 0, 7), (9, 6.5, 5.5)), ell((7.2, 0, 5), (3.4, 3.6, 2.9))], k=1.6)
    for i in range(2):
        for s in (1, -1):
            B.tube('m', [V3(9.6, s * 1.3, 4.2 - i * .6), V3(11.6, s * 2.2, 3.8 - i * .6), V3(12.8, s * .5, 3.4 - i * .6)], [.5, .35, .05], tag='claw', k=.2)   # pincers
    B.balls('m', MITE_EYES, [.6, .6], tag='eye', k=.08)
    rng = np.random.default_rng(71)
    for i in range(26):
        a = rng.uniform(0, 2 * math.pi); h = rng.uniform(.15, 1.)
        c = V3(-1 + 8 * math.cos(a) * math.sqrt(1 - h * h * .6), 5.7 * math.sin(a) * math.sqrt(1 - h * h * .6), 7 + 5 * h)
        out = norm(c - V3(-1, 0, 5)); L = rng.uniform(2.5, 5.)
        B.tube('m', [c - out * .6, c + out * L * .6 + V3(0, 0, .6), c + out * L + V3(-.8, 0, .4)], [.85, .5, .05], tag='thorn', k=.4)
    B.disp(lambda Q: .12 * fbm(Q / 1., 2, 73))
    return B


def mite_leg():
    B = Body()
    pts = [V3(0, 0, 0), V3(0, 4, 2.5), V3(.5, 7.5, 1.5), V3(1, 9, -4)]
    B.tube('k', pts, [1., .8, .6, .2], k=.5)
    B.tube('k', [V3(0, 4, 3), V3(0, 4.6, 4.8)], [.4, .05], tag='thorn', k=.2)
    return B


def mite_look(body, ops, V, N):
    tags = body.tag_mask(V)
    n = fbm(V / 2., 2, 75) * .5 + .5
    col = lin(70, 58, 38)[None] * (.6 + .6 * n[:, None])
    col = col * (1 - .4 * smooth(.3, .8, N[:, 2])[:, None]) + lin(64, 88, 40)[None] * (.4 * smooth(.3, .8, N[:, 2])[:, None])   # moss on its back
    for tag, c in (('thorn', lin(150, 128, 96)), ('claw', lin(40, 28, 20)), ('eye', lin(255, 120, 40))):
        m = tags.get(tag, np.zeros(len(V)))[:, None]; col = col * (1 - m) + c[None] * m
    ao = P.sdf_ao(V, N, ops, .3)
    return col * (.35 + .65 * ao)[:, None], tags.get('eye', np.zeros(len(V))) * .9


# ------------------------------------------------------------------ main
def static(name, body, h, ratio, look):
    ob, V = P.field_object(name, body, h, ratio, look)
    MANIFEST[name] = {'uv_cm': round(P.unwrap(ob), 2)}
    P.add_material(ob, 'Skin')
    if REVIEW: P.review(ob, name)
    P.export_static(ob, name)


if __name__ == '__main__':
    if 'redcap' in ONLY:
        rig = P.rig_build()
        body = redcap_body()
        ob, V = P.field_object('SK_Redcap', body, .55, .3, redcap_look)
        P.add_weights(ob, body.weights(V))
        MANIFEST['SK_Redcap'] = {'uv_cm': round(P.unwrap(ob), 2)}
        P.add_material(ob, 'Skin')
        if REVIEW: P.review(ob, 'SK_Redcap')
        P.export_skinned(rig, ob, 'SK_Redcap')
    if 'displacer' in ONLY:
        static('SM_DisplacerBody', displacer_body(), 1.3, .3, fur_look)
        static('SM_DisplacerHead', displacer_head(), .5, .3, fur_look)
        static('SM_DisplacerUpper', displacer_upper(), .8, .25, fur_look)
        static('SM_DisplacerLower', displacer_lower(), .6, .25, fur_look)
        static('SM_DisplacerTail', displacer_tail(), .7, .25, fur_look)
        static('SM_DisplacerTentacle', displacer_tentacle(), .6, .2, fur_look)
        static('SM_DisplacerPad', displacer_pad(), .4, .3, fur_look)
    if 'mite' in ONLY:
        static('SM_ThornMiteBody', mite_body(), .2, .18, mite_look)
        static('SM_ThornMiteLeg', mite_leg(), .15, .3, mite_look)
    P.OUT.mkdir(parents=True, exist_ok=True)
    old = json.loads((P.OUT / 'creatures.json').read_text()) if (P.OUT / 'creatures.json').exists() else {}
    old.update(MANIFEST)
    (P.OUT / 'creatures.json').write_text(json.dumps(old, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_FEY_CREATURES_BUILT', sorted(MANIFEST))
