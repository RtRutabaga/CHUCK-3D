"""Chuck v1.1/v1.2 body proportions (docs/RIG-CONTRACT-V1.md, "v1.1 shape amendment").

The accepted v1.0 bone table (SourceAssets/Chuck/rig_proposal.json) and the
legacy study geometry (Tools/build_chuck_model.py) are authored in the same
source space. This module maps both through one function, so the mesh, the
bones, the clips and every validator agree on the v1.1 proportions without
editing the accepted table.

Measured on References/ArtDirection/Chuck-Turnaround.png at 65 cm feet-to-ear
scale, the goal has the crotch at about 19 cm (was 16), the jacket hem at
about 25 cm (was 23), the collar top at about 53 cm (was 49), the eyes at about
57 cm (was 54.6), ears about 7 cm tall (was 8.6) and about 29 cm across the
sleeves (was 34). So:
  * paws (z <= 4 cm) are unchanged, and the legs stretch up to the hip, which
    rises LIFT cm;
  * the torso, arms and jacket ride up LIFT cm;
  * the head compresses back so the ear tip stays at exactly 65 cm;
  * the torso/jacket narrow to TORSO_NARROW between hips and neck, and the
    whole arm chain (clavicle to fingers) narrows to ARM_NARROW.
v1.2 (2026-09-27, Chuck-Snout-Fur-Target.png): the snout is shortened to
SNOUT of its length in front of SNOUT_FROM for every head part and head bone
(head/jaw tails, socket_cigarette), so the mouth corner and the cigarette
move back with it.
Pure Python (no bpy) so Blender, Unreal-side Python and plain scripts can use it.
"""

LIFT = 3.0          # cm the hip and everything up to the neck rise
LEG_FROM = 4.0      # below this (paws, ankles) nothing moves
HIP = 19.5          # source hip height (pelvis/thigh head)
NECK = 47.0         # source height where the head compression starts
TOP = 65.0          # ear tip: contract height, never moves
TORSO_NARROW = .88  # Y scale of torso and jacket between hips and neck
ARM_NARROW = .88    # Y scale of the whole arm chain
LEG_SPREAD = 1.1    # Y scale of the leg chain (hips and paws further apart: the goal's
                    # wider stance; narrow legs plus hip sway read as a waddle)
LEG_BONES = ('thigh', 'calf', 'foot', 'toes', 'ik_foot')
ARM_PARTS = ('Sleeve', 'Cuff', 'Hand', 'Finger', 'Thumb', 'FingerClaw', 'ThumbClaw')
TORSO_PARTS = ('Torso', 'LightChest', 'ChestFur', 'BellyFur', 'OpenJacket', 'Zipper',
               'ZipperTape', 'Pocket', 'HemStitch', 'BackSeam')
ARM_BONES = ('clavicle', 'upperarm', 'lowerarm', 'hand', 'fingers', 'thumb', 'ik_hand')
HEAD_PARTS = ('Head', 'MuzzleLight', 'Nose', 'EyeLid', 'Eye', 'Mouth', 'Whisker', 'CheekFur', 'Ear')
HEAD_BONES = ('head', 'jaw', 'socket_cigarette')
SNOUT = .74         # snout length factor in front of SNOUT_FROM (v1.2; .82 before the face-proportion target)
SNOUT_FROM = 2.0    # source x where the snout compression starts


def _smooth(x, lo, hi):
    t = min(1., max(0., (x - lo) / (hi - lo)))
    return t * t * (3 - 2 * t)


def lift(z):
    """Vertical offset of a source height."""
    if z <= LEG_FROM:
        return 0.
    if z <= HIP:
        return LIFT * (z - LEG_FROM) / (HIP - LEG_FROM)
    if z <= NECK:
        return LIFT
    if z <= TOP:
        return LIFT * (TOP - z) / (TOP - NECK)
    return 0.


def Z(z):
    """Source height -> v1.1 height (use for any hard-coded source height)."""
    return z + lift(z)


def torso_scale(z):
    """Y scale at a source height for torso/jacket geometry."""
    return 1. - (1. - TORSO_NARROW) * _smooth(z, HIP, HIP + 4.5) * (1. - _smooth(z, NECK - 1., NECK + 3.))


def kind_of(label):
    """'arm', 'torso', 'head' or 'other' (legs, paws, tail)."""
    if label in ARM_PARTS: return 'arm'
    if label in TORSO_PARTS: return 'torso'
    return 'head' if label in HEAD_PARTS else 'other'


def X(x):
    """Source x -> v1.2 x for head parts (snout compression)."""
    return x if x <= SNOUT_FROM else SNOUT_FROM + (x - SNOUT_FROM) * SNOUT


def source_x(x):
    """Inverse of X() for head parts."""
    return x if x <= SNOUT_FROM else SNOUT_FROM + (x - SNOUT_FROM) / SNOUT


def point(p, kind='other'):
    """Map a source point (x, y, z) to v1.1."""
    x, y, z = p
    scale = ARM_NARROW if kind == 'arm' else torso_scale(z) if kind == 'torso' else 1.
    return (X(x) if kind == 'head' else x, y * scale, Z(z))


def is_arm_bone(name):
    return name.split('_')[0] in ARM_BONES or name.startswith('ik_hand')


def effective_table(base):
    """v1.1 bone table from the accepted v1.0 table (same names, parents, flags)."""
    out = {}
    for name, bone in base.items():
        kind = 'arm' if is_arm_bone(name) else 'head' if name in HEAD_BONES else 'other'
        b = dict(bone)
        b['head'] = [round(c, 4) for c in point(bone['head'], kind)]
        b['tail'] = [round(c, 4) for c in point(bone['tail'], kind)]
        if name.split('_')[0] in LEG_BONES or name.startswith('ik_foot'):
            for key in ('head', 'tail'):
                b[key][1] = round(b[key][1] * LEG_SPREAD, 4)
        out[name] = b
    return out


def load_effective_table(root):
    """Effective table given the repository root (pathlib.Path)."""
    import json
    base = json.loads((root / 'SourceAssets/Chuck/rig_proposal.json').read_text(encoding='utf-8-sig'))
    return effective_table(base)
